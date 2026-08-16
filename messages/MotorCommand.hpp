#pragma once

#include <cstdint>
#include <string_view>

namespace rtos::messages {

struct MotorCommand {
    static constexpr std::string_view name{"MotorCommand"};
    static constexpr std::uint32_t defaultRoutingId{1002};
    std::int32_t targetRpm;
};

}  // namespace rtos::messages
