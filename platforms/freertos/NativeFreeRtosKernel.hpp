/**
 * @file
 * @brief Declares the native FreeRTOS kernel binding.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include "rtos/platform/freertos/FreeRtosKernel.hpp"

#include "FreeRTOS.h"
#include "task.h"

#include <array>
#include <cstddef>

namespace rtos::platform::freertos {

/** @brief Static-allocation binding from FreeRtosKernel to native FreeRTOS APIs. */
class NativeFreeRtosKernel final : public FreeRtosKernel {
public:
    /** @brief Total statically provisioned task slots. */
    static constexpr std::size_t maximumTasks{FreeRtosAdapterMaximumTasks};
    /** @brief Stack words available in each task slot. */
    static constexpr std::size_t maximumStackDepth{FreeRtosAdapterMaximumStackDepth};

    /** @copydoc FreeRtosKernel::createTask */
    [[nodiscard]] TaskHandle createTask(
        TaskEntry entry,
        std::string_view name,
        std::uint32_t stackDepth,
        void* context,
        std::uint32_t priority
    ) override;
    /** @copydoc FreeRtosKernel::deleteTask */
    void deleteTask(TaskHandle handle) noexcept override;
    /** @copydoc FreeRtosKernel::tickCount */
    [[nodiscard]] Tick tickCount() const noexcept override;
    /** @copydoc FreeRtosKernel::delayUntil */
    void delayUntil(Tick& previousWakeTime, Tick period) override;
    /** @copydoc FreeRtosKernel::deleteCurrentTask */
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
