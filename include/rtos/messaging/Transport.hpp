#pragma once

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <string>
#include <string_view>
#include <vector>

namespace rtos::messaging {

using RoutingId = std::uint32_t;

enum class TransportType {
    inProcess,
    interProcess,
};

[[nodiscard]] constexpr std::string_view toString(const TransportType transport) noexcept
{
    switch (transport) {
    case TransportType::inProcess:
        return "IN_PROCESS";
    case TransportType::interProcess:
        return "IPC";
    }
    return "UNKNOWN";
}

struct TransportMessage {
    RoutingId routingId{};
    std::string messageName;
    std::vector<std::byte> payload;
    std::chrono::steady_clock::time_point queuedAt{};
};

class MessageTransport {
public:
    MessageTransport() = default;
    MessageTransport(const MessageTransport&) = delete;
    MessageTransport& operator=(const MessageTransport&) = delete;
    virtual ~MessageTransport() = default;

    virtual void send(TransportMessage message) = 0;
};

}  // namespace rtos::messaging
