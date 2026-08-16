/**
 * @file
 * @brief Implements the native FreeRTOS kernel binding.
 */

#include "platforms/freertos/NativeFreeRtosKernel.hpp"

#include <algorithm>
#include <limits>

namespace rtos::platform::freertos {

bool NativeFreeRtosKernel::createTask(
    const TaskEntry entry,
    const std::string_view name,
    const std::uint32_t stackDepth,
    void* const context,
    const std::uint32_t priority
)
{
    const auto storage = std::ranges::find(tasks_, false, &TaskStorage::occupied);
    if (
        storage == tasks_.end() || stackDepth == 0 || stackDepth > maximumStackDepth
        || priority > std::numeric_limits<UBaseType_t>::max()
    ) {
        return false;
    }

    storage->occupied = true;
    const auto handle = xTaskCreateStatic(
        entry,
        name.data(),
        static_cast<configSTACK_DEPTH_TYPE>(stackDepth),
        context,
        static_cast<UBaseType_t>(priority),
        storage->stack.data(),
        &storage->controlBlock
    );
    if (handle == nullptr) {
        storage->occupied = false;
        return false;
    }
    return true;
}

Tick NativeFreeRtosKernel::tickCount() const noexcept
{
    return static_cast<Tick>(xTaskGetTickCount());
}

void NativeFreeRtosKernel::delayUntil(Tick& previousWakeTime, const Tick period)
{
    auto nativeWakeTime = static_cast<TickType_t>(previousWakeTime);
    static_cast<void>(xTaskDelayUntil(&nativeWakeTime, static_cast<TickType_t>(period)));
    previousWakeTime = static_cast<Tick>(nativeWakeTime);
}

void NativeFreeRtosKernel::deleteCurrentTask() noexcept
{
    vTaskDelete(nullptr);
}

}  // namespace rtos::platform::freertos
