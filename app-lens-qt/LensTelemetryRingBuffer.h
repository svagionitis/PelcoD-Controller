#pragma once

/// @file LensTelemetryRingBuffer.h
/// @brief Zero-heap circular history of optical telemetry and latencies for diagnostics and strip-charting.

#include <cstddef>
#include <cstdint>

#include <array>
#include <mutex>

#include <QObject>
#include <QString>
#include <QVariantList>

namespace LensApp {

/// @struct TelemetrySample
/// @brief Zero-heap point-in-time optical telemetry sample.
struct TelemetrySample {
    std::uint64_t timestampNs { 0U };
    std::uint32_t frameIndex { 0U };
    std::uint8_t channel { 0U };
    std::uint16_t focusPosition { 0U };
    std::uint16_t zoomPosition { 0U };
    double metricScore { 0.0 };
    std::uint8_t afState { 0U };
    std::uint8_t limitFlags { 0U };
    std::uint32_t decodeLatencyUs { 0U };
    std::uint32_t metricLatencyUs { 0U };
    std::uint32_t loopLatencyUs { 0U };
    float currentFps { 0.0F };
};

/// @class LensTelemetryRingBuffer
/// @brief Thread-safe, pre-allocated circular ring buffer holding 300 historical telemetry samples.
class LensTelemetryRingBuffer : public QObject {
    Q_OBJECT

    Q_PROPERTY(int capacity READ capacity CONSTANT)
    Q_PROPERTY(int count READ count NOTIFY historyUpdated)
    Q_PROPERTY(double latestMetricScore READ latestMetricScore NOTIFY historyUpdated)
    Q_PROPERTY(double minMetricScore READ minMetricScore NOTIFY historyUpdated)
    Q_PROPERTY(double maxMetricScore READ maxMetricScore NOTIFY historyUpdated)
    Q_PROPERTY(double avgMetricScore READ avgMetricScore NOTIFY historyUpdated)
    Q_PROPERTY(int latestMetricLatencyUs READ latestMetricLatencyUs NOTIFY historyUpdated)
    Q_PROPERTY(int avgMetricLatencyUs READ avgMetricLatencyUs NOTIFY historyUpdated)
    Q_PROPERTY(int maxMetricLatencyUs READ maxMetricLatencyUs NOTIFY historyUpdated)
    Q_PROPERTY(int latestLoopLatencyUs READ latestLoopLatencyUs NOTIFY historyUpdated)
    Q_PROPERTY(int avgLoopLatencyUs READ avgLoopLatencyUs NOTIFY historyUpdated)

public:
    static constexpr std::size_t BufferCapacity { 300U };

    explicit LensTelemetryRingBuffer(QObject* parent = nullptr);
    ~LensTelemetryRingBuffer() override = default;

    LensTelemetryRingBuffer(const LensTelemetryRingBuffer&) = delete;
    LensTelemetryRingBuffer& operator=(const LensTelemetryRingBuffer&) = delete;
    LensTelemetryRingBuffer(LensTelemetryRingBuffer&&) = delete;
    LensTelemetryRingBuffer& operator=(LensTelemetryRingBuffer&&) = delete;

    /// @brief Push new telemetry sample into circular ring buffer (zero dynamic heap allocation).
    void appendSample(const TelemetrySample& sample) noexcept;

    /// @brief Reset all stored samples.
    Q_INVOKABLE void clear() noexcept;

    [[nodiscard]] int capacity() const noexcept;
    [[nodiscard]] int count() const noexcept;
    [[nodiscard]] double latestMetricScore() const noexcept;
    [[nodiscard]] double minMetricScore() const noexcept;
    [[nodiscard]] double maxMetricScore() const noexcept;
    [[nodiscard]] double avgMetricScore() const noexcept;
    [[nodiscard]] int latestMetricLatencyUs() const noexcept;
    [[nodiscard]] int avgMetricLatencyUs() const noexcept;
    [[nodiscard]] int maxMetricLatencyUs() const noexcept;
    [[nodiscard]] int latestLoopLatencyUs() const noexcept;
    [[nodiscard]] int avgLoopLatencyUs() const noexcept;

    /// @brief Retrieves normalized [0.0 .. 1.0] metric scores for QML strip-chart.
    [[nodiscard]] Q_INVOKABLE QVariantList getMetricHistory() const;

    /// @brief Retrieves normalized [0.0 .. 1.0] focus positions for QML strip-chart.
    [[nodiscard]] Q_INVOKABLE QVariantList getFocusHistory() const;

    /// @brief Retrieves normalized [0.0 .. 1.0] zoom positions for QML strip-chart.
    [[nodiscard]] Q_INVOKABLE QVariantList getZoomHistory() const;

    /// @brief Exports buffered telemetry to CSV file.
    [[nodiscard]] Q_INVOKABLE bool exportCsv(const QString& filePath) const;

signals:
    void historyUpdated();

private:
    mutable std::mutex m_mutex { };
    std::array<TelemetrySample, BufferCapacity> m_buffer { };
    std::size_t m_head { 0U };
    std::size_t m_count { 0U };
};

} // namespace LensApp

Q_DECLARE_METATYPE(LensApp::TelemetrySample)
