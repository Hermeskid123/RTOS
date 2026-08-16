/**
 * @file
 * @brief Defines ModelFrameworkTests coverage for the RTOS framework test suite.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#include "TestFramework.hpp"

#include "messages/MotorStatus.hpp"
#include "models/ControlModel/ControlModel.hpp"
#include "models/MotorModel/MotorModel.hpp"
#include "models/SensorModel/SensorModel.hpp"
#include "rtos/logging/Logger.hpp"
#include "rtos/model/BaseModel.hpp"
#include "rtos/model/ModelRunner.hpp"

#include <cstdint>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace {

class ScopedSubscriberModel {
public:
    ScopedSubscriberModel(rtos::messaging::DispatchPort& port, int& callbackCount)
        : callbackCount_{callbackCount},
          subscription_{port.subscribe<rtos::messages::MotorStatus>(
              [this](const rtos::messages::MotorStatus&) { ++callbackCount_; }
          )}
    {
    }

private:
    int& callbackCount_;
    rtos::messaging::SubscriptionHandle subscription_;
};

}  // namespace

TEST_CASE("example models implement the base model lifecycle")
{
    REQUIRE((std::is_base_of_v<rtos::model::BaseModel, rtos::models::SensorModel>));
    REQUIRE((std::is_base_of_v<rtos::model::BaseModel, rtos::models::ControlModel>));
    REQUIRE((std::is_base_of_v<rtos::model::BaseModel, rtos::models::MotorModel>));
}

TEST_CASE("example models exchange sensor command and motor status messages")
{
    rtos::messaging::DispatchPort port;
    std::ostringstream logOutput;
    rtos::logging::Logger logger{logOutput, rtos::logging::LogLevel::debug};
    rtos::models::SensorModel sensor{port, logger};
    rtos::models::ControlModel control{port, logger};
    rtos::models::MotorModel motor{port, logger};
    std::vector<std::int32_t> reportedRpms;

    const auto statusSubscription = port.subscribe<rtos::messages::MotorStatus>(
        [&reportedRpms](const rtos::messages::MotorStatus& status)
        {
            reportedRpms.push_back(status.currentRpm);
        }
    );
    REQUIRE(statusSubscription.active());

    std::vector<rtos::model::BaseModel*> models{&sensor, &control, &motor};
    for (auto* model : models) {
        model->initialize();
        model->begin();
    }

    sensor.setSensorValue(12.5);
    sensor.operate();
    port.dispatchAll();
    control.operate();
    port.dispatchAll();
    motor.operate();
    port.dispatchAll();

    REQUIRE(control.latestSensorValue() == 12.5);
    REQUIRE(control.targetRpm() == 1250);
    REQUIRE(motor.targetRpm() == 1250);
    REQUIRE(motor.currentRpm() == 1250);
    REQUIRE(reportedRpms == std::vector<std::int32_t>({1250}));

    const std::string records = logOutput.str();
    REQUIRE(records.find("SensorModel BEGIN started") != std::string::npos);
    REQUIRE(records.find("SensorModel TX SensorData value=12.500000") != std::string::npos);
    REQUIRE(records.find("ControlModel RX SensorData value=12.500000") != std::string::npos);
    REQUIRE(records.find("ControlModel TX MotorCommand targetRpm=1250") != std::string::npos);
    REQUIRE(records.find("MotorModel RX MotorCommand targetRpm=1250") != std::string::npos);
    REQUIRE(records.find("MotorModel TX MotorStatus currentRpm=1250") != std::string::npos);
}

TEST_CASE("frozen models do not operate and terminated motor resets")
{
    rtos::messaging::DispatchPort port;
    std::ostringstream logOutput;
    rtos::logging::Logger logger{logOutput, rtos::logging::LogLevel::debug};
    rtos::models::SensorModel sensor{port, logger};
    rtos::models::MotorModel motor{port, logger};

    sensor.initialize();
    sensor.begin();
    sensor.freeze();
    sensor.operate();
    const auto frozenReport = port.dispatchAll();

    REQUIRE(frozenReport.messagesDispatched == 0);

    motor.initialize();
    motor.begin();
    motor.operate();
    motor.terminate();

    REQUIRE(motor.currentRpm() == 0);
    REQUIRE(motor.targetRpm() == 0);
}

TEST_CASE("model runner reports every lifecycle control status")
{
    rtos::messaging::DispatchPort port;
    std::ostringstream logOutput;
    rtos::logging::Logger logger{logOutput, rtos::logging::LogLevel::debug};
    rtos::models::SensorModel sensor{port, logger};
    rtos::models::ControlModel control{port, logger};
    rtos::model::ModelRunner runner;
    runner.add("SensorModel", sensor);
    runner.add("ControlModel", control);

    const auto initialized = runner.initialize();
    REQUIRE(initialized.size() == 2);
    REQUIRE(initialized[0].status == rtos::model::ControlStatus::stopped);
    REQUIRE(initialized[1].status == rtos::model::ControlStatus::stopped);

    const auto running = runner.begin();
    REQUIRE(running[0].status == rtos::model::ControlStatus::running);
    REQUIRE(running[1].status == rtos::model::ControlStatus::running);

    const auto stopped = runner.freeze();
    REQUIRE(stopped[0].status == rtos::model::ControlStatus::stopped);
    REQUIRE(stopped[1].status == rtos::model::ControlStatus::stopped);

    const auto terminated = runner.terminate();
    REQUIRE(terminated[0].status == rtos::model::ControlStatus::terminated);
    REQUIRE(terminated[1].status == rtos::model::ControlStatus::terminated);
    REQUIRE(rtos::model::toString(terminated[0].status) == "TERMINATED");
}

TEST_CASE("destroying a model removes callbacks that capture this")
{
    rtos::messaging::DispatchPort port;
    int callbackCount{};

    {
        ScopedSubscriberModel model{port, callbackCount};
        REQUIRE(port.subscriberCount<rtos::messages::MotorStatus>() == 1);
        port.send(rtos::messages::MotorStatus{900});
        port.dispatchAll();
        REQUIRE(callbackCount == 1);
        port.send(rtos::messages::MotorStatus{1000});
    }

    REQUIRE(port.subscriberCount<rtos::messages::MotorStatus>() == 0);
    const auto report = port.dispatchAll();
    REQUIRE(callbackCount == 1);
    REQUIRE(report.messagesWithoutSubscribers == 1);
}
