/**
 * @file
 * @brief Declares the public PerformanceMetrics framework API.
 * @details Included in the complete release 1.0.0 Doxygen reference.
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace rtos::simulation {

/** @brief Raw timing and traffic observations for one simulation frame. */
struct FramePerformance {
    /** Authoritative simulation frame number. */
    std::uint64_t frame{};
    /** Worker processes scheduled during the frame. */
    std::size_t workerCount{};
    /** Messages handled or discarded during dispatch. */
    std::size_t messagesProcessed{};
    /** Pending messages observed for the frame. */
    std::size_t queueDepth{};
    /** Greatest queue depth observed by any worker. */
    std::size_t queueHighWaterMark{};
    /** Message latency measurements represented by this frame. */
    std::size_t dispatchLatencySamples{};
    /** Whether frame execution exceeded its deadline. */
    bool deadlineMissed{};
    /** End-to-end frame execution wall time. */
    std::chrono::nanoseconds frameExecutionTime{};
    /** Absolute deviation from the target frame start. */
    std::chrono::nanoseconds jitter{};
    /** Wall time for the parallel model-operation phase. */
    std::chrono::nanoseconds parallelModelExecutionTime{};
    /** Sum of CPU time consumed by worker processes. */
    std::chrono::nanoseconds workerCpuTime{};
    /** Sum of message publication-to-dispatch latencies. */
    std::chrono::nanoseconds totalDispatchLatency{};
    /** Longest message dispatch latency in the frame. */
    std::chrono::nanoseconds maximumDispatchLatency{};
    /** Time workers spent invoking callbacks. */
    std::chrono::nanoseconds callbackExecutionTime{};
    /** Wall time for the parallel dispatch phase. */
    std::chrono::nanoseconds dispatchPhaseTime{};
};

/** @brief Aggregated performance metrics since the latest reset. */
struct PerformanceSummary {
    /** Recorded frame count. */
    std::size_t frames{};
    /** Maximum worker count observed. */
    std::size_t workers{};
    /** Total processed messages. */
    std::size_t messagesProcessed{};
    /** Frames that exceeded their deadline. */
    std::size_t deadlineMisses{};
    /** Greatest queue depth observed. */
    std::size_t queueHighWaterMark{};
    /** Mean processed messages per frame. */
    double averageMessagesPerFrame{};
    /** Mean pending queue depth per frame. */
    double averageQueueDepth{};
    /** Mean frame wall time in milliseconds. */
    double averageFrameExecutionMilliseconds{};
    /** Maximum frame wall time in milliseconds. */
    double maximumFrameExecutionMilliseconds{};
    /** Mean parallel model-phase wall time in milliseconds. */
    double averageModelExecutionMilliseconds{};
    /** Mean publication-to-dispatch latency in milliseconds. */
    double averageDispatchLatencyMilliseconds{};
    /** Maximum publication-to-dispatch latency in milliseconds. */
    double maximumDispatchLatencyMilliseconds{};
    /** Mean callback execution time per frame in milliseconds. */
    double averageCallbackExecutionMilliseconds{};
    /** Mean dispatch-phase wall time in milliseconds. */
    double averageDispatchPhaseMilliseconds{};
    /** Mean scheduling jitter in milliseconds. */
    double averageJitterMilliseconds{};
    /** Maximum scheduling jitter in milliseconds. */
    double maximumJitterMilliseconds{};
    /** Normalized worker CPU utilization percentage. */
    double cpuUtilizationPercent{};
};

/** @brief Accumulates deterministic host-simulation performance statistics. */
class PerformanceMetrics final {
public:
    /** @brief Clears all observations and restores a zero summary. */
    void reset() noexcept;
    /** @brief Adds one completed frame to the aggregate. */
    void record(const FramePerformance& frame) noexcept;
    /** @brief Computes an aggregate snapshot from recorded frames. */
    [[nodiscard]] PerformanceSummary summary() const noexcept;
    /** @brief Returns the most recently recorded frame. */
    [[nodiscard]] const FramePerformance& latest() const noexcept;

private:
    FramePerformance latest_;
    std::size_t frames_{};
    std::size_t workers_{};
    std::size_t messagesProcessed_{};
    std::size_t deadlineMisses_{};
    std::size_t queueHighWaterMark_{};
    std::size_t totalQueueDepth_{};
    std::size_t dispatchLatencySamples_{};
    std::chrono::nanoseconds totalFrameExecutionTime_{};
    std::chrono::nanoseconds maximumFrameExecutionTime_{};
    std::chrono::nanoseconds totalModelExecutionTime_{};
    std::chrono::nanoseconds totalWorkerCpuTime_{};
    std::chrono::nanoseconds totalDispatchLatency_{};
    std::chrono::nanoseconds maximumDispatchLatency_{};
    std::chrono::nanoseconds totalCallbackExecutionTime_{};
    std::chrono::nanoseconds totalDispatchPhaseTime_{};
    std::chrono::nanoseconds totalJitter_{};
    std::chrono::nanoseconds maximumJitter_{};
};

}  // namespace rtos::simulation
