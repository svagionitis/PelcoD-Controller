/// @file TelemetryRingBuffer.cpp
/// @brief Implementation of circular history buffer for stabilization telemetry.

#include "TelemetryRingBuffer.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <cmath>
#include <mutex>

namespace StabilizerApp {

TelemetryRingBuffer::TelemetryRingBuffer(QObject* parent)
    : QObject(parent)
{
}

void TelemetryRingBuffer::appendSample(const Stabilizer::StabilizerDiagnostics& diag) noexcept
{
    TelemetrySample sample { };
    sample.frameIndex = diag.frameIndex;
    sample.rawTx = diag.rawTransform.tx;
    sample.rawTy = diag.rawTransform.ty;
    sample.smoothTx = diag.smoothedTransform.tx;
    sample.smoothTy = diag.smoothedTransform.ty;

    const float dx = sample.rawTx - sample.smoothTx;
    const float dy = sample.rawTy - sample.smoothTy;
    sample.jitterVariance = (dx * dx) + (dy * dy);

    // Approximate margin utilization based on correction displacement
    const float corrMag = std::sqrt((diag.correctionTransform.tx * diag.correctionTransform.tx)
        + (diag.correctionTransform.ty * diag.correctionTransform.ty));
    // Assume typical margin of ~80px for 1080p
    sample.marginUtilization = std::clamp(corrMag / 80.0F, 0.0F, 1.0F);
    sample.isSoftMarginDamped = (sample.marginUtilization >= 0.80F);

    sample.inlierCount = diag.inlierCount;
    sample.inlierRatio = diag.inlierRatio;
    sample.totalLatencyUs = diag.latencyUs.totalFrameUs;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        ring_[head_] = sample;
        head_ = (head_ + 1U) % BufferCapacity;
        if (count_ < BufferCapacity) {
            count_ += 1U;
        }
        latestJitter_ = static_cast<double>(sample.jitterVariance);
        latestMargin_ = static_cast<double>(sample.marginUtilization);
        isSoftMarginEngaged_ = sample.isSoftMarginDamped;
    }

    emit historyUpdated();
}

void TelemetryRingBuffer::clear() noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    head_ = 0U;
    count_ = 0U;
    latestJitter_ = 0.0;
    latestMargin_ = 0.0;
    isSoftMarginEngaged_ = false;
    emit historyUpdated();
}

int TelemetryRingBuffer::capacity() const noexcept
{
    return static_cast<int>(BufferCapacity);
}

int TelemetryRingBuffer::count() const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(count_);
}

double TelemetryRingBuffer::latestJitter() const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    return latestJitter_;
}

double TelemetryRingBuffer::latestMarginUtilization() const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    return latestMargin_;
}

bool TelemetryRingBuffer::isSoftMarginEngaged() const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    return isSoftMarginEngaged_;
}

QVariantList TelemetryRingBuffer::getRawTrajectory(int axis) const
{
    QVariantList result;
    std::lock_guard<std::mutex> lock(mutex_);
    if (count_ == 0U) {
        return result;
    }

    result.reserve(static_cast<int>(count_));
    const std::size_t start = (count_ == BufferCapacity) ? head_ : 0U;

    for (std::size_t i = 0U; i < count_; ++i) {
        const std::size_t idx = (start + i) % BufferCapacity;
        const float val = (axis == 0) ? ring_[idx].rawTx : ring_[idx].rawTy;
        // Normalize roughly within [-40.0 .. +40.0] -> [0.0 .. 1.0] centered at 0.5
        const float norm = std::clamp((val + 40.0F) / 80.0F, 0.0F, 1.0F);
        result.append(static_cast<double>(norm));
    }
    return result;
}

QVariantList TelemetryRingBuffer::getSmoothTrajectory(int axis) const
{
    QVariantList result;
    std::lock_guard<std::mutex> lock(mutex_);
    if (count_ == 0U) {
        return result;
    }

    result.reserve(static_cast<int>(count_));
    const std::size_t start = (count_ == BufferCapacity) ? head_ : 0U;

    for (std::size_t i = 0U; i < count_; ++i) {
        const std::size_t idx = (start + i) % BufferCapacity;
        const float val = (axis == 0) ? ring_[idx].smoothTx : ring_[idx].smoothTy;
        const float norm = std::clamp((val + 40.0F) / 80.0F, 0.0F, 1.0F);
        result.append(static_cast<double>(norm));
    }
    return result;
}

QVariantList TelemetryRingBuffer::getJitterHistory() const
{
    QVariantList result;
    std::lock_guard<std::mutex> lock(mutex_);
    if (count_ == 0U) {
        return result;
    }

    result.reserve(static_cast<int>(count_));
    const std::size_t start = (count_ == BufferCapacity) ? head_ : 0U;

    for (std::size_t i = 0U; i < count_; ++i) {
        const std::size_t idx = (start + i) % BufferCapacity;
        // Scale jitter variance [0 .. 100 px^2] -> [0.0 .. 1.0]
        const float norm = std::clamp(ring_[idx].jitterVariance / 100.0F, 0.0F, 1.0F);
        result.append(static_cast<double>(norm));
    }
    return result;
}

} // namespace StabilizerApp
