#include "PtsSyncManager.h"
#include "MpegTsKlvMuxer.h"
#include <algorithm>
#include <cmath>

namespace Klv {

PtsSyncManager::PtsSyncManager(const PtsSyncConfig& config,
                               std::shared_ptr<IMasterTimeBase> timeBase)
    : m_config(config)
    , m_timeBase(std::move(timeBase)) {
    if (!m_timeBase) {
        m_timeBase = std::make_shared<SystemMasterTimeBase>(m_config.timeBaseMode);
    }
}

void PtsSyncManager::setSyncCallback(SyncOutputCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback = std::move(callback);
}

void PtsSyncManager::bindMuxer(MpegTsKlvMuxer* muxer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_boundMuxer = muxer;
}

void PtsSyncManager::setConfig(const PtsSyncConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

PtsSyncConfig PtsSyncManager::config() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

PtsSyncStats PtsSyncManager::stats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void PtsSyncManager::resetStats() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats = PtsSyncStats {};
}

void PtsSyncManager::pushVideoAccessUnit(VideoAccessUnit au) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.totalFrames++;
    m_videoQueue.push_back(std::move(au));
    processQueuesLocked();
}

void PtsSyncManager::pushVideoFrame(const std::uint8_t* data,
                                    std::size_t size,
                                    std::uint64_t ptsUs,
                                    std::optional<std::uint64_t> dtsUs,
                                    bool isKeyframe) {
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

void PtsSyncManager::pushTelemetryMessage(UasDatalinkMessage message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.totalKlvPackets++;
    const std::uint64_t ts = message.precisionTimeStampUs.value_or(m_timeBase->nowUs());
    QueuedTelemetry q;
    q.timestampUs = ts;
    q.message = std::move(message);
    m_telemetryQueue.push_back(std::move(q));
    processQueuesLocked();
}

void PtsSyncManager::pushRawKlv(const std::uint8_t* data,
                                std::size_t size,
                                std::uint64_t timestampUs) {
    if (data == nullptr || size == 0U) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.totalKlvPackets++;
    QueuedTelemetry q;
    q.timestampUs = timestampUs;
    q.rawBytes.assign(data, data + size);
    m_telemetryQueue.push_back(std::move(q));
    processQueuesLocked();
}

std::size_t PtsSyncManager::findBestTelemetry(std::uint64_t targetUs) const noexcept {
    if (m_telemetryQueue.empty()) {
        return 0U;
    }

    std::size_t bestIdx = 0U;
    std::uint64_t minDiff = (targetUs > m_telemetryQueue[0].timestampUs)
                                ? (targetUs - m_telemetryQueue[0].timestampUs)
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

void PtsSyncManager::dispatchSynchronized(const SynchronizedAccessUnit& syncd) {
    if (m_boundMuxer != nullptr) {
        static_cast<void>(m_boundMuxer->muxSynchronizedFrame(syncd.videoAu, syncd.metadataMessage));
    }
    if (m_callback) {
        m_callback(syncd);
    }
}

void PtsSyncManager::processQueuesLocked() {
    if (m_videoQueue.empty()) {
        return;
    }

    const std::uint64_t bufferDepthUs = static_cast<std::uint64_t>(m_config.jitterBufferDepthMs) * 1000ULL;
    const std::uint64_t maxDeltaUs = static_cast<std::uint64_t>(m_config.maxSyncDeltaMs) * 1000ULL;

    while (!m_videoQueue.empty()) {
        const auto& frontVideo = m_videoQueue.front();

        // Check if we should hold this frame in jitter buffer to await closer telemetry
        const bool hasSufficientBuffer = (m_videoQueue.size() > 1U) ||
            (!m_telemetryQueue.empty() && m_telemetryQueue.back().timestampUs >= frontVideo.ptsUs) ||
            (m_videoQueue.back().ptsUs >= frontVideo.ptsUs + bufferDepthUs);

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

            const auto deltaUs = static_cast<std::int64_t>(telem.timestampUs) -
                                 static_cast<std::int64_t>(syncd.videoAu.ptsUs);
            const auto absDeltaUs = static_cast<std::uint64_t>(std::abs(deltaUs));

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
                    const auto clampDelta = (deltaUs > 0)
                                                ? static_cast<std::int64_t>(maxDeltaUs)
                                                : -static_cast<std::int64_t>(maxDeltaUs);
                    const auto clampedPts = static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(syncd.videoAu.ptsUs) + clampDelta);
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

        dispatchSynchronized(syncd);
    }
}

std::size_t PtsSyncManager::flush() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::size_t count = 0U;

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
            syncd.skewDeltaUs = static_cast<std::int64_t>(telem.timestampUs) -
                               static_cast<std::int64_t>(syncd.videoAu.ptsUs);

            for (std::size_t p = 0U; p <= bestIdx && !m_telemetryQueue.empty(); ++p) {
                m_telemetryQueue.pop_front();
            }
        }

        dispatchSynchronized(syncd);
        count++;
    }

    m_telemetryQueue.clear();
    return count;
}

void PtsSyncManager::reset() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_videoQueue.clear();
    m_telemetryQueue.clear();
    m_stats = PtsSyncStats {};
}

} // namespace Klv
