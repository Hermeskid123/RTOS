/**
 * @file
 * @brief Declares the ControlModel example application model.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

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

/** @brief Example controller converting SensorData into MotorCommand messages. */
class ControlModel final : public model::BaseModel {
public:
    /** @brief Creates subscriber and publisher endpoints on `port`. */
    ControlModel(messaging::DispatchPort& port, logging::Logger& logger);

    /** @copydoc model::BaseModel::initialize */
    model::ControlStatus initialize() override;
    /** @copydoc model::BaseModel::begin */
    model::ControlStatus begin() override;
    /** @copydoc model::BaseModel::freeze */
    model::ControlStatus freeze() override;
    /** @copydoc model::BaseModel::operate */
    model::ControlStatus operate() override;
    /** @copydoc model::BaseModel::terminate */
    model::ControlStatus terminate() override;
    /** @copydoc model::BaseModel::status */
    [[nodiscard]] model::ControlStatus status() const noexcept override;

    /** @brief Returns the most recently received sensor sample. */
    [[nodiscard]] double latestSensorValue() const;
    /** @brief Returns the command computed from the latest sample. */
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
