#include "TestFramework.hpp"

#include "apps/rtos_sim/ModelProcess.hpp"
#include "messages/MotorCommand.hpp"
#include "messages/SensorData.hpp"

#include <algorithm>
#include <chrono>

TEST_CASE("models run in distinct processes and route messages over IPC")
{
    auto sensor = rtos::host::ModelProcess::launch(
        "SensorModel", rtos::logging::LogLevel::error, false, false
    );
    auto control = rtos::host::ModelProcess::launch(
        "ControlModel", rtos::logging::LogLevel::error, false, false
    );

    REQUIRE(sensor->processId() != control->processId());
    REQUIRE(sensor->processId() != 0);
    REQUIRE(control->processId() != 0);
    REQUIRE(std::ranges::all_of(
        sensor->topology(),
        [](const rtos::messaging::PortTopology& endpoint)
        {
            return endpoint.transport == rtos::messaging::TransportType::interProcess;
        }
    ));

    static_cast<void>(sensor->initialize());
    static_cast<void>(control->initialize());
    static_cast<void>(sensor->begin());
    static_cast<void>(control->begin());

    const auto sensorFrame = sensor->operate(1, std::chrono::milliseconds{10});
    REQUIRE(sensorFrame.messages.size() == 1);
    REQUIRE(sensorFrame.executionTime >= std::chrono::nanoseconds::zero());
    REQUIRE(
        sensorFrame.messages[0].queuedAt
        != std::chrono::steady_clock::time_point{}
    );
    REQUIRE(
        sensorFrame.messages[0].routingId
        == rtos::messages::SensorData::defaultRoutingId
    );
    static_cast<void>(control->deliver(sensorFrame.messages[0]));
    const auto dispatch = control->dispatch();
    REQUIRE(dispatch.dispatch.queueDepthAtStart == 1);
    REQUIRE(dispatch.dispatch.queueHighWaterMark >= 1);
    REQUIRE(
        dispatch.dispatch.totalDispatchLatency
        >= std::chrono::nanoseconds::zero()
    );

    const auto controlFrame = control->operate(2, std::chrono::milliseconds{20});
    REQUIRE(controlFrame.messages.size() == 1);
    REQUIRE(
        controlFrame.messages[0].routingId
        == rtos::messages::MotorCommand::defaultRoutingId
    );

    static_cast<void>(sensor->terminate());
    static_cast<void>(control->terminate());
}
