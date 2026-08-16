/**
 * @file
 * @brief Declares the MotorModel example application model.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include "rtos/messaging/DispatchPort.hpp"
#include "rtos/messaging/SubscriptionHandle.hpp"
#include "rtos/model/BaseModel.hpp"

#include <cstdint>

namespace rtos::messages {
struct MotorCommand;
}

namespace rtos::logging {
class Logger;
}

namespace rtos::models {

/** @brief Example actuator consuming commands and publishing motor status. */
class MotorModel final : public model::BaseModel {
public:
    /** @brief Creates subscriber and publisher endpoints on `port`. */
    MotorModel(messaging::DispatchPort& port, logging::Logger& logger);

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

    /** @brief Returns the simulated current motor speed. */
    [[nodiscard]] std::int32_t currentRpm() const;
    /** @brief Returns the most recently received commanded speed. */
    [[nodiscard]] std::int32_t targetRpm() const;

private:
    void onMotorCommand(const messages::MotorCommand& command);

    messaging::DispatchPort::Port motorCommandPort_;
    messaging::DispatchPort::Port motorStatusPort_;
    logging::Logger& logger_;
    std::int32_t currentRpm_{};
    std::int32_t targetRpm_{};
    messaging::SubscriptionHandle motorCommandSubscription_;
    model::ControlStatus status_{model::ControlStatus::stopped};
};

}  // namespace rtos::models
