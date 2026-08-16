#include "rtos/messaging/DispatchPort.hpp"

namespace rtos::messaging {

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
            continue;
        }

        ++report.messagesDispatched;
        report.callbacksInvoked += callbacksInvoked;
    }

    return report;
}

}  // namespace rtos::messaging
