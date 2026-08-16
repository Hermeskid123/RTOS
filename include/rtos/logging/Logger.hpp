/**
 * @file
 * @brief Declares the public Logger framework API.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include <iosfwd>
#include <string>
#include <string_view>
#include <unordered_map>

namespace rtos::logging {

/** @brief Severity threshold assigned to a log record. */
enum class LogLevel {
    /** Fine-grained execution tracing. */
    trace,
    /** Developer diagnostics. */
    debug,
    /** Normal operational information. */
    info,
    /** Recoverable abnormal condition. */
    warn,
    /** Operation failure. */
    error,
    /** Unrecoverable process failure. */
    fatal,
};

/** @brief Returns the stable uppercase name of a log level. */
[[nodiscard]] std::string_view toString(LogLevel level) noexcept;

/** @brief Thread-compatible formatted logger with component debug controls. */
class Logger {
public:
    /** @brief Creates a logger writing to `output` at or above `minimumLevel`. */
    explicit Logger(std::ostream& output, LogLevel minimumLevel = LogLevel::info) noexcept;

    /** @brief Changes the minimum emitted severity. */
    void setLevel(LogLevel minimumLevel) noexcept;
    /** @brief Returns the current severity threshold. */
    [[nodiscard]] LogLevel level() const noexcept;
    /** @brief Globally enables or suppresses log output. */
    void setEnabled(bool enabled) noexcept;
    /** @brief Reports whether logging is globally enabled. */
    [[nodiscard]] bool enabled() const noexcept;
    /** @brief Enables or suppresses DEBUG records for one component. */
    void setComponentDebugEnabled(std::string component, bool enabled);

    /**
     * @brief Emits one structured log record when filters permit it.
     * @param level Record severity.
     * @param component Model or subsystem name.
     * @param event Short event category.
     * @param message Human-readable event detail.
     */
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
