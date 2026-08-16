/**
 * @file
 * @brief Declares the public DispatchPort framework API.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include "rtos/messaging/DispatchReport.hpp"
#include "rtos/messaging/MessageTraffic.hpp"
#include "rtos/messaging/PortTopology.hpp"
#include "rtos/messaging/QueueConfiguration.hpp"
#include "rtos/messaging/SubscriptionRegistry.hpp"
#include "rtos/messaging/Transport.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <array>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
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

/**
 * @brief Deferred, bounded, strongly typed publish/subscribe router.
 * @details Send operations copy trivially-copyable payloads into local inline
 * storage or an external transport. dispatchAll() captures one batch, ensuring
 * callback publications remain deferred to a later dispatch boundary.
 */
class DispatchPort {
public:
    /** @brief Typed named endpoint created and owned logically by a model. */
    class Port {
    public:
        /** @brief Registers a callback through a subscriber endpoint. */
        template<typename Message, typename Callback>
        [[nodiscard]] SubscriptionHandle subscribe(Callback&& callback)
        {
            return owner_->subscribeFrom<Message>(
                number_, std::forward<Callback>(callback)
            );
        }

        /** @brief Publishes a copied lvalue through a publisher endpoint. */
        template<typename Message>
        SendResult send(const Message& message)
        {
            return owner_->sendFrom(number_, message);
        }

        /** @brief Publishes an rvalue through a publisher endpoint. */
        template<typename Message>
            requires(!std::is_lvalue_reference_v<Message>)
        SendResult send(Message&& message)
        {
            return owner_->sendFrom(number_, std::forward<Message>(message));
        }

        /** @brief Returns the endpoint number unique within its owner. */
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

    /**
     * @brief Creates a dispatch boundary and preallocates its message queues.
     * @param name Diagnostic port name.
     * @param transportType Local or inter-process transport classification.
     * @param transport Optional non-owning external transport sink.
     * @param queueConfiguration Immutable queue bounds and overflow policy.
     * @throws std::invalid_argument If queue bounds are invalid.
     */
    explicit DispatchPort(
        std::string name = "main",
        TransportType transportType = TransportType::inProcess,
        MessageTransport* transport = nullptr,
        QueueConfiguration queueConfiguration = {}
    );
    /** @brief Convenience constructor for a bounded in-process port. */
    DispatchPort(std::string name, QueueConfiguration queueConfiguration);
    DispatchPort(const DispatchPort&) = delete;
    DispatchPort& operator=(const DispatchPort&) = delete;
    DispatchPort(DispatchPort&&) = delete;
    DispatchPort& operator=(DispatchPort&&) = delete;
    ~DispatchPort() = default;

    /** @brief Registers an unnamed exact-type subscriber callback. */
    template<typename Message, typename Callback>
    [[nodiscard]] SubscriptionHandle subscribe(Callback&& callback)
    {
        using SubscribedMessage = std::remove_cvref_t<Message>;
        auto handle = subscriptions_.add<SubscribedMessage>(
            std::forward<Callback>(callback)
        );
        std::scoped_lock lock{mutex_};
        auto& traffic = traffic_[typeid(SubscribedMessage)];
        traffic.messageName = messageName<SubscribedMessage>();
        traffic.subscribers = subscriptions_.count<SubscribedMessage>();
        return handle;
    }

    /**
     * @brief Creates a named typed endpoint for diagnostics and route validation.
     * @return Lightweight endpoint bound to this dispatch port.
     */
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
        std::scoped_lock lock{mutex_};
        const auto number = nextPortNumber_++;
        ports_.push_back(PortRecord{
            number,
            std::move(name),
            messageName<PortMessage>(),
            typeid(PortMessage),
            direction,
            transportType_,
            routingId,
            sizeof(PortMessage),
        });
        auto& traffic = traffic_[typeid(PortMessage)];
        traffic.messageName = messageName<PortMessage>();
        if (direction == PortDirection::publisher) {
            ++traffic.publishers;
        }
        return Port{*this, number};
    }

    /** @brief Publishes a copied message using its default route. */
    template<typename Message>
    SendResult send(const Message& message)
    {
        return enqueue<std::remove_cv_t<Message>>(
            message, defaultRoutingId<Message>()
        );
    }

    /** @brief Publishes an rvalue message using its default route. */
    template<typename Message>
        requires(!std::is_lvalue_reference_v<Message>)
    SendResult send(Message&& message)
    {
        return enqueue<std::remove_cvref_t<Message>>(
            std::forward<Message>(message), defaultRoutingId<Message>()
        );
    }

    /** @brief Returns active subscribers registered for an exact type. */
    template<typename Message>
    [[nodiscard]] std::size_t subscriberCount() const
    {
        return subscriptions_.count<Message>();
    }

