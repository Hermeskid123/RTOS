/**
 * @file
 * @brief Defines DispatchPortTests coverage for the RTOS framework test suite.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#include "TestFramework.hpp"

#include "messages/MotorCommand.hpp"
#include "messages/MotorStatus.hpp"
#include "messages/SensorData.hpp"
#include "rtos/messaging/DispatchPort.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <thread>
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

    const auto subscription = port.subscribe<MotorCommand>(
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

    const auto subscription = port.subscribe<MotorCommand>(
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

    const auto subscription = port.subscribe<MotorCommand>(
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

    const auto subscription = port.subscribe<MotorCommand>(
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

    const auto subscription = port.subscribe<MotorCommand>(
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

    const auto firstSubscription = port.subscribe<MotorCommand>(
        [&firstReceived](const MotorCommand& command) { firstReceived = command.targetRpm; }
    );
    const auto secondSubscription = port.subscribe<MotorCommand>(
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

    const auto motorSubscription = port.subscribe<MotorCommand>(
        [&motorModel](const MotorCommand& command) { motorModel.receive(command); }
    );
    const auto loggerSubscription = port.subscribe<MotorCommand>(
        [&loggerModel](const MotorCommand& command) { loggerModel.receive(command); }
    );
    const auto sensorSubscription = port.subscribe<SensorData>(
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

    const auto subscription = port.subscribe<MotorStatus>(
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

    const auto subscription = port.subscribe<MotorCommand>(
        [&receivedMessages](const MotorCommand&) { ++receivedMessages; }
    );

    port.send(MotorCommand{1000});
    port.dispatchAll();
    port.dispatchAll();

    REQUIRE(receivedMessages == 1);
}

TEST_CASE("dispatch port reports its name queue and per-message traffic")
{
    rtos::messaging::DispatchPort port{"diagnostic"};
    const auto subscription = port.subscribe<MotorCommand>([](const MotorCommand&) {});

    port.send(MotorCommand{1800});
    port.send(MotorStatus{1700});

    REQUIRE(port.name() == "diagnostic");
    REQUIRE(port.pendingMessageCount() == 2);

    port.dispatchAll();
    const auto traffic = port.messageTraffic();

    REQUIRE(port.pendingMessageCount() == 0);
    REQUIRE(traffic.size() == 2);
    REQUIRE(traffic[0].messageName == "MotorCommand");
    REQUIRE(traffic[0].publishers == 0);
    REQUIRE(traffic[0].subscribers == 1);
    REQUIRE(traffic[0].messagesSent == 1);
    REQUIRE(traffic[0].messagesReceived == 1);
    REQUIRE(traffic[0].messagesDispatched == 1);
    REQUIRE(traffic[1].messageName == "MotorStatus");
    REQUIRE(traffic[1].publishers == 0);
    REQUIRE(traffic[1].subscribers == 0);
    REQUIRE(traffic[1].messagesSent == 1);
    REQUIRE(traffic[1].messagesWithoutSubscribers == 1);
}

TEST_CASE("named ports receive numbers and report publisher subscriber topology")
{
    rtos::messaging::DispatchPort port{"main"};
    auto publisher = port.createPort<MotorCommand>(
        "MotorCommand_port",
        rtos::messaging::PortDirection::publisher
    );
    auto subscriber = port.createPort<MotorCommand>(
        "MotorCommand_port",
        rtos::messaging::PortDirection::subscriber
    );
    int receivedRpm{};

    const auto subscription = subscriber.subscribe<MotorCommand>(
        [&receivedRpm](const MotorCommand& command) { receivedRpm = command.targetRpm; }
    );
    publisher.send(MotorCommand{2200});
    port.dispatchAll();

    const auto topology = port.portTopology();
    REQUIRE(publisher.number() == 1);
    REQUIRE(subscriber.number() == 2);
    REQUIRE(receivedRpm == 2200);
    REQUIRE(topology.size() == 2);
    REQUIRE(topology[0].name == "MotorCommand_port");
    REQUIRE(topology[0].routingId == MotorCommand::defaultRoutingId);
    REQUIRE(topology[0].transport == rtos::messaging::TransportType::inProcess);
    REQUIRE(topology[0].subscriberPorts == std::vector<std::size_t>({2}));
    REQUIRE(topology[0].publisherPorts.empty());
    REQUIRE(topology[1].publisherPorts == std::vector<std::size_t>({1}));
    REQUIRE(topology[1].subscriberPorts.empty());

    const auto traffic = port.messageTraffic();
    REQUIRE(traffic.size() == 1);
    REQUIRE(traffic[0].publishers == 1);
    REQUIRE(traffic[0].messagesReceived == 1);
}

TEST_CASE("IPC dispatch ports route using message defaults and explicit overrides")
{
    class CapturingTransport final : public rtos::messaging::MessageTransport {
    public:
        void send(rtos::messaging::TransportMessage message) override
        {
            messages.push_back(std::move(message));
        }

        std::vector<rtos::messaging::TransportMessage> messages;
    } transport;

    rtos::messaging::DispatchPort publisher{
        "control",
        rtos::messaging::TransportType::interProcess,
        &transport
    };
    rtos::messaging::DispatchPort subscriber{"motor"};
    auto output = publisher.createPort<MotorCommand>(
        "command.out", rtos::messaging::PortDirection::publisher
    );
    auto input = subscriber.createPort<MotorCommand>(
        "command.in", rtos::messaging::PortDirection::subscriber
    );
    int receivedRpm{};
    const auto subscription = input.subscribe<MotorCommand>(
        [&receivedRpm](const MotorCommand& command) { receivedRpm = command.targetRpm; }
    );

    output.send(MotorCommand{2400});
    REQUIRE(transport.messages.size() == 1);
    REQUIRE(transport.messages[0].routingId == MotorCommand::defaultRoutingId);
    REQUIRE(subscriber.receive(transport.messages[0]));
    subscriber.dispatchAll();
    REQUIRE(receivedRpm == 2400);

    auto alternate = publisher.createPort<MotorStatus>(
        "status.out", rtos::messaging::PortDirection::publisher, 9000
    );
    alternate.send(MotorStatus{2100});
    REQUIRE(transport.messages.back().routingId == 9000);
}

TEST_CASE("resetting a subscription handle prevents queued message delivery")
{
    rtos::messaging::DispatchPort port;
    int callbackCount{};
    auto subscription = port.subscribe<MotorCommand>(
        [&callbackCount](const MotorCommand&) { ++callbackCount; }
    );

    port.send(MotorCommand{1200});
    REQUIRE(subscription.active());
    subscription.reset();
    const auto report = port.dispatchAll();

    REQUIRE(!subscription.active());
    REQUIRE(port.subscriberCount<MotorCommand>() == 0);
    REQUIRE(callbackCount == 0);
    REQUIRE(report.messagesWithoutSubscribers == 1);
}

TEST_CASE("unsubscribe during dispatch invalidates a callback already in the snapshot")
{
    rtos::messaging::DispatchPort port;
    rtos::messaging::SubscriptionHandle secondSubscription;
    int firstCallbackCount{};
    int secondCallbackCount{};
    const auto firstSubscription = port.subscribe<MotorCommand>(
        [&](const MotorCommand&)
        {
            ++firstCallbackCount;
            secondSubscription.reset();
        }
    );
    secondSubscription = port.subscribe<MotorCommand>(
        [&secondCallbackCount](const MotorCommand&) { ++secondCallbackCount; }
    );

    port.send(MotorCommand{1300});
    const auto report = port.dispatchAll();

    REQUIRE(firstSubscription.active());
    REQUIRE(firstCallbackCount == 1);
    REQUIRE(secondCallbackCount == 0);
    REQUIRE(report.callbacksInvoked == 1);
    REQUIRE(port.subscriberCount<MotorCommand>() == 1);
}

TEST_CASE("moving a subscription handle transfers unsubscribe ownership")
{
    rtos::messaging::DispatchPort port;
    auto original = port.subscribe<MotorCommand>([](const MotorCommand&) {});
    auto owner = std::move(original);

    REQUIRE(!original.active());
    REQUIRE(owner.active());
    REQUIRE(port.subscriberCount<MotorCommand>() == 1);

    owner.reset();
    REQUIRE(port.subscriberCount<MotorCommand>() == 0);
}

TEST_CASE("concurrent publishers enqueue every message without loss")
{
    constexpr int publisherCount{8};
    constexpr int messagesPerPublisher{250};
    rtos::messaging::DispatchPort port;
    std::atomic<int> callbacksInvoked{};
    const auto subscription = port.subscribe<MotorCommand>(
        [&callbacksInvoked](const MotorCommand&)
        {
            callbacksInvoked.fetch_add(1, std::memory_order_relaxed);
        }
    );

    std::vector<std::jthread> publishers;
    for (int publisher = 0; publisher < publisherCount; ++publisher) {
        publishers.emplace_back(
            [&port]
            {
                for (int message = 0; message < messagesPerPublisher; ++message) {
                    port.send(MotorCommand{message});
                }
            }
        );
    }
    for (auto& publisher : publishers) {
        publisher.join();
    }

    REQUIRE(
        port.pendingMessageCount()
        == static_cast<std::size_t>(publisherCount * messagesPerPublisher)
    );
    const auto report = port.dispatchAll();
    REQUIRE(report.messagesDispatched == publisherCount * messagesPerPublisher);
    REQUIRE(callbacksInvoked.load(std::memory_order_relaxed)
            == publisherCount * messagesPerPublisher);
    REQUIRE(report.queueHighWaterMark == publisherCount * messagesPerPublisher);
}

TEST_CASE("publishing remains safe while another thread dispatches")
{
    constexpr int publisherCount{4};
    constexpr int messagesPerPublisher{300};
    rtos::messaging::DispatchPort port;
    std::atomic<int> publishersRemaining{publisherCount};
    std::atomic<int> callbacksInvoked{};
    const auto subscription = port.subscribe<MotorCommand>(
        [&callbacksInvoked](const MotorCommand&)
        {
            callbacksInvoked.fetch_add(1, std::memory_order_relaxed);
        }
    );

    std::jthread dispatcher{
        [&]
        {
            while (
                publishersRemaining.load(std::memory_order_acquire) != 0
                || port.pendingMessageCount() != 0
            ) {
                static_cast<void>(port.dispatchAll());
                std::this_thread::yield();
            }
        }
    };
    std::vector<std::jthread> publishers;
    for (int publisher = 0; publisher < publisherCount; ++publisher) {
        publishers.emplace_back(
            [&]
            {
                for (int message = 0; message < messagesPerPublisher; ++message) {
                    port.send(MotorCommand{message});
                }
                publishersRemaining.fetch_sub(1, std::memory_order_release);
            }
        );
    }
    for (auto& publisher : publishers) {
        publisher.join();
    }
    dispatcher.join();

    REQUIRE(callbacksInvoked.load(std::memory_order_relaxed)
            == publisherCount * messagesPerPublisher);
    REQUIRE(port.pendingMessageCount() == 0);
}

TEST_CASE("unsubscribe waits for a callback running on another thread")
{
    using namespace std::chrono_literals;

    rtos::messaging::DispatchPort port;
    std::promise<void> callbackEntered;
    std::promise<void> allowCallbackToExit;
    auto exitSignal = allowCallbackToExit.get_future().share();
    std::atomic<bool> resetStarted{};
    std::atomic<bool> callbackFinished{};
    auto subscription = port.subscribe<MotorCommand>(
        [&](const MotorCommand&)
        {
            callbackEntered.set_value();
            exitSignal.wait();
            callbackFinished.store(true, std::memory_order_release);
        }
    );
    port.send(MotorCommand{1500});

    std::jthread dispatcher{[&port] { static_cast<void>(port.dispatchAll()); }};
    callbackEntered.get_future().wait();
    auto reset = std::async(
        std::launch::async,
        [&]
        {
            resetStarted.store(true, std::memory_order_release);
            subscription.reset();
        }
    );
    while (!resetStarted.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    REQUIRE(reset.wait_for(2ms) == std::future_status::timeout);
    REQUIRE(!callbackFinished.load(std::memory_order_acquire));
    allowCallbackToExit.set_value();
    reset.get();
    dispatcher.join();

    REQUIRE(callbackFinished.load(std::memory_order_acquire));
    REQUIRE(!subscription.active());
}

TEST_CASE("bounded queue rejects newest messages when full")
{
    using namespace rtos::messaging;
    DispatchPort port{"bounded", QueueConfiguration{2, 32, QueueFullPolicy::rejectNewest}};
    std::vector<int> received;
    const auto subscription = port.subscribe<MotorCommand>(
        [&received](const MotorCommand& message) { received.push_back(message.targetRpm); }
    );

    REQUIRE(port.send(MotorCommand{1}) == SendResult::queued);
    REQUIRE(port.send(MotorCommand{2}) == SendResult::queued);
    REQUIRE(port.send(MotorCommand{3}) == SendResult::rejectedQueueFull);
    REQUIRE(port.pendingMessageCount() == 2);
    static_cast<void>(port.dispatchAll());

    REQUIRE(received == std::vector<int>({1, 2}));
    const auto statistics = port.queueStatistics();
    REQUIRE(statistics.capacity == 2);
    REQUIRE(statistics.highWaterMark == 2);
    REQUIRE(statistics.rejectedQueueFull == 1);
}

TEST_CASE("bounded queue can drop newest messages when full")
{
    using namespace rtos::messaging;
    DispatchPort port{"bounded", QueueConfiguration{1, 32, QueueFullPolicy::dropNewest}};

    REQUIRE(port.send(MotorCommand{1}) == SendResult::queued);
    REQUIRE(port.send(MotorCommand{2}) == SendResult::droppedNewest);
    REQUIRE(port.queueStatistics().droppedNewest == 1);
}

TEST_CASE("bounded queue can replace the oldest message when full")
{
    using namespace rtos::messaging;
    DispatchPort port{"bounded", QueueConfiguration{2, 32, QueueFullPolicy::dropOldest}};
    std::vector<int> received;
    const auto subscription = port.subscribe<MotorCommand>(
        [&received](const MotorCommand& message) { received.push_back(message.targetRpm); }
    );

    REQUIRE(port.send(MotorCommand{1}) == SendResult::queued);
    REQUIRE(port.send(MotorCommand{2}) == SendResult::queued);
    REQUIRE(port.send(MotorCommand{3}) == SendResult::queuedAfterDroppingOldest);
    static_cast<void>(port.dispatchAll());

    REQUIRE(received == std::vector<int>({2, 3}));
    REQUIRE(port.queueStatistics().droppedOldest == 1);
}

TEST_CASE("bounded queue rejects messages larger than its configured payload")
{
    struct LargeMessage {
        std::byte payload[64];
    };
    using namespace rtos::messaging;
    DispatchPort port{"bounded", QueueConfiguration{2, 16}};

    REQUIRE(port.send(LargeMessage{}) == SendResult::messageTooLarge);
    REQUIRE(port.pendingMessageCount() == 0);
    REQUIRE(port.queueStatistics().rejectedOversize == 1);
}
