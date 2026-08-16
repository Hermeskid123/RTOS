#include "models/ControlModel/ControlModel.hpp"
#include "models/MotorModel/MotorModel.hpp"
#include "models/SensorModel/SensorModel.hpp"
#include "rtos/logging/Logger.hpp"
#include "rtos/messaging/DispatchPort.hpp"
#include "rtos/model/ModelConfiguration.hpp"
#include "rtos/model/ModelRunner.hpp"

#include <algorithm>
#include <charconv>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <spawn.h>
#include <sys/types.h>
#include <unistd.h>

extern char** environ;
#endif

#if defined(__linux__)
#include <sys/prctl.h>
#endif

namespace {

constexpr std::string_view version{"0.2.0"};

#ifndef RTOS_DEFAULT_MODEL_CONFIG
#define RTOS_DEFAULT_MODEL_CONFIG "xml/models.xml"
#endif

struct LoggingOptions {
    rtos::logging::LogLevel level{rtos::logging::LogLevel::error};
    bool enabled{true};
};

struct ProcessOptions {
    LoggingOptions logging;
    std::filesystem::path modelConfiguration{RTOS_DEFAULT_MODEL_CONFIG};
};

void printHelp()
{
    std::cout << "RTOS Model Simulator\n\n"
                 "Usage: rtos_sim [--debug | --info | --noLogging] [--models <file>]\n\n"
                 "Process options:\n"
                 "  --debug          Include DEBUG and higher log records\n"
                 "  --info           Include INFO and higher log records\n"
                 "  --noLogging      Disable every log record\n"
                 "  --models <file>  Select the models XML configuration\n"
                 "  --help           Show this help message\n"
                 "  --version        Show the simulator version\n\n"
                 "Interactive commands:\n"
                 "  start            Start simulator, models, and execution\n"
                 "  start sim        Initialize the simulator and models\n"
                 "  start models     Begin or resume all models\n"
                 "  stop models      Stop execution and freeze all models\n"
                 "  run              Run continuously in the background\n"
                 "  run <frames>     Execute a fixed number of frames\n"
                 "  status           Show every model control status\n"
                 "  ports            Show all ROS Messaging ports\n"
                 "  messages         Show created and sent message traffic\n"
                 "  models           Show loaded model configuration\n"
                 "  stop sim         Stop execution and terminate all models\n"
                 "  help             Show process options and commands\n"
                 "  quit             Terminate and exit\n";
}

void printReports(const std::vector<rtos::model::ModelStatusReport>& reports)
{
    for (const auto& report : reports) {
        std::cout << report.name << ": " << rtos::model::toString(report.status) << '\n';
    }
}

class SimulatorCli {
public:
    SimulatorCli(
        const LoggingOptions options,
        rtos::model::ModelConfiguration configuration
    )
        : logger_{std::cout, options.level},
          port_{"main"},
          configuration_{std::move(configuration)}
    {
        logger_.setEnabled(options.enabled);
        for (const auto& model : configuration_.models()) {
            logger_.setComponentDebugEnabled(model.name, model.debugEnabled);
        }

        configureDebuggerAttachment();
        createConfiguredModels();
    }

    int run()
    {
        std::cout << "RTOS Model Simulator " << version << "\n"
                  << "Type 'help' for commands.\n";

        std::string command;
        while (std::cout << "rtos> " && std::getline(std::cin, command)) {
            if (!execute(command)) {
                terminateSimulation();
                return 0;
            }
        }

        terminateSimulation();
        return 0;
    }

private:
    bool execute(const std::string& command)
    {
        if (command.empty()) {
            return true;
        }
        if (command == "help") {
            printHelp();
        } else if (command == "start") {
            startAll();
        } else if (command == "start sim" || command == "start-sim") {
            startSimulation();
        } else if (command == "start models" || command == "start-models") {
            startModels();
        } else if (command == "stop models" || command == "stop-models") {
            stopModels();
        } else if (command == "status") {
            printStatuses();
        } else if (command == "ports" || command == "show ports") {
            printPorts();
        } else if (
            command == "messages" || command == "msgs" || command == "show messages"
        ) {
            printMessages();
        } else if (command == "models" || command == "show models") {
            printConfiguredModels();
        } else if (command == "stop sim" || command == "stop-sim") {
            terminateSimulation();
        } else if (command == "quit" || command == "exit") {
            return false;
        } else if (command == "run") {
            startContinuousRun();
        } else if (command.starts_with("run ")) {
            const auto frameCount = parseFrameCount(command);
            if (frameCount.has_value()) {
                runFrames(*frameCount);
            }
        } else {
            std::cout << "Unknown command: " << command << "\n";
        }
        return true;
    }

