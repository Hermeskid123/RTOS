/**
 * @file
 * @brief Declares the MotorStatus project message payload.
 */

#pragma once

#include <cstdint>
#include <string_view>

namespace rtos::messages {

struct MotorStatus {
    static constexpr std::string_view name{"MotorStatus"};
    static constexpr std::uint32_t defaultRoutingId{1003};
    std::int32_t currentRpm;
};

}  // namespace rtos::messages
