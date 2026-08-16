#include "TestFramework.hpp"

#include "messages/MotorCommand.hpp"
#include "messages/MotorStatus.hpp"
#include "messages/SensorData.hpp"
#include "rtos/messaging/DispatchPort.hpp"

#include <type_traits>
#include <vector>

namespace {

using rtos::messages::MotorCommand;
using rtos::messages::MotorStatus;
using rtos::messages::SensorData;

struct MotorModelProbe {
    void receive(const MotorCommand& command)
    {
        ++messagesReceived;
        lastTargetRpm = command.targetRpm;
    }

    int messagesReceived{};
    int lastTargetRpm{};
};

struct LoggerModelProbe {
    void receive(const MotorCommand& command)
    {
        ++messagesReceived;
        lastTargetRpm = command.targetRpm;
    }

    int messagesReceived{};
    int lastTargetRpm{};
};

struct ControlModelProbe {
    void receive(const SensorData& data)
    {
        ++messagesReceived;
        lastSensorValue = data.value;
    }

    int messagesReceived{};
    double lastSensorValue{};
};

static_assert(std::is_default_constructible_v<rtos::messaging::DispatchPort>);
static_assert(!std::is_copy_constructible_v<rtos::messaging::DispatchPort>);

}  // namespace

TEST_CASE("send defers callback execution until dispatch")
{
    rtos::messaging::DispatchPort port;
    bool callbackInvoked{};

    port.subscribe<MotorCommand>(
        [&callbackInvoked](const MotorCommand&) { callbackInvoked = true; }
    );

    port.send(MotorCommand{1500});
    REQUIRE(!callbackInvoked);

    port.dispatchAll();
    REQUIRE(callbackInvoked);
}

TEST_CASE("queued messages own a copy of publisher data")
{
    rtos::messaging::DispatchPort port;
    int receivedRpm{};

    port.subscribe<MotorCommand>(
        [&receivedRpm](const MotorCommand& command) { receivedRpm = command.targetRpm; }
    );

    {
        MotorCommand command{2100};
        port.send<MotorCommand>(command);
        command.targetRpm = 0;
    }

    port.dispatchAll();
    REQUIRE(receivedRpm == 2100);
}

TEST_CASE("multiple queued messages retain publication order")
{
    rtos::messaging::DispatchPort port;
    std::vector<int> receivedValues;

    port.subscribe<MotorCommand>(
        [&receivedValues](const MotorCommand& command)
        {
            receivedValues.push_back(command.targetRpm);
        }
    );

    port.send(MotorCommand{1000});
    port.send(MotorCommand{1500});
    port.send(MotorCommand{2000});
    port.dispatchAll();

    REQUIRE(receivedValues == std::vector<int>({1000, 1500, 2000}));
}

TEST_CASE("messages sent from callbacks wait for the next dispatch cycle")
{
    rtos::messaging::DispatchPort port;
    int callbackCount{};

    port.subscribe<MotorCommand>(
        [&port, &callbackCount](const MotorCommand& command)
        {
            ++callbackCount;
            if (command.targetRpm == 1000) {
                port.send(MotorCommand{2000});
            }
        }
    );

    port.send(MotorCommand{1000});
    port.dispatchAll();
    REQUIRE(callbackCount == 1);

    port.dispatchAll();
    REQUIRE(callbackCount == 2);
}

TEST_CASE("nested dispatch cannot cross the active dispatch boundary")
{
    rtos::messaging::DispatchPort port;
    std::vector<int> receivedValues;

    port.subscribe<MotorCommand>(
        [&port, &receivedValues](const MotorCommand& command)
        {
            receivedValues.push_back(command.targetRpm);
            if (command.targetRpm == 1000) {
                port.send(MotorCommand{2000});
                port.dispatchAll();
            }
        }
    );

    port.send(MotorCommand{1000});
    port.dispatchAll();
    REQUIRE(receivedValues == std::vector<int>({1000}));

    port.dispatchAll();
    REQUIRE(receivedValues == std::vector<int>({1000, 2000}));
}

