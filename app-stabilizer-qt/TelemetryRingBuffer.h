#pragma once

/// @file TelemetryRingBuffer.h
/// @brief Zero-heap circular history of stabilization telemetry for QML strip-chart drawing.

#include <cstddef>
#include <cstdint>

#include <array>
#include <mutex>

#include <QObject>
#include <QVariantList>

#include "VideoStabilizerEngine.h"

namespace StabilizerApp {

/// @struct TelemetrySample
/// @brief Snapshot of trajectory and tracking health for a single frame.
struct TelemetrySample {
    std::uint32_t frameIndex { 0U };
    float rawTx { 0.0F };
    float rawTy { 0.0F };
    float smoothTx { 0.0F };
    float smoothTy { 0.0F };
    float jitterVariance { 0.0F };
    float marginUtilization { 0.0F };
    std::uint32_t inlierCount { 0U };
    float inlierRatio { 0.0F };
    std::uint32_t totalLatencyUs { 0U };
    bool isSoftMarginDamped { false };
};

/// @class TelemetryRingBuffer
/// @brief Pre-allocated circular ring buffer holding 150 historical telemetry samples.
class TelemetryRingBuffer : public QObject {
    Q_OBJECT

    Q_PROPERTY(int capacity READ capacity CONSTANT)
    Q_PROPERTY(int count READ count NOTIFY historyUpdated)
    Q_PROPERTY(double latestJitter READ latestJitter NOTIFY historyUpdated)
    Q_PROPERTY(double latestMarginUtilization READ latestMarginUtilization NOTIFY historyUpdated)
    Q_PROPERTY(bool isSoftMarginEngaged READ isSoftMarginEngaged NOTIFY historyUpdated)

public:
    static constexpr std::size_t BufferCapacity { 150U };

    explicit TelemetryRingBuffer(QObject* parent = nullptr);
    ~TelemetryRingBuffer() override = default;

    TelemetryRingBuffer(const TelemetryRingBuffer&) = delete;
    TelemetryRingBuffer& operator=(const TelemetryRingBuffer&) = delete;
    TelemetryRingBuffer(TelemetryRingBuffer&&) = delete;
    TelemetryRingBuffer& operator=(TelemetryRingBuffer&&) = delete;

    /// @brief Push new diagnostics sample into ring buffer (Zero dynamic heap allocation).
    /// @param diag Diagnostics record from VideoStabilizerEngine.
    void appendSample(const Stabilizer::StabilizerDiagnostics& diag) noexcept;

    /// @brief Clear buffer contents.
    Q_INVOKABLE void clear() noexcept;

    /// @brief Total capacity of ring buffer.
    [[nodiscard]] int capacity() const noexcept;

    /// @brief Number of active valid samples in buffer.
    [[nodiscard]] int count() const noexcept;

    /// @brief Most recent jitter variance metric.
    [[nodiscard]] double latestJitter() const noexcept;

    /// @brief Most recent crop margin utilization ratio [0.0 .. 1.0].
    [[nodiscard]] double latestMarginUtilization() const noexcept;

    /// @brief Whether soft damping is actively engaging.
    [[nodiscard]] bool isSoftMarginEngaged() const noexcept;

    /// @brief Copies normalized raw trajectory coordinates for QML Canvas.
    /// @param axis 0 for X, 1 for Y.
    /// @return Array of normalized points [0.0 .. 1.0].
    [[nodiscard]] Q_INVOKABLE QVariantList getRawTrajectory(int axis) const;

    /// @brief Copies normalized smoothed trajectory coordinates for QML Canvas.
    /// @param axis 0 for X, 1 for Y.
    /// @return Array of normalized points [0.0 .. 1.0].
    [[nodiscard]] Q_INVOKABLE QVariantList getSmoothTrajectory(int axis) const;

    /// @brief Copies jitter variance curve points for QML Canvas strip-chart.
    [[nodiscard]] Q_INVOKABLE QVariantList getJitterHistory() const;

signals:
    /// @brief Emitted when new sample is added.
    void historyUpdated();

private:
    mutable std::mutex mutex_ { };
    std::array<TelemetrySample, BufferCapacity> ring_ { };
    std::size_t head_ { 0U };
    std::size_t count_ { 0U };
    double latestJitter_ { 0.0 };
    double latestMargin_ { 0.0 };
    bool isSoftMarginEngaged_ { false };
};

} // namespace StabilizerApp
