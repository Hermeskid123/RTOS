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

struct ModelProcessReply {
    model::ControlStatus status{model::ControlStatus::stopped};
    messaging::DispatchReport dispatch;
    std::vector<messaging::TransportMessage> messages;
    std::chrono::nanoseconds executionTime{};
    std::chrono::nanoseconds processCpuTime{};
};

class ModelProcess final {
public:
    static std::unique_ptr<ModelProcess> launch(
        std::string name,
        logging::LogLevel logLevel,
        bool loggingEnabled,
        bool debugEnabled
    );

    ModelProcess(const ModelProcess&) = delete;
    ModelProcess& operator=(const ModelProcess&) = delete;
    ~ModelProcess();

    [[nodiscard]] ModelProcessReply initialize();
    [[nodiscard]] ModelProcessReply begin();
    [[nodiscard]] ModelProcessReply freeze();
    [[nodiscard]] ModelProcessReply operate(
        std::uint64_t frame,
        std::chrono::steady_clock::duration elapsed
    );
    [[nodiscard]] ModelProcessReply deliver(
        const messaging::TransportMessage& message
    );
    [[nodiscard]] ModelProcessReply dispatch();
    [[nodiscard]] ModelProcessReply status();
    [[nodiscard]] ModelProcessReply terminate();

    [[nodiscard]] std::string_view name() const noexcept;
    [[nodiscard]] int processId() const noexcept;
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
