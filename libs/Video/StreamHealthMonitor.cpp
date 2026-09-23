/// @file StreamHealthMonitor.cpp
/// @brief Implementation of StreamHealthMonitor diagnostic state machine and pixel difference metrics.

#include "StreamHealthMonitor.h"

#include <algorithm>
#include <cmath>

namespace Video {

namespace {

    inline double getSteadyNowSeconds() noexcept
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        return std::chrono::duration<double>(now).count();
    }

    inline std::uint8_t extractLuminance(const std::uint8_t* p, PixelFormat format) noexcept
    {
        if (format == PixelFormat::BGR24) {
            return static_cast<std::uint8_t>((77U * p[2] + 150U * p[1] + 29U * p[0]) >> 8U);
        }
        return static_cast<std::uint8_t>((77U * p[0] + 150U * p[1] + 29U * p[2]) >> 8U);
    }

    inline int bytesPerPixel(PixelFormat /*format*/) noexcept
    {
        return 3;
    }

} // namespace

StreamHealthMonitor::StreamHealthMonitor(StreamHealthConfig config)
    : m_config(config)
{
}

void StreamHealthMonitor::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    ingestFrame(data, width, height, format, -1.0);
}

void StreamHealthMonitor::ingestFrame(
    const std::uint8_t* data, int width, int height, PixelFormat format, double timestampSec)
{
    if (data == nullptr || width <= 0 || height <= 0) {
        return;
    }

    const double nowSec = (timestampSec >= 0.0) ? timestampSec : getSteadyNowSeconds();

    const std::size_t gridStep = (m_config.sampleGridStep > 0U) ? m_config.sampleGridStep : 8U;
    const int bpp = bytesPerPixel(format);
    const int pitch = width * bpp;

    const int sampleCols = (width + static_cast<int>(gridStep) - 1) / static_cast<int>(gridStep);
    const int sampleRows = (height + static_cast<int>(gridStep) - 1) / static_cast<int>(gridStep);
    const std::size_t sampleCount = static_cast<std::size_t>(sampleCols * sampleRows);

    std::vector<std::uint8_t> currentFingerprint(sampleCount, 0U);

    double sumLuminance = 0.0;
    double sumSqLuminance = 0.0;
    std::size_t saturatedCount = 0U;
    double totalDiff = 0.0;

    std::size_t sIdx = 0U;
    for (int y = 0; y < height; y += static_cast<int>(gridStep)) {
        const std::uint8_t* rowPtr = data + y * pitch;
        for (int x = 0; x < width; x += static_cast<int>(gridStep)) {
            const std::uint8_t lum = extractLuminance(rowPtr + x * bpp, format);
            currentFingerprint[sIdx++] = lum;

            const double lumD = static_cast<double>(lum);
            sumLuminance += lumD;
            sumSqLuminance += lumD * lumD;
            if (lum >= 250U) {
                saturatedCount++;
            }
        }
    }

    HealthCallback cbToInvoke = nullptr;
    ReconnectCallback reconnToInvoke = nullptr;
    StreamHealthState oldState = StreamHealthState::Healthy;
    StreamHealthState newState = StreamHealthState::Healthy;
    StreamHealthMetrics metricsSnapshot {};

    {
        std::scoped_lock lock(m_mutex);

        const double countD = static_cast<double>(sampleCount);
        const double meanLum = (countD > 0.0) ? (sumLuminance / countD) : 0.0;
        const double varLum = (countD > 1.0)
            ? std::max(0.0, (sumSqLuminance - (sumLuminance * sumLuminance / countD)) / (countD - 1.0))
            : 0.0;

        m_metrics.meanLuminance = meanLum;
        m_metrics.luminanceVariance = varLum;
        m_metrics.totalFramesAnalyzed++;

        // Compute frame difference against previous fingerprint
        double meanDiff = 0.0;
        if (!m_previousFingerprint.empty() && m_previousFingerprint.size() == currentFingerprint.size()
            && m_previousFingerprintWidth == width && m_previousFingerprintHeight == height) {
            for (std::size_t i = 0U; i < sampleCount; ++i) {
                totalDiff += std::abs(
                    static_cast<double>(currentFingerprint[i]) - static_cast<double>(m_previousFingerprint[i]));
            }
            meanDiff = (countD > 0.0) ? (totalDiff / countD) : 0.0;
        } else {
            meanDiff = 255.0; // First frame or size changed
        }
        m_metrics.frameDifference = meanDiff;

        m_previousFingerprint = std::move(currentFingerprint);
        m_previousFingerprintWidth = width;
        m_previousFingerprintHeight = height;

        // Track FPS using rolling window
        m_recentFrameTimestamps.push_back(nowSec);
        if (m_recentFrameTimestamps.size() > kFpsHistoryCapacity) {
            m_recentFrameTimestamps.erase(m_recentFrameTimestamps.begin());
        }

        if (m_recentFrameTimestamps.size() >= 2U) {
            const double dt = m_recentFrameTimestamps.back() - m_recentFrameTimestamps.front();
            if (dt > 1e-4) {
                m_metrics.measuredFps = static_cast<double>(m_recentFrameTimestamps.size() - 1U) / dt;
            }
        } else {
            m_metrics.measuredFps = m_config.nominalFps;
        }

        const double previousFrameTimeSec = m_lastFrameTimeSec;
        m_lastFrameTimeSec = nowSec;
        m_metrics.secondsSinceLastFrame = 0.0;
        m_hasReceivedFirstFrame = true;

        // Diagnose candidate state
        StreamHealthState candidate = StreamHealthState::Healthy;

        if (meanLum <= m_config.blackoutLuminanceThreshold && varLum <= m_config.blackoutVarianceThreshold) {
            candidate = StreamHealthState::Blackout;
            m_isCandidateFrozen = false;
            m_metrics.frozenDurationSec = 0.0;
        } else if (meanLum >= m_config.whiteoutLuminanceThreshold
            && (static_cast<double>(saturatedCount) / countD) >= m_config.whiteoutSaturationRatio) {
            candidate = StreamHealthState::Whiteout;
            m_isCandidateFrozen = false;
            m_metrics.frozenDurationSec = 0.0;
        } else if (m_metrics.totalFramesAnalyzed > 1U && meanDiff <= m_config.freezeDifferenceThreshold) {
            // Frame is identical or within noise margin
            if (!m_isCandidateFrozen) {
                m_isCandidateFrozen = true;
                m_freezeStartTimeSec = m_hasReceivedFirstFrame ? previousFrameTimeSec : nowSec;
            }
            const double frozenSec = std::max(0.0, nowSec - m_freezeStartTimeSec);
            m_metrics.frozenDurationSec = frozenSec;

            if (frozenSec >= m_config.freezeDurationThresholdSec) {
                candidate = StreamHealthState::Frozen;
            } else if (m_metrics.state == StreamHealthState::Frozen) {
                candidate = StreamHealthState::Frozen; // Latch until motion breaks freeze
            }
        } else {
            // Motion detected; cancel freeze
            m_isCandidateFrozen = false;
            m_metrics.frozenDurationSec = 0.0;

            if (m_metrics.totalFramesAnalyzed >= 5U
                && m_metrics.measuredFps < (m_config.nominalFps * m_config.degradedFpsRatio)) {
                candidate = StreamHealthState::Degraded;
            } else {
                candidate = StreamHealthState::Healthy;
            }
        }

        const StreamHealthState stateBefore = m_metrics.state;
        evaluateStateTransitionLocked(candidate, nowSec);

        if (m_metrics.state != stateBefore) {
            cbToInvoke = m_healthCallback;
            oldState = stateBefore;
            newState = m_metrics.state;
            metricsSnapshot = m_metrics;

            if (m_config.autoReconnectOnFailure && m_reconnectCallback
                && (newState == StreamHealthState::Frozen || newState == StreamHealthState::SignalLoss
                    || newState == StreamHealthState::Blackout || newState == StreamHealthState::Whiteout)) {
                reconnToInvoke = m_reconnectCallback;
            }
        }
    }

    if (cbToInvoke) {
        cbToInvoke(oldState, newState, metricsSnapshot);
    }
    if (reconnToInvoke) {
        reconnToInvoke();
    }
}

