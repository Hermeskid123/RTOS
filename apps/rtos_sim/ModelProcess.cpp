#include "apps/rtos_sim/ModelProcess.hpp"

#include "models/ControlModel/ControlModel.hpp"
#include "models/MotorModel/MotorModel.hpp"
#include "models/SensorModel/SensorModel.hpp"
#include "messages/MotorCommand.hpp"
#include "messages/MotorStatus.hpp"
#include "messages/SensorData.hpp"
#include "rtos/messaging/DispatchPort.hpp"
#include "rtos/model/BaseModel.hpp"
#include "rtos/simulation/SimulatorCore.hpp"

#include <cerrno>
#include <csignal>
#include <ctime>
#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <utility>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/prctl.h>
#endif

namespace rtos::host {
namespace {

enum class Command : std::uint32_t {
    initialize,
    begin,
    freeze,
    operate,
    deliver,
    dispatch,
    status,
    terminate,
};

struct CommandHeader {
    std::uint32_t command{};
    std::uint64_t frame{};
    std::int64_t elapsedNanoseconds{};
    messaging::RoutingId routingId{};
    std::uint32_t nameSize{};
    std::uint32_t payloadSize{};
};

struct ResponseHeader {
    std::int32_t status{};
    std::uint32_t messageCount{};
    std::uint64_t messagesDispatched{};
    std::uint64_t callbacksInvoked{};
    std::uint64_t messagesWithoutSubscribers{};
    std::uint64_t queueDepthAtStart{};
    std::uint64_t queueHighWaterMark{};
    std::int64_t totalDispatchLatencyNanoseconds{};
    std::int64_t maximumDispatchLatencyNanoseconds{};
    std::int64_t callbackExecutionNanoseconds{};
    std::int64_t dispatchNanoseconds{};
    std::int64_t executionNanoseconds{};
    std::int64_t processCpuNanoseconds{};
};

struct MessageHeader {
    messaging::RoutingId routingId{};
    std::uint32_t nameSize{};
    std::uint32_t payloadSize{};
    std::int64_t queuedAtNanoseconds{};
};

void writeExact(const int fd, const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const std::byte*>(data);
    while (size != 0) {
        const auto written = ::write(fd, bytes, size);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            throw std::runtime_error{"Model IPC write failed: " + std::string{std::strerror(errno)}};
        }
        bytes += written;
        size -= static_cast<std::size_t>(written);
    }
}

void readExact(const int fd, void* data, std::size_t size)
{
    auto* bytes = static_cast<std::byte*>(data);
    while (size != 0) {
        const auto received = ::read(fd, bytes, size);
        if (received < 0 && errno == EINTR) {
            continue;
        }
        if (received <= 0) {
            throw std::runtime_error{"Model IPC read failed"};
        }
        bytes += received;
        size -= static_cast<std::size_t>(received);
    }
}

void writeMessage(const int fd, const messaging::TransportMessage& message)
{
    const MessageHeader header{
        message.routingId,
        static_cast<std::uint32_t>(message.messageName.size()),
        static_cast<std::uint32_t>(message.payload.size()),
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            message.queuedAt.time_since_epoch()
        ).count(),
    };
    writeExact(fd, &header, sizeof(header));
    writeExact(fd, message.messageName.data(), message.messageName.size());
    writeExact(fd, message.payload.data(), message.payload.size());
}

messaging::TransportMessage readMessage(const int fd, const MessageHeader& header)
{
    messaging::TransportMessage message;
    message.routingId = header.routingId;
    message.messageName.resize(header.nameSize);
    message.payload.resize(header.payloadSize);
    message.queuedAt = std::chrono::steady_clock::time_point{
        std::chrono::nanoseconds{header.queuedAtNanoseconds}
    };
    readExact(fd, message.messageName.data(), message.messageName.size());
    readExact(fd, message.payload.data(), message.payload.size());
    return message;
}

class BufferedTransport final : public messaging::MessageTransport {
public:
    void send(messaging::TransportMessage message) override
    {
        std::scoped_lock lock{mutex_};
        messages_.push_back(std::move(message));
    }

    std::vector<messaging::TransportMessage> take()
    {
        std::scoped_lock lock{mutex_};
        return std::exchange(messages_, {});
    }

private:
    std::vector<messaging::TransportMessage> messages_;
    std::mutex mutex_;
};

std::unique_ptr<model::BaseModel> createModel(
    const std::string_view name,
    messaging::DispatchPort& port,
    logging::Logger& logger
)
{
    if (name == "SensorModel") {
        return std::make_unique<models::SensorModel>(port, logger);
    }
    if (name == "ControlModel") {
        return std::make_unique<models::ControlModel>(port, logger);
    }
    if (name == "MotorModel") {
        return std::make_unique<models::MotorModel>(port, logger);
    }
    throw std::runtime_error{"Unknown model worker: " + std::string{name}};
}

[[noreturn]] void runWorker(
    const std::string& name,
    const int commandFd,
    const int responseFd,
    const logging::LogLevel logLevel,
    const bool loggingEnabled,
    const bool debugEnabled
)
{
#if defined(__linux__)
    static_cast<void>(::prctl(PR_SET_PDEATHSIG, SIGTERM));
    static_cast<void>(::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY));
