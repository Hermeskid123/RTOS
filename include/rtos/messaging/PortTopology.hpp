#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace rtos::messaging {

enum class PortDirection {
    publisher,
    subscriber,
};

struct PortTopology {
    std::size_t number{};
    std::string name;
    std::string messageName;
    PortDirection direction;
    std::vector<std::size_t> publisherPorts;
    std::vector<std::size_t> subscriberPorts;
};

}  // namespace rtos::messaging
