/**
 * @file
 * @brief Declares the MotorModel example application model.
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

class MotorModel final : public model::BaseModel {
public:
    MotorModel(messaging::DispatchPort& port, logging::Logger& logger);

    model::ControlStatus initialize() override;
    model::ControlStatus begin() override;
    model::ControlStatus freeze() override;
    model::ControlStatus operate() override;
    model::ControlStatus terminate() override;
    [[nodiscard]] model::ControlStatus status() const noexcept override;

    [[nodiscard]] std::int32_t currentRpm() const;
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
