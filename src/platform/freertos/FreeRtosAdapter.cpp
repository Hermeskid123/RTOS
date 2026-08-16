/**
 * @file
 * @brief Implements the FreeRtosAdapter framework API.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#include "rtos/platform/freertos/FreeRtosAdapter.hpp"

#include "rtos/messaging/DispatchPort.hpp"
#include "rtos/model/BaseModel.hpp"

#include <algorithm>
#include <cstring>

namespace rtos::platform::freertos {

FreeRtosAdapter::FreeRtosAdapter(
    FreeRtosKernel& kernel,
    messaging::DispatchPort& dispatchPort,
    const TaskConfiguration messagingTask
) noexcept
    : kernel_{kernel}, dispatchPort_{dispatchPort}, messagingTask_{messagingTask}
{
}

bool FreeRtosAdapter::addModel(
    const std::string_view name,
    model::BaseModel& model,
    const TaskConfiguration configuration
) noexcept
{
    if (
        started_.load(std::memory_order_acquire) || modelCount_ == models_.size()
        || name.empty()
        || name.size() > maximumTaskNameLength || configuration.periodTicks == 0
        || configuration.stackDepth == 0
    ) {
        return false;
    }

    auto& context = models_[modelCount_++];
    context.owner = this;
    context.model = &model;
    context.configuration = configuration;
    std::copy(name.begin(), name.end(), context.name.begin());
    context.name[name.size()] = '\0';
    return true;
}

bool FreeRtosAdapter::start()
{
    if (messagingTask_.periodTicks == 0 || messagingTask_.stackDepth == 0) {
        return false;
    }
    bool expected{};
    if (!started_.compare_exchange_strong(expected, true)) {
        return false;
    }
    running_.store(true, std::memory_order_release);
    std::array<TaskHandle, maximumModels + 1> createdTasks{};
    std::size_t createdTaskCount{};

    const auto rollback = [&] {
        while (createdTaskCount > 0) {
            kernel_.deleteTask(createdTasks[--createdTaskCount]);
        }
        stop();
    };

    for (std::size_t index = 0; index < modelCount_; ++index) {
        auto& context = models_[index];
        const auto handle = kernel_.createTask(
            &FreeRtosAdapter::modelTaskEntry,
            context.name.data(),
            context.configuration.stackDepth,
            &context,
            context.configuration.priority
        );
        if (handle == nullptr) {
            rollback();
            return false;
        }
        createdTasks[createdTaskCount++] = handle;
    }

    const auto messagingHandle = kernel_.createTask(
        &FreeRtosAdapter::messagingTaskEntry,
        "ROSDispatch",
        messagingTask_.stackDepth,
        this,
        messagingTask_.priority
    );
    if (messagingHandle == nullptr) {
        rollback();
        return false;
    }
    return true;
}

void FreeRtosAdapter::stop() noexcept
{
    running_.store(false, std::memory_order_release);
}

bool FreeRtosAdapter::running() const noexcept
{
    return running_.load(std::memory_order_acquire);
}

std::size_t FreeRtosAdapter::modelCount() const noexcept
{
    return modelCount_;
}

void FreeRtosAdapter::modelTaskEntry(void* const context)
{
    auto& modelContext = *static_cast<ModelTaskContext*>(context);
    modelContext.owner->runModelTask(modelContext);
}

void FreeRtosAdapter::messagingTaskEntry(void* const context)
{
    static_cast<FreeRtosAdapter*>(context)->runMessagingTask();
}

void FreeRtosAdapter::runModelTask(ModelTaskContext& context)
{
    static_cast<void>(context.model->initialize());
    static_cast<void>(context.model->begin());
    auto previousWakeTime = kernel_.tickCount();
    while (running()) {
        static_cast<void>(context.model->operate());
        kernel_.delayUntil(previousWakeTime, context.configuration.periodTicks);
    }
    static_cast<void>(context.model->terminate());
    kernel_.deleteCurrentTask();
}

void FreeRtosAdapter::runMessagingTask()
{
    auto previousWakeTime = kernel_.tickCount();
    while (running()) {
        static_cast<void>(dispatchPort_.dispatchAll());
        kernel_.delayUntil(previousWakeTime, messagingTask_.periodTicks);
    }
    kernel_.deleteCurrentTask();
}

}  // namespace rtos::platform::freertos
