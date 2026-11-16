/// @file LensTelemetryRingBuffer.cpp
/// @brief Implementation of circular history buffer for optical telemetry and latencies.

#include "LensTelemetryRingBuffer.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <mutex>

#include <QFile>
#include <QTextStream>

namespace LensApp {

LensTelemetryRingBuffer::LensTelemetryRingBuffer(QObject* parent)
    : QObject(parent)
{
}

void LensTelemetryRingBuffer::appendSample(const TelemetrySample& sample) noexcept
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buffer[m_head] = sample;
        m_head = (m_head + 1U) % BufferCapacity;
        if (m_count < BufferCapacity) {
            m_count += 1U;
        }
    }

    emit historyUpdated();
}

void LensTelemetryRingBuffer::clear() noexcept
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_head = 0U;
        m_count = 0U;
    }

    emit historyUpdated();
}

int LensTelemetryRingBuffer::capacity() const noexcept
{
    return static_cast<int>(BufferCapacity);
}

int LensTelemetryRingBuffer::count() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_count);
}

double LensTelemetryRingBuffer::latestMetricScore() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_count == 0U) {
        return 0.0;
    }
    const std::size_t lastIdx = (m_head + BufferCapacity - 1U) % BufferCapacity;
    return m_buffer[lastIdx].metricScore;
}

double LensTelemetryRingBuffer::minMetricScore() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_count == 0U) {
        return 0.0;
    }
    double minVal = m_buffer[0].metricScore;
    const std::size_t start = (m_count == BufferCapacity) ? m_head : 0U;
    for (std::size_t i = 0U; i < m_count; ++i) {
        const std::size_t idx = (start + i) % BufferCapacity;
        minVal = std::min(minVal, m_buffer[idx].metricScore);
    }
    return minVal;
}

double LensTelemetryRingBuffer::maxMetricScore() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_count == 0U) {
        return 0.0;
    }
    double maxVal = m_buffer[0].metricScore;
    const std::size_t start = (m_count == BufferCapacity) ? m_head : 0U;
    for (std::size_t i = 0U; i < m_count; ++i) {
        const std::size_t idx = (start + i) % BufferCapacity;
        maxVal = std::max(maxVal, m_buffer[idx].metricScore);
    }
    return maxVal;
}

double LensTelemetryRingBuffer::avgMetricScore() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_count == 0U) {
        return 0.0;
    }
    double sum = 0.0;
    const std::size_t start = (m_count == BufferCapacity) ? m_head : 0U;
    for (std::size_t i = 0U; i < m_count; ++i) {
        const std::size_t idx = (start + i) % BufferCapacity;
        sum += m_buffer[idx].metricScore;
    }
    return sum / static_cast<double>(m_count);
}

int LensTelemetryRingBuffer::latestMetricLatencyUs() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_count == 0U) {
        return 0;
    }
    const std::size_t lastIdx = (m_head + BufferCapacity - 1U) % BufferCapacity;
    return static_cast<int>(m_buffer[lastIdx].metricLatencyUs);
}

int LensTelemetryRingBuffer::avgMetricLatencyUs() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_count == 0U) {
        return 0;
    }
    std::uint64_t sum = 0U;
    const std::size_t start = (m_count == BufferCapacity) ? m_head : 0U;
    for (std::size_t i = 0U; i < m_count; ++i) {
        const std::size_t idx = (start + i) % BufferCapacity;
        sum += static_cast<std::uint64_t>(m_buffer[idx].metricLatencyUs);
    }
    return static_cast<int>(sum / static_cast<std::uint64_t>(m_count));
}

int LensTelemetryRingBuffer::maxMetricLatencyUs() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_count == 0U) {
        return 0;
    }
    std::uint32_t maxVal = 0U;
    const std::size_t start = (m_count == BufferCapacity) ? m_head : 0U;
    for (std::size_t i = 0U; i < m_count; ++i) {
        const std::size_t idx = (start + i) % BufferCapacity;
        maxVal = std::max(maxVal, m_buffer[idx].metricLatencyUs);
    }
    return static_cast<int>(maxVal);
}

