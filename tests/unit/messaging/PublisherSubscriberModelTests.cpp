#include "TestFramework.hpp"

#include "rtos/messaging/DispatchPort.hpp"

#include <vector>

namespace {

struct DummyMessage {
    int value;
};

class DummyPublisherModel {
public:
    explicit DummyPublisherModel(rtos::messaging::DispatchPort& port)
        : port_{port}
    {
    }

    void update()
    {
        port_.send(DummyMessage{++nextValue_});
    }

private:
    rtos::messaging::DispatchPort& port_;
    int nextValue_{};
};

class DummySubscriberModel {
public:
    explicit DummySubscriberModel(rtos::messaging::DispatchPort& port)
        : subscription_{port.subscribe<DummyMessage>(
              [this](const DummyMessage& message)
              {
                  receivedValues_.push_back(message.value);
              }
          )}
    {
    }

    [[nodiscard]] const std::vector<int>& receivedValues() const
    {
        return receivedValues_;
    }

private:
    rtos::messaging::SubscriptionHandle subscription_;
    std::vector<int> receivedValues_;
};

}  // namespace

TEST_CASE("subscriber model receives data five times from publisher model")
{
    rtos::messaging::DispatchPort port;
    DummyPublisherModel publisher{port};
    DummySubscriberModel subscriber{port};

    for (int frame = 0; frame < 5; ++frame) {
        publisher.update();
        const auto report = port.dispatchAll();

        REQUIRE(report.messagesDispatched == 1);
        REQUIRE(report.callbacksInvoked == 1);
    }

    REQUIRE(subscriber.receivedValues() == std::vector<int>({1, 2, 3, 4, 5}));
}
