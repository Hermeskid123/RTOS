/**
 * @file
 * @brief Declares the public QueueConfiguration framework API.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include <cstddef>
#include <string_view>

namespace rtos::messaging {

/** @brief Largest payload supported by the dispatch port's inline storage. */
inline constexpr std::size_t maximumSupportedMessageSize{256};

/** @brief Action taken when an incoming queue has reached capacity. */
enum class QueueFullPolicy {
    /** Reject the attempted publication and preserve the queue. */
    rejectNewest,
    /** Drop the attempted publication and preserve the queue. */
    dropNewest,
    /** Remove the oldest entry and accept the new publication. */
    dropOldest,
};

/** @brief Returns the stable diagnostic name for a queue-full policy. */
[[nodiscard]] constexpr std::string_view toString(const QueueFullPolicy policy) noexcept
{
    switch (policy) {
    case QueueFullPolicy::rejectNewest:
        return "REJECT_NEWEST";
    case QueueFullPolicy::dropNewest:
        return "DROP_NEWEST";
    case QueueFullPolicy::dropOldest:
        return "DROP_OLDEST";
    }
    return "UNKNOWN";
}

/** @brief Outcome of one DispatchPort publication attempt. */
enum class SendResult {
    /** Message entered the local queue. */
    queued,
    /** Message was handed to an external transport. */
    transported,
    /** Full queue rejected the message under `rejectNewest`. */
    rejectedQueueFull,
    /** Full queue discarded the message under `dropNewest`. */
    droppedNewest,
    /** Message was queued after evicting the oldest entry. */
    queuedAfterDroppingOldest,
    /** Payload exceeds the configured maximum message size. */
    messageTooLarge,
};

/** @brief Tests whether a send result represents accepted ownership. */
[[nodiscard]] constexpr bool accepted(const SendResult result) noexcept
{
    return result == SendResult::queued || result == SendResult::transported
        || result == SendResult::queuedAfterDroppingOldest;
}

/** @brief Immutable bounded-queue settings supplied at construction. */
struct QueueConfiguration {
    /** Maximum number of pending messages. */
    std::size_t depth{4096};
    /** Maximum payload bytes accepted by this port. */
    std::size_t maximumMessageSize{maximumSupportedMessageSize};
    /** Overflow behavior applied at `depth`. */
    QueueFullPolicy fullPolicy{QueueFullPolicy::rejectNewest};
};

/** @brief Snapshot of queue occupancy and rejection counters. */
struct QueueStatistics {
    /** Configured queue depth. */
    std::size_t capacity{};
    /** Messages waiting for a future dispatch pass. */
    std::size_t pending{};
    /** Greatest observed pending count. */
    std::size_t highWaterMark{};
    /** Messages rejected by `rejectNewest`. */
    std::size_t rejectedQueueFull{};
    /** Messages discarded by `dropNewest`. */
    std::size_t droppedNewest{};
    /** Old entries evicted by `dropOldest`. */
    std::size_t droppedOldest{};
    /** Messages rejected for exceeding `maximumMessageSize`. */
    std::size_t rejectedOversize{};
};

}  // namespace rtos::messaging
