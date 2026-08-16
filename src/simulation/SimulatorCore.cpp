/**
 * @file
 * @brief Implements the SimulatorCore framework API.
 */

#include "rtos/simulation/SimulatorCore.hpp"

namespace rtos::simulation {

SimulatorCore& SimulatorCore::instance() noexcept
{
    static SimulatorCore core;
    return core;
}

void SimulatorCore::start()
{
    clock_.stop();
    frameCounter_.reset();
    clock_.reset();
    clock_.start();
}

void SimulatorCore::stop()
{
    clock_.stop();
}

void SimulatorCore::advanceFrame() noexcept
{
    frameCounter_.advance();
}

void SimulatorCore::synchronizeFrame(
    const FrameCounter::value_type frame,
    const SimulationClock::duration elapsed
)
{
    frameCounter_.set(frame);
    clock_.synchronize(elapsed);
}

const FrameCounter& SimulatorCore::frameCounter() const noexcept
{
    return frameCounter_;
}

const SimulationClock& SimulatorCore::clock() const noexcept
{
    return clock_;
}

}  // namespace rtos::simulation
