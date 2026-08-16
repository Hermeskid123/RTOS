#include "rtos/messaging/SubscriptionRegistry.hpp"

namespace rtos::messaging {

SubscriptionRegistry::SubscriptionRegistry()
    : state_{std::make_shared<detail::SubscriptionState>()}
{
}

std::size_t SubscriptionRegistry::count(const std::type_index type) const
{
    const auto subscribers = state_->subscribers.find(type);
    return subscribers == state_->subscribers.end() ? 0 : subscribers->second.size();
}

std::size_t SubscriptionRegistry::dispatch(
    const std::type_index type,
    const void* const payload
) const
{
    const auto subscribers = state_->subscribers.find(type);
    if (subscribers == state_->subscribers.end()) {
        return 0;
    }

    const auto dispatchSubscribers = subscribers->second;
    std::size_t callbacksInvoked{};
    for (const auto& slot : dispatchSubscribers) {
        if (slot->active) {
            slot->callback(payload);
            ++callbacksInvoked;
        }
    }

    return callbacksInvoked;
}

}  // namespace rtos::messaging
