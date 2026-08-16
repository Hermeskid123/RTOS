/**
 * @file
 * @brief Declares the public DispatchReport framework API.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include <chrono>
#include <cstddef>

namespace rtos::messaging {

/** @brief Measurements and routing counts from one dispatch boundary. */
struct DispatchReport {
    /** Messages delivered to at least one matching subscriber. */
    std::size_t messagesDispatched{};
    /** Subscriber callbacks invoked across all messages. */
    std::size_t callbacksInvoked{};
    /** Messages discarded because no subscriber matched. */
    std::size_t messagesWithoutSubscribers{};
    /** Number of messages captured when the dispatch pass began. */
    std::size_t queueDepthAtStart{};
    /** Dispatch port's lifetime maximum pending depth. */
    std::size_t queueHighWaterMark{};
    /** Sum of publication-to-dispatch latencies. */
    std::chrono::nanoseconds totalDispatchLatency{};
    /** Longest publication-to-dispatch latency in this pass. */
    std::chrono::nanoseconds maximumDispatchLatency{};
    /** Time spent executing subscriber callbacks. */
    std::chrono::nanoseconds callbackExecutionTime{};
    /** Total wall time spent in this dispatch call. */
    std::chrono::nanoseconds dispatchDuration{};

    /** @brief Returns handled and unhandled messages in this pass. */
    [[nodiscard]] std::size_t messagesProcessed() const noexcept
    {
        return messagesDispatched + messagesWithoutSubscribers;
    }
};

}  // namespace rtos::messaging
