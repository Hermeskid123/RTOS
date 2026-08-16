/**
 * @file
 * @brief Declares the public SimulationClock framework API.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include <chrono>
#include <mutex>

namespace rtos::simulation {

class SimulatorCore;

/** @brief Mutex-protected monotonic simulation clock. */
class SimulationClock final {
public:
    /** @brief Underlying host monotonic clock. */
    using clock_type = std::chrono::steady_clock;
    /** @brief Duration type used for synchronized elapsed time. */
    using duration = clock_type::duration;

    SimulationClock(const SimulationClock&) = delete;
    SimulationClock& operator=(const SimulationClock&) = delete;
    SimulationClock(SimulationClock&&) = delete;
    SimulationClock& operator=(SimulationClock&&) = delete;

    /** @brief Returns total elapsed time, including the current running span. */
    [[nodiscard]] duration elapsed() const;
    /** @brief Reports whether elapsed time is actively advancing. */
    [[nodiscard]] bool isRunning() const;

private:
    friend class SimulatorCore;

    SimulationClock() = default;

    void start();
    void stop();
    void reset();
    void synchronize(duration elapsed);

    mutable std::mutex mutex_;
    clock_type::time_point startedAt_{};
    duration elapsedBeforeStart_{};
    bool running_{};
};

}  // namespace rtos::simulation
