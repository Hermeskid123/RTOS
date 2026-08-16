/**
 * @file
 * @brief Declares the public FrameCounter framework API.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include <atomic>
#include <cstdint>

namespace rtos::simulation {

class SimulatorCore;

/** @brief Atomic read-only frame view exposed to application models. */
class FrameCounter final {
public:
    /** @brief Unsigned frame sequence type. */
    using value_type = std::uint64_t;

    FrameCounter(const FrameCounter&) = delete;
    FrameCounter& operator=(const FrameCounter&) = delete;
    FrameCounter(FrameCounter&&) = delete;
    FrameCounter& operator=(FrameCounter&&) = delete;

    /** @brief Returns the current authoritative frame number. */
    [[nodiscard]] value_type current() const noexcept
    {
        return current_.load(std::memory_order_relaxed);
    }

private:
    friend class SimulatorCore;

    FrameCounter() = default;

    void reset() noexcept
    {
        current_.store(0, std::memory_order_relaxed);
    }

    void advance() noexcept
    {
        current_.fetch_add(1, std::memory_order_relaxed);
    }

    void set(const value_type frame) noexcept
    {
        current_.store(frame, std::memory_order_relaxed);
    }

    std::atomic<value_type> current_{};
};

}  // namespace rtos::simulation
