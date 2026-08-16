#include "rtos/messaging/DispatchPort.hpp"

#include <algorithm>
#include <utility>

namespace rtos::messaging {

DispatchPort::DispatchPort(
    std::string name,
    const TransportType transportType,
    MessageTransport* const transport,
    const QueueConfiguration queueConfiguration
)
    : name_{std::move(name)}, transportType_{transportType}, transport_{transport},
      queueConfiguration_{queueConfiguration}
{
    if ((transportType_ == TransportType::interProcess) != (transport_ != nullptr)) {
        throw std::invalid_argument{"IPC dispatch ports require a message transport"};
    }
    if (queueConfiguration_.depth == 0) {
        throw std::invalid_argument{"Message queue depth must be greater than zero"};
    }
    if (
        queueConfiguration_.maximumMessageSize == 0
        || queueConfiguration_.maximumMessageSize > maximumSupportedMessageSize
    ) {
        throw std::invalid_argument{"Unsupported maximum message size"};
    }
    incomingQueue_.reserve(queueConfiguration_.depth);
    dispatchQueue_.reserve(queueConfiguration_.depth);
    queueStatistics_.capacity = queueConfiguration_.depth;
}

DispatchPort::DispatchPort(std::string name, const QueueConfiguration queueConfiguration)
    : DispatchPort{
          std::move(name), TransportType::inProcess, nullptr, queueConfiguration
      }
{
}

DispatchReport DispatchPort::dispatchAll()
{
    const auto dispatchStarted = std::chrono::steady_clock::now();
    DispatchReport report;
    {
        std::scoped_lock lock{mutex_};
        if (dispatchInProgress_) {
            return {};
        }
        dispatchInProgress_ = true;
        dispatchQueue_.clear();
        dispatchQueue_.swap(incomingQueue_);
        report.queueDepthAtStart = dispatchQueue_.size();
        report.queueHighWaterMark = queueHighWaterMark_;
    }

    try {
        for (const auto& message : dispatchQueue_) {
            const auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - message.queuedAt
            );
            report.totalDispatchLatency += latency;
            report.maximumDispatchLatency = std::max(
                report.maximumDispatchLatency, latency
            );

            const auto callbackStarted = std::chrono::steady_clock::now();
            const auto callbacksInvoked = subscriptions_.dispatch(
                message.type,
                message.payload.data()
            );
            report.callbackExecutionTime +=
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - callbackStarted
                );

            std::scoped_lock lock{mutex_};
            if (callbacksInvoked == 0) {
                ++report.messagesWithoutSubscribers;
                ++traffic_.at(message.type).messagesWithoutSubscribers;
                continue;
            }

            ++report.messagesDispatched;
            report.callbacksInvoked += callbacksInvoked;
            ++traffic_.at(message.type).messagesDispatched;
            traffic_.at(message.type).messagesReceived += callbacksInvoked;
        }
    } catch (...) {
        std::scoped_lock lock{mutex_};
        dispatchInProgress_ = false;
        dispatchQueue_.clear();
        throw;
    }

    {
        std::scoped_lock lock{mutex_};
        dispatchInProgress_ = false;
        dispatchQueue_.clear();
    }
    report.dispatchDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - dispatchStarted
    );
    return report;
}

QueueConfiguration DispatchPort::queueConfiguration() const noexcept
{
    return queueConfiguration_;
}

QueueStatistics DispatchPort::queueStatistics() const noexcept
{
    std::scoped_lock lock{mutex_};
    auto statistics = queueStatistics_;
    statistics.pending = incomingQueue_.size();
    return statistics;
}

std::string_view DispatchPort::name() const noexcept
{
    return name_;
}

std::size_t DispatchPort::pendingMessageCount() const noexcept
{
    std::scoped_lock lock{mutex_};
    return incomingQueue_.size();
}

std::vector<MessageTraffic> DispatchPort::messageTraffic() const
{
    std::scoped_lock lock{mutex_};
    std::vector<MessageTraffic> traffic;
    traffic.reserve(traffic_.size());
    for (const auto& entry : traffic_) {
        auto counters = entry.second;
        counters.subscribers = subscriptions_.count(entry.first);
        traffic.push_back(std::move(counters));
    }

    std::ranges::sort(
        traffic,
        {},
        [](const MessageTraffic& counters) { return counters.messageName; }
    );
    return traffic;
}

std::vector<PortTopology> DispatchPort::portTopology() const
{
    std::scoped_lock lock{mutex_};
    std::vector<PortTopology> topology;
    topology.reserve(ports_.size());

    for (const auto& port : ports_) {
        PortTopology entry{
            port.number,
            port.name,
            port.messageName,
            port.direction,
            port.transport,
            port.routingId,
            {},
            {},
        };

        for (const auto& connectedPort : ports_) {
            if (
                port.number == connectedPort.number
                || port.messageType != connectedPort.messageType
                || port.direction == connectedPort.direction
            ) {
                continue;
            }

            if (connectedPort.direction == PortDirection::publisher) {
                entry.publisherPorts.push_back(connectedPort.number);
            } else {
                entry.subscriberPorts.push_back(connectedPort.number);
            }
        }
        topology.push_back(std::move(entry));
    }

    return topology;
}

TransportType DispatchPort::transportType() const noexcept
{
    return transportType_;
}

bool DispatchPort::receive(const TransportMessage& message)
{
    std::scoped_lock lock{mutex_};
    const auto port = std::ranges::find_if(
        ports_,
        [&message](const PortRecord& candidate)
        {
            return candidate.direction == PortDirection::subscriber
                && candidate.routingId == message.routingId;
        }
    );
    if (port == ports_.end()) {
        return false;
    }

    if (
        message.payload.size() != port->messageSize
        || message.payload.size() > queueConfiguration_.maximumMessageSize
    ) {
        ++queueStatistics_.rejectedOversize;
        return false;
    }
    if (incomingQueue_.size() == queueConfiguration_.depth) {
        switch (queueConfiguration_.fullPolicy) {
        case QueueFullPolicy::rejectNewest:
            ++queueStatistics_.rejectedQueueFull;
            return false;
        case QueueFullPolicy::dropNewest:
            ++queueStatistics_.droppedNewest;
            return false;
        case QueueFullPolicy::dropOldest:
            incomingQueue_.erase(incomingQueue_.begin());
            ++queueStatistics_.droppedOldest;
            break;
        }
    }

    auto& traffic = traffic_[port->messageType];
    traffic.messageName = port->messageName;
    PendingMessage pending{port->messageType};
    pending.payloadSize = message.payload.size();
    pending.queuedAt = message.queuedAt == std::chrono::steady_clock::time_point{}
        ? std::chrono::steady_clock::now()
        : message.queuedAt;
    std::memcpy(pending.payload.data(), message.payload.data(), message.payload.size());
    incomingQueue_.push_back(std::move(pending));
    queueHighWaterMark_ = std::max(queueHighWaterMark_, incomingQueue_.size());
    queueStatistics_.highWaterMark = queueHighWaterMark_;
    return true;
}

}  // namespace rtos::messaging
