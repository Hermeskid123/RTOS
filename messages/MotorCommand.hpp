/**
 * @file
 * @brief Declares the MotorCommand project message payload.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include <cstdint>
#include <string_view>

namespace rtos::messages {

/** @brief Requests that the motor model move toward a target speed. */
struct MotorCommand {
    /** Stable transport and diagnostics name. */
    static constexpr std::string_view name{"MotorCommand"};
    /** Default inter-process routing identifier. */
    static constexpr std::uint32_t defaultRoutingId{1002};
    /** Requested shaft speed in revolutions per minute. */
    std::int32_t targetRpm;
};

}  // namespace rtos::messages
