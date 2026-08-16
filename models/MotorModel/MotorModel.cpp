#include "models/MotorModel/MotorModel.hpp"

#include "messages/MotorCommand.hpp"
#include "messages/MotorStatus.hpp"
#include "rtos/logging/Logger.hpp"

#include <string>

namespace rtos::models {

MotorModel::MotorModel(messaging::DispatchPort& port, logging::Logger& logger)
    : motorCommandPort_{port.createPort<messages::MotorCommand>(
          "MotorCommand_port",
          messaging::PortDirection::subscriber
      )},
      motorStatusPort_{port.createPort<messages::MotorStatus>(
          "MotorStatus_port",
          messaging::PortDirection::publisher
      )},
      logger_{logger}
{
}

model::ControlStatus MotorModel::initialize()
{
    status_ = model::ControlStatus::stopped;
    currentRpm_ = 0;
    targetRpm_ = 0;

    if (!subscriptionRegistered_) {
        motorCommandPort_.subscribe<messages::MotorCommand>(
            [this](const messages::MotorCommand& command) { onMotorCommand(command); }
        );
        subscriptionRegistered_ = true;
    }

    logger_.log(logging::LogLevel::debug, "MotorModel", "INITIALIZE", "initialized");
    return status_;
}

model::ControlStatus MotorModel::begin()
{
    status_ = model::ControlStatus::running;
    logger_.log(logging::LogLevel::debug, "MotorModel", "BEGIN", "started");
    return status_;
}

model::ControlStatus MotorModel::freeze()
{
    status_ = model::ControlStatus::stopped;
    logger_.log(logging::LogLevel::debug, "MotorModel", "FREEZE", "paused");
    return status_;
}

model::ControlStatus MotorModel::operate()
{
    if (status_ != model::ControlStatus::running) {
        return status_;
    }

    currentRpm_ = targetRpm_;
    motorStatusPort_.send(messages::MotorStatus{currentRpm_});
    logger_.log(
        logging::LogLevel::debug,
        "MotorModel",
        "TX",
        "MotorStatus currentRpm=" + std::to_string(currentRpm_)
    );
    return status_;
}

model::ControlStatus MotorModel::terminate()
{
    status_ = model::ControlStatus::terminated;
    currentRpm_ = 0;
    targetRpm_ = 0;
    logger_.log(logging::LogLevel::debug, "MotorModel", "TERMINATE", "terminated");
    return status_;
}

model::ControlStatus MotorModel::status() const noexcept
{
    return status_;
}

std::int32_t MotorModel::currentRpm() const
{
    return currentRpm_;
}

std::int32_t MotorModel::targetRpm() const
{
    return targetRpm_;
}

void MotorModel::onMotorCommand(const messages::MotorCommand& command)
{
    targetRpm_ = command.targetRpm;
    logger_.log(
        logging::LogLevel::debug,
        "MotorModel",
        "RX",
        "MotorCommand targetRpm=" + std::to_string(command.targetRpm)
    );
}

}  // namespace rtos::models
