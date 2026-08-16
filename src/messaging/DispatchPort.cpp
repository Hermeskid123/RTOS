#include "rtos/messaging/DispatchPort.hpp"

#include <algorithm>
#include <utility>

namespace rtos::messaging {

DispatchPort::DispatchPort(std::string name)
    : name_{std::move(name)}
{
}

DispatchReport DispatchPort::dispatchAll()
{
    if (dispatchInProgress_) {
        return {};
    }

    DispatchScope dispatchScope{dispatchInProgress_};
    std::vector<PendingMessage> dispatchQueue;
    dispatchQueue.swap(incomingQueue_);
    DispatchReport report;

    for (const auto& message : dispatchQueue) {
        const auto callbacksInvoked = subscriptions_.dispatch(
            message.type,
            message.payload.get()
        );
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

    return report;
}

std::string_view DispatchPort::name() const noexcept
{
    return name_;
}

std::size_t DispatchPort::pendingMessageCount() const noexcept
{
    return incomingQueue_.size();
}

std::vector<MessageTraffic> DispatchPort::messageTraffic() const
{
    std::vector<MessageTraffic> traffic;
    traffic.reserve(traffic_.size());
    for (const auto& entry : traffic_) {
        traffic.push_back(entry.second);
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
    std::vector<PortTopology> topology;
    topology.reserve(ports_.size());

    for (const auto& port : ports_) {
        PortTopology entry{
            port.number,
            port.name,
            port.messageName,
            port.direction,
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

}  // namespace rtos::messaging
