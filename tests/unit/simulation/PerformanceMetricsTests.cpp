/**
 * @file
 * @brief Defines PerformanceMetricsTests coverage for the RTOS framework test suite.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#include "TestFramework.hpp"

#include "rtos/simulation/PerformanceMetrics.hpp"

#include <chrono>

TEST_CASE("performance metrics aggregate parallel frame measurements")
{
    using namespace std::chrono_literals;

    rtos::simulation::PerformanceMetrics metrics;
    metrics.record(rtos::simulation::FramePerformance{
        .frame = 1,
        .workerCount = 3,
        .messagesProcessed = 2,
        .queueDepth = 2,
        .queueHighWaterMark = 2,
        .dispatchLatencySamples = 2,
        .deadlineMissed = false,
        .frameExecutionTime = 4ms,
        .jitter = 1ms,
        .parallelModelExecutionTime = 2ms,
        .workerCpuTime = 3ms,
        .totalDispatchLatency = 6ms,
        .maximumDispatchLatency = 4ms,
        .callbackExecutionTime = 1ms,
        .dispatchPhaseTime = 2ms,
    });
    metrics.record(rtos::simulation::FramePerformance{
        .frame = 2,
        .workerCount = 3,
        .messagesProcessed = 1,
        .queueDepth = 1,
        .queueHighWaterMark = 4,
        .dispatchLatencySamples = 1,
        .deadlineMissed = true,
        .frameExecutionTime = 6ms,
        .jitter = 3ms,
        .parallelModelExecutionTime = 4ms,
        .workerCpuTime = 4ms,
        .totalDispatchLatency = 3ms,
        .maximumDispatchLatency = 3ms,
        .callbackExecutionTime = 2ms,
        .dispatchPhaseTime = 3ms,
    });

    const auto summary = metrics.summary();
    REQUIRE(summary.frames == 2);
    REQUIRE(summary.workers == 3);
    REQUIRE(summary.messagesProcessed == 3);
    REQUIRE(summary.deadlineMisses == 1);
    REQUIRE(summary.queueHighWaterMark == 4);
    REQUIRE(summary.averageMessagesPerFrame == 1.5);
    REQUIRE(summary.averageQueueDepth == 1.5);
    REQUIRE(summary.averageFrameExecutionMilliseconds == 5.0);
    REQUIRE(summary.maximumFrameExecutionMilliseconds == 6.0);
    REQUIRE(summary.averageModelExecutionMilliseconds == 3.0);
    REQUIRE(summary.averageDispatchLatencyMilliseconds == 3.0);
    REQUIRE(summary.maximumDispatchLatencyMilliseconds == 4.0);
    REQUIRE(summary.averageJitterMilliseconds == 2.0);
}
