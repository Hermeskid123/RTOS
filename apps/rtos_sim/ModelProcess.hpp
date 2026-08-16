/**
 * @file
 * @brief Declares the host model worker process interface.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include "rtos/logging/Logger.hpp"
#include "rtos/messaging/DispatchReport.hpp"
#include "rtos/messaging/PortTopology.hpp"
#include "rtos/messaging/Transport.hpp"
#include "rtos/model/ControlStatus.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace rtos::host {

/** @brief Complete response returned by one worker-process command. */
struct ModelProcessReply {
    /** Model lifecycle state after the command. */
    model::ControlStatus status{model::ControlStatus::stopped};
    /** Dispatch measurements, when the command dispatched messages. */
    messaging::DispatchReport dispatch;
    /** Outbound transport envelopes published while handling the command. */
    std::vector<messaging::TransportMessage> messages;
    /** Worker wall time spent executing the command. */
    std::chrono::nanoseconds executionTime{};
    /** Worker process CPU time consumed by the command. */
    std::chrono::nanoseconds processCpuTime{};
};

/**
 * @brief Parent-side owner and synchronous command channel for one model worker.
 * @details Each instance owns a child PID and two pipe descriptors. Calls are
 * serialized so one command/reply transaction is active at a time.
 */
class ModelProcess final {
public:
    /**
     * @brief Forks a worker hosting the named built-in model.
     * @return Owning process proxy.
     * @throws std::runtime_error If pipes, fork, or worker setup fails.
     */
    static std::unique_ptr<ModelProcess> launch(
        std::string name,
        logging::LogLevel logLevel,
        bool loggingEnabled,
        bool debugEnabled
    );

    ModelProcess(const ModelProcess&) = delete;
    ModelProcess& operator=(const ModelProcess&) = delete;
    /** @brief Terminates and reaps the child process if necessary. */
    ~ModelProcess();

    /** @brief Requests model initialization. */
    [[nodiscard]] ModelProcessReply initialize();
    /** @brief Requests model begin/resume. */
    [[nodiscard]] ModelProcessReply begin();
    /** @brief Requests model freeze. */
    [[nodiscard]] ModelProcessReply freeze();
    /** @brief Synchronizes time and requests one model operation. */
    [[nodiscard]] ModelProcessReply operate(
        std::uint64_t frame,
        std::chrono::steady_clock::duration elapsed
    );
    /** @brief Delivers one routed transport message into the worker. */
    [[nodiscard]] ModelProcessReply deliver(
        const messaging::TransportMessage& message
    );
    /** @brief Requests one deferred message dispatch boundary. */
    [[nodiscard]] ModelProcessReply dispatch();
    /** @brief Queries the worker model's current lifecycle state. */
    [[nodiscard]] ModelProcessReply status();
    /** @brief Requests final cleanup and process exit. */
    [[nodiscard]] ModelProcessReply terminate();

    /** @brief Returns the configured model name. */
    [[nodiscard]] std::string_view name() const noexcept;
    /** @brief Returns the child process identifier. */
    [[nodiscard]] int processId() const noexcept;
    /** @brief Returns the endpoint topology received during launch. */
    [[nodiscard]] const std::vector<messaging::PortTopology>& topology() const noexcept;

private:
    ModelProcess(std::string name, int processId, int commandFd, int responseFd);
    [[nodiscard]] ModelProcessReply request(
        std::uint32_t command,
        std::uint64_t frame = 0,
        std::chrono::steady_clock::duration elapsed = {},
        const messaging::TransportMessage* message = nullptr
    );

    std::string name_;
    int processId_{};
    int commandFd_{-1};
    int responseFd_{-1};
    model::ControlStatus status_{model::ControlStatus::stopped};
    bool terminated_{};
    std::mutex requestMutex_;
    std::vector<messaging::PortTopology> topology_;
};

}  // namespace rtos::host
