/**
 * @file
 * @brief Defines LoggerTests coverage for the RTOS framework test suite.
 */

#include "TestFramework.hpp"

#include "rtos/logging/Logger.hpp"

#include <sstream>
#include <string>

TEST_CASE("logger writes the standard record fields")
{
    std::ostringstream output;
    rtos::logging::Logger logger{output, rtos::logging::LogLevel::debug};

    logger.log(
        rtos::logging::LogLevel::info,
        "TestComponent",
        "TEST_EVENT",
        "test message"
    );

    const std::string record = output.str();
    REQUIRE(record.find("INFO") != std::string::npos);
    REQUIRE(record.find("TestComponent") != std::string::npos);
    REQUIRE(record.find("TEST_EVENT") != std::string::npos);
    REQUIRE(record.find("test message") != std::string::npos);
}

TEST_CASE("logger filters records below its configured level")
{
    std::ostringstream output;
    rtos::logging::Logger logger{output, rtos::logging::LogLevel::warn};

    logger.log(rtos::logging::LogLevel::debug, "Test", "FILTER", "hidden");
    logger.log(rtos::logging::LogLevel::error, "Test", "FILTER", "visible");

    const std::string records = output.str();
    REQUIRE(records.find("hidden") == std::string::npos);
    REQUIRE(records.find("visible") != std::string::npos);
}

TEST_CASE("logger exposes and updates its minimum level")
{
    std::ostringstream output;
    rtos::logging::Logger logger{output};

    REQUIRE(logger.level() == rtos::logging::LogLevel::info);
    logger.setLevel(rtos::logging::LogLevel::trace);
    REQUIRE(logger.level() == rtos::logging::LogLevel::trace);
}

TEST_CASE("logger can disable all records including fatal messages")
{
    std::ostringstream output;
    rtos::logging::Logger logger{output, rtos::logging::LogLevel::error};

    logger.setEnabled(false);
    logger.log(rtos::logging::LogLevel::fatal, "Test", "FATAL", "hidden");

    REQUIRE(!logger.enabled());
    REQUIRE(output.str().empty());

    logger.setEnabled(true);
    logger.log(rtos::logging::LogLevel::error, "Test", "ERROR", "visible");
    REQUIRE(logger.enabled());
    REQUIRE(output.str().find("visible") != std::string::npos);
}

TEST_CASE("error level includes error and fatal records only")
{
    std::ostringstream output;
    rtos::logging::Logger logger{output, rtos::logging::LogLevel::error};

    logger.log(rtos::logging::LogLevel::warn, "Test", "WARN", "hidden warning");
    logger.log(rtos::logging::LogLevel::error, "Test", "ERROR", "visible error");
    logger.log(rtos::logging::LogLevel::fatal, "Test", "FATAL", "visible fatal");

    const std::string records = output.str();
    REQUIRE(records.find("hidden warning") == std::string::npos);
    REQUIRE(records.find("visible error") != std::string::npos);
    REQUIRE(records.find("visible fatal") != std::string::npos);
}

TEST_CASE("logger filters debug records for a configured component")
{
    std::ostringstream output;
    rtos::logging::Logger logger{output, rtos::logging::LogLevel::debug};
    logger.setComponentDebugEnabled("QuietModel", false);

    logger.log(rtos::logging::LogLevel::debug, "QuietModel", "DEBUG", "hidden");
    logger.log(rtos::logging::LogLevel::info, "QuietModel", "INFO", "visible info");
    logger.log(rtos::logging::LogLevel::debug, "LoudModel", "DEBUG", "visible debug");

    const std::string records = output.str();
    REQUIRE(records.find("hidden") == std::string::npos);
    REQUIRE(records.find("visible info") != std::string::npos);
    REQUIRE(records.find("visible debug") != std::string::npos);
}
