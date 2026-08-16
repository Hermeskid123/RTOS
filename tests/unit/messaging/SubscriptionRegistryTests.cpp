/**
 * @file
 * @brief Defines SubscriptionRegistryTests coverage for the RTOS framework test suite.
 */

#include "TestFramework.hpp"

#include "messages/MotorCommand.hpp"
#include "messages/SensorData.hpp"
#include "rtos/messaging/SubscriptionRegistry.hpp"

#include <atomic>
#include <type_traits>
#include <thread>
#include <vector>

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

TEST_CASE("subscription registration and removal are thread safe")
{
    rtos::messaging::SubscriptionRegistry registry;
    std::atomic<bool> everyHandleActive{true};
    std::vector<std::jthread> workers;
    for (int worker = 0; worker < 8; ++worker) {
        workers.emplace_back(
            [&registry, &everyHandleActive]
            {
                for (int iteration = 0; iteration < 200; ++iteration) {
                    auto subscription = registry.add<MotorCommand>(
                        [](const MotorCommand&) {}
                    );
                    if (!subscription.active()) {
                        everyHandleActive.store(false, std::memory_order_relaxed);
                    }
                }
            }
        );
    }
    for (auto& worker : workers) {
        worker.join();
    }

    REQUIRE(everyHandleActive.load(std::memory_order_relaxed));
    REQUIRE(registry.count<MotorCommand>() == 0);
}
