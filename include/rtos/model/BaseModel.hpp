/**
 * @file
 * @brief Declares the public BaseModel framework API.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include "rtos/model/ControlStatus.hpp"
#include "rtos/simulation/SimulatorCore.hpp"

namespace rtos::model {

/**
 * @brief Abstract lifecycle contract implemented by every application model.
 * @details Models obtain read-only frame and clock views from SimulatorCore and
 * remain independent of concrete host-thread or RTOS scheduling APIs.
 */
class BaseModel {
public:
    /** @brief Binds the model to the process-local simulator time sources. */
    BaseModel() noexcept
        : frameCounter_{simulation::SimulatorCore::instance().frameCounter()},
          clock_{simulation::SimulatorCore::instance().clock()}
    {
    }
    BaseModel(const BaseModel&) = delete;
    BaseModel& operator=(const BaseModel&) = delete;
    BaseModel(BaseModel&&) = delete;
    BaseModel& operator=(BaseModel&&) = delete;
    virtual ~BaseModel() = default;

    /** @brief Prepares dependencies and subscriptions before execution. */
    virtual ControlStatus initialize() = 0;
    /** @brief Starts or resumes scheduled execution. */
    virtual ControlStatus begin() = 0;
    /** @brief Pauses execution without tearing down the model. */
    virtual ControlStatus freeze() = 0;
    /** @brief Performs one scheduler-controlled model cycle. */
    virtual ControlStatus operate() = 0;
    /** @brief Performs final cleanup and enters the terminated state. */
    virtual ControlStatus terminate() = 0;
    /** @brief Returns the model's current lifecycle state. */
    [[nodiscard]] virtual ControlStatus status() const noexcept = 0;

    /** @brief Returns the authoritative frame visible to this model. */
    [[nodiscard]] simulation::FrameCounter::value_type currentFrame() const noexcept
    {
        return frameCounter_.current();
    }

    /** @brief Returns monotonic elapsed simulation time. */
    [[nodiscard]] simulation::SimulationClock::duration clockTime() const
    {
        return clock_.elapsed();
    }

private:
    const simulation::FrameCounter& frameCounter_;
    const simulation::SimulationClock& clock_;
};

}  // namespace rtos::model
