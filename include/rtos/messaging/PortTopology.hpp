/**
 * @file
 * @brief Declares the public PortTopology framework API.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include "rtos/messaging/Transport.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace rtos::messaging {

/** @brief Declares whether a named port publishes or subscribes. */
enum class PortDirection {
    /** Port may send messages of its configured type. */
    publisher,
    /** Port may register callbacks for its configured type. */
    subscriber,
};

/** @brief Introspection record for one typed messaging endpoint. */
struct PortTopology {
    /** Sequential identifier unique within the owning dispatch port. */
    std::size_t number{};
    /** Application-assigned endpoint name. */
    std::string name;
    /** Stable or compiler-derived message type name. */
    std::string messageName;
    /** Endpoint direction. */
    PortDirection direction;
    /** Transport used by the owning dispatch port. */
    TransportType transport{TransportType::inProcess};
    /** Numeric route used for transported messages. */
    RoutingId routingId{};
    /** Compatible publisher endpoint numbers. */
    std::vector<std::size_t> publisherPorts;
    /** Compatible subscriber endpoint numbers. */
    std::vector<std::size_t> subscriberPorts;
};

}  // namespace rtos::messaging
