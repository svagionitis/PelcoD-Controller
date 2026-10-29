/// @file StanagBroadcastEngine.cpp
/// @brief Master orchestrator coordinating live video, KLV telemetry, and MPEG-TS broadcasting.

#include "StanagBroadcastEngine.h"
#include "CbrRatePacer.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

#include "KlvTypes.h"
#include "MasterTimeBase.h"
#include "MpegTsKlvMuxer.h"
#include "MpegTsMuxerTypes.h"
#include "MpegTsNetworkPublisher.h"
#include "PtsSyncManager.h"
#include "PtsSyncTypes.h"
#include "TsNetworkTypes.h"
#include "VideoNaluParser.h"
#include "VideoTypes.h"

namespace Klv {

/// @class StanagBroadcastEngine::Impl
/// @brief Private implementation holding worker thread, queues, and orchestrator state.
class StanagBroadcastEngine::Impl {
public:
    explicit Impl(const StanagBroadcastConfig& config)
        : m_config(config)
        , m_muxer(config.muxerConfig)
        , m_syncManager(PtsSyncConfig { })
        , m_naluParser(config.muxerConfig.videoCodec)
    {
        // Configure PTS sync manager to clamp within <= 50 ms
        PtsSyncConfig syncCfg { };
        syncCfg.maxSyncDeltaMs = config.muxerConfig.maxSyncDeltaMs;
        syncCfg.syncPolicy = PtsSyncPolicy::NearestFrame;
        m_syncManager.setConfig(syncCfg);

        // Bind internal MpegTsKlvMuxer packet callback
        m_muxer.setPacketCallback(
            [this](const std::uint8_t* packet, std::size_t size) { handleTsPacket(packet, size); });

        // Bind internal PtsSyncManager synchronized output callback
        m_syncManager.setSyncCallback([this](const SynchronizedAccessUnit& syncd) { handleSynchronizedUnit(syncd); });

        // Bind internal VideoNaluParser Access Unit callback
        m_naluParser.setUnitCallback([this](const VideoAccessUnit& au) { enqueueVideoUnit(au); });
    }

    ~Impl()
    {
        stop();
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    [[nodiscard]] bool start()
    {
        if (m_running.load()) {
            return true;
        }

        if (m_config.enableNetworkPublishing) {
            m_publisher = std::make_unique<Transport::MpegTsNetworkPublisher>(m_config.networkConfig);
            if (!m_publisher->open()) {
                m_publisher.reset();
                return false;
            }
        }

        if (m_config.enableCbrPacing) {
            Transport::CbrPacerConfig pacerCfg = m_config.cbrConfig;
            pacerCfg.autoStartWorker = true;
            m_pacer = std::make_unique<Transport::CbrRatePacer>(
                pacerCfg, [this](const std::uint8_t* pkt, std::size_t sz) { dispatchPacedPacket(pkt, sz); });
        }

        m_running.store(true);
        m_lastBitrateCalcTime = std::chrono::steady_clock::now();
        m_bytesSinceLastCalc = 0U;

        m_workerThread = std::thread(&Impl::workerLoop, this);
        return true;
    }

    void stop()
    {
        if (!m_running.exchange(false)) {
            return;
        }

        m_cv.notify_all();

        if (m_workerThread.joinable()) {
            m_workerThread.join();
        }

        if (m_pacer) {
            m_pacer->stopWorker();
            m_pacer.reset();
        }

        if (m_publisher) {
            m_publisher->close();
            m_publisher.reset();
        }
    }

    [[nodiscard]] bool isRunning() const noexcept
    {
        return m_running.load();
    }

    void setPacketCallback(TsPacketCallback callback)
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_packetCallback = std::move(callback);
    }

    bool pushVideoChunk(const std::uint8_t* data, std::size_t size, std::uint64_t timestampUs)
    {
        if (data == nullptr || size == 0U) {
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.videoFramesIngested++;
        }

        const std::size_t parsedUnits = m_naluParser.pushChunk(data, size, timestampUs);
        return parsedUnits > 0U || m_running.load();
    }

    bool pushVideoAccessUnit(VideoAccessUnit au)
    {
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.videoFramesIngested++;
        }
        return enqueueVideoUnit(std::move(au));
    }

    bool pushTelemetry(UasDatalinkMessage message)
    {
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.telemetryIngested++;
        }

