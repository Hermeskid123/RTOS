/**
 * @file
 * @brief Declares the public FreeRtosKernel framework API.
 */

#pragma once

#include <cstdint>
#include <string_view>

namespace rtos::platform::freertos {

using Tick = std::uint32_t;
using TaskEntry = void (*)(void*);

class FreeRtosKernel {
public:
    FreeRtosKernel() = default;
    FreeRtosKernel(const FreeRtosKernel&) = delete;
    FreeRtosKernel& operator=(const FreeRtosKernel&) = delete;
    virtual ~FreeRtosKernel() = default;

    [[nodiscard]] virtual bool createTask(
        TaskEntry entry,
        std::string_view name,
        std::uint32_t stackDepth,
        void* context,
        std::uint32_t priority
    ) = 0;
    [[nodiscard]] virtual Tick tickCount() const noexcept = 0;
    virtual void delayUntil(Tick& previousWakeTime, Tick period) = 0;
    virtual void deleteCurrentTask() noexcept = 0;
};

}  // namespace rtos::platform::freertos
