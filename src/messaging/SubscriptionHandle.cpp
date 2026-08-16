#include "rtos/messaging/SubscriptionHandle.hpp"

#include <algorithm>
#include <utility>

namespace rtos::messaging {

void detail::SubscriptionState::remove(
    const std::type_index type,
    const std::size_t id
) noexcept
{
    const auto subscribersForType = subscribers.find(type);
    if (subscribersForType == subscribers.end()) {
        return;
    }

    auto& slots = subscribersForType->second;
    const auto slot = std::ranges::find_if(
        slots,
        [id](const std::shared_ptr<SubscriptionSlot>& candidate)
        {
            return candidate->id == id;
        }
    );
    if (slot != slots.end()) {
        (*slot)->active = false;
        slots.erase(slot);
    }
    if (slots.empty()) {
        subscribers.erase(subscribersForType);
    }
}

SubscriptionHandle::SubscriptionHandle(
    std::weak_ptr<detail::SubscriptionState> state,
    const std::type_index type,
    const std::size_t id
) noexcept
    : state_{std::move(state)}, type_{type}, id_{id}
{
}

SubscriptionHandle::SubscriptionHandle(SubscriptionHandle&& other) noexcept
    : state_{std::move(other.state_)}, type_{other.type_}, id_{other.id_}
{
    other.id_ = 0;
}

SubscriptionHandle& SubscriptionHandle::operator=(SubscriptionHandle&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    reset();
    state_ = std::move(other.state_);
    type_ = other.type_;
    id_ = other.id_;
    other.id_ = 0;
    return *this;
}

SubscriptionHandle::~SubscriptionHandle()
{
    reset();
}

void SubscriptionHandle::reset() noexcept
{
    if (id_ == 0) {
        return;
    }
    if (const auto state = state_.lock()) {
        state->remove(type_, id_);
    }
    state_.reset();
    id_ = 0;
}

bool SubscriptionHandle::active() const noexcept
{
    return id_ != 0 && !state_.expired();
}

SubscriptionHandle::operator bool() const noexcept
{
    return active();
}

}  // namespace rtos::messaging
