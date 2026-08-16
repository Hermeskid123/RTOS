#include "rtos/messaging/SubscriptionRegistry.hpp"

namespace rtos::messaging {

std::size_t SubscriptionRegistry::dispatch(
    const std::type_index type,
    const void* const payload
) const
{
    const auto subscribers = subscribers_.find(type);
    if (subscribers == subscribers_.end()) {
        return 0;
    }

    const auto dispatchSubscribers = subscribers->second;
    for (const auto& callback : dispatchSubscribers) {
        callback(payload);
    }

    return dispatchSubscribers.size();
}

}  // namespace rtos::messaging
