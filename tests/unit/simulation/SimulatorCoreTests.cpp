/**
 * @file
 * @brief Defines SimulatorCoreTests coverage for the RTOS framework test suite.
 */

#include "TestFramework.hpp"

#include "rtos/model/BaseModel.hpp"
#include "rtos/simulation/SimulatorCore.hpp"

#include <chrono>
#include <thread>

namespace {

class CoreAwareModel final : public rtos::model::BaseModel {
public:
    rtos::model::ControlStatus initialize() override { return status_; }
    rtos::model::ControlStatus begin() override
    {
        status_ = rtos::model::ControlStatus::running;
        return status_;
    }
    rtos::model::ControlStatus freeze() override
    {
        status_ = rtos::model::ControlStatus::stopped;
        return status_;
    }
    rtos::model::ControlStatus operate() override { return status_; }
    rtos::model::ControlStatus terminate() override
    {
        status_ = rtos::model::ControlStatus::terminated;
        return status_;
    }
    [[nodiscard]] rtos::model::ControlStatus status() const noexcept override
    {
        return status_;
    }

private:
    rtos::model::ControlStatus status_{rtos::model::ControlStatus::stopped};
};

}  // namespace

TEST_CASE("simulator core provides one shared frame counter and clock")
{
    auto& first = rtos::simulation::SimulatorCore::instance();
    auto& second = rtos::simulation::SimulatorCore::instance();

    REQUIRE(&first == &second);
    REQUIRE(&first.frameCounter() == &second.frameCounter());
    REQUIRE(&first.clock() == &second.clock());
}

TEST_CASE("base models observe frames advanced by the simulator core")
{
    auto& core = rtos::simulation::SimulatorCore::instance();
    CoreAwareModel first;
    CoreAwareModel second;

    core.start();
    REQUIRE(first.currentFrame() == 0);
    core.advanceFrame();
    core.advanceFrame();

    REQUIRE(first.currentFrame() == 2);
    REQUIRE(second.currentFrame() == 2);
    core.stop();
}

TEST_CASE("simulation clock starts resets and stops with the simulator core")
{
    using namespace std::chrono_literals;

    auto& core = rtos::simulation::SimulatorCore::instance();
    CoreAwareModel model;

    core.start();
    REQUIRE(core.clock().isRunning());
    std::this_thread::sleep_for(2ms);
    REQUIRE(model.clockTime() > rtos::simulation::SimulationClock::duration::zero());

    core.stop();
    const auto stoppedAt = model.clockTime();
    std::this_thread::sleep_for(2ms);
    REQUIRE(!core.clock().isRunning());
    REQUIRE(model.clockTime() == stoppedAt);

    core.start();
    REQUIRE(model.currentFrame() == 0);
    core.stop();
}
