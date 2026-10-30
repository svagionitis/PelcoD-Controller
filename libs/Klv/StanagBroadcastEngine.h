#pragma once

/// @file StanagBroadcastEngine.h
/// @brief Master orchestrator coordinating live video, KLV telemetry, and MPEG-TS broadcasting.

#include <cstddef>
#include <cstdint>

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "CbrRatePacer.h"
#include "KlvTypes.h"
#include "MpegTsMuxerTypes.h"
#include "TsNetworkTypes.h"
#include "VideoTypes.h"

namespace Klv {

/// @struct StanagBroadcastConfig
/// @brief Master configuration settings for StanagBroadcastEngine.
struct StanagBroadcastConfig {
    /// @brief MPEG-TS multiplexer configuration.
    MpegTsMuxerConfig muxerConfig {};

    /// @brief Network socket and multicast configuration.
    Transport::TsNetworkConfig networkConfig {};

    /// @brief Whether to transmit TS packets via network publisher socket.
    bool enableNetworkPublishing { true };

    /// @brief Maximum capacity for bounded video access unit queue.
    std::size_t maxVideoQueueSize { 128U };

    /// @brief Maximum capacity for bounded telemetry message queue.
    std::size_t maxTelemetryQueueSize { 256U };

    /// @brief Whether to enable Constant Bitrate (CBR) rate pacing with null TS packet injection.
    bool enableCbrPacing { false };

    /// @brief Configuration for CBR rate pacing when enabled.
    Transport::CbrPacerConfig cbrConfig {};

    /// @brief Whether to activate the embedded RTSP server for client pull streaming.
    bool enableRtspServer { false };

    /// @brief Configuration settings for embedded RTSP server when enabled.
    Transport::RtspServerConfig rtspConfig {};
};

/// @struct StanagBroadcastStats
/// @brief Runtime diagnostics and performance metrics for the broadcast engine.
struct StanagBroadcastStats {
    /// @brief Total video frames/chunks ingested.
    std::size_t videoFramesIngested { 0U };

    /// @brief Total video frames multiplexed and emitted.
    std::size_t videoFramesProcessed { 0U };

    /// @brief Total telemetry messages/packets ingested.
    std::size_t telemetryIngested { 0U };

    /// @brief Total telemetry messages multiplexed and emitted.
    std::size_t telemetryProcessed { 0U };

    /// @brief Total video frames dropped due to queue saturation.
    std::size_t droppedVideoFrames { 0U };

    /// @brief Total telemetry items dropped due to queue saturation.
    std::size_t droppedTelemetry { 0U };

    /// @brief Total 188-byte TS packets emitted.
    std::size_t tsPacketsEmitted { 0U };

    /// @brief Current calculated broadcast bitrate in kilobits per second (kbps).
    double currentBitrateKbps { 0.0 };

    /// @brief Average PTS skew between video and telemetry in milliseconds.
    double averagePtsSkewMs { 0.0 };

    /// @brief True if network socket publisher is actively connected/open.
    bool isNetworkOpen { false };

    /// @brief Total ISO/IEC 13818-1 Null TS packets injected by the CBR pacer.
    std::size_t nullPacketsInjected { 0U };

    /// @brief Whether CBR rate pacing is currently active.
    bool isCbrActive { false };

    /// @brief Active RTSP clients connected to embedded RTSP server.
    std::size_t activeRtspClients { 0U };

    /// @brief Streaming RTSP clients actively receiving RTP MP2T data.
    std::size_t streamingRtspClients { 0U };

    /// @brief Total RTP packets dispatched by the embedded RTSP server.
    std::uint64_t rtspPacketsDispatched { 0U };

    /// @brief Whether embedded RTSP server is currently active.
    bool isRtspServerActive { false };
};

/// @class StanagBroadcastEngine
/// @brief High-level facade coordinating video parsing, PTS synchronization, and MPEG-TS network streaming.
class StanagBroadcastEngine {
public:
    /// @brief Callback signature for custom packet tapping.
    using TsPacketCallback = std::function<void(const std::uint8_t* packet, std::size_t size)>;