int LensTelemetryRingBuffer::latestLoopLatencyUs() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_count == 0U) {
        return 0;
    }
    const std::size_t lastIdx = (m_head + BufferCapacity - 1U) % BufferCapacity;
    return static_cast<int>(m_buffer[lastIdx].loopLatencyUs);
}

int LensTelemetryRingBuffer::avgLoopLatencyUs() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_count == 0U) {
        return 0;
    }
    std::uint64_t sum = 0U;
    const std::size_t start = (m_count == BufferCapacity) ? m_head : 0U;
    for (std::size_t i = 0U; i < m_count; ++i) {
        const std::size_t idx = (start + i) % BufferCapacity;
        sum += static_cast<std::uint64_t>(m_buffer[idx].loopLatencyUs);
    }
    return static_cast<int>(sum / static_cast<std::uint64_t>(m_count));
}

QVariantList LensTelemetryRingBuffer::getMetricHistory() const
{
    QVariantList result;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_count == 0U) {
        return result;
    }

    result.reserve(static_cast<int>(m_count));

    double minVal = m_buffer[0].metricScore;
    double maxVal = m_buffer[0].metricScore;
    const std::size_t start = (m_count == BufferCapacity) ? m_head : 0U;

    for (std::size_t i = 0U; i < m_count; ++i) {
        const std::size_t idx = (start + i) % BufferCapacity;
        const double val = m_buffer[idx].metricScore;
        minVal = std::min(minVal, val);
        maxVal = std::max(maxVal, val);
    }

    const double range = maxVal - minVal;
    for (std::size_t i = 0U; i < m_count; ++i) {
        const std::size_t idx = (start + i) % BufferCapacity;
        double norm = 0.5;
        if (range > 1e-6) {
            norm = std::clamp((m_buffer[idx].metricScore - minVal) / range, 0.0, 1.0);
        }
        result.append(norm);
    }
    return result;
}

QVariantList LensTelemetryRingBuffer::getFocusHistory() const
{
    QVariantList result;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_count == 0U) {
        return result;
    }

    result.reserve(static_cast<int>(m_count));
    const std::size_t start = (m_count == BufferCapacity) ? m_head : 0U;

    for (std::size_t i = 0U; i < m_count; ++i) {
        const std::size_t idx = (start + i) % BufferCapacity;
        const double norm = static_cast<double>(m_buffer[idx].focusPosition) / 65535.0;
        result.append(std::clamp(norm, 0.0, 1.0));
    }
    return result;
}

QVariantList LensTelemetryRingBuffer::getZoomHistory() const
{
    QVariantList result;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_count == 0U) {
        return result;
    }

    result.reserve(static_cast<int>(m_count));
    const std::size_t start = (m_count == BufferCapacity) ? m_head : 0U;

    for (std::size_t i = 0U; i < m_count; ++i) {
        const std::size_t idx = (start + i) % BufferCapacity;
        const double norm = static_cast<double>(m_buffer[idx].zoomPosition) / 65535.0;
        result.append(std::clamp(norm, 0.0, 1.0));
    }
    return result;
}

bool LensTelemetryRingBuffer::exportCsv(const QString& filePath) const
{
    if (filePath.trimmed().isEmpty()) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    QTextStream out(&file);

    std::lock_guard<std::mutex> lock(m_mutex);

    // CSV Header
    out << "TimestampNs,FrameIndex,Channel,FocusPosition,ZoomPosition,MetricScore,AfState,"
           "LimitFlags,DecodeLatencyUs,MetricLatencyUs,LoopLatencyUs,Fps\n";

    const std::size_t start = (m_count == BufferCapacity) ? m_head : 0U;
    for (std::size_t i = 0U; i < m_count; ++i) {
        const std::size_t idx = (start + i) % BufferCapacity;
        const auto& s = m_buffer[idx];
        out << s.timestampNs << ',' << s.frameIndex << ',' << static_cast<int>(s.channel) << ',' << s.focusPosition
            << ',' << s.zoomPosition << ',' << s.metricScore << ',' << static_cast<int>(s.afState) << ','
            << static_cast<int>(s.limitFlags) << ',' << s.decodeLatencyUs << ',' << s.metricLatencyUs << ','
            << s.loopLatencyUs << ',' << s.currentFps << '\n';
    }

    file.close();
    return true;
}

} // namespace LensApp
