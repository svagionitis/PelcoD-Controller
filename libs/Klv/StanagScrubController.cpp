#include "StanagScrubController.h"
#include "Video/IVideoDecoder.h"

#include <algorithm>

namespace Klv {

StanagScrubController::StanagScrubController() = default;

void StanagScrubController::attachDecoder(Video::IVideoDecoder* decoder) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_decoder = decoder;
}

void StanagScrubController::setTimeIndex(TelemetryTimeIndex index)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_timeIndex = std::move(index);
    if (!m_timeIndex.isFinalized()) {
        m_timeIndex.finalize();
    }
    m_currentFrameIndex = 0U;
    m_stats.totalVideoFrames = m_timeIndex.videoFrameCount();
    m_stats.totalKeyframes = m_timeIndex.keyframeCount();
    m_stats.totalKlvPackets = m_timeIndex.klvPacketCount();
    m_stats.indexDurationSec = m_timeIndex.durationSeconds();
}

const TelemetryTimeIndex& StanagScrubController::timeIndex() const noexcept
{
    return m_timeIndex;
}

void StanagScrubController::setScrubCallback(ScrubFrameCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback = std::move(callback);
}

void StanagScrubController::setSeekMode(ScrubSeekMode mode) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_seekMode = mode;
}

ScrubSeekMode StanagScrubController::seekMode() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_seekMode;
}

void StanagScrubController::setInterpolate(bool enable) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_interpolateTelemetry = enable;
}

bool StanagScrubController::isInterpolateEnabled() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_interpolateTelemetry;
}

void StanagScrubController::setSyncTolerance(std::uint32_t toleranceMs) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_syncToleranceMs = toleranceMs;
}

bool StanagScrubController::scrubToSeconds(double seconds)
{
    std::uint64_t targetPts { 0U };
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        targetPts = m_timeIndex.secondsToPts(seconds);
    }
    return scrubToPts(targetPts);
}

bool StanagScrubController::scrubToPts(std::uint64_t pts)
{
    ScrubFrameCallback cb {};
    SynchronizedScrubFrame frame {};

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!executeSeekLocked(pts)) {
            return false;
        }
        cb = m_callback;
        frame = m_currentFrame;
    }

    if (cb) {
        cb(frame);
    }
    return true;
}

bool StanagScrubController::stepFrames(int frameDelta)
{
    std::uint64_t targetPts { 0U };
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_timeIndex.videoFrameCount() == 0U) {
            return false;
        }

        const int maxIdx = static_cast<int>(m_timeIndex.videoFrameCount()) - 1;
        const int desiredIdx = std::clamp(static_cast<int>(m_currentFrameIndex) + frameDelta, 0, maxIdx);
        m_currentFrameIndex = static_cast<std::uint32_t>(desiredIdx);

        // Approximate target PTS based on frame index
        const double frameTimeSec = (m_timeIndex.videoFrameCount() > 1U)
            ? (static_cast<double>(desiredIdx) * m_timeIndex.durationSeconds() / static_cast<double>(maxIdx))
            : 0.0;
        targetPts = m_timeIndex.secondsToPts(frameTimeSec);
    }

    return scrubToPts(targetPts);
}

SynchronizedScrubFrame StanagScrubController::currentFrame() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentFrame;
}

ScrubStats StanagScrubController::stats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

bool StanagScrubController::executeSeekLocked(std::uint64_t targetPts)
{
    if (m_timeIndex.empty()) {
        return false;
    }

    std::uint64_t framePts = targetPts;
    bool isKeyframe = false;

    const auto optVf = m_timeIndex.findVideoFrame(targetPts);
    if (optVf) {
        framePts = optVf->ptsTicks;
        isKeyframe = optVf->isKeyframe;
        m_currentFrameIndex = optVf->frameIndex;
    }

    // Command decoder seek if attached
    if (m_decoder) {
        const double seekSec = m_timeIndex.ptsToSeconds(framePts);
        static_cast<void>(m_decoder->seek(seekSec));
    }

    UasDatalinkMessage telemetry {};
    std::int64_t skewUs = 0;
    bool isInterpolated = false;

    // Direct exact PTS match check
    const auto optKlv = m_timeIndex.findTelemetry(framePts, 1000U); // 1 ms threshold for exact match
    if (optKlv && optKlv->message) {
        telemetry = *optKlv->message;
        const std::int64_t ptsDiff = static_cast<std::int64_t>(framePts) - static_cast<std::int64_t>(optKlv->ptsTicks);
        skewUs = (ptsDiff * 1000000) / 90000;
        isInterpolated = false;
        ++m_stats.cacheHitCount;
    } else if (m_interpolateTelemetry) {
        // Continuous interpolation between surrounding packets
        KlvIndexEntry before {};
        KlvIndexEntry after {};
        if (m_timeIndex.findTelemetryBounds(framePts, before, after)) {
            const std::uint64_t gapTicks = (after.ptsTicks >= before.ptsTicks)
                ? (after.ptsTicks - before.ptsTicks) : 0U;
            const std::uint64_t gapMs = (gapTicks * 1000U) / 90000U;

            if (before.message && after.message && before.ptsTicks < after.ptsTicks && gapMs <= 1000U) {
                telemetry = TelemetryInterpolator::interpolate(*before.message, before.ptsTicks,
                                                               *after.message, after.ptsTicks, framePts);
                skewUs = 0;
                isInterpolated = true;
                ++m_stats.cacheHitCount;
            } else if (before.message) {
                telemetry = *before.message;
                const std::int64_t diff = static_cast<std::int64_t>(framePts) - static_cast<std::int64_t>(before.ptsTicks);
                skewUs = (diff * 1000000) / 90000;
                isInterpolated = false;
                ++m_stats.cacheHitCount;
            } else if (after.message) {
                telemetry = *after.message;
                const std::int64_t diff = static_cast<std::int64_t>(framePts) - static_cast<std::int64_t>(after.ptsTicks);
                skewUs = (diff * 1000000) / 90000;
                isInterpolated = false;
                ++m_stats.cacheHitCount;
            }
        }
    } else {
        // Nearest packet within sync tolerance
        const auto optNearest = m_timeIndex.findTelemetry(framePts, static_cast<std::uint64_t>(m_syncToleranceMs) * 1000U);
        if (optNearest && optNearest->message) {
            telemetry = *optNearest->message;
            const std::int64_t ptsDiff = static_cast<std::int64_t>(framePts) - static_cast<std::int64_t>(optNearest->ptsTicks);
            skewUs = (ptsDiff * 1000000) / 90000;
            isInterpolated = false;
            ++m_stats.cacheHitCount;
        } else {
            ++m_stats.cacheMissCount;
        }
    }

    m_currentFrame.ptsTicks = framePts;
    m_currentFrame.timeSeconds = m_timeIndex.ptsToSeconds(framePts);
    m_currentFrame.utcTimestampUs = m_timeIndex.ptsToUtc(framePts);
    m_currentFrame.telemetry = telemetry;
    m_currentFrame.skewDeltaUs = skewUs;
    m_currentFrame.isKeyframe = isKeyframe;
    m_currentFrame.isInterpolated = isInterpolated;

    return true;
}

} // namespace Klv