        QueuedTelemetryItem item { };
        item.timestampUs = message.precisionTimeStampUs.value_or(0U);
        item.message = std::move(message);

        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_telemetryQueue.size() >= m_config.maxTelemetryQueueSize) {
            std::lock_guard<std::mutex> statsLock(m_statsMutex);
            m_stats.droppedTelemetry++;
            return false;
        }

        m_telemetryQueue.push_back(std::move(item));
        m_cv.notify_one();
        return true;
    }

    bool pushRawKlv(const std::uint8_t* data, std::size_t size, std::uint64_t timestampUs)
    {
        if (data == nullptr || size == 0U) {
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.telemetryIngested++;
        }

        QueuedTelemetryItem item { };
        item.timestampUs = timestampUs;
        item.rawBytes.assign(data, data + size);

        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_telemetryQueue.size() >= m_config.maxTelemetryQueueSize) {
            std::lock_guard<std::mutex> statsLock(m_statsMutex);
            m_stats.droppedTelemetry++;
            return false;
        }

        m_telemetryQueue.push_back(std::move(item));
        m_cv.notify_one();
        return true;
    }

    void flush()
    {
        // Flush video parser accumulation buffer
        static_cast<void>(m_naluParser.flush());

        // Process all items remaining in queues and flush sync manager
        std::lock_guard<std::mutex> procLock(m_processMutex);
        processQueues();
        static_cast<void>(m_syncManager.flush());

        if (m_pacer) {
            m_pacer->flush();
        }
    }

    [[nodiscard]] StanagBroadcastStats stats() const
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        auto currentStats = m_stats;
        currentStats.isNetworkOpen = (m_publisher != nullptr && m_publisher->isOpen());

        const auto syncStats = m_syncManager.stats();
        currentStats.averagePtsSkewMs = syncStats.averageSkewMs;

        if (m_config.enableCbrPacing && m_pacer) {
            currentStats.nullPacketsInjected = m_pacer->nullPacketsInjected();
            currentStats.isCbrActive = true;
        }

        return currentStats;
    }

