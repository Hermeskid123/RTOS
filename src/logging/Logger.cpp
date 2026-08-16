/**
 * @file
 * @brief Implements the Logger framework API.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#include "rtos/logging/Logger.hpp"

#include <array>
#include <chrono>
#include <iomanip>
#include <ostream>
#include <utility>

namespace rtos::logging {
namespace {

constexpr std::array<std::string_view, 6> levelNames{
    "TRACE",
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL",
};

}  // namespace

std::string_view toString(const LogLevel level) noexcept
{
    return levelNames.at(static_cast<std::size_t>(level));
}

Logger::Logger(std::ostream& output, const LogLevel minimumLevel) noexcept
    : output_{&output}, minimumLevel_{minimumLevel}
{
}

void Logger::setLevel(const LogLevel minimumLevel) noexcept
{
    minimumLevel_ = minimumLevel;
}

LogLevel Logger::level() const noexcept
{
    return minimumLevel_;
}

void Logger::setEnabled(const bool enabled) noexcept
{
    enabled_ = enabled;
}

bool Logger::enabled() const noexcept
{
    return enabled_;
}

void Logger::setComponentDebugEnabled(std::string component, const bool enabled)
{
    componentDebugEnabled_.insert_or_assign(std::move(component), enabled);
}

void Logger::log(
    const LogLevel level,
    const std::string_view component,
    const std::string_view event,
    const std::string_view message
)
{
    if (!enabled_ || level < minimumLevel_) {
        return;
    }
    if (level == LogLevel::debug) {
        const auto componentSetting = componentDebugEnabled_.find(std::string{component});
        if (
            componentSetting != componentDebugEnabled_.end()
            && !componentSetting->second
        ) {
            return;
        }
    }

    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ) % 1000;

    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif

    *output_ << std::put_time(&localTime, "%H:%M:%S") << '.'
             << std::setfill('0') << std::setw(3) << milliseconds.count() << ' '
             << std::left << std::setfill(' ') << std::setw(5) << toString(level) << ' '
             << component << ' ' << event << ' ' << message << '\n';
}

}  // namespace rtos::logging
