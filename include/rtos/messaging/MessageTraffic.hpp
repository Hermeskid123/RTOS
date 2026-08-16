/**
 * @file
 * @brief Declares the public MessageTraffic framework API.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include <cstddef>
#include <string>

namespace rtos::messaging {

/** @brief Cumulative routing counters for one exact message type. */
struct MessageTraffic {
    /** Stable or compiler-derived message type name. */
    std::string messageName;
    /** Registered publisher endpoint count. */
    std::size_t publishers{};
    /** Active subscriber callback count. */
    std::size_t subscribers{};
    /** Publication attempts observed by the dispatch port. */
    std::size_t messagesSent{};
    /** Subscriber callback deliveries completed. */
    std::size_t messagesReceived{};
    /** Queued messages delivered to at least one subscriber. */
    std::size_t messagesDispatched{};
    /** Queued messages discarded without a matching subscriber. */
    std::size_t messagesWithoutSubscribers{};
};

}  // namespace rtos::messaging
