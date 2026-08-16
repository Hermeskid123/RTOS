/**
 * @file
 * @brief Declares the public MessageTraffic framework API.
 */

#pragma once

#include <cstddef>
#include <string>

namespace rtos::messaging {

struct MessageTraffic {
    std::string messageName;
    std::size_t publishers{};
    std::size_t subscribers{};
    std::size_t messagesSent{};
    std::size_t messagesReceived{};
    std::size_t messagesDispatched{};
    std::size_t messagesWithoutSubscribers{};
};

}  // namespace rtos::messaging
