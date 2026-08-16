#include "rtos/logging/Logger.hpp"

#include <array>
#include <chrono>
#include <iomanip>
#include <ostream>

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

void Logger::log(
    const LogLevel level,
    const std::string_view component,
    const std::string_view event,
    const std::string_view message
)
{
    if (level < minimumLevel_) {
        return;
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
