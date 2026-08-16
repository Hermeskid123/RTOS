/**
 * @file
 * @brief Declares the public ControlStatus framework API.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include <string_view>

namespace rtos::model {

/** @brief Lifecycle state reported by a model operation. */
enum class ControlStatus {
    /** The model is initialized or frozen and is not operating. */
    stopped,
    /** The model has begun and may perform scheduled work. */
    running,
    /** The model has completed teardown and cannot resume. */
    terminated,
};

/**
 * @brief Converts a lifecycle state to its stable diagnostic spelling.
 * @param status State to convert.
 * @return Uppercase state name, or `UNKNOWN` for an invalid value.
 */
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
