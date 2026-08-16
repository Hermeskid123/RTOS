#include "rtos/simulation/PerformanceMetrics.hpp"

#include <algorithm>
#include <thread>

namespace rtos::simulation {
namespace {

double milliseconds(const std::chrono::nanoseconds duration) noexcept
{
    return std::chrono::duration<double, std::milli>{duration}.count();
}

}  // namespace

void PerformanceMetrics::reset() noexcept
{
    *this = PerformanceMetrics{};
}

void PerformanceMetrics::record(const FramePerformance& frame) noexcept
{
    latest_ = frame;
    ++frames_;
    workers_ = std::max(workers_, frame.workerCount);
    messagesProcessed_ += frame.messagesProcessed;
    deadlineMisses_ += frame.deadlineMissed ? 1 : 0;
    queueHighWaterMark_ = std::max(queueHighWaterMark_, frame.queueHighWaterMark);
    totalQueueDepth_ += frame.queueDepth;
    dispatchLatencySamples_ += frame.dispatchLatencySamples;
    totalFrameExecutionTime_ += frame.frameExecutionTime;
    maximumFrameExecutionTime_ = std::max(
        maximumFrameExecutionTime_, frame.frameExecutionTime
    );
    totalModelExecutionTime_ += frame.parallelModelExecutionTime;
    totalWorkerCpuTime_ += frame.workerCpuTime;
    totalDispatchLatency_ += frame.totalDispatchLatency;
    maximumDispatchLatency_ = std::max(
        maximumDispatchLatency_, frame.maximumDispatchLatency
    );
    totalCallbackExecutionTime_ += frame.callbackExecutionTime;
    totalDispatchPhaseTime_ += frame.dispatchPhaseTime;
    totalJitter_ += frame.jitter;
    maximumJitter_ = std::max(maximumJitter_, frame.jitter);
}

PerformanceSummary PerformanceMetrics::summary() const noexcept
{
    PerformanceSummary result;
    result.frames = frames_;
    result.workers = workers_;
    result.messagesProcessed = messagesProcessed_;
    result.deadlineMisses = deadlineMisses_;
    result.queueHighWaterMark = queueHighWaterMark_;
    if (frames_ == 0) {
        return result;
    }

    const auto frameDivisor = static_cast<double>(frames_);
    result.averageMessagesPerFrame = messagesProcessed_ / frameDivisor;
    result.averageQueueDepth = totalQueueDepth_ / frameDivisor;
    result.averageFrameExecutionMilliseconds =
        milliseconds(totalFrameExecutionTime_) / frameDivisor;
    result.maximumFrameExecutionMilliseconds = milliseconds(maximumFrameExecutionTime_);
    result.averageModelExecutionMilliseconds =
        milliseconds(totalModelExecutionTime_) / frameDivisor;
    result.averageDispatchLatencyMilliseconds = dispatchLatencySamples_ == 0
        ? 0.0
        : milliseconds(totalDispatchLatency_)
            / static_cast<double>(dispatchLatencySamples_);
    result.maximumDispatchLatencyMilliseconds = milliseconds(maximumDispatchLatency_);
    result.averageCallbackExecutionMilliseconds =
        milliseconds(totalCallbackExecutionTime_) / frameDivisor;
    result.averageDispatchPhaseMilliseconds =
        milliseconds(totalDispatchPhaseTime_) / frameDivisor;
    result.averageJitterMilliseconds = milliseconds(totalJitter_) / frameDivisor;
    result.maximumJitterMilliseconds = milliseconds(maximumJitter_);

    const auto hardwareThreads = std::max(1U, std::thread::hardware_concurrency());
    const auto availableCpuTime = totalFrameExecutionTime_ * hardwareThreads;
    if (availableCpuTime > std::chrono::nanoseconds::zero()) {
        result.cpuUtilizationPercent = 100.0
            * static_cast<double>(totalWorkerCpuTime_.count())
            / static_cast<double>(availableCpuTime.count());
    }
    return result;
}

const FramePerformance& PerformanceMetrics::latest() const noexcept
{
    return latest_;
}

}  // namespace rtos::simulation