    /** @brief Routes the current queue batch and returns routing measurements. */
    DispatchReport dispatchAll();
    /** @brief Returns the diagnostic name supplied at construction. */
    [[nodiscard]] std::string_view name() const noexcept;
    /** @brief Returns messages waiting for a future dispatch boundary. */
    [[nodiscard]] std::size_t pendingMessageCount() const noexcept;
    /** @brief Returns immutable queue configuration. */
    [[nodiscard]] QueueConfiguration queueConfiguration() const noexcept;
    /** @brief Returns a synchronized queue statistics snapshot. */
    [[nodiscard]] QueueStatistics queueStatistics() const noexcept;
    /** @brief Returns cumulative counters sorted for diagnostics. */
    [[nodiscard]] std::vector<MessageTraffic> messageTraffic() const;
    /** @brief Returns named endpoint relationships and routing metadata. */
    [[nodiscard]] std::vector<PortTopology> portTopology() const;
    /** @brief Returns this port's transport classification. */
    [[nodiscard]] TransportType transportType() const noexcept;
    /**
     * @brief Accepts a serialized transport envelope into the local queue.
     * @return `true` when the payload was recognized and queued.
     */
    [[nodiscard]] bool receive(const TransportMessage& message);

private:
    struct PendingMessage {
        explicit PendingMessage(const std::type_index messageType) noexcept
            : type{messageType}
        {
        }

        std::type_index type;
        std::size_t payloadSize{};
        alignas(std::max_align_t)
            std::array<std::byte, maximumSupportedMessageSize> payload{};
        std::chrono::steady_clock::time_point queuedAt;
    };

    struct PortRecord {
        std::size_t number;
        std::string name;
        std::string messageName;
        std::type_index messageType;
        PortDirection direction;
        TransportType transport;
        RoutingId routingId;
        std::size_t messageSize{};
    };

    template<typename StoredMessage, typename Message>
    [[nodiscard]] SendResult enqueue(Message&& message, const RoutingId routingId)
    {
        static_assert(
            std::is_trivially_copyable_v<StoredMessage>,
            "Transported messages must be trivially copyable"
        );
        static_assert(
            alignof(StoredMessage) <= alignof(std::max_align_t),
            "Over-aligned messages are not supported by inline queue storage"
        );
        const auto queuedAt = std::chrono::steady_clock::now();
        {
            std::scoped_lock lock{mutex_};
            auto& traffic = traffic_[typeid(StoredMessage)];
            traffic.messageName = messageName<StoredMessage>();
            ++traffic.messagesSent;
            if (sizeof(StoredMessage) > queueConfiguration_.maximumMessageSize) {
                ++queueStatistics_.rejectedOversize;
                return SendResult::messageTooLarge;
            }
        }
        if (transport_ != nullptr) {
            TransportMessage transported{
                routingId,
                messageName<StoredMessage>(),
                std::vector<std::byte>(sizeof(StoredMessage)),
            };
            transported.queuedAt = queuedAt;
            std::memcpy(
                transported.payload.data(), std::addressof(message), sizeof(StoredMessage)
            );
            transport_->send(std::move(transported));
            return SendResult::transported;
        }
        {
            std::scoped_lock lock{mutex_};
            auto result = SendResult::queued;
            if (incomingQueue_.size() == queueConfiguration_.depth) {
                switch (queueConfiguration_.fullPolicy) {
                case QueueFullPolicy::rejectNewest:
                    ++queueStatistics_.rejectedQueueFull;
                    return SendResult::rejectedQueueFull;
                case QueueFullPolicy::dropNewest:
                    ++queueStatistics_.droppedNewest;
                    return SendResult::droppedNewest;
                case QueueFullPolicy::dropOldest:
                    incomingQueue_.erase(incomingQueue_.begin());
                    ++queueStatistics_.droppedOldest;
                    result = SendResult::queuedAfterDroppingOldest;
                    break;
                }
            }
            PendingMessage pending{typeid(StoredMessage)};
            pending.payloadSize = sizeof(StoredMessage);
            pending.queuedAt = queuedAt;
            std::memcpy(pending.payload.data(), std::addressof(message), sizeof(StoredMessage));
            incomingQueue_.push_back(std::move(pending));
            queueHighWaterMark_ = std::max(queueHighWaterMark_, incomingQueue_.size());
            queueStatistics_.highWaterMark = queueHighWaterMark_;
            return result;
        }
    }

    template<typename Message, typename Callback>
    [[nodiscard]] SubscriptionHandle subscribeFrom(
        const std::size_t number,
        Callback&& callback
    )
    {
        validatePort<Message>(number, PortDirection::subscriber);
        return subscribe<Message>(std::forward<Callback>(callback));
    }

    template<typename Message>
    [[nodiscard]] SendResult sendFrom(const std::size_t number, Message&& message)
    {
        using SentMessage = std::remove_cvref_t<Message>;
        RoutingId routingId{};
        {
            std::scoped_lock lock{mutex_};
            validatePortLocked<SentMessage>(number, PortDirection::publisher);
            const auto port = std::ranges::find(ports_, number, &PortRecord::number);
            routingId = port->routingId;
        }
        return enqueue<SentMessage>(std::forward<Message>(message), routingId);
    }

    template<typename Message>
    void validatePort(const std::size_t number, const PortDirection direction) const
    {
        std::scoped_lock lock{mutex_};
        validatePortLocked<Message>(number, direction);
    }

    template<typename Message>
    void validatePortLocked(
        const std::size_t number,
        const PortDirection direction
    ) const
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
    mutable std::mutex mutex_;
    SubscriptionRegistry subscriptions_;
    std::vector<PendingMessage> incomingQueue_;
    std::vector<PendingMessage> dispatchQueue_;
    std::unordered_map<std::type_index, MessageTraffic> traffic_;
    std::vector<PortRecord> ports_;
    std::size_t nextPortNumber_{1};
    bool dispatchInProgress_{};
    std::size_t queueHighWaterMark_{};
    QueueConfiguration queueConfiguration_;
    QueueStatistics queueStatistics_;
};

}  // namespace rtos::messaging
