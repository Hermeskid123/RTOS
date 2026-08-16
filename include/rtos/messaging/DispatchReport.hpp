/**
 * @file
 * @brief Declares the public DispatchReport framework API.
 */

#pragma once

#include <chrono>
#include <cstddef>

namespace rtos::messaging {

struct DispatchReport {
    std::size_t messagesDispatched{};
    std::size_t callbacksInvoked{};
    std::size_t messagesWithoutSubscribers{};
    std::size_t queueDepthAtStart{};
    std::size_t queueHighWaterMark{};
    std::chrono::nanoseconds totalDispatchLatency{};
    std::chrono::nanoseconds maximumDispatchLatency{};
    std::chrono::nanoseconds callbackExecutionTime{};
    std::chrono::nanoseconds dispatchDuration{};

    [[nodiscard]] std::size_t messagesProcessed() const noexcept
    {
        return messagesDispatched + messagesWithoutSubscribers;
    }
};

}  // namespace rtos::messaging
