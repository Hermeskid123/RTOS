/**
 * @file
 * @brief Declares the native FreeRTOS kernel binding.
 */

#pragma once

#include "rtos/platform/freertos/FreeRtosKernel.hpp"

#include "FreeRTOS.h"
#include "task.h"

#include <array>
#include <cstddef>

namespace rtos::platform::freertos {

class NativeFreeRtosKernel final : public FreeRtosKernel {
public:
    static constexpr std::size_t maximumTasks{FreeRtosAdapterMaximumTasks};
    static constexpr std::size_t maximumStackDepth{FreeRtosAdapterMaximumStackDepth};

    [[nodiscard]] TaskHandle createTask(
        TaskEntry entry,
        std::string_view name,
        std::uint32_t stackDepth,
        void* context,
        std::uint32_t priority
    ) override;
    void deleteTask(TaskHandle handle) noexcept override;
    [[nodiscard]] Tick tickCount() const noexcept override;
    void delayUntil(Tick& previousWakeTime, Tick period) override;
    void deleteCurrentTask() noexcept override;

private:
    struct TaskStorage {
        StaticTask_t controlBlock{};
        std::array<StackType_t, maximumStackDepth> stack{};
        bool occupied{};
    };

    std::array<TaskStorage, maximumTasks> tasks_{};
};

}  // namespace rtos::platform::freertos