private:
    struct QueuedTelemetryItem {
        std::uint64_t timestampUs { 0U };
        std::optional<UasDatalinkMessage> message { };
        std::vector<std::uint8_t> rawBytes { };
    };

    StanagBroadcastConfig m_config { };
    std::atomic<bool> m_running { false };
    std::thread m_workerThread { };

    std::mutex m_queueMutex { };
    std::mutex m_processMutex { };
    std::condition_variable m_cv { };
    std::deque<VideoAccessUnit> m_videoQueue { };
    std::deque<QueuedTelemetryItem> m_telemetryQueue { };

    mutable std::mutex m_callbackMutex { };
    TsPacketCallback m_packetCallback { };

    mutable std::mutex m_statsMutex { };
    StanagBroadcastStats m_stats { };
    std::chrono::steady_clock::time_point m_lastBitrateCalcTime { };
    std::size_t m_bytesSinceLastCalc { 0U };

    MpegTsKlvMuxer m_muxer;
    PtsSyncManager m_syncManager;
    VideoNaluParser m_naluParser;
    std::unique_ptr<Transport::MpegTsNetworkPublisher> m_publisher { };
    std::unique_ptr<Transport::CbrRatePacer> m_pacer { };

    bool enqueueVideoUnit(VideoAccessUnit au)
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_videoQueue.size() >= m_config.maxVideoQueueSize) {
            std::lock_guard<std::mutex> statsLock(m_statsMutex);
            m_stats.droppedVideoFrames++;
            return false;
        }

        m_videoQueue.push_back(std::move(au));
        m_cv.notify_one();
        return true;
    }

    void workerLoop()
    {
        while (m_running.load()) {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_cv.wait_for(lock, std::chrono::milliseconds(10),
                [this]() { return !m_running.load() || !m_videoQueue.empty() || !m_telemetryQueue.empty(); });

            if (!m_running.load() && m_videoQueue.empty() && m_telemetryQueue.empty()) {
                break;
            }

            lock.unlock();
            {
                std::lock_guard<std::mutex> procLock(m_processMutex);
                processQueues();
            }
        }

        // Drain queues upon exit
        {
            std::lock_guard<std::mutex> procLock(m_processMutex);
            processQueues();
            static_cast<void>(m_syncManager.flush());
        }
    }

    void processQueues()
    {
        std::deque<VideoAccessUnit> localVideo;
        std::deque<QueuedTelemetryItem> localTelem;

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            localVideo.swap(m_videoQueue);
            localTelem.swap(m_telemetryQueue);
        }

        for (auto& telem : localTelem) {
            if (telem.message.has_value()) {
                m_syncManager.pushTelemetryMessage(std::move(*telem.message));
            } else if (!telem.rawBytes.empty()) {
                m_syncManager.pushRawKlv(telem.rawBytes.data(), telem.rawBytes.size(), telem.timestampUs);
            }
        }

        for (auto& videoAu : localVideo) {
            m_syncManager.pushVideoAccessUnit(std::move(videoAu));
        }
    }

    void handleSynchronizedUnit(const SynchronizedAccessUnit& syncd)
    {
        {
            std::lock_guard<std::mutex> statsLock(m_statsMutex);
            m_stats.videoFramesProcessed++;
            if (syncd.metadataMessage.has_value() || !syncd.rawKlvPacket.empty()) {
                m_stats.telemetryProcessed++;
            }
        }

        if (syncd.metadataMessage.has_value()) {
            static_cast<void>(m_muxer.muxSynchronizedFrame(syncd.videoAu, syncd.metadataMessage));
        } else if (!syncd.rawKlvPacket.empty()) {
            static_cast<void>(
                m_muxer.muxKlvPacket(syncd.rawKlvPacket.data(), syncd.rawKlvPacket.size(), syncd.videoAu.ptsUs));
            static_cast<void>(m_muxer.muxVideoAccessUnit(syncd.videoAu));
        } else {
            static_cast<void>(m_muxer.muxVideoAccessUnit(syncd.videoAu));
        }
    }

    void handleTsPacket(const std::uint8_t* packet, std::size_t size)
    {
        if (packet == nullptr || size != MpegTsKlvMuxer::kTsPacketSize) {
            return;
        }

        if (m_config.enableCbrPacing && m_pacer) {
            static_cast<void>(m_pacer->submitTsPackets(packet, 1U));
            return;
        }

        dispatchPacedPacket(packet, size);
    }

    void dispatchPacedPacket(const std::uint8_t* packet, std::size_t size)
    {
        if (packet == nullptr || size != MpegTsKlvMuxer::kTsPacketSize) {
            return;
        }

        {
            std::lock_guard<std::mutex> statsLock(m_statsMutex);
            m_stats.tsPacketsEmitted++;
            m_bytesSinceLastCalc += size;

            if (m_config.enableCbrPacing && m_pacer) {
                m_stats.nullPacketsInjected = m_pacer->nullPacketsInjected();
                m_stats.isCbrActive = true;
            }

            const auto now = std::chrono::steady_clock::now();
            const auto elapsedMs
                = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastBitrateCalcTime).count();
            if (elapsedMs >= 500) {
                const double bits = static_cast<double>(m_bytesSinceLastCalc) * 8.0;
                m_stats.currentBitrateKbps = bits / static_cast<double>(elapsedMs);
                m_bytesSinceLastCalc = 0U;
                m_lastBitrateCalcTime = now;
            }
        }

        if (m_publisher && m_publisher->isOpen()) {
            static_cast<void>(m_publisher->pushPacket(packet, size));
        }

        {
            std::lock_guard<std::mutex> cbLock(m_callbackMutex);
            if (m_packetCallback) {
                m_packetCallback(packet, size);
            }
        }
    }
};

StanagBroadcastEngine::StanagBroadcastEngine(const StanagBroadcastConfig& config)
    : m_impl(std::make_unique<Impl>(config))
{
}

StanagBroadcastEngine::~StanagBroadcastEngine() = default;

bool StanagBroadcastEngine::start()
{
    return m_impl->start();
}

void StanagBroadcastEngine::stop()
{
    m_impl->stop();
}

bool StanagBroadcastEngine::isRunning() const noexcept
{
    return m_impl->isRunning();
}

void StanagBroadcastEngine::setPacketCallback(TsPacketCallback callback)
{
    m_impl->setPacketCallback(std::move(callback));
}

bool StanagBroadcastEngine::pushVideoChunk(const std::uint8_t* data, std::size_t size, std::uint64_t timestampUs)
{
    return m_impl->pushVideoChunk(data, size, timestampUs);
}

bool StanagBroadcastEngine::pushVideoAccessUnit(VideoAccessUnit au)
{
    return m_impl->pushVideoAccessUnit(std::move(au));
}

bool StanagBroadcastEngine::pushTelemetry(UasDatalinkMessage message)
{
    return m_impl->pushTelemetry(std::move(message));
}

bool StanagBroadcastEngine::pushRawKlv(const std::uint8_t* data, std::size_t size, std::uint64_t timestampUs)
{
    return m_impl->pushRawKlv(data, size, timestampUs);
}

void StanagBroadcastEngine::flush()
{
    m_impl->flush();
}

StanagBroadcastStats StanagBroadcastEngine::stats() const
{
    return m_impl->stats();
}

} // namespace Klv
