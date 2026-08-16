#include "TestFramework.hpp"

#include "messages/MotorCommand.hpp"
#include "messages/SensorData.hpp"
#include "rtos/messaging/SubscriptionRegistry.hpp"

#include <type_traits>

namespace {

using rtos::messages::MotorCommand;
using rtos::messages::SensorData;

static_assert(!std::is_copy_constructible_v<rtos::messaging::SubscriptionHandle>);
static_assert(std::is_move_constructible_v<rtos::messaging::SubscriptionHandle>);

}  // namespace

TEST_CASE("subscription registry counts callbacks by exact message type")
{
    rtos::messaging::SubscriptionRegistry registry;

    REQUIRE(registry.count<MotorCommand>() == 0);
    REQUIRE(registry.count<SensorData>() == 0);

    const auto firstMotor = registry.add<MotorCommand>([](const MotorCommand&) {});
    const auto secondMotor = registry.add<MotorCommand>([](const MotorCommand&) {});
    const auto sensor = registry.add<SensorData>([](const SensorData&) {});

    REQUIRE(registry.count<MotorCommand>() == 2);
    REQUIRE(registry.count<SensorData>() == 1);
}

TEST_CASE("subscription registry canonicalizes qualified message types")
{
    rtos::messaging::SubscriptionRegistry registry;

    const auto subscription = registry.add<const MotorCommand>([](const MotorCommand&) {});

    REQUIRE(registry.count<MotorCommand>() == 1);
    REQUIRE(registry.count<const MotorCommand&>() == 1);
    REQUIRE(registry.count<SensorData>() == 0);
}

TEST_CASE("subscription handles remain safe after registry destruction")
{
    rtos::messaging::SubscriptionHandle subscription;
    {
        rtos::messaging::SubscriptionRegistry registry;
        subscription = registry.add<MotorCommand>([](const MotorCommand&) {});
        REQUIRE(subscription.active());
    }

    REQUIRE(!subscription.active());
    subscription.reset();
    REQUIRE(!subscription.active());
}
