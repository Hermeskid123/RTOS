/**
 * @file
 * @brief Declares the SensorModel example application model.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include "rtos/messaging/DispatchPort.hpp"
#include "rtos/model/BaseModel.hpp"

namespace rtos::logging {
class Logger;
}

namespace rtos::models {

/** @brief Example producer that publishes one SensorData sample per cycle. */
class SensorModel final : public model::BaseModel {
public:
    /** @brief Creates the model and its named publisher endpoint. */
    SensorModel(messaging::DispatchPort& port, logging::Logger& logger);

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

    /** @brief Replaces the value published by the next active cycle. */
    void setSensorValue(double value);
    /** @brief Returns the currently configured sample value. */
    [[nodiscard]] double sensorValue() const;

private:
    messaging::DispatchPort::Port sensorDataPort_;
    logging::Logger& logger_;
    double sensorValue_{};
    model::ControlStatus status_{model::ControlStatus::stopped};
};

}  // namespace rtos::models
