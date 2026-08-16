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