    void startSimulation()
    {
        std::scoped_lock lock{simulationMutex_};
        if (initialized_) {
            std::cout << "Simulator is already initialized.\n";
            return;
        }

        initialized_ = true;
        launchConfiguredDebuggers();
        logger_.log(
            rtos::logging::LogLevel::info,
            "Host",
            "START",
            "Simulator initialized"
        );
        printReports(runner_.initialize());
    }

    void startAll()
    {
        if (worker_.joinable()) {
            std::cout << "Simulation is already running.\n";
            return;
        }
        startModels();
        startContinuousRun();
    }

    void startModels()
    {
        startSimulationIfNeeded();
        std::scoped_lock lock{simulationMutex_};
        printReports(runner_.begin());
    }

    void stopModels()
    {
        stopWorker();
        std::scoped_lock lock{simulationMutex_};
        if (!initialized_) {
            std::cout << "Simulator is not initialized.\n";
            return;
        }
        printReports(runner_.freeze());
    }

    void terminateSimulation()
    {
        stopWorker();
        std::scoped_lock lock{simulationMutex_};
        if (!initialized_) {
            return;
        }

        printReports(runner_.terminate());
        logger_.log(
            rtos::logging::LogLevel::info,
            "Host",
            "STOP",
            "Simulator terminated"
        );
        initialized_ = false;
    }

    void startContinuousRun()
    {
        if (worker_.joinable()) {
            std::cout << "Simulation is already running.\n";
            return;
        }
        if (!modelsAreRunning()) {
            std::cout << "Start the simulator and models before running frames.\n";
            return;
        }

        worker_ = std::jthread(
            [this](const std::stop_token stopToken)
            {
                while (!stopToken.stop_requested()) {
                    executeFrame();
                    std::this_thread::sleep_for(std::chrono::milliseconds{10});
                }
            }
        );
        std::cout << "Simulation is running in the background.\n";
    }

    void runFrames(const std::size_t count)
    {
        if (worker_.joinable()) {
            std::cout << "Stop the continuous run before executing fixed frames.\n";
            return;
        }
        if (!modelsAreRunning()) {
            std::cout << "Start the simulator and models before running frames.\n";
            return;
        }

        for (std::size_t offset = 0; offset < count; ++offset) {
            executeFrame();
        }
    }

    void executeFrame()
    {
        std::scoped_lock lock{simulationMutex_};
        ++frame_;
        if (sensor_) {
            sensor_->setSensorValue(static_cast<double>(frame_));
        }
        const auto modelReports = runner_.operate();
        static_cast<void>(modelReports);
        const auto dispatch = port_.dispatchAll();

        std::ostringstream message;
        message << "frame=" << frame_
                << " processed=" << dispatch.messagesProcessed()
                << " callbacks=" << dispatch.callbacksInvoked;
        logger_.log(
            rtos::logging::LogLevel::info,
            "ROS.DispatchPort",
            "DISPATCH",
            message.str()
        );
    }

    void stopWorker()
    {
        if (!worker_.joinable()) {
            return;
        }
        worker_.request_stop();
        worker_.join();
        std::cout << "Simulation execution stopped.\n";
    }

    void startSimulationIfNeeded()
    {
        {
            std::scoped_lock lock{simulationMutex_};
            if (initialized_) {
                return;
            }
        }
        startSimulation();
    }