    /// @brief Constructs engine with specified configuration.
    /// @param[in] config Broadcaster configuration settings.
    explicit StanagBroadcastEngine(const StanagBroadcastConfig& config = {});

    /// @brief Destructor. Ensures worker thread is stopped and network sockets are closed.
    ~StanagBroadcastEngine();

    StanagBroadcastEngine(const StanagBroadcastEngine&) = delete;
    StanagBroadcastEngine& operator=(const StanagBroadcastEngine&) = delete;
    StanagBroadcastEngine(StanagBroadcastEngine&&) = delete;
    StanagBroadcastEngine& operator=(StanagBroadcastEngine&&) = delete;

    /// @brief Starts worker thread and opens network publisher socket if enabled.
    /// @return True if initialized and started successfully.
    [[nodiscard]] bool start();

    /// @brief Stops worker thread and shuts down network streaming.
    void stop();

    /// @brief Checks if broadcast engine is currently active.
    /// @return True if worker thread is running.
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Sets optional packet callback to receive emitted 188-byte TS packets.
    /// @param[in] callback Packet observer callback.
    void setPacketCallback(TsPacketCallback callback);

    /// @brief Pushes a raw Annex B video frame or chunk for NALU parsing and broadcast.
    /// @param[in] data Pointer to Annex B buffer.
    /// @param[in] size Buffer size in bytes.
    /// @param[in] timestampUs Microsecond presentation timestamp (PTS).
    /// @return True if queued successfully; false if queue overflowed.
    bool pushVideoChunk(const std::uint8_t* data, std::size_t size, std::uint64_t timestampUs);

    /// @brief Pushes a parsed VideoAccessUnit directly into the broadcast pipeline.
    /// @param[in] au Assembled video access unit.
    /// @return True if queued successfully; false if queue overflowed.
    bool pushVideoAccessUnit(VideoAccessUnit au);

    /// @brief Pushes a high-level UasDatalinkMessage into the broadcast pipeline (primary metadata PID).
    /// @param[in] message Telemetry message.
    /// @return True if queued successfully; false if queue overflowed.
    bool pushTelemetry(UasDatalinkMessage message);

    /// @brief Pushes a high-level UasDatalinkMessage into the broadcast pipeline for a specific PID.
    /// @param[in] pid Target elementary stream PID.
    /// @param[in] message Telemetry message.
    /// @return True if queued successfully; false if queue overflowed.
    bool pushTelemetry(std::uint16_t pid, UasDatalinkMessage message);

    /// @brief Pushes a pre-encoded KLV byte packet into the broadcast pipeline (primary metadata PID).
    /// @param[in] data Pointer to raw KLV bytes.
    /// @param[in] size Size of packet in bytes.
    /// @param[in] timestampUs Microsecond timestamp.
    /// @return True if queued successfully; false if queue overflowed.
    bool pushRawKlv(const std::uint8_t* data, std::size_t size, std::uint64_t timestampUs);

    /// @brief Pushes a pre-encoded KLV byte packet into the broadcast pipeline for a specific PID.
    /// @param[in] pid Target elementary stream PID.
    /// @param[in] data Pointer to raw KLV bytes.
    /// @param[in] size Size of packet in bytes.
    /// @param[in] timestampUs Microsecond timestamp.
    /// @return True if queued successfully; false if queue overflowed.
    bool pushRawKlv(std::uint16_t pid, const std::uint8_t* data, std::size_t size, std::uint64_t timestampUs);

    /// @brief Flushes all queued frames through synchronization and multiplexer.
    void flush();

    /// @brief Retrieves real-time runtime diagnostics.
    /// @return Current snapshot of broadcast metrics.
    [[nodiscard]] StanagBroadcastStats stats() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Klv
