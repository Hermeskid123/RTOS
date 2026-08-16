#pragma once

#include <cstdint>
#include <string_view>

namespace rtos::messages {

struct MotorStatus {
    static constexpr std::string_view name{"MotorStatus"};
    std::int32_t currentRpm;
};

}  // namespace rtos::messages
