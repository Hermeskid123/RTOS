/**
 * @file
 * @brief Declares the public SimulatorCore framework API.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include "rtos/simulation/FrameCounter.hpp"
#include "rtos/simulation/SimulationClock.hpp"

namespace rtos::simulation {

/** @brief Process-local authority for simulation frame and elapsed time. */
class SimulatorCore final {
public:
    /** @brief Returns the singleton simulator state container. */
    static SimulatorCore& instance() noexcept;

    SimulatorCore(const SimulatorCore&) = delete;
    SimulatorCore& operator=(const SimulatorCore&) = delete;
    SimulatorCore(SimulatorCore&&) = delete;
    SimulatorCore& operator=(SimulatorCore&&) = delete;

    /** @brief Resets state and starts the monotonic clock. */
    void start();
    /** @brief Stops the clock while preserving elapsed time. */
    void stop();
    /** @brief Advances the authoritative frame by one. */
    void advanceFrame() noexcept;
    /** @brief Mirrors coordinator-owned frame and elapsed time into a worker. */
    void synchronizeFrame(
        FrameCounter::value_type frame,
        SimulationClock::duration elapsed
    );

    /** @brief Returns a read-only frame-counter view. */
    [[nodiscard]] const FrameCounter& frameCounter() const noexcept;
    /** @brief Returns a read-only simulation-clock view. */
    [[nodiscard]] const SimulationClock& clock() const noexcept;

private:
    SimulatorCore() = default;

    FrameCounter frameCounter_;
    SimulationClock clock_;
};

}  // namespace rtos::simulation
