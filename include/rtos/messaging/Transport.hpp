/**
 * @file
 * @brief Declares the public Transport framework API.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <string>
#include <string_view>
#include <vector>

namespace rtos::messaging {

/** @brief Numeric route shared by matching publisher and subscriber ports. */
using RoutingId = std::uint32_t;

/** @brief Mechanism used to move a published message. */
enum class TransportType {
    /** Queue and dispatch within the current process. */
    inProcess,
    /** Serialize through the host process boundary. */
    interProcess,
};

/**
 * @brief Converts a transport type to its diagnostic spelling.
 * @param transport Transport value to convert.
 * @return Stable uppercase name, or `UNKNOWN` for an invalid value.
 */
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

/** @brief Type-erased message envelope used by external transports. */
struct TransportMessage {
    /** Destination route identifier. */
    RoutingId routingId{};
    /** Human-readable payload type name. */
    std::string messageName;
    /** Byte-for-byte copy of the trivially-copyable message payload. */
    std::vector<std::byte> payload;
    /** Original publication time used for latency metrics. */
    std::chrono::steady_clock::time_point queuedAt{};
};

/** @brief Abstract sink used by DispatchPort for out-of-process messages. */
class MessageTransport {
public:
    MessageTransport() = default;
    MessageTransport(const MessageTransport&) = delete;
    MessageTransport& operator=(const MessageTransport&) = delete;
    virtual ~MessageTransport() = default;

    /**
     * @brief Transfers ownership of a serialized message to the transport.
     * @param message Envelope to deliver.
     */
    virtual void send(TransportMessage message) = 0;
};

}  // namespace rtos::messaging
