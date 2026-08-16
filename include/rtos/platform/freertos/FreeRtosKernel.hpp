/**
 * @file
 * @brief Declares the public FreeRtosKernel framework API.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include <cstdint>
#include <string_view>

namespace rtos::platform::freertos {

/** @brief Portable RTOS tick count type used by the adapter. */
using Tick = std::uint32_t;
/** @brief C-compatible task entry point accepted by a kernel binding. */
using TaskEntry = void (*)(void*);
/** @brief Opaque native task handle. */
using TaskHandle = void*;

/** @brief Abstract kernel services required by FreeRtosAdapter. */
class FreeRtosKernel {
public:
    FreeRtosKernel() = default;
    FreeRtosKernel(const FreeRtosKernel&) = delete;
    FreeRtosKernel& operator=(const FreeRtosKernel&) = delete;
    virtual ~FreeRtosKernel() = default;

    /** @brief Creates a task and returns its native handle, or null on failure. */
    [[nodiscard]] virtual TaskHandle createTask(
        TaskEntry entry,
        std::string_view name,
        std::uint32_t stackDepth,
        void* context,
        std::uint32_t priority
    ) = 0;
    /** @brief Deletes the task identified by `handle`. */
    virtual void deleteTask(TaskHandle handle) noexcept = 0;
    /** @brief Returns the current kernel tick count. */
    [[nodiscard]] virtual Tick tickCount() const noexcept = 0;
    /** @brief Delays periodically relative to `previousWakeTime`. */
    virtual void delayUntil(Tick& previousWakeTime, Tick period) = 0;
    /** @brief Deletes the task executing this call. */
    virtual void deleteCurrentTask() noexcept = 0;
};

}  // namespace rtos::platform::freertos
