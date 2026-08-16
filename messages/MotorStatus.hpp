/**
 * @file
 * @brief Declares the MotorStatus project message payload.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include <cstdint>
#include <string_view>

namespace rtos::messages {

/** @brief Reports the motor model's simulated shaft speed. */
struct MotorStatus {
    /** Stable transport and diagnostics name. */
    static constexpr std::string_view name{"MotorStatus"};
    /** Default inter-process routing identifier. */
    static constexpr std::uint32_t defaultRoutingId{1003};
    /** Current shaft speed in revolutions per minute. */
    std::int32_t currentRpm;
};

}  // namespace rtos::messages
