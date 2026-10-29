#include "PtsSyncManager.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "MpegTsKlvMuxer.h"

namespace Klv {

PtsSyncManager::PtsSyncManager(const PtsSyncConfig& config, std::shared_ptr<IMasterTimeBase> timeBase)
    : m_config(config)
    , m_timeBase(std::move(timeBase))
{
    if (!m_timeBase) {
        m_timeBase = std::make_shared<SystemMasterTimeBase>(m_config.timeBaseMode);
    }
}

void PtsSyncManager::setSyncCallback(SyncOutputCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback = std::move(callback);
}

void PtsSyncManager::bindMuxer(MpegTsKlvMuxer* muxer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_boundMuxer = muxer;
}

void PtsSyncManager::setConfig(const PtsSyncConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

PtsSyncConfig PtsSyncManager::config() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

PtsSyncStats PtsSyncManager::stats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void PtsSyncManager::resetStats()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats = PtsSyncStats {};
}

void PtsSyncManager::pushVideoAccessUnit(VideoAccessUnit au)
{
    std::vector<SynchronizedAccessUnit> toDispatch;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stats.totalFrames++;

        if (m_config.maxQueueDepth > 0U && m_videoQueue.size() >= m_config.maxQueueDepth) {
            m_videoQueue.pop_front();
            m_stats.droppedPackets++;
        }

        m_videoQueue.push_back(std::move(au));
        processQueuesLocked(toDispatch);
    }
    dispatchBatch(toDispatch);
}

void PtsSyncManager::pushVideoFrame(const std::uint8_t* data, std::size_t size, std::uint64_t ptsUs,
    std::optional<std::uint64_t> dtsUs, bool isKeyframe)
{
    VideoAccessUnit au;
    au.codec = VideoCodec::H264;
    au.ptsUs = ptsUs;
    au.dtsUs = dtsUs.value_or(ptsUs);
    au.hasDts = dtsUs.has_value();
    au.isKeyframe = isKeyframe;
    au.primarySliceType = isKeyframe ? VideoSliceType::I : VideoSliceType::P;
    if (data != nullptr && size > 0U) {
        au.data.assign(data, data + size);
    }
    pushVideoAccessUnit(std::move(au));
}

void PtsSyncManager::pushTelemetryMessage(UasDatalinkMessage message)
{
    std::vector<SynchronizedAccessUnit> toDispatch;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stats.totalKlvPackets++;

        if (m_config.maxQueueDepth > 0U && m_telemetryQueue.size() >= m_config.maxQueueDepth) {
            m_telemetryQueue.pop_front();
            m_stats.droppedPackets++;
        }

        const std::uint64_t ts = message.precisionTimeStampUs.value_or(m_timeBase->nowUs());
        QueuedTelemetry q;
        q.timestampUs = ts;
        q.message = std::move(message);
        m_telemetryQueue.push_back(std::move(q));
        processQueuesLocked(toDispatch);
    }
    dispatchBatch(toDispatch);
}

void PtsSyncManager::pushRawKlv(const std::uint8_t* data, std::size_t size, std::uint64_t timestampUs)
{
    if (data == nullptr || size == 0U) {
        return;
    }
    std::vector<SynchronizedAccessUnit> toDispatch;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stats.totalKlvPackets++;

        if (m_config.maxQueueDepth > 0U && m_telemetryQueue.size() >= m_config.maxQueueDepth) {
            m_telemetryQueue.pop_front();
            m_stats.droppedPackets++;
        }

        QueuedTelemetry q;
        q.timestampUs = timestampUs;
        q.rawBytes.assign(data, data + size);
        m_telemetryQueue.push_back(std::move(q));
        processQueuesLocked(toDispatch);
    }
    dispatchBatch(toDispatch);
}

