/**
 * @file
 * @brief Defines ModelConfigurationTests coverage for the RTOS framework test suite.
 */

#include "TestFramework.hpp"

#include "rtos/model/ModelConfiguration.hpp"

#include <stdexcept>

TEST_CASE("model configuration parses enabled debug and gdb arguments")
{
    const auto configuration = rtos::model::ModelConfiguration::parse(
        R"(<models>
               <model name="SensorModel" enabled="true" debug="false" gdb="true" />
               <model name="MotorModel" enabled="false" debug="true" gdb="false" />
           </models>)"
    );

    const auto* sensor = configuration.find("SensorModel");
    const auto* motor = configuration.find("MotorModel");
    REQUIRE(sensor != nullptr);
    REQUIRE(sensor->enabled);
    REQUIRE(!sensor->debugEnabled);
    REQUIRE(sensor->gdbEnabled);
    REQUIRE(motor != nullptr);
    REQUIRE(!motor->enabled);
    REQUIRE(motor->debugEnabled);
    REQUIRE(!motor->gdbEnabled);
}

TEST_CASE("model configuration rejects invalid boolean arguments")
{
    bool rejected{};
    try {
        static_cast<void>(rtos::model::ModelConfiguration::parse(
            R"(<models><model name="SensorModel" enabled="sometimes" /></models>)"
        ));
    } catch (const std::runtime_error&) {
        rejected = true;
    }

    REQUIRE(rejected);
}
