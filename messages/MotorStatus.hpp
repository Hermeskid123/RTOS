#pragma once

#include <cstdint>

namespace rtos::messages {

struct MotorStatus {
    std::int32_t currentRpm;
};

}  // namespace rtos::messages
