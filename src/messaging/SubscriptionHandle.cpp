#include "rtos/messaging/SubscriptionHandle.hpp"

#include <algorithm>
#include <utility>

namespace rtos::messaging {
namespace {

thread_local detail::SubscriptionSlot* executingSlot{};

}  // namespace

bool detail::SubscriptionSlot::invoke(const void* const payload)
{
    {
        std::scoped_lock lock{executionMutex};
        if (!active.load(std::memory_order_acquire)) {
            return false;
        }
        ++callbacksInFlight;
    }

    auto* const previousSlot = executingSlot;
    executingSlot = this;
    try {
        callback(payload);
    } catch (...) {
        executingSlot = previousSlot;
        {
            std::scoped_lock lock{executionMutex};
            --callbacksInFlight;
        }
        executionComplete.notify_all();
        throw;
    }
    executingSlot = previousSlot;
    {
        std::scoped_lock lock{executionMutex};
        --callbacksInFlight;
    }
    executionComplete.notify_all();
    return true;
}

void detail::SubscriptionSlot::deactivateAndWait() noexcept
{
    active.store(false, std::memory_order_release);
    if (executingSlot == this) {
        return;
    }
    std::unique_lock lock{executionMutex};
    executionComplete.wait(lock, [this] { return callbacksInFlight == 0; });
}

void detail::SubscriptionState::remove(
    const std::type_index type,
    const std::size_t id
) noexcept
{
    std::shared_ptr<SubscriptionSlot> removedSlot;
    {
        std::scoped_lock lock{mutex};
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
            removedSlot = *slot;
            removedSlot->active.store(false, std::memory_order_release);
            slots.erase(slot);
        }
        if (slots.empty()) {
            subscribers.erase(subscribersForType);
        }
    }
    if (removedSlot != nullptr) {
        removedSlot->deactivateAndWait();
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
