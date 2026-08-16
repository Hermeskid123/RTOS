#pragma once

#include <cstdint>

namespace rtos::messages {

struct MotorCommand {
    std::int32_t targetRpm;
};

}  // namespace rtos::messages
