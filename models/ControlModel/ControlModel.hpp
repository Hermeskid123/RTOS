#pragma once

#include "rtos/messaging/DispatchPort.hpp"
#include "rtos/messaging/SubscriptionHandle.hpp"
#include "rtos/model/BaseModel.hpp"

#include <cstdint>

namespace rtos::messages {
struct SensorData;
}

namespace rtos::logging {
class Logger;
}

namespace rtos::models {

class ControlModel final : public model::BaseModel {
public:
    ControlModel(messaging::DispatchPort& port, logging::Logger& logger);

    model::ControlStatus initialize() override;
    model::ControlStatus begin() override;
    model::ControlStatus freeze() override;
    model::ControlStatus operate() override;
    model::ControlStatus terminate() override;
    [[nodiscard]] model::ControlStatus status() const noexcept override;

    [[nodiscard]] double latestSensorValue() const;
    [[nodiscard]] std::int32_t targetRpm() const;

private:
    void onSensorData(const messages::SensorData& data);

    messaging::DispatchPort::Port sensorDataPort_;
    messaging::DispatchPort::Port motorCommandPort_;
    logging::Logger& logger_;
    double latestSensorValue_{};
    std::int32_t targetRpm_{};
    messaging::SubscriptionHandle sensorDataSubscription_;
    bool hasSensorData_{};
    model::ControlStatus status_{model::ControlStatus::stopped};
};

}  // namespace rtos::models