void StreamHealthMonitor::checkTimeout(double nowSec)
{
    const double currentSec = (nowSec >= 0.0) ? nowSec : getSteadyNowSeconds();

    HealthCallback cbToInvoke = nullptr;
    ReconnectCallback reconnToInvoke = nullptr;
    StreamHealthState oldState = StreamHealthState::Healthy;
    StreamHealthState newState = StreamHealthState::Healthy;
    StreamHealthMetrics metricsSnapshot {};

    {
        std::scoped_lock lock(m_mutex);

        if (!m_hasReceivedFirstFrame) {
            return;
        }

        const double elapsed = std::max(0.0, currentSec - m_lastFrameTimeSec);
        m_metrics.secondsSinceLastFrame = elapsed;

        StreamHealthState candidate = m_metrics.state;
        if (elapsed >= m_config.signalLossTimeoutSec) {
            candidate = StreamHealthState::SignalLoss;
            m_metrics.measuredFps = 0.0;
        }

        oldState = m_metrics.state;
        evaluateStateTransitionLocked(candidate, currentSec);
        newState = m_metrics.state;

        if (newState != oldState) {
            cbToInvoke = m_healthCallback;
            metricsSnapshot = m_metrics;

            if (m_config.autoReconnectOnFailure && m_reconnectCallback && newState == StreamHealthState::SignalLoss) {
                reconnToInvoke = m_reconnectCallback;
            }
        }
    }

    if (cbToInvoke) {
        cbToInvoke(oldState, newState, metricsSnapshot);
    }
    if (reconnToInvoke) {
        reconnToInvoke();
    }
}

