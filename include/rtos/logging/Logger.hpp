/**
 * @file
 * @brief Declares the public Logger framework API.
 */

#pragma once

#include <iosfwd>
#include <string>
#include <string_view>
#include <unordered_map>

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
    void setEnabled(bool enabled) noexcept;
    [[nodiscard]] bool enabled() const noexcept;
    void setComponentDebugEnabled(std::string component, bool enabled);

    void log(
        LogLevel level,
        std::string_view component,
        std::string_view event,
        std::string_view message
    );

private:
    std::ostream* output_;
    LogLevel minimumLevel_;
    bool enabled_{true};
    std::unordered_map<std::string, bool> componentDebugEnabled_;
};

}  // namespace rtos::logging
