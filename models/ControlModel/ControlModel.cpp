#include "models/ControlModel/ControlModel.hpp"

#include "messages/MotorCommand.hpp"
#include "messages/SensorData.hpp"
#include "rtos/logging/Logger.hpp"

#include <string>

namespace rtos::models {

namespace {

constexpr std::int32_t rpmPerSensorUnit{100};

}  // namespace

ControlModel::ControlModel(messaging::DispatchPort& port, logging::Logger& logger)
    : sensorDataPort_{port.createPort<messages::SensorData>(
          "SensorData_port",
          messaging::PortDirection::subscriber
      )},
      motorCommandPort_{port.createPort<messages::MotorCommand>(
          "MotorCommand_port",
          messaging::PortDirection::publisher
      )},
      logger_{logger}
{
}

model::ControlStatus ControlModel::initialize()
{
    status_ = model::ControlStatus::stopped;
    hasSensorData_ = false;
    targetRpm_ = 0;

    if (!sensorDataSubscription_) {
        sensorDataSubscription_ = sensorDataPort_.subscribe<messages::SensorData>(
            [this](const messages::SensorData& data) { onSensorData(data); }
        );
    }

    logger_.log(logging::LogLevel::debug, "ControlModel", "INITIALIZE", "initialized");
    return status_;
}

model::ControlStatus ControlModel::begin()
{
    status_ = model::ControlStatus::running;
    logger_.log(logging::LogLevel::debug, "ControlModel", "BEGIN", "started");
    return status_;
}

model::ControlStatus ControlModel::freeze()
{
    status_ = model::ControlStatus::stopped;
    logger_.log(logging::LogLevel::debug, "ControlModel", "FREEZE", "paused");
    return status_;
}

model::ControlStatus ControlModel::operate()
{
    if (status_ == model::ControlStatus::running && hasSensorData_) {
        motorCommandPort_.send(messages::MotorCommand{targetRpm_});
        logger_.log(
            logging::LogLevel::debug,
            "ControlModel",
            "TX",
            "MotorCommand targetRpm=" + std::to_string(targetRpm_)
        );
    }

    return status_;
}

model::ControlStatus ControlModel::terminate()
{
    sensorDataSubscription_.reset();
    status_ = model::ControlStatus::terminated;
    hasSensorData_ = false;
    logger_.log(logging::LogLevel::debug, "ControlModel", "TERMINATE", "terminated");
    return status_;
}

model::ControlStatus ControlModel::status() const noexcept
{
    return status_;
}

double ControlModel::latestSensorValue() const
{
    return latestSensorValue_;
}

std::int32_t ControlModel::targetRpm() const
{
    return targetRpm_;
}

void ControlModel::onSensorData(const messages::SensorData& data)
{
    latestSensorValue_ = data.value;
    targetRpm_ = static_cast<std::int32_t>(data.value * rpmPerSensorUnit);
    hasSensorData_ = true;
    logger_.log(
        logging::LogLevel::debug,
        "ControlModel",
        "RX",
        "SensorData value=" + std::to_string(data.value)
    );
}

}  // namespace rtos::models
