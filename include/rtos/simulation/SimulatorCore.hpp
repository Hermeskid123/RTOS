#pragma once

#include "rtos/simulation/FrameCounter.hpp"
#include "rtos/simulation/SimulationClock.hpp"

namespace rtos::simulation {

class SimulatorCore final {
public:
    static SimulatorCore& instance() noexcept;

    SimulatorCore(const SimulatorCore&) = delete;
    SimulatorCore& operator=(const SimulatorCore&) = delete;
    SimulatorCore(SimulatorCore&&) = delete;
    SimulatorCore& operator=(SimulatorCore&&) = delete;

    void start();
    void stop();
    void advanceFrame() noexcept;
    void synchronizeFrame(
        FrameCounter::value_type frame,
        SimulationClock::duration elapsed
    );

    [[nodiscard]] const FrameCounter& frameCounter() const noexcept;
    [[nodiscard]] const SimulationClock& clock() const noexcept;

private:
    SimulatorCore() = default;

    FrameCounter frameCounter_;
    SimulationClock clock_;
};

}  // namespace rtos::simulation