#endif
    logging::Logger logger{std::cout, logLevel};
    logger.setEnabled(loggingEnabled);
    logger.setComponentDebugEnabled(name, debugEnabled);
    BufferedTransport transport;
    messaging::DispatchPort port{
        name + ".dispatch", messaging::TransportType::interProcess, &transport
    };
    auto model = createModel(name, port, logger);
    auto& core = simulation::SimulatorCore::instance();

    while (true) {
        CommandHeader request{};
        readExact(commandFd, &request, sizeof(request));
        const auto command = static_cast<Command>(request.command);
        messaging::TransportMessage delivered;
        if (command == Command::deliver) {
            delivered = readMessage(
                commandFd,
                MessageHeader{
                    request.routingId,
                    request.nameSize,
                    request.payloadSize,
                    request.elapsedNanoseconds
                }
            );
        }

        model::ControlStatus status = model->status();
        messaging::DispatchReport dispatch;
        std::chrono::nanoseconds executionTime{};
        std::chrono::nanoseconds processCpuTime{};
        switch (command) {
        case Command::initialize:
            core.start();
            status = model->initialize();
            break;
        case Command::begin:
            status = model->begin();
            break;
        case Command::freeze:
            status = model->freeze();
            break;
        case Command::operate:
            core.synchronizeFrame(
                request.frame,
                std::chrono::nanoseconds{request.elapsedNanoseconds}
            );
            if (auto* sensor = dynamic_cast<models::SensorModel*>(model.get())) {
                sensor->setSensorValue(static_cast<double>(request.frame));
            }
            {
                const auto startedAt = std::chrono::steady_clock::now();
                const auto cpuStartedAt = std::clock();
                status = model->operate();
                executionTime = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - startedAt
                );
                processCpuTime = std::chrono::nanoseconds{
                    static_cast<std::int64_t>(
                        static_cast<long double>(std::clock() - cpuStartedAt)
                        * 1'000'000'000.0L / CLOCKS_PER_SEC
                    )
                };
            }
            break;
        case Command::deliver:
            static_cast<void>(port.receive(delivered));
            break;
        case Command::dispatch:
            dispatch = port.dispatchAll();
            break;
        case Command::status:
            break;
        case Command::terminate:
            status = model->terminate();
            core.stop();
            break;
        }

        auto messages = transport.take();
        const ResponseHeader response{
            static_cast<std::int32_t>(status),
            static_cast<std::uint32_t>(messages.size()),
            dispatch.messagesDispatched,
            dispatch.callbacksInvoked,
            dispatch.messagesWithoutSubscribers,
            dispatch.queueDepthAtStart,
            dispatch.queueHighWaterMark,
            dispatch.totalDispatchLatency.count(),
            dispatch.maximumDispatchLatency.count(),
            dispatch.callbackExecutionTime.count(),
            dispatch.dispatchDuration.count(),
            executionTime.count(),
            processCpuTime.count(),
        };
        writeExact(responseFd, &response, sizeof(response));
        for (const auto& message : messages) {
            writeMessage(responseFd, message);
        }
        if (command == Command::terminate) {
            std::cout.flush();
            std::cerr.flush();
            ::close(commandFd);
            ::close(responseFd);
            ::_exit(0);
        }
    }
}

std::vector<messaging::PortTopology> topologyFor(const std::string_view name)
{
    using messaging::PortDirection;
    using messaging::TransportType;
    if (name == "SensorModel") {
        return {{1, "SensorData_port", "SensorData", PortDirection::publisher,
                 TransportType::interProcess, messages::SensorData::defaultRoutingId, {}, {}}};
    }
    if (name == "ControlModel") {
        return {
            {1, "SensorData_port", "SensorData", PortDirection::subscriber,
             TransportType::interProcess, messages::SensorData::defaultRoutingId, {}, {}},
            {2, "MotorCommand_port", "MotorCommand", PortDirection::publisher,
             TransportType::interProcess, messages::MotorCommand::defaultRoutingId, {}, {}},
        };
    }
    return {
        {1, "MotorCommand_port", "MotorCommand", PortDirection::subscriber,
         TransportType::interProcess, messages::MotorCommand::defaultRoutingId, {}, {}},
        {2, "MotorStatus_port", "MotorStatus", PortDirection::publisher,
         TransportType::interProcess, messages::MotorStatus::defaultRoutingId, {}, {}},
    };
}

}  // namespace

std::unique_ptr<ModelProcess> ModelProcess::launch(
    std::string name,
    const logging::LogLevel logLevel,
    const bool loggingEnabled,
    const bool debugEnabled
)
{
    std::cout.flush();
    std::cerr.flush();
    int commands[2]{};
    int responses[2]{};
    if (::pipe(commands) != 0 || ::pipe(responses) != 0) {
        throw std::runtime_error{"Unable to create model IPC pipes"};
    }
    const auto processId = ::fork();
    if (processId < 0) {
        throw std::runtime_error{"Unable to fork model process"};
    }
    if (processId == 0) {
        ::close(commands[1]);
        ::close(responses[0]);
        runWorker(
            name, commands[0], responses[1], logLevel, loggingEnabled, debugEnabled
        );
    }
    ::close(commands[0]);
    ::close(responses[1]);
    return std::unique_ptr<ModelProcess>{
        new ModelProcess{std::move(name), processId, commands[1], responses[0]}
    };
}

