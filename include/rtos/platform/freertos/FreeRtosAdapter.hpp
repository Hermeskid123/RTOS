#pragma once

#include "rtos/platform/freertos/FreeRtosKernel.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace rtos::messaging {
class DispatchPort;
}

namespace rtos::model {
class BaseModel;
}

namespace rtos::platform::freertos {

struct TaskConfiguration {
    std::uint32_t stackDepth{1024};
    std::uint32_t priority{1};
    Tick periodTicks{1};
};

class FreeRtosAdapter {
public:
    static constexpr std::size_t maximumModels{16};
    static constexpr std::size_t maximumTaskNameLength{31};

    FreeRtosAdapter(
        FreeRtosKernel& kernel,
        messaging::DispatchPort& dispatchPort,
        TaskConfiguration messagingTask = TaskConfiguration{1024, 2, 1}
    ) noexcept;
    FreeRtosAdapter(const FreeRtosAdapter&) = delete;
    FreeRtosAdapter& operator=(const FreeRtosAdapter&) = delete;

    [[nodiscard]] bool addModel(
        std::string_view name,
        model::BaseModel& model,
        TaskConfiguration configuration = {}
    ) noexcept;
    [[nodiscard]] bool start();
    void stop() noexcept;

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] std::size_t modelCount() const noexcept;

private:
    struct ModelTaskContext {
        FreeRtosAdapter* owner{};
        model::BaseModel* model{};
        TaskConfiguration configuration{};
        std::array<char, maximumTaskNameLength + 1> name{};
    };

    static void modelTaskEntry(void* context);
    static void messagingTaskEntry(void* context);
    void runModelTask(ModelTaskContext& context);
    void runMessagingTask();

    FreeRtosKernel& kernel_;
    messaging::DispatchPort& dispatchPort_;
    TaskConfiguration messagingTask_;
    std::array<ModelTaskContext, maximumModels> models_{};
    std::size_t modelCount_{};
    std::atomic<bool> started_{};
    std::atomic<bool> running_{};
};

}  // namespace rtos::platform::freertos
