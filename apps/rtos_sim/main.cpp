#include "apps/rtos_sim/ModelProcess.hpp"
#include "rtos/logging/Logger.hpp"
#include "rtos/messaging/MessageTraffic.hpp"
#include "rtos/model/ModelConfiguration.hpp"
#include "rtos/model/ModelRunner.hpp"
#include "rtos/simulation/SimulatorCore.hpp"
#include "rtos/simulation/PerformanceMetrics.hpp"

#include <algorithm>
#include <charconv>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <future>
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
    std::chrono::nanoseconds framePeriod{std::chrono::milliseconds{10}};
    std::optional<std::size_t> frameCount;
    bool showMetrics{};
};

void printHelp()
{
    std::cout << "RTOS Model Simulator\n\n"
                 "Usage: rtos_sim [options]\n\n"
                 "Process options:\n"
                 "  --debug          Include DEBUG and higher log records\n"
                 "  --info           Include INFO and higher log records\n"
                 "  --noLogging      Disable every log record\n"
                 "  --models <file>  Select the models XML configuration\n"
                 "  --frames <count>  Run a fixed number of frames and exit\n"
                 "  --frame-rate <hz> Set the simulation frame rate (default: 100)\n"
                 "  --metrics        Print metrics after a fixed frame run\n"
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
                 "  metrics          Show parallel execution performance\n"
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
        ProcessOptions options,
        rtos::model::ModelConfiguration configuration
    )
        : logger_{std::cout, options.logging.level},
          configuration_{std::move(configuration)},
          loggingOptions_{options.logging},
          framePeriod_{options.framePeriod},
          startupFrameCount_{options.frameCount},
          showStartupMetrics_{options.showMetrics}
    {
        logger_.setEnabled(options.logging.enabled);
        for (const auto& model : configuration_.models()) {
            logger_.setComponentDebugEnabled(model.name, model.debugEnabled);
        }

        configureDebuggerAttachment();
    }

    int run()
    {
        std::cout << "RTOS Model Simulator " << version << "\n"
                  << "Frame Rate: "
                  << std::chrono::duration<double>{1} / framePeriod_ << " Hz\n";

        if (startupFrameCount_.has_value()) {
            startModels();
            runFrames(*startupFrameCount_);
            if (showStartupMetrics_) {
                printMetrics();
            }
            terminateSimulation();
            return 0;
        }

        std::cout << "Type 'help' for commands.\n";

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
        } else if (command == "metrics" || command == "performance") {
            printMetrics();
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

        core_.start();
        performanceMetrics_.reset();
        launchModelProcesses();
        initialized_ = true;
        launchConfiguredDebuggers();
        logger_.log(
            rtos::logging::LogLevel::info,
            "Host",
            "START",
            "Simulator initialized"
        );
        std::vector<rtos::model::ModelStatusReport> reports;
        for (auto& process : processes_) {
            reports.push_back({std::string{process->name()}, process->initialize().status});
        }
        printReports(reports);
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
        std::vector<rtos::model::ModelStatusReport> reports;
        for (auto& process : processes_) {
            reports.push_back({std::string{process->name()}, process->begin().status});
        }
        printReports(reports);
    }

    void stopModels()
    {
        stopWorker();
        std::scoped_lock lock{simulationMutex_};
        if (!initialized_) {
            std::cout << "Simulator is not initialized.\n";
            return;
        }
        std::vector<rtos::model::ModelStatusReport> reports;
        for (auto& process : processes_) {
            reports.push_back({std::string{process->name()}, process->freeze().status});
        }
        printReports(reports);
    }

    void terminateSimulation()
    {
        stopWorker();
        std::scoped_lock lock{simulationMutex_};
        if (!initialized_) {
            return;
        }

        std::vector<rtos::model::ModelStatusReport> reports;
        for (auto& process : processes_) {
            reports.push_back({std::string{process->name()}, process->terminate().status});
        }
        printReports(reports);
        logger_.log(
            rtos::logging::LogLevel::info,
            "Host",
            "STOP",
            "Simulator terminated"
        );
        core_.stop();
        processes_.clear();
        launchedDebuggers_.clear();
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
                auto nextFrame = std::chrono::steady_clock::now();
                while (!stopToken.stop_requested()) {
                    executeFrame(nextFrame);
                    nextFrame += framePeriod_;
                    std::this_thread::sleep_until(nextFrame);
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

        auto nextFrame = std::chrono::steady_clock::now();
        for (std::size_t offset = 0; offset < count; ++offset) {
            executeFrame(nextFrame);
            nextFrame += framePeriod_;
            if (offset + 1 < count) {
                std::this_thread::sleep_until(nextFrame);
            }
        }
    }

    void executeFrame(const std::chrono::steady_clock::time_point scheduledStart)
    {
        std::scoped_lock lock{simulationMutex_};
        const auto frameStarted = std::chrono::steady_clock::now();
        const auto jitter = frameStarted >= scheduledStart
            ? frameStarted - scheduledStart
            : scheduledStart - frameStarted;
        core_.advanceFrame();
        const auto frame = core_.frameCounter().current();
        auto routedMessages = std::exchange(deferredMessages_, {});
        const auto elapsed = core_.clock().elapsed();
        std::vector<std::future<rtos::host::ModelProcessReply>> operateFutures;
        operateFutures.reserve(processes_.size());
        for (auto& process : processes_) {
            auto* const processPointer = process.get();
            operateFutures.push_back(std::async(
                std::launch::async,
                [processPointer, frame, elapsed]
                {
                    return processPointer->operate(frame, elapsed);
                }
            ));
        }

        std::chrono::nanoseconds parallelModelExecutionTime{};
        std::chrono::nanoseconds workerCpuTime{};
        for (auto& future : operateFutures) {
            auto reply = future.get();
            parallelModelExecutionTime = std::max(
                parallelModelExecutionTime, reply.executionTime
            );
            workerCpuTime += reply.processCpuTime;
            for (auto& outgoing : reply.messages) {
                auto& counters = traffic_[outgoing.messageName];
                counters.messageName = outgoing.messageName;
                ++counters.messagesSent;
                routedMessages.push_back(std::move(outgoing));
            }
        }

        rtos::messaging::DispatchReport dispatch;
        for (const auto& routed : routedMessages) {
            std::size_t subscriberCount{};
            for (const auto& process : processes_) {
                subscriberCount += static_cast<std::size_t>(std::ranges::count_if(
                    process->topology(),
                    [&routed](const rtos::messaging::PortTopology& endpoint)
                    {
                        return endpoint.direction
                                == rtos::messaging::PortDirection::subscriber
                            && endpoint.routingId == routed.routingId;
                    }
                ));
            }
            auto& counters = traffic_[routed.messageName];
            if (subscriberCount == 0) {
                ++counters.messagesWithoutSubscribers;
                ++dispatch.messagesWithoutSubscribers;
            } else {
                ++counters.messagesDispatched;
                counters.messagesReceived += subscriberCount;
                ++dispatch.messagesDispatched;
                dispatch.callbacksInvoked += subscriberCount;
            }
        }

        std::vector<std::future<void>> deliveryFutures;
        deliveryFutures.reserve(processes_.size());
        for (auto& process : processes_) {
            auto* const processPointer = process.get();
            deliveryFutures.push_back(std::async(
                std::launch::async,
                [processPointer, &routedMessages]
                {
                    for (const auto& routed : routedMessages) {
                        static_cast<void>(processPointer->deliver(routed));
                    }
                }
            ));
        }
        for (auto& future : deliveryFutures) {
            future.get();
        }

        const auto dispatchStarted = std::chrono::steady_clock::now();
        std::vector<std::future<rtos::host::ModelProcessReply>> dispatchFutures;
        dispatchFutures.reserve(processes_.size());
        for (auto& process : processes_) {
            auto* const processPointer = process.get();
            dispatchFutures.push_back(std::async(
                std::launch::async,
                [processPointer] { return processPointer->dispatch(); }
            ));
        }

        std::size_t queueDepth{};
        std::size_t queueHighWaterMark{};
        std::chrono::nanoseconds totalDispatchLatency{};
        std::chrono::nanoseconds maximumDispatchLatency{};
        std::chrono::nanoseconds callbackExecutionTime{};
        for (auto& future : dispatchFutures) {
            auto reply = future.get();
            queueDepth += reply.dispatch.queueDepthAtStart;
            queueHighWaterMark = std::max(
                queueHighWaterMark, reply.dispatch.queueHighWaterMark
            );
            totalDispatchLatency += reply.dispatch.totalDispatchLatency;
            maximumDispatchLatency = std::max(
                maximumDispatchLatency, reply.dispatch.maximumDispatchLatency
            );
            callbackExecutionTime += reply.dispatch.callbackExecutionTime;
            for (auto& deferred : reply.messages) {
                deferredMessages_.push_back(std::move(deferred));
            }
        }
        const auto dispatchPhaseTime =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - dispatchStarted
            );

        const auto frameExecutionTime =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - frameStarted
            );
        performanceMetrics_.record(rtos::simulation::FramePerformance{
            .frame = frame,
            .workerCount = processes_.size(),
            .messagesProcessed = dispatch.messagesProcessed(),
            .queueDepth = queueDepth,
            .queueHighWaterMark = queueHighWaterMark,
            .dispatchLatencySamples = queueDepth,
            .deadlineMissed = frameExecutionTime > framePeriod_,
            .frameExecutionTime = frameExecutionTime,
            .jitter = std::chrono::duration_cast<std::chrono::nanoseconds>(jitter),
            .parallelModelExecutionTime = parallelModelExecutionTime,
            .workerCpuTime = workerCpuTime,
            .totalDispatchLatency = totalDispatchLatency,
            .maximumDispatchLatency = maximumDispatchLatency,
            .callbackExecutionTime = callbackExecutionTime,
            .dispatchPhaseTime = dispatchPhaseTime,
        });

        std::ostringstream message;
        message << "frame=" << frame
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

    [[nodiscard]] bool modelsAreRunning()
    {
        std::scoped_lock lock{simulationMutex_};
        if (!initialized_ || processes_.empty()) {
            return false;
        }
        return std::ranges::all_of(
            processes_,
            [](const std::unique_ptr<rtos::host::ModelProcess>& process)
            {
                return process->status().status == rtos::model::ControlStatus::running;
            }
        );
    }

    void printStatuses()
    {
        std::scoped_lock lock{simulationMutex_};
        for (const auto& configuredModel : configuration_.models()) {
            const auto process = std::ranges::find_if(
                processes_,
                [&configuredModel](const auto& candidate)
                {
                    return candidate->name() == configuredModel.name;
                }
            );
            const auto status = process == processes_.end()
                ? rtos::model::ControlStatus::stopped
                : (*process)->status().status;
            std::cout << configuredModel.name << ": "
                      << rtos::model::toString(status) << '\n';
        }
    }

    void printPorts() const
    {
        std::scoped_lock lock{simulationMutex_};
        constexpr int processWidth{10};
        constexpr int numberWidth{8};
        constexpr int nameWidth{24};
        constexpr int messageWidth{18};
        constexpr int transportWidth{14};
        constexpr int routingWidth{12};

        std::cout << "ROS Messaging Dispatch Ports\n"
                  << std::left << std::setw(processWidth) << "PID"
                  << std::setw(numberWidth) << "Port #"
                  << std::setw(nameWidth) << "Name"
                  << std::setw(messageWidth) << "Message"
                  << std::setw(transportWidth) << "Transport"
                  << std::setw(routingWidth) << "Routing ID" << '\n'
                  << std::string(
                         processWidth + numberWidth + nameWidth + messageWidth
                             + transportWidth + routingWidth,
                         '-'
                     )
                  << '\n';

        for (const auto& process : processes_) {
            for (const auto& endpoint : process->topology()) {
                std::cout << std::left << std::setw(processWidth) << process->processId()
                          << std::setw(numberWidth) << endpoint.number
                          << std::setw(nameWidth) << endpoint.name
                          << std::setw(messageWidth) << endpoint.messageName
                          << std::setw(transportWidth)
                          << rtos::messaging::toString(endpoint.transport)
                          << std::setw(routingWidth) << endpoint.routingId << '\n';
            }
        }
    }

    void printMessages() const
    {
        std::scoped_lock lock{simulationMutex_};
        if (traffic_.empty()) {
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

        for (const auto& [ignoredName, message] : traffic_) {
            static_cast<void>(ignoredName);
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

    void printMetrics() const
    {
        std::scoped_lock lock{simulationMutex_};
        const auto metrics = performanceMetrics_.summary();
        std::cout << "Parallel Performance Metrics\n"
                  << "Frames:                    " << metrics.frames << '\n'
                  << "Workers:                   " << metrics.workers << '\n'
                  << "Messages/frame:            " << std::fixed << std::setprecision(3)
                  << metrics.averageMessagesPerFrame << '\n'
                  << "Frame execution avg/max:   "
                  << metrics.averageFrameExecutionMilliseconds << " / "
                  << metrics.maximumFrameExecutionMilliseconds << " ms\n"
                  << "Model execution avg:       "
                  << metrics.averageModelExecutionMilliseconds << " ms\n"
                  << "Dispatch latency avg/max:  "
                  << metrics.averageDispatchLatencyMilliseconds << " / "
                  << metrics.maximumDispatchLatencyMilliseconds << " ms\n"
                  << "Callback execution avg:    "
                  << metrics.averageCallbackExecutionMilliseconds << " ms\n"
                  << "Dispatch phase avg:        "
                  << metrics.averageDispatchPhaseMilliseconds << " ms\n"
                  << "Queue depth avg:           " << metrics.averageQueueDepth << '\n'
                  << "Queue high-water mark:     " << metrics.queueHighWaterMark << '\n'
                  << "Jitter avg/max:            " << metrics.averageJitterMilliseconds
                  << " / " << metrics.maximumJitterMilliseconds << " ms\n"
                  << "Deadline misses:           " << metrics.deadlineMisses << '\n'
                  << "Worker CPU utilization:    " << metrics.cpuUtilizationPercent
                  << "%\n";
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

    void launchModelProcesses()
    {
        if (!processes_.empty()) {
            return;
        }
        traffic_.clear();
        deferredMessages_.clear();
        for (const auto& model : configuration_.models()) {
            if (!model.enabled) {
                continue;
            }
            processes_.push_back(rtos::host::ModelProcess::launch(
                model.name,
                loggingOptions_.level,
                loggingOptions_.enabled,
                model.debugEnabled
            ));
            for (const auto& endpoint : processes_.back()->topology()) {
                auto& counters = traffic_[endpoint.messageName];
                counters.messageName = endpoint.messageName;
                if (endpoint.direction == rtos::messaging::PortDirection::publisher) {
                    ++counters.publishers;
                } else {
                    ++counters.subscribers;
                }
            }
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
        const std::string processId = modelProcessId(model);
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

    [[nodiscard]] std::string modelProcessId(
        const rtos::model::ModelConfig& model
    ) const
    {
        if (!model.enabled) {
            return "-";
        }
        const auto process = std::ranges::find_if(
            processes_,
            [&model](const auto& candidate) { return candidate->name() == model.name; }
        );
        return process == processes_.end()
            ? "pending"
            : std::to_string((*process)->processId());
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

    rtos::logging::Logger logger_;
    rtos::model::ModelConfiguration configuration_;
    LoggingOptions loggingOptions_;
    std::vector<std::unique_ptr<rtos::host::ModelProcess>> processes_;
    std::unordered_map<std::string, rtos::messaging::MessageTraffic> traffic_;
    std::vector<rtos::messaging::TransportMessage> deferredMessages_;
    rtos::simulation::SimulatorCore& core_{rtos::simulation::SimulatorCore::instance()};
    rtos::simulation::PerformanceMetrics performanceMetrics_;
    mutable std::mutex simulationMutex_;
    bool initialized_{};
    std::chrono::nanoseconds framePeriod_;
    std::optional<std::size_t> startupFrameCount_;
    bool showStartupMetrics_{};
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
        } else if (option == "--frames" && index + 1 < argc) {
            std::size_t frameCount{};
            const std::string_view value{argv[++index]};
            const auto result = std::from_chars(
                value.data(), value.data() + value.size(), frameCount
            );
            if (result.ec != std::errc{} || result.ptr != value.end() || frameCount == 0) {
                std::cerr << "--frames accepts only a positive frame count.\n";
                return std::nullopt;
            }
            options.frameCount = frameCount;
        } else if (option == "--frame-rate" && index + 1 < argc) {
            std::uint64_t frameRate{};
            const std::string_view value{argv[++index]};
            const auto result = std::from_chars(
                value.data(), value.data() + value.size(), frameRate
            );
            if (
                result.ec != std::errc{} || result.ptr != value.end()
                || frameRate == 0 || frameRate > 1'000'000'000
            ) {
                std::cerr << "--frame-rate accepts an integer from 1 to 1000000000.\n";
                return std::nullopt;
            }
            options.framePeriod = std::chrono::nanoseconds{1'000'000'000 / frameRate};
        } else if (option == "--metrics") {
            options.showMetrics = true;
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
        return SimulatorCli{std::move(*options), std::move(configuration)}.run();
    } catch (const std::exception& error) {
        std::cerr << "Configuration error: " << error.what() << '\n';
        return 1;
    }
}
