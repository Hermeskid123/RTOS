/**
 * @file
 * @brief Implements the SimulationClock framework API.
 */

#include "rtos/simulation/SimulationClock.hpp"

namespace rtos::simulation {

SimulationClock::duration SimulationClock::elapsed() const
{
    std::scoped_lock lock{mutex_};
    if (!running_) {
        return elapsedBeforeStart_;
    }
    return elapsedBeforeStart_ + (clock_type::now() - startedAt_);
}

bool SimulationClock::isRunning() const
{
    std::scoped_lock lock{mutex_};
    return running_;
}

void SimulationClock::start()
{
    std::scoped_lock lock{mutex_};
    if (running_) {
        return;
    }
    startedAt_ = clock_type::now();
    running_ = true;
}

void SimulationClock::stop()
{
    std::scoped_lock lock{mutex_};
    if (!running_) {
        return;
    }
    elapsedBeforeStart_ += clock_type::now() - startedAt_;
    running_ = false;
}

void SimulationClock::reset()
{
    std::scoped_lock lock{mutex_};
    elapsedBeforeStart_ = duration::zero();
    startedAt_ = clock_type::time_point{};
    running_ = false;
}

void SimulationClock::synchronize(const duration elapsed)
{
    std::scoped_lock lock{mutex_};
    elapsedBeforeStart_ = elapsed;
    startedAt_ = clock_type::now();
}

}  // namespace rtos::simulation