void StreamHealthMonitor::evaluateStateTransitionLocked(StreamHealthState candidateState, double /*nowSec*/)
{
    if (candidateState == m_metrics.state) {
        return;
    }

    // State transition detected
    switch (candidateState) {
    case StreamHealthState::Frozen:
        m_metrics.freezeCount++;
        break;
    case StreamHealthState::SignalLoss:
        m_metrics.signalLossCount++;
        break;
    case StreamHealthState::Blackout:
        m_metrics.blackoutCount++;
        break;
    case StreamHealthState::Whiteout:
        m_metrics.whiteoutCount++;
        break;
    case StreamHealthState::Healthy:
    case StreamHealthState::Degraded:
        break;
    }

    m_metrics.state = candidateState;
}

StreamHealthState StreamHealthMonitor::getState() const
{
    std::scoped_lock lock(m_mutex);
    return m_metrics.state;
}

StreamHealthMetrics StreamHealthMonitor::getMetrics() const
{
    std::scoped_lock lock(m_mutex);
    return m_metrics;
}

void StreamHealthMonitor::setConfig(const StreamHealthConfig& config)
{
    std::scoped_lock lock(m_mutex);
    m_config = config;
}

StreamHealthConfig StreamHealthMonitor::getConfig() const
{
    std::scoped_lock lock(m_mutex);
    return m_config;
}

void StreamHealthMonitor::setHealthCallback(HealthCallback callback)
{
    std::scoped_lock lock(m_mutex);
    m_healthCallback = std::move(callback);
}

void StreamHealthMonitor::setReconnectCallback(ReconnectCallback callback)
{
    std::scoped_lock lock(m_mutex);
    m_reconnectCallback = std::move(callback);
}

void StreamHealthMonitor::reset()
{
    std::scoped_lock lock(m_mutex);
    m_metrics = StreamHealthMetrics {};
    m_previousFingerprint.clear();
    m_previousFingerprintWidth = 0;
    m_previousFingerprintHeight = 0;
    m_lastFrameTimeSec = 0.0;
    m_freezeStartTimeSec = 0.0;
    m_hasReceivedFirstFrame = false;
    m_isCandidateFrozen = false;
    m_recentFrameTimestamps.clear();
}

std::string StreamHealthMonitor::stateToString(StreamHealthState state) noexcept
{
    switch (state) {
    case StreamHealthState::Healthy:
        return "Healthy";
    case StreamHealthState::Degraded:
        return "Degraded";
    case StreamHealthState::Frozen:
        return "Frozen";
    case StreamHealthState::SignalLoss:
        return "SignalLoss";
    case StreamHealthState::Blackout:
        return "Blackout";
    case StreamHealthState::Whiteout:
        return "Whiteout";
    }
    return "Unknown";
}

} // namespace Video