    [[nodiscard]] bool modelsAreRunning() const
    {
        std::scoped_lock lock{simulationMutex_};
        if (!initialized_ || runner_.size() == 0) {
            return false;
        }
        const auto statuses = runner_.statuses();
        return std::ranges::all_of(
            statuses,
            [](const rtos::model::ModelStatusReport& report)
            {
                return report.status == rtos::model::ControlStatus::running;
            }
        );
    }

    void printStatuses() const
    {
        std::scoped_lock lock{simulationMutex_};
        const auto reports = runner_.statuses();
        for (const auto& configuredModel : configuration_.models()) {
            const auto report = std::ranges::find(
                reports,
                configuredModel.name,
                &rtos::model::ModelStatusReport::name
            );
            const auto status = report == reports.end()
                ? rtos::model::ControlStatus::stopped
                : report->status;
            std::cout << configuredModel.name << ": "
                      << rtos::model::toString(status) << '\n';
        }
    }

    void printPorts() const
    {
        std::scoped_lock lock{simulationMutex_};
        const auto topology = port_.portTopology();
        constexpr int numberWidth{8};
        constexpr int nameWidth{24};
        constexpr int messageWidth{18};
        constexpr int connectionWidth{16};

        std::cout << "ROS Messaging Ports — bus " << port_.name()
                  << " (pending=" << port_.pendingMessageCount() << ")\n"
                  << std::left << std::setw(numberWidth) << "Port #"
                  << std::setw(nameWidth) << "Name"
                  << std::setw(messageWidth) << "Message"
                  << std::setw(connectionWidth) << "Publishers"
                  << std::setw(connectionWidth) << "Subscribers" << '\n'
                  << std::string(
                         numberWidth + nameWidth + messageWidth + connectionWidth * 2,
                         '-'
                     )
                  << '\n';

        for (const auto& endpoint : topology) {
            std::cout << std::left << std::setw(numberWidth) << endpoint.number
                      << std::setw(nameWidth) << endpoint.name
                      << std::setw(messageWidth) << endpoint.messageName
                      << std::setw(connectionWidth) << formatPortNumbers(
                             endpoint.publisherPorts
                         )
                      << std::setw(connectionWidth) << formatPortNumbers(
                             endpoint.subscriberPorts
                         )
                      << '\n';
        }
    }

    void printMessages() const
    {
        std::scoped_lock lock{simulationMutex_};
        const auto traffic = port_.messageTraffic();
        if (traffic.empty()) {
            std::cout << "No messages have been registered or sent.\n";
            return;
        }

        constexpr int messageWidth{20};
        constexpr int countWidth{13};

        std::cout << "ROS Messaging Traffic\n"
                  << std::left << std::setw(messageWidth) << "Message"
                  << std::right << std::setw(countWidth) << "Publishers"
                  << std::setw(countWidth) << "Subscribers"
                  << std::setw(countWidth) << "Sent"
                  << std::setw(countWidth) << "Received"
                  << std::setw(countWidth) << "Dispatched"
                  << std::setw(countWidth) << "Unhandled" << '\n'
                  << std::string(messageWidth + countWidth * 6, '-') << '\n';

        for (const auto& message : traffic) {
            std::cout << std::left << std::setw(messageWidth) << message.messageName
                      << std::right << std::setw(countWidth) << message.publishers
                      << std::setw(countWidth) << message.subscribers
                      << std::setw(countWidth) << message.messagesSent
                      << std::setw(countWidth) << message.messagesReceived
                      << std::setw(countWidth) << message.messagesDispatched
                      << std::setw(countWidth) << message.messagesWithoutSubscribers
                      << '\n';
        }
    }

