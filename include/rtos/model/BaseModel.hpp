#pragma once

#include "rtos/model/ControlStatus.hpp"
#include "rtos/simulation/SimulatorCore.hpp"

namespace rtos::model {

class BaseModel {
public:
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

    virtual ControlStatus initialize() = 0;
    virtual ControlStatus begin() = 0;
    virtual ControlStatus freeze() = 0;
    virtual ControlStatus operate() = 0;
    virtual ControlStatus terminate() = 0;
    [[nodiscard]] virtual ControlStatus status() const noexcept = 0;

    [[nodiscard]] simulation::FrameCounter::value_type currentFrame() const noexcept
    {
        return frameCounter_.current();
    }

    [[nodiscard]] simulation::SimulationClock::duration clockTime() const
    {
        return clock_.elapsed();
    }

private:
    const simulation::FrameCounter& frameCounter_;
    const simulation::SimulationClock& clock_;
};

}  // namespace rtos::model
