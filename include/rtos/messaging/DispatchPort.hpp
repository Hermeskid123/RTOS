#pragma once

#include "rtos/messaging/DispatchReport.hpp"
#include "rtos/messaging/SubscriptionRegistry.hpp"

#include <cstddef>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

namespace rtos::messaging {

class DispatchPort {
public:
    DispatchPort() = default;
    DispatchPort(const DispatchPort&) = delete;
    DispatchPort& operator=(const DispatchPort&) = delete;
    DispatchPort(DispatchPort&&) = delete;
    DispatchPort& operator=(DispatchPort&&) = delete;
    ~DispatchPort() = default;

    template<typename Message, typename Callback>
    void subscribe(Callback&& callback)
    {
        subscriptions_.add<Message>(std::forward<Callback>(callback));
    }

    template<typename Message>
    void send(const Message& message)
    {
        enqueue<std::remove_cv_t<Message>>(message);
    }

    template<typename Message>
        requires(!std::is_lvalue_reference_v<Message>)
    void send(Message&& message)
    {
        enqueue<std::remove_cvref_t<Message>>(std::forward<Message>(message));
    }

    template<typename Message>
    [[nodiscard]] std::size_t subscriberCount() const
    {
        return subscriptions_.count<Message>();
    }

    DispatchReport dispatchAll();

private:
    struct PendingMessage {
        std::type_index type;
        std::shared_ptr<const void> payload;
    };

    class DispatchScope {
    public:
        explicit DispatchScope(bool& dispatchInProgress) noexcept
            : dispatchInProgress_{dispatchInProgress}
        {
            dispatchInProgress_ = true;
        }

        DispatchScope(const DispatchScope&) = delete;
        DispatchScope& operator=(const DispatchScope&) = delete;

        ~DispatchScope()
        {
            dispatchInProgress_ = false;
        }

    private:
        bool& dispatchInProgress_;
    };

    template<typename StoredMessage, typename Message>
    void enqueue(Message&& message)
    {
        incomingQueue_.push_back(PendingMessage{
            typeid(StoredMessage),
            std::make_shared<StoredMessage>(std::forward<Message>(message)),
        });
    }

    SubscriptionRegistry subscriptions_;
    std::vector<PendingMessage> incomingQueue_;
    bool dispatchInProgress_{};
};

}  // namespace rtos::messaging