    void printConfiguredModels() const
    {
        std::scoped_lock lock{simulationMutex_};
        constexpr int nameWidth{24};
        constexpr int settingWidth{12};
        constexpr int processWidth{12};
        std::cout << "Model Configuration\n"
                  << std::left << std::setw(nameWidth) << "Model"
                  << std::setw(settingWidth) << "Enabled"
                  << std::setw(settingWidth) << "Debug"
                  << std::setw(settingWidth) << "GDB"
                  << std::setw(processWidth) << "PID" << '\n'
                  << std::string(nameWidth + settingWidth * 3 + processWidth, '-')
                  << '\n';
        for (const auto& model : configuration_.models()) {
            std::cout << std::left << std::setw(nameWidth) << model.name
                      << std::setw(settingWidth) << (model.enabled ? "yes" : "no")
                      << std::setw(settingWidth) << (model.debugEnabled ? "yes" : "no")
                      << std::setw(settingWidth) << (model.gdbEnabled ? "yes" : "no")
                      << std::setw(processWidth) << modelProcessId(model)
                      << '\n';
        }
    }

    void createConfiguredModels()
    {
        const auto& sensorConfig = requireModelConfig("SensorModel");
        const auto& controlConfig = requireModelConfig("ControlModel");
        const auto& motorConfig = requireModelConfig("MotorModel");

        if (sensorConfig.enabled) {
            sensor_ = std::make_unique<rtos::models::SensorModel>(port_, logger_);
            runner_.add("SensorModel", *sensor_);
        }
        if (controlConfig.enabled) {
            control_ = std::make_unique<rtos::models::ControlModel>(port_, logger_);
            runner_.add("ControlModel", *control_);
        }
        if (motorConfig.enabled) {
            motor_ = std::make_unique<rtos::models::MotorModel>(port_, logger_);
            runner_.add("MotorModel", *motor_);
        }
    }

    void configureDebuggerAttachment()
    {
        const bool debuggerRequested = std::ranges::any_of(
            configuration_.models(),
            [](const rtos::model::ModelConfig& model)
            {
                return model.enabled && (model.debugEnabled || model.gdbEnabled);
            }
        );
        if (!debuggerRequested) {
            return;
        }

#if defined(__linux__)
        if (::prctl(PR_SET_DUMPABLE, 1) == -1) {
            std::cerr << "Unable to mark rtos_sim as debugger-attachable: "
                      << std::strerror(errno) << '\n';
            return;
        }
        if (::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) == -1) {
            std::cerr << "Unable to grant GDB ptrace permission: "
                      << std::strerror(errno) << '\n';
            return;
        }
        debuggerAttachmentEnabled_ = true;
#elif defined(__unix__) || defined(__APPLE__)
        debuggerAttachmentEnabled_ = true;
#else
        std::cerr << "Automatic debugger attachment permission is unsupported on this "
                     "platform.\n";
#endif
    }

    [[nodiscard]] const rtos::model::ModelConfig& requireModelConfig(
        const std::string_view name
    ) const
    {
        const auto* config = configuration_.find(name);
        if (config == nullptr) {
            throw std::runtime_error{"Missing model configuration: " + std::string{name}};
        }
        return *config;
    }

    void launchConfiguredDebuggers()
    {
        for (const auto& model : configuration_.models()) {
            if (
                model.enabled && model.gdbEnabled
                && !launchedDebuggers_.contains(model.name)
            ) {
                launchGdb(model);
            }
        }
    }

    void launchGdb(const rtos::model::ModelConfig& model)
    {
        if (!debuggerAttachmentEnabled_) {
            std::cerr << "GDB attachment permission is unavailable for " << model.name
                      << ".\n";
            return;
        }
#if defined(__unix__) || defined(__APPLE__)
        const std::string processId = std::to_string(::getpid());
        const std::string title = "GDB - " + model.name;
        char* arguments[]{
            const_cast<char*>("xterm"),
            const_cast<char*>("-T"),
            const_cast<char*>(title.c_str()),
            const_cast<char*>("-e"),
            const_cast<char*>("gdb"),
            const_cast<char*>("-p"),
            const_cast<char*>(processId.c_str()),
            nullptr,
        };

        ::signal(SIGCHLD, SIG_IGN);
        pid_t terminalProcess{};
        const int result = ::posix_spawnp(
            &terminalProcess,
            "xterm",
            nullptr,
            nullptr,
            arguments,
            environ
        );
        if (result != 0) {
            std::cerr << "Unable to launch xterm GDB for " << model.name
                      << ": " << std::strerror(result) << '\n';
            return;
        }

        launchedDebuggers_.insert(model.name);
        std::cout << "Launched xterm PID " << terminalProcess
                  << " with gdb -p " << processId
                  << " for " << model.name << ".\n";
#else
        std::cerr << "Automatic xterm GDB launch is unsupported for " << model.name
                  << " on this platform.\n";
#endif
    }