std::size_t PtsSyncManager::findBestTelemetry(std::uint64_t targetUs) const noexcept
{
    if (m_telemetryQueue.empty()) {
        return 0U;
    }

    std::size_t bestIdx = 0U;
    std::uint64_t minDiff = (targetUs > m_telemetryQueue[0].timestampUs) ? (targetUs - m_telemetryQueue[0].timestampUs)
                                                                         : (m_telemetryQueue[0].timestampUs - targetUs);

    for (std::size_t i = 1U; i < m_telemetryQueue.size(); ++i) {
        const std::uint64_t curTs = m_telemetryQueue[i].timestampUs;
        const std::uint64_t diff = (targetUs > curTs) ? (targetUs - curTs) : (curTs - targetUs);
        if (diff < minDiff) {
            minDiff = diff;
            bestIdx = i;
        }
    }

    return bestIdx;
}

void PtsSyncManager::dispatchSynchronized(const SynchronizedAccessUnit& syncd)
{
    if (m_boundMuxer != nullptr) {
        static_cast<void>(m_boundMuxer->muxSynchronizedFrame(syncd.videoAu, syncd.metadataMessage));
    }
    if (m_callback) {
        m_callback(syncd);
    }
}

void PtsSyncManager::dispatchBatch(const std::vector<SynchronizedAccessUnit>& batch)
{
    for (const auto& syncd : batch) {
        dispatchSynchronized(syncd);
    }
}

void PtsSyncManager::processQueuesLocked(std::vector<SynchronizedAccessUnit>& outBatch)
{
    if (m_videoQueue.empty()) {
        return;
    }

    const std::uint64_t bufferDepthUs = static_cast<std::uint64_t>(m_config.jitterBufferDepthMs) * 1000ULL;
    const std::uint64_t maxDeltaUs = static_cast<std::uint64_t>(m_config.maxSyncDeltaMs) * 1000ULL;

    while (!m_videoQueue.empty()) {
        const auto& frontVideo = m_videoQueue.front();

        // Check if we should hold this frame in jitter buffer to await closer telemetry
        const bool hasSufficientBuffer = (m_videoQueue.size() > 1U)
            || (!m_telemetryQueue.empty() && m_telemetryQueue.back().timestampUs >= frontVideo.ptsUs)
            || (m_videoQueue.back().ptsUs >= frontVideo.ptsUs + bufferDepthUs);

        if (!hasSufficientBuffer && m_telemetryQueue.empty()) {
            break;
        }

        VideoAccessUnit videoAu = std::move(m_videoQueue.front());
        m_videoQueue.pop_front();

        SynchronizedAccessUnit syncd;
        syncd.videoAu = std::move(videoAu);

        if (!m_telemetryQueue.empty()) {
            std::size_t bestIdx = findBestTelemetry(syncd.videoAu.ptsUs);
            const auto& telem = m_telemetryQueue[bestIdx];

            const bool telemAhead = (telem.timestampUs >= syncd.videoAu.ptsUs);
            const std::uint64_t absDeltaUs
                = telemAhead ? (telem.timestampUs - syncd.videoAu.ptsUs) : (syncd.videoAu.ptsUs - telem.timestampUs);
            constexpr auto kMaxInt64AsUint64 = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
            const auto deltaUs = telemAhead ? static_cast<std::int64_t>(std::min(absDeltaUs, kMaxInt64AsUint64))
                                            : -static_cast<std::int64_t>(std::min(absDeltaUs, kMaxInt64AsUint64));

            syncd.skewDeltaUs = deltaUs;
            syncd.metadataMessage = telem.message;
            syncd.rawKlvPacket = telem.rawBytes;

            bool wasClamped = false;

            if (absDeltaUs > maxDeltaUs) {
                m_stats.isCompliant = false;

                if (m_config.syncPolicy == PtsSyncPolicy::NearestFrame) {
                    if (syncd.metadataMessage.has_value() && !m_config.preserveSensorTimestamp) {
                        syncd.metadataMessage->precisionTimeStampUs = syncd.videoAu.ptsUs;
                    }
                    wasClamped = true;
                    m_stats.clampedPackets++;
                } else if (m_config.syncPolicy == PtsSyncPolicy::StrictClamp) {
                    std::uint64_t clampedPts = syncd.videoAu.ptsUs;
                    if (telemAhead) {
                        clampedPts = (UINT64_MAX - syncd.videoAu.ptsUs >= maxDeltaUs)
                            ? (syncd.videoAu.ptsUs + maxDeltaUs)
                            : UINT64_MAX;
                    } else {
                        clampedPts = (syncd.videoAu.ptsUs >= maxDeltaUs) ? (syncd.videoAu.ptsUs - maxDeltaUs) : 0ULL;
                    }
                    if (syncd.metadataMessage.has_value() && !m_config.preserveSensorTimestamp) {
                        syncd.metadataMessage->precisionTimeStampUs = clampedPts;
                    }
                    wasClamped = true;
                    m_stats.clampedPackets++;
                } else if (m_config.syncPolicy == PtsSyncPolicy::DropStale) {
                    syncd.metadataMessage.reset();
                    syncd.rawKlvPacket.clear();
                    m_stats.droppedPackets++;
                }
            } else {
                m_stats.isCompliant = true;
            }

            syncd.wasClamped = wasClamped;

            // Update rolling skew metrics
            const double skewMs = static_cast<double>(deltaUs) / 1000.0;
            m_stats.currentSkewMs = skewMs;
            m_stats.averageSkewMs = (0.95 * m_stats.averageSkewMs) + (0.05 * skewMs);
            m_stats.maxSkewMs = std::max(m_stats.maxSkewMs, std::abs(skewMs));

            // Purge consumed and earlier telemetry
            for (std::size_t p = 0U; p <= bestIdx && !m_telemetryQueue.empty(); ++p) {
                m_telemetryQueue.pop_front();
            }
        }

        outBatch.push_back(std::move(syncd));
    }
}

