#pragma once

#include <cstddef>
#include <string_view>

namespace rtos::messaging {

inline constexpr std::size_t maximumSupportedMessageSize{256};

enum class QueueFullPolicy {
    rejectNewest,
    dropNewest,
    dropOldest,
};

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

enum class SendResult {
    queued,
    transported,
    rejectedQueueFull,
    droppedNewest,
    queuedAfterDroppingOldest,
    messageTooLarge,
};

[[nodiscard]] constexpr bool accepted(const SendResult result) noexcept
{
    return result == SendResult::queued || result == SendResult::transported
        || result == SendResult::queuedAfterDroppingOldest;
}

struct QueueConfiguration {
    std::size_t depth{4096};
    std::size_t maximumMessageSize{maximumSupportedMessageSize};
    QueueFullPolicy fullPolicy{QueueFullPolicy::rejectNewest};
};

struct QueueStatistics {
    std::size_t capacity{};
    std::size_t pending{};
    std::size_t highWaterMark{};
    std::size_t rejectedQueueFull{};
    std::size_t droppedNewest{};
    std::size_t droppedOldest{};
    std::size_t rejectedOversize{};
};

}  // namespace rtos::messaging
