/**
 * @file
 * @brief Declares the public FreeRtosAdapter framework API.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

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

/** @brief Static task sizing, priority, and period settings. */
struct TaskConfiguration {
    /** Requested stack depth in kernel-defined words. */
    std::uint32_t stackDepth{1024};
    /** Kernel scheduling priority. */
    std::uint32_t priority{1};
    /** Period between task cycles in kernel ticks. */
    Tick periodTicks{1};
};

/** @brief Maps portable models and messaging onto periodic FreeRTOS tasks. */
class FreeRtosAdapter {
public:
    /** @brief Maximum model tasks supported without dynamic allocation. */
    static constexpr std::size_t maximumModels{16};
    /** @brief Maximum stored task name excluding its terminator. */
    static constexpr std::size_t maximumTaskNameLength{31};

    /** @brief Creates a single-use adapter over non-owning kernel and port references. */
    FreeRtosAdapter(
        FreeRtosKernel& kernel,
        messaging::DispatchPort& dispatchPort,
        TaskConfiguration messagingTask = TaskConfiguration{1024, 2, 1}
    ) noexcept;
    FreeRtosAdapter(const FreeRtosAdapter&) = delete;
    FreeRtosAdapter& operator=(const FreeRtosAdapter&) = delete;

    /**
     * @brief Registers a model before start() without allocating.
     * @return `false` for invalid settings, capacity exhaustion, or a started adapter.
     */
    [[nodiscard]] bool addModel(
        std::string_view name,
        model::BaseModel& model,
        TaskConfiguration configuration = {}
    ) noexcept;
    /** @brief Creates all model tasks and the dedicated messaging task. */
    [[nodiscard]] bool start();
    /** @brief Requests orderly termination from all adapter tasks. */
    void stop() noexcept;

    /** @brief Reports whether tasks should continue running. */
    [[nodiscard]] bool running() const noexcept;
    /** @brief Returns the number of registered model tasks. */
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
