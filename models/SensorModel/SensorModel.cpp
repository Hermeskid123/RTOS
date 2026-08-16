#include "models/SensorModel/SensorModel.hpp"

#include "messages/SensorData.hpp"
#include "rtos/logging/Logger.hpp"

#include <string>

namespace rtos::models {

SensorModel::SensorModel(messaging::DispatchPort& port, logging::Logger& logger)
    : sensorDataPort_{port.createPort<messages::SensorData>(
          "SensorData_port",
          messaging::PortDirection::publisher
      )},
      logger_{logger}
{
}

model::ControlStatus SensorModel::initialize()
{
    status_ = model::ControlStatus::stopped;
    logger_.log(logging::LogLevel::debug, "SensorModel", "INITIALIZE", "initialized");
    return status_;
}

model::ControlStatus SensorModel::begin()
{
    status_ = model::ControlStatus::running;
    logger_.log(logging::LogLevel::debug, "SensorModel", "BEGIN", "started");
    return status_;
}

model::ControlStatus SensorModel::freeze()
{
    status_ = model::ControlStatus::stopped;
    logger_.log(logging::LogLevel::debug, "SensorModel", "FREEZE", "paused");
    return status_;
}

model::ControlStatus SensorModel::operate()
{
    if (status_ == model::ControlStatus::running) {
        sensorDataPort_.send(messages::SensorData{sensorValue_});
        logger_.log(
            logging::LogLevel::debug,
            "SensorModel",
            "TX",
            "SensorData value=" + std::to_string(sensorValue_)
        );
    }

    return status_;
}

model::ControlStatus SensorModel::terminate()
{
    status_ = model::ControlStatus::terminated;
    logger_.log(logging::LogLevel::debug, "SensorModel", "TERMINATE", "terminated");
    return status_;
}

model::ControlStatus SensorModel::status() const noexcept
{
    return status_;
}

void SensorModel::setSensorValue(const double value)
{
    sensorValue_ = value;
}

double SensorModel::sensorValue() const
{
    return sensorValue_;
}

}  // namespace rtos::models
