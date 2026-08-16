#pragma once

#include <string_view>

namespace rtos::model {

enum class ControlStatus {
    stopped,
    running,
    terminated,
};

[[nodiscard]] constexpr std::string_view toString(const ControlStatus status) noexcept
{
    switch (status) {
    case ControlStatus::stopped:
        return "STOPPED";
    case ControlStatus::running:
        return "RUNNING";
    case ControlStatus::terminated:
        return "TERMINATED";
    }

    return "UNKNOWN";
}

}  // namespace rtos::model
