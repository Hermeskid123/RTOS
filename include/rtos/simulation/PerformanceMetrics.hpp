#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace rtos::simulation {

struct FramePerformance {
    std::uint64_t frame{};
    std::size_t workerCount{};
    std::size_t messagesProcessed{};
    std::size_t queueDepth{};
    std::size_t queueHighWaterMark{};
    std::size_t dispatchLatencySamples{};
    bool deadlineMissed{};
    std::chrono::nanoseconds frameExecutionTime{};
    std::chrono::nanoseconds jitter{};
    std::chrono::nanoseconds parallelModelExecutionTime{};
    std::chrono::nanoseconds workerCpuTime{};
    std::chrono::nanoseconds totalDispatchLatency{};
    std::chrono::nanoseconds maximumDispatchLatency{};
    std::chrono::nanoseconds callbackExecutionTime{};
    std::chrono::nanoseconds dispatchPhaseTime{};
};

struct PerformanceSummary {
    std::size_t frames{};
    std::size_t workers{};
    std::size_t messagesProcessed{};
    std::size_t deadlineMisses{};
    std::size_t queueHighWaterMark{};
    double averageMessagesPerFrame{};
    double averageQueueDepth{};
    double averageFrameExecutionMilliseconds{};
    double maximumFrameExecutionMilliseconds{};
    double averageModelExecutionMilliseconds{};
    double averageDispatchLatencyMilliseconds{};
    double maximumDispatchLatencyMilliseconds{};
    double averageCallbackExecutionMilliseconds{};
    double averageDispatchPhaseMilliseconds{};
    double averageJitterMilliseconds{};
    double maximumJitterMilliseconds{};
    double cpuUtilizationPercent{};
};

class PerformanceMetrics final {
public:
    void reset() noexcept;
    void record(const FramePerformance& frame) noexcept;
    [[nodiscard]] PerformanceSummary summary() const noexcept;
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