    [[nodiscard]] static std::string modelProcessId(
        const rtos::model::ModelConfig& model
    )
    {
        if (!model.enabled) {
            return "-";
        }
#if defined(__unix__) || defined(__APPLE__)
        return std::to_string(::getpid());
#else
        return "shared";
#endif
    }

    static std::optional<std::size_t> parseFrameCount(const std::string& command)
    {
        const std::string_view argument{command.data() + 4, command.size() - 4};
        std::size_t frameCount{};
        const auto result = std::from_chars(
            argument.data(),
            argument.data() + argument.size(),
            frameCount
        );
        if (
            result.ec != std::errc{} || result.ptr != argument.data() + argument.size()
            || frameCount == 0
        ) {
            std::cout << "Run accepts only a positive frame count.\n";
            return std::nullopt;
        }
        return frameCount;
    }

    static std::string formatPortNumbers(const std::vector<std::size_t>& ports)
    {
        if (ports.empty()) {
            return "-";
        }

        std::ostringstream output;
        for (std::size_t index = 0; index < ports.size(); ++index) {
            if (index != 0) {
                output << ',';
            }
            output << ports[index];
        }
        return output.str();
    }

    rtos::logging::Logger logger_;
    rtos::messaging::DispatchPort port_;
    rtos::model::ModelConfiguration configuration_;
    std::unique_ptr<rtos::models::SensorModel> sensor_;
    std::unique_ptr<rtos::models::ControlModel> control_;
    std::unique_ptr<rtos::models::MotorModel> motor_;
    rtos::model::ModelRunner runner_;
    mutable std::mutex simulationMutex_;
    bool initialized_{};
    std::size_t frame_{};
    std::jthread worker_;
    std::unordered_set<std::string> launchedDebuggers_;
    bool debuggerAttachmentEnabled_{};
};

std::optional<ProcessOptions> parseOptions(const int argc, char* argv[])
{
    ProcessOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view option{argv[index]};
        if (option == "--debug") {
            options.logging.level = rtos::logging::LogLevel::debug;
            options.logging.enabled = true;
        } else if (option == "--info") {
            options.logging.level = rtos::logging::LogLevel::info;
            options.logging.enabled = true;
        } else if (option == "--noLogging" || option == "--noLoging") {
            options.logging.enabled = false;
        } else if (option == "--models" && index + 1 < argc) {
            options.modelConfiguration = argv[++index];
        } else {
            std::cerr << "Unknown option: " << option << "\n"
                      << "Run rtos_sim --help for usage.\n";
            return std::nullopt;
        }
    }
    return options;
}

}  // namespace

int main(const int argc, char* argv[])
{
    if (argc == 2 && std::string_view{argv[1]} == "--help") {
        printHelp();
        return 0;
    }
    if (argc == 2 && std::string_view{argv[1]} == "--version") {
        std::cout << "rtos_sim " << version << '\n';
        return 0;
    }

    const auto options = parseOptions(argc, argv);
    if (!options.has_value()) {
        return 1;
    }
    try {
        auto configuration = rtos::model::ModelConfiguration::load(
            options->modelConfiguration
        );
        return SimulatorCli{options->logging, std::move(configuration)}.run();
    } catch (const std::exception& error) {
        std::cerr << "Configuration error: " << error.what() << '\n';
        return 1;
    }
}
