#pragma once

#include "rtos/messaging/DispatchPort.hpp"
#include "rtos/model/BaseModel.hpp"

namespace rtos::logging {
class Logger;
}

namespace rtos::models {

class SensorModel final : public model::BaseModel {
public:
    SensorModel(messaging::DispatchPort& port, logging::Logger& logger);

    model::ControlStatus initialize() override;
    model::ControlStatus begin() override;
    model::ControlStatus freeze() override;
    model::ControlStatus operate() override;
    model::ControlStatus terminate() override;
    [[nodiscard]] model::ControlStatus status() const noexcept override;

    void setSensorValue(double value);
    [[nodiscard]] double sensorValue() const;

private:
    messaging::DispatchPort::Port sensorDataPort_;
    logging::Logger& logger_;
    double sensorValue_{};
    model::ControlStatus status_{model::ControlStatus::stopped};
};

}  // namespace rtos::models