TEST_CASE("all subscribers for a message type receive the message")
{
    rtos::messaging::DispatchPort port;
    int firstReceived{};
    int secondReceived{};

    port.subscribe<MotorCommand>(
        [&firstReceived](const MotorCommand& command) { firstReceived = command.targetRpm; }
    );
    port.subscribe<MotorCommand>(
        [&secondReceived](const MotorCommand& command) { secondReceived = command.targetRpm; }
    );

    REQUIRE(port.subscriberCount<MotorCommand>() == 2);
    REQUIRE(port.subscriberCount<SensorData>() == 0);

    port.send(MotorCommand{1750});
    const auto report = port.dispatchAll();

    REQUIRE(firstReceived == 1750);
    REQUIRE(secondReceived == 1750);
    REQUIRE(report.messagesDispatched == 1);
    REQUIRE(report.callbacksInvoked == 2);
    REQUIRE(report.messagesWithoutSubscribers == 0);
    REQUIRE(report.messagesProcessed() == 1);
}

TEST_CASE("project messages follow the milestone routing matrix")
{
    rtos::messaging::DispatchPort port;
    MotorModelProbe motorModel;
    LoggerModelProbe loggerModel;
    ControlModelProbe controlModel;

    port.subscribe<MotorCommand>(
        [&motorModel](const MotorCommand& command) { motorModel.receive(command); }
    );
    port.subscribe<MotorCommand>(
        [&loggerModel](const MotorCommand& command) { loggerModel.receive(command); }
    );
    port.subscribe<SensorData>(
        [&controlModel](const SensorData& data) { controlModel.receive(data); }
    );

    port.send(MotorCommand{1250});
    port.send(SensorData{42.5});
    const auto report = port.dispatchAll();

    REQUIRE(motorModel.messagesReceived == 1);
    REQUIRE(motorModel.lastTargetRpm == 1250);
    REQUIRE(loggerModel.messagesReceived == 1);
    REQUIRE(loggerModel.lastTargetRpm == 1250);
    REQUIRE(controlModel.messagesReceived == 1);
    REQUIRE(controlModel.lastSensorValue == 42.5);
    REQUIRE(report.messagesDispatched == 2);
    REQUIRE(report.callbacksInvoked == 3);
    REQUIRE(report.messagesWithoutSubscribers == 0);
}

TEST_CASE("messages without subscribers are discarded safely")
{
    rtos::messaging::DispatchPort port;
    int receivedMessages{};

    port.send(MotorStatus{900});
    const auto unhandledReport = port.dispatchAll();

    REQUIRE(unhandledReport.messagesDispatched == 0);
    REQUIRE(unhandledReport.callbacksInvoked == 0);
    REQUIRE(unhandledReport.messagesWithoutSubscribers == 1);
    REQUIRE(unhandledReport.messagesProcessed() == 1);

    port.subscribe<MotorStatus>(
        [&receivedMessages](const MotorStatus&) { ++receivedMessages; }
    );
    port.dispatchAll();

    REQUIRE(receivedMessages == 0);

    port.send(MotorStatus{1000});
    const auto handledReport = port.dispatchAll();
    REQUIRE(receivedMessages == 1);
    REQUIRE(handledReport.messagesDispatched == 1);
    REQUIRE(handledReport.callbacksInvoked == 1);
    REQUIRE(handledReport.messagesWithoutSubscribers == 0);
}

TEST_CASE("an empty dispatch returns an empty routing report")
{
    rtos::messaging::DispatchPort port;

    const auto report = port.dispatchAll();

    REQUIRE(report.messagesDispatched == 0);
    REQUIRE(report.callbacksInvoked == 0);
    REQUIRE(report.messagesWithoutSubscribers == 0);
    REQUIRE(report.messagesProcessed() == 0);
}

TEST_CASE("a completed dispatch does not redeliver messages")
{
    rtos::messaging::DispatchPort port;
    int receivedMessages{};

    port.subscribe<MotorCommand>(
        [&receivedMessages](const MotorCommand&) { ++receivedMessages; }
    );

    port.send(MotorCommand{1000});
    port.dispatchAll();
    port.dispatchAll();

    REQUIRE(receivedMessages == 1);
}
