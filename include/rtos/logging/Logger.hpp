#pragma once

#include <iosfwd>
#include <string_view>

namespace rtos::logging {

enum class LogLevel {
    trace,
    debug,
    info,
    warn,
    error,
    fatal,
};

[[nodiscard]] std::string_view toString(LogLevel level) noexcept;

class Logger {
public:
    explicit Logger(std::ostream& output, LogLevel minimumLevel = LogLevel::info) noexcept;

    void setLevel(LogLevel minimumLevel) noexcept;
    [[nodiscard]] LogLevel level() const noexcept;

    void log(
        LogLevel level,
        std::string_view component,
        std::string_view event,
        std::string_view message
    );

private:
    std::ostream* output_;
    LogLevel minimumLevel_;
};

}  // namespace rtos::logging
