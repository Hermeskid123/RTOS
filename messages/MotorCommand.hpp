#pragma once

#include <cstdint>
#include <string_view>

namespace rtos::messages {

struct MotorCommand {
    static constexpr std::string_view name{"MotorCommand"};
    std::int32_t targetRpm;
};

}  // namespace rtos::messages
