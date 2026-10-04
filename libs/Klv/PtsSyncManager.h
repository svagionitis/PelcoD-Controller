#pragma once

/// @file PtsSyncManager.h
/// @brief STANAG 4609 PTS Synchronization Manager for live video and KLV metadata.

#include "MasterTimeBase.h"
#include "PtsSyncTypes.h"
#include <deque>
#include <functional>
#include <memory>
#include <mutex>

namespace Klv {

class MpegTsKlvMuxer;

/// @class PtsSyncManager
/// @brief Enforces the STANAG 4609 <= 50ms PTS synchronization between video and telemetry.
class PtsSyncManager {
public:
    /// @brief Callback signature for dispatched synchronized frame and telemetry pairs.
    using SyncOutputCallback = std::function<void(const SynchronizedAccessUnit& item)>;

    /// @brief Constructs a PTS synchronization manager with configuration and optional time base.
    /// @param[in] config Initial sync configuration.
    /// @param[in] timeBase Shared master time base reference (nullptr to use default system time base).
    explicit PtsSyncManager(const PtsSyncConfig& config = {},
                            std::shared_ptr<IMasterTimeBase> timeBase = nullptr);

    /// @brief Destructor.
    ~PtsSyncManager() = default;

    /// @brief Configures output synchronization callback.
    /// @param[in] callback Function invoked when a frame is synchronized.
    void setSyncCallback(SyncOutputCallback callback);

    /// @brief Binds manager to an MpegTsKlvMuxer instance for direct TS packetization.
    /// @param[in] muxer Target muxer pointer (or nullptr to unbind).
    void bindMuxer(MpegTsKlvMuxer* muxer);

    /// @brief Updates manager configuration.
    /// @param[in] config New configuration parameters.
    void setConfig(const PtsSyncConfig& config);

    /// @brief Gets active configuration.
    /// @return Active configuration.
    [[nodiscard]] PtsSyncConfig config() const;

    /// @brief Gets real-time synchronization diagnostics and metrics.
    /// @return Current PtsSyncStats snapshot.
    [[nodiscard]] PtsSyncStats stats() const;

    /// @brief Resets accumulated synchronization statistics.
    void resetStats() noexcept;

    /// @brief Ingests an assembled VideoAccessUnit into the synchronization buffer.
    /// @param[in] au Video access unit.
    void pushVideoAccessUnit(VideoAccessUnit au);

    /// @brief Ingests a raw Annex B video frame into the synchronization buffer.
    /// @param[in] data Pointer to raw Annex B byte buffer.
    /// @param[in] size Size of frame in bytes.
    /// @param[in] ptsUs Presentation timestamp in microseconds.
    /// @param[in] dtsUs Optional decode timestamp in microseconds.
    /// @param[in] isKeyframe True if frame is an IDR/keyframe.
    void pushVideoFrame(const std::uint8_t* data,
                        std::size_t size,
                        std::uint64_t ptsUs,
                        std::optional<std::uint64_t> dtsUs = std::nullopt,
                        bool isKeyframe = false);

    /// @brief Ingests a high-level UasDatalinkMessage into the synchronization queue.
    /// @param[in] message Telemetry message.
    void pushTelemetryMessage(UasDatalinkMessage message);

    /// @brief Ingests a raw pre-encoded KLV byte packet.
    /// @param[in] data Pointer to raw KLV bytes.
    /// @param[in] size Size of KLV packet in bytes.
    /// @param[in] timestampUs Microsecond timestamp.
    void pushRawKlv(const std::uint8_t* data,
                    std::size_t size,
                    std::uint64_t timestampUs);

    /// @brief Flushes all queued video frames and pairs them with available telemetry.
    /// @return Number of synchronized frames emitted.
    std::size_t flush();

    /// @brief Clears all internal queues and resets clock state.
    void reset() noexcept;

private:
    struct QueuedTelemetry {
        std::uint64_t timestampUs { 0U };
        std::optional<UasDatalinkMessage> message {};
        std::vector<std::uint8_t> rawBytes {};
    };

    mutable std::mutex m_mutex {};
    PtsSyncConfig m_config {};
    std::shared_ptr<IMasterTimeBase> m_timeBase {};
    SyncOutputCallback m_callback {};
    MpegTsKlvMuxer* m_boundMuxer { nullptr };

    std::deque<VideoAccessUnit> m_videoQueue {};
    std::deque<QueuedTelemetry> m_telemetryQueue {};
    PtsSyncStats m_stats {};

    void processQueuesLocked();
    void dispatchSynchronized(const SynchronizedAccessUnit& syncd);
    [[nodiscard]] std::size_t findBestTelemetry(std::uint64_t targetUs) const noexcept;
};

} // namespace Klv
