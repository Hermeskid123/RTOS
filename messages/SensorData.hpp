#pragma once

#include <string_view>

namespace rtos::messages {

struct SensorData {
    static constexpr std::string_view name{"SensorData"};
    double value;
};

}  // namespace rtos::messages
