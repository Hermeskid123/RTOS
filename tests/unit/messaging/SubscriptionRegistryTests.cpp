#include "TestFramework.hpp"

#include "messages/MotorCommand.hpp"
#include "messages/SensorData.hpp"
#include "rtos/messaging/SubscriptionRegistry.hpp"

namespace {

using rtos::messages::MotorCommand;
using rtos::messages::SensorData;

}  // namespace

TEST_CASE("subscription registry counts callbacks by exact message type")
{
    rtos::messaging::SubscriptionRegistry registry;

    REQUIRE(registry.count<MotorCommand>() == 0);
    REQUIRE(registry.count<SensorData>() == 0);

    registry.add<MotorCommand>([](const MotorCommand&) {});
    registry.add<MotorCommand>([](const MotorCommand&) {});
    registry.add<SensorData>([](const SensorData&) {});

    REQUIRE(registry.count<MotorCommand>() == 2);
    REQUIRE(registry.count<SensorData>() == 1);
}

TEST_CASE("subscription registry canonicalizes qualified message types")
{
    rtos::messaging::SubscriptionRegistry registry;

    registry.add<const MotorCommand>([](const MotorCommand&) {});

    REQUIRE(registry.count<MotorCommand>() == 1);
    REQUIRE(registry.count<const MotorCommand&>() == 1);
    REQUIRE(registry.count<SensorData>() == 0);
}