ModelProcess::ModelProcess(
    std::string name,
    const int processId,
    const int commandFd,
    const int responseFd
)
    : name_{std::move(name)},
      processId_{processId},
      commandFd_{commandFd},
      responseFd_{responseFd},
      topology_{topologyFor(name_)}
{
}

ModelProcess::~ModelProcess()
{
    if (!terminated_) {
        try {
            static_cast<void>(terminate());
        } catch (...) {
            ::kill(processId_, SIGTERM);
        }
    }
    if (commandFd_ >= 0) {
        ::close(commandFd_);
    }
    if (responseFd_ >= 0) {
        ::close(responseFd_);
    }
    if (processId_ > 0) {
        static_cast<void>(::waitpid(processId_, nullptr, 0));
    }
}

ModelProcessReply ModelProcess::initialize() { return request(static_cast<std::uint32_t>(Command::initialize)); }
ModelProcessReply ModelProcess::begin() { return request(static_cast<std::uint32_t>(Command::begin)); }
ModelProcessReply ModelProcess::freeze() { return request(static_cast<std::uint32_t>(Command::freeze)); }
ModelProcessReply ModelProcess::dispatch() { return request(static_cast<std::uint32_t>(Command::dispatch)); }
ModelProcessReply ModelProcess::status() { return request(static_cast<std::uint32_t>(Command::status)); }

ModelProcessReply ModelProcess::operate(
    const std::uint64_t frame,
    const std::chrono::steady_clock::duration elapsed
)
{
    return request(static_cast<std::uint32_t>(Command::operate), frame, elapsed);
}

ModelProcessReply ModelProcess::deliver(const messaging::TransportMessage& message)
{
    return request(static_cast<std::uint32_t>(Command::deliver), 0, {}, &message);
}

ModelProcessReply ModelProcess::terminate()
{
    auto reply = request(static_cast<std::uint32_t>(Command::terminate));
    terminated_ = true;
    return reply;
}

ModelProcessReply ModelProcess::request(
    const std::uint32_t command,
    const std::uint64_t frame,
    const std::chrono::steady_clock::duration elapsed,
    const messaging::TransportMessage* const message
)
{
    std::scoped_lock lock{requestMutex_};
    const CommandHeader request{
        command,
        frame,
        message == nullptr
            ? std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count()
            : std::chrono::duration_cast<std::chrono::nanoseconds>(
                  message->queuedAt.time_since_epoch()
              ).count(),
        message == nullptr ? 0 : message->routingId,
        message == nullptr ? 0 : static_cast<std::uint32_t>(message->messageName.size()),
        message == nullptr ? 0 : static_cast<std::uint32_t>(message->payload.size()),
    };
    writeExact(commandFd_, &request, sizeof(request));
    if (message != nullptr) {
        writeExact(commandFd_, message->messageName.data(), message->messageName.size());
        writeExact(commandFd_, message->payload.data(), message->payload.size());
    }

    ResponseHeader response{};
    readExact(responseFd_, &response, sizeof(response));
    ModelProcessReply reply;
    reply.status = static_cast<model::ControlStatus>(response.status);
    reply.dispatch.messagesDispatched = response.messagesDispatched;
    reply.dispatch.callbacksInvoked = response.callbacksInvoked;
    reply.dispatch.messagesWithoutSubscribers = response.messagesWithoutSubscribers;
    reply.dispatch.queueDepthAtStart = response.queueDepthAtStart;
    reply.dispatch.queueHighWaterMark = response.queueHighWaterMark;
    reply.dispatch.totalDispatchLatency =
        std::chrono::nanoseconds{response.totalDispatchLatencyNanoseconds};
    reply.dispatch.maximumDispatchLatency =
        std::chrono::nanoseconds{response.maximumDispatchLatencyNanoseconds};
    reply.dispatch.callbackExecutionTime =
        std::chrono::nanoseconds{response.callbackExecutionNanoseconds};
    reply.dispatch.dispatchDuration =
        std::chrono::nanoseconds{response.dispatchNanoseconds};
    reply.executionTime = std::chrono::nanoseconds{response.executionNanoseconds};
    reply.processCpuTime = std::chrono::nanoseconds{response.processCpuNanoseconds};
    reply.messages.reserve(response.messageCount);
    for (std::uint32_t index = 0; index < response.messageCount; ++index) {
        MessageHeader header{};
        readExact(responseFd_, &header, sizeof(header));
        reply.messages.push_back(readMessage(responseFd_, header));
    }
    status_ = reply.status;
    return reply;
}

std::string_view ModelProcess::name() const noexcept { return name_; }
int ModelProcess::processId() const noexcept { return processId_; }
const std::vector<messaging::PortTopology>& ModelProcess::topology() const noexcept { return topology_; }

}  // namespace rtos::host