std::size_t PtsSyncManager::flush()
{
    std::vector<SynchronizedAccessUnit> toDispatch;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        while (!m_videoQueue.empty()) {
            VideoAccessUnit videoAu = std::move(m_videoQueue.front());
            m_videoQueue.pop_front();

            SynchronizedAccessUnit syncd;
            syncd.videoAu = std::move(videoAu);

            if (!m_telemetryQueue.empty()) {
                const std::size_t bestIdx = findBestTelemetry(syncd.videoAu.ptsUs);
                const auto& telem = m_telemetryQueue[bestIdx];

                syncd.metadataMessage = telem.message;
                syncd.rawKlvPacket = telem.rawBytes;

                const bool telemAhead = (telem.timestampUs >= syncd.videoAu.ptsUs);
                const std::uint64_t absDeltaUs = telemAhead ? (telem.timestampUs - syncd.videoAu.ptsUs)
                                                            : (syncd.videoAu.ptsUs - telem.timestampUs);
                constexpr auto kMaxInt64AsUint64 = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
                const auto deltaUs = telemAhead ? static_cast<std::int64_t>(std::min(absDeltaUs, kMaxInt64AsUint64))
                                                : -static_cast<std::int64_t>(std::min(absDeltaUs, kMaxInt64AsUint64));
                syncd.skewDeltaUs = deltaUs;

                for (std::size_t p = 0U; p <= bestIdx && !m_telemetryQueue.empty(); ++p) {
                    m_telemetryQueue.pop_front();
                }
            }

            toDispatch.push_back(std::move(syncd));
        }

        m_telemetryQueue.clear();
    }

    const std::size_t count = toDispatch.size();
    dispatchBatch(toDispatch);
    return count;
}

void PtsSyncManager::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_videoQueue.clear();
    m_telemetryQueue.clear();
    m_stats = PtsSyncStats {};
}

} // namespace Klv
