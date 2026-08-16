#pragma once

#include <cstddef>
#include <functional>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rtos::messaging {

class DispatchPort;

class SubscriptionRegistry {
public:
    template<typename Message, typename Callback>
    void add(Callback&& callback)
    {
        using SubscribedMessage = std::remove_cvref_t<Message>;

        static_assert(
            std::is_invocable_v<Callback&, const SubscribedMessage&>,
            "A subscriber must be callable with const Message&"
        );

        std::function<void(const SubscribedMessage&)> typedCallback{
            std::forward<Callback>(callback)
        };
        subscribers_[typeid(SubscribedMessage)].emplace_back(
            [callback = std::move(typedCallback)](const void* payload)
            {
                callback(*static_cast<const SubscribedMessage*>(payload));
            }
        );
    }

    template<typename Message>
    [[nodiscard]] std::size_t count() const
    {
        const auto subscribers = subscribers_.find(typeid(std::remove_cvref_t<Message>));
        return subscribers == subscribers_.end() ? 0 : subscribers->second.size();
    }

private:
    friend class DispatchPort;

    using Subscriber = std::function<void(const void*)>;

    [[nodiscard]] std::size_t dispatch(std::type_index type, const void* payload) const;

    std::unordered_map<std::type_index, std::vector<Subscriber>> subscribers_;
};

}  // namespace rtos::messaging
