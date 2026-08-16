/**
 * @file
 * @brief Declares the public SubscriptionRegistry framework API.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include "rtos/messaging/SubscriptionHandle.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <utility>

namespace rtos::messaging {

class DispatchPort;

/** @brief Thread-safe exact-type callback registry owned by DispatchPort. */
class SubscriptionRegistry {
public:
    /** @brief Creates an empty registry with shared lifetime state. */
    SubscriptionRegistry();
    SubscriptionRegistry(const SubscriptionRegistry&) = delete;
    SubscriptionRegistry& operator=(const SubscriptionRegistry&) = delete;

    /**
     * @brief Registers a callback for an exact message type.
     * @tparam Message Message type delivered to the callback.
     * @tparam Callback Callable accepting `const Message&`.
     * @param callback Callable to retain until the returned handle is reset.
     * @return Move-only subscription lifetime handle.
     */
    template<typename Message, typename Callback>
    [[nodiscard]] SubscriptionHandle add(Callback&& callback)
    {
        using SubscribedMessage = std::remove_cvref_t<Message>;

        static_assert(
            std::is_invocable_v<Callback&, const SubscribedMessage&>,
            "A subscriber must be callable with const Message&"
        );

        std::function<void(const SubscribedMessage&)> typedCallback{
            std::forward<Callback>(callback)
        };
        const auto type = std::type_index{typeid(SubscribedMessage)};
        std::scoped_lock lock{state_->mutex};
        const auto id = state_->nextId++;
        state_->subscribers[type].push_back(std::make_shared<detail::SubscriptionSlot>(
            id,
            [callback = std::move(typedCallback)](const void* payload)
            {
                callback(*static_cast<const SubscribedMessage*>(payload));
            }
        ));
        return SubscriptionHandle{state_, type, id};
    }

    /** @brief Returns the number of active callbacks for `Message`. */
    template<typename Message>
    [[nodiscard]] std::size_t count() const
    {
        std::scoped_lock lock{state_->mutex};
        const auto subscribers = state_->subscribers.find(
            typeid(std::remove_cvref_t<Message>)
        );
        return subscribers == state_->subscribers.end()
            ? 0
            : subscribers->second.size();
    }

private:
    friend class DispatchPort;

    [[nodiscard]] std::size_t dispatch(std::type_index type, const void* payload) const;
    [[nodiscard]] std::size_t count(std::type_index type) const;

    std::shared_ptr<detail::SubscriptionState> state_;
};

}  // namespace rtos::messaging
