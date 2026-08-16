/**
 * @file
 * @brief Declares the SensorData project message payload.
 */

#pragma once

#include <cstdint>
#include <string_view>

namespace rtos::messages {

struct SensorData {
    static constexpr std::string_view name{"SensorData"};
    static constexpr std::uint32_t defaultRoutingId{1001};
    double value;
};

}  // namespace rtos::messages
