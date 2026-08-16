#pragma once

#include "rtos/messaging/DispatchReport.hpp"
#include "rtos/messaging/MessageTraffic.hpp"
#include "rtos/messaging/PortTopology.hpp"
#include "rtos/messaging/SubscriptionRegistry.hpp"
#include "rtos/messaging/Transport.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rtos::messaging {

class DispatchPort {
public:
    class Port {
    public:
        template<typename Message, typename Callback>
        void subscribe(Callback&& callback)
        {
            owner_->subscribeFrom<Message>(number_, std::forward<Callback>(callback));
        }

        template<typename Message>
        void send(const Message& message)
        {
            owner_->sendFrom(number_, message);
        }

        template<typename Message>
            requires(!std::is_lvalue_reference_v<Message>)
        void send(Message&& message)
        {
            owner_->sendFrom(number_, std::forward<Message>(message));
        }

        [[nodiscard]] std::size_t number() const noexcept { return number_; }

    private:
        friend class DispatchPort;

        Port(DispatchPort& owner, const std::size_t number) noexcept
            : owner_{&owner}, number_{number}
        {
        }

        DispatchPort* owner_;
        std::size_t number_;
    };

    explicit DispatchPort(
        std::string name = "main",
        TransportType transportType = TransportType::inProcess,
        MessageTransport* transport = nullptr
    );
    DispatchPort(const DispatchPort&) = delete;
    DispatchPort& operator=(const DispatchPort&) = delete;
    DispatchPort(DispatchPort&&) = delete;
    DispatchPort& operator=(DispatchPort&&) = delete;
    ~DispatchPort() = default;

    template<typename Message, typename Callback>
    void subscribe(Callback&& callback)
    {
        using SubscribedMessage = std::remove_cvref_t<Message>;
        subscriptions_.add<SubscribedMessage>(std::forward<Callback>(callback));
        auto& traffic = traffic_[typeid(SubscribedMessage)];
        traffic.messageName = messageName<SubscribedMessage>();
        traffic.subscribers = subscriptions_.count<SubscribedMessage>();
    }

    template<typename Message>
    [[nodiscard]] Port createPort(
        std::string name,
        const PortDirection direction,
        const RoutingId routingId = defaultRoutingId<Message>()
    )
    {
        using PortMessage = std::remove_cvref_t<Message>;
        static_assert(
            std::is_trivially_copyable_v<PortMessage>,
            "Transported messages must be trivially copyable"
        );
        const auto number = nextPortNumber_++;
        ports_.push_back(PortRecord{
            number,
            std::move(name),
            messageName<PortMessage>(),
            typeid(PortMessage),
            direction,
            transportType_,
            routingId,
            [](const std::span<const std::byte> payload) -> std::shared_ptr<const void>
            {
                if (payload.size() != sizeof(PortMessage)) {
                    throw std::runtime_error{"Invalid transported message size"};
                }
                auto message = std::make_shared<PortMessage>();
                std::memcpy(message.get(), payload.data(), sizeof(PortMessage));
                return message;
            },
        });
        auto& traffic = traffic_[typeid(PortMessage)];
        traffic.messageName = messageName<PortMessage>();
        if (direction == PortDirection::publisher) {
            ++traffic.publishers;
        }
        return Port{*this, number};
    }

    template<typename Message>
    void send(const Message& message)
    {
        enqueue<std::remove_cv_t<Message>>(message, defaultRoutingId<Message>());
    }

    template<typename Message>
        requires(!std::is_lvalue_reference_v<Message>)
    void send(Message&& message)
    {
        enqueue<std::remove_cvref_t<Message>>(
            std::forward<Message>(message), defaultRoutingId<Message>()
        );
    }

    template<typename Message>
    [[nodiscard]] std::size_t subscriberCount() const
    {
        return subscriptions_.count<Message>();
    }

    DispatchReport dispatchAll();
    [[nodiscard]] std::string_view name() const noexcept;
    [[nodiscard]] std::size_t pendingMessageCount() const noexcept;
    [[nodiscard]] std::vector<MessageTraffic> messageTraffic() const;
    [[nodiscard]] std::vector<PortTopology> portTopology() const;
    [[nodiscard]] TransportType transportType() const noexcept;
    [[nodiscard]] bool receive(const TransportMessage& message);

private:
    struct PendingMessage {
        std::type_index type;
        std::shared_ptr<const void> payload;
    };

    struct PortRecord {
        std::size_t number;
        std::string name;
        std::string messageName;
        std::type_index messageType;
        PortDirection direction;
        TransportType transport;
        RoutingId routingId;
        std::function<std::shared_ptr<const void>(std::span<const std::byte>)> decode;
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
        ~DispatchScope() { dispatchInProgress_ = false; }

    private:
        bool& dispatchInProgress_;
    };

    template<typename StoredMessage, typename Message>
    void enqueue(Message&& message, const RoutingId routingId)
    {
        static_assert(
            std::is_trivially_copyable_v<StoredMessage>,
            "Transported messages must be trivially copyable"
        );
        auto& traffic = traffic_[typeid(StoredMessage)];
        traffic.messageName = messageName<StoredMessage>();
        ++traffic.messagesSent;
        if (transport_ != nullptr) {
            TransportMessage transported{
                routingId,
                messageName<StoredMessage>(),
                std::vector<std::byte>(sizeof(StoredMessage)),
            };
            std::memcpy(
                transported.payload.data(), std::addressof(message), sizeof(StoredMessage)
            );
            transport_->send(std::move(transported));
            return;
        }
        incomingQueue_.push_back(PendingMessage{
            typeid(StoredMessage),
            std::make_shared<StoredMessage>(std::forward<Message>(message)),
        });
    }

    template<typename Message, typename Callback>
    void subscribeFrom(const std::size_t number, Callback&& callback)
    {
        validatePort<Message>(number, PortDirection::subscriber);
        subscribe<Message>(std::forward<Callback>(callback));
    }

    template<typename Message>
    void sendFrom(const std::size_t number, Message&& message)
    {
        using SentMessage = std::remove_cvref_t<Message>;
        validatePort<SentMessage>(number, PortDirection::publisher);
        const auto port = std::ranges::find(ports_, number, &PortRecord::number);
        enqueue<SentMessage>(std::forward<Message>(message), port->routingId);
    }

    template<typename Message>
    void validatePort(const std::size_t number, const PortDirection direction) const
    {
        const auto port = std::ranges::find(ports_, number, &PortRecord::number);
        if (
            port == ports_.end() || port->messageType != typeid(std::remove_cvref_t<Message>)
            || port->direction != direction
        ) {
            throw std::logic_error{"Message used with an incompatible port"};
        }
    }

    template<typename Message>
    [[nodiscard]] static std::string messageName()
    {
        if constexpr (requires { Message::name; }) {
            return std::string{Message::name};
        }
        return typeid(Message).name();
    }

    template<typename Message>
    [[nodiscard]] static consteval RoutingId defaultRoutingId()
    {
        if constexpr (requires { Message::defaultRoutingId; }) {
            return Message::defaultRoutingId;
        }
        return 0;
    }

    std::string name_;
    TransportType transportType_;
    MessageTransport* transport_;
    SubscriptionRegistry subscriptions_;
    std::vector<PendingMessage> incomingQueue_;
    std::unordered_map<std::type_index, MessageTraffic> traffic_;
    std::vector<PortRecord> ports_;
    std::size_t nextPortNumber_{1};
    bool dispatchInProgress_{};
};

}  // namespace rtos::messaging
