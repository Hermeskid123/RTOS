/**
 * @file
 * @brief Defines FreeRtosAdapterTests coverage for the RTOS framework test suite.
 */

#include "TestFramework.hpp"

#include "messages/MotorCommand.hpp"
#include "rtos/messaging/DispatchPort.hpp"
#include "rtos/model/BaseModel.hpp"
#include "rtos/platform/freertos/FreeRtosAdapter.hpp"

#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace {

class FakeKernel final : public rtos::platform::freertos::FreeRtosKernel {
public:
    struct Task {
        rtos::platform::freertos::TaskEntry entry;
        std::string name;
        std::uint32_t stackDepth;
        void* context;
        std::uint32_t priority;
        bool deleted{};
    };

    rtos::platform::freertos::TaskHandle createTask(
        const rtos::platform::freertos::TaskEntry entry,
        const std::string_view name,
        const std::uint32_t stackDepth,
        void* const context,
        const std::uint32_t priority
    ) override
    {
        if (tasks.size() == failTaskCreationAt) {
            return nullptr;
        }
        tasks.push_back(Task{entry, std::string{name}, stackDepth, context, priority});
        return &tasks.back();
    }

    void deleteTask(const rtos::platform::freertos::TaskHandle handle) noexcept override
    {
        static_cast<Task*>(handle)->deleted = true;
        ++tasksDeletedByHandle;
    }

    rtos::platform::freertos::Tick tickCount() const noexcept override
    {
        return currentTick;
    }

    void delayUntil(
        rtos::platform::freertos::Tick& previousWakeTime,
        const rtos::platform::freertos::Tick period
    ) override
    {
        previousWakeTime += period;
        currentTick = previousWakeTime;
        if (onDelay) {
            onDelay();
        }
    }

    void deleteCurrentTask() noexcept override
    {
        ++tasksDeleted;
    }

    void execute(const std::string_view name)
    {
        for (const auto& task : tasks) {
            if (task.name == name) {
                task.entry(task.context);
                return;
            }
        }
        test::fail("task exists", __FILE__, __LINE__);
    }

    std::deque<Task> tasks;
    std::function<void()> onDelay;
    mutable rtos::platform::freertos::Tick currentTick{};
    std::size_t tasksDeleted{};
    std::size_t tasksDeletedByHandle{};
    std::size_t failTaskCreationAt{static_cast<std::size_t>(-1)};
};

class ProbeModel final : public rtos::model::BaseModel {
public:
    rtos::model::ControlStatus initialize() override
    {
        ++initializeCalls;
        status_ = rtos::model::ControlStatus::stopped;
        return status_;
    }

    rtos::model::ControlStatus begin() override
    {
        ++beginCalls;
        status_ = rtos::model::ControlStatus::running;
        return status_;
    }

    rtos::model::ControlStatus freeze() override { return status_; }

    rtos::model::ControlStatus operate() override
    {
        ++operateCalls;
        return status_;
    }

    rtos::model::ControlStatus terminate() override
    {
        ++terminateCalls;
        status_ = rtos::model::ControlStatus::terminated;
        return status_;
    }

    rtos::model::ControlStatus status() const noexcept override { return status_; }

    std::size_t initializeCalls{};
    std::size_t beginCalls{};
    std::size_t operateCalls{};
    std::size_t terminateCalls{};

private:
    rtos::model::ControlStatus status_{rtos::model::ControlStatus::stopped};
};

}  // namespace

TEST_CASE("FreeRTOS adapter creates one task per model and a messaging task")
{
    FakeKernel kernel;
    rtos::messaging::DispatchPort port;
    ProbeModel first;
    ProbeModel second;
    rtos::platform::freertos::FreeRtosAdapter adapter{kernel, port};

    REQUIRE(adapter.addModel("First", first, {512, 3, 5}));
    REQUIRE(adapter.addModel("Second", second, {768, 4, 10}));
    REQUIRE(adapter.start());

    REQUIRE(adapter.modelCount() == 2);
    REQUIRE(kernel.tasks.size() == 3);
    REQUIRE(kernel.tasks[0].name == "First");
    REQUIRE(kernel.tasks[0].stackDepth == 512);
    REQUIRE(kernel.tasks[0].priority == 3);
    REQUIRE(kernel.tasks[1].name == "Second");
    REQUIRE(kernel.tasks[2].name == "ROSDispatch");
    adapter.stop();
    REQUIRE(!adapter.addModel("Late", first));
    REQUIRE(!adapter.start());
}

TEST_CASE("FreeRTOS model task drives the unchanged model lifecycle")
{
    FakeKernel kernel;
    rtos::messaging::DispatchPort port;
    ProbeModel model;
    rtos::platform::freertos::FreeRtosAdapter adapter{kernel, port};
    REQUIRE(adapter.addModel("Probe", model, {512, 2, 7}));
    REQUIRE(adapter.start());
    kernel.onDelay = [&adapter] { adapter.stop(); };

    kernel.execute("Probe");

    REQUIRE(model.initializeCalls == 1);
    REQUIRE(model.beginCalls == 1);
    REQUIRE(model.operateCalls == 1);
    REQUIRE(model.terminateCalls == 1);
    REQUIRE(kernel.currentTick == 7);
    REQUIRE(kernel.tasksDeleted == 1);
}

TEST_CASE("FreeRTOS messaging task dispatches the bounded port")
{
    FakeKernel kernel;
    rtos::messaging::DispatchPort port{
        "freertos", rtos::messaging::QueueConfiguration{2, 32}
    };
    int received{};
    const auto subscription = port.subscribe<rtos::messages::MotorCommand>(
        [&received](const rtos::messages::MotorCommand&) { ++received; }
    );
    static_cast<void>(port.send(rtos::messages::MotorCommand{1800}));
    rtos::platform::freertos::FreeRtosAdapter adapter{kernel, port};
    REQUIRE(adapter.start());
    kernel.onDelay = [&adapter] { adapter.stop(); };

    kernel.execute("ROSDispatch");

    REQUIRE(received == 1);
    REQUIRE(port.pendingMessageCount() == 0);
    REQUIRE(kernel.tasksDeleted == 1);
}

TEST_CASE("FreeRTOS adapter rolls back tasks when startup fails")
{
    FakeKernel kernel;
    kernel.failTaskCreationAt = 2;
    rtos::messaging::DispatchPort port;
    ProbeModel first;
    ProbeModel second;
    rtos::platform::freertos::FreeRtosAdapter adapter{kernel, port};
    REQUIRE(adapter.addModel("First", first));
    REQUIRE(adapter.addModel("Second", second));

    REQUIRE(!adapter.start());

    REQUIRE(!adapter.running());
    REQUIRE(kernel.tasks.size() == 2);
    REQUIRE(kernel.tasksDeletedByHandle == 2);
    REQUIRE(kernel.tasks[0].deleted);
    REQUIRE(kernel.tasks[1].deleted);
}
