#pragma once

/// @file Live555RtspServer.h
/// @brief High-performance embedded RTSP streaming server based on Live555.

#include <cstddef>
#include <cstdint>

#include <memory>
#include <string>
#include <vector>

#include "Live555FrameTypes.h"
#include "RtspServerConfig.h"

namespace Video {

/// @class Live555RtspServer
/// @brief Embedded RTSP streaming server orchestrating Live555 media sessions and ingest pipelines.
/// @details Thread-safe interface allowing video and metadata producers to publish
///          live H.264 or MPEG-TS streams to connected RTSP clients and unit test fixtures.
class Live555RtspServer final {
public:
    /// @brief Constructs server with specified configuration.
    /// @param[in] config Server configuration settings.
    explicit Live555RtspServer(RtspServerConfig config);

    /// @brief Destructor. Stops server and joins worker thread.
    ~Live555RtspServer();

    Live555RtspServer(const Live555RtspServer&) = delete;
    Live555RtspServer& operator=(const Live555RtspServer&) = delete;
    Live555RtspServer(Live555RtspServer&&) = delete;
    Live555RtspServer& operator=(Live555RtspServer&&) = delete;

    /// @brief Starts RTSP server on background worker thread.
    /// @return True if server started and bound successfully to port.
    [[nodiscard]] bool start();

    /// @brief Stops RTSP server, closes all client sessions, and joins thread.
    void stop();

    /// @brief Checks if server is actively listening and running.
    /// @return True if running.
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Retrieves the actual bound listening port (useful when port 0 was specified).
    /// @return Listening port number.
    [[nodiscard]] std::uint16_t listeningPort() const noexcept;

    /// @brief Retrieves full RTSP URL for client connection.
    /// @param[in] host Hostname or IP to format (defaults to "127.0.0.1").
    /// @return Full URL (e.g. "rtsp://127.0.0.1:8554/live").
    [[nodiscard]] std::string getUrl(const std::string& host = "127.0.0.1") const;

    /// @brief Ingests raw Annex-B H.264 NAL unit for client broadcast.
    /// @details Thread-safe; can be called from any producer thread.
    /// @param[in] naluData Pointer to NAL unit buffer (including or excluding start code).
    /// @param[in] naluSize Size of NAL unit in bytes.
    /// @param[in] ptsSeconds Presentation timestamp in seconds (0.0 for immediate).
    /// @return True if successfully queued; false if queue overflowed or invalid input.
    [[nodiscard]] bool pushH264Nalu(const std::uint8_t* naluData, std::size_t naluSize, double ptsSeconds = 0.0);

    /// @brief Ingests pre-multiplexed MPEG-2 TS packets (e.g., from MpegTsKlvMuxer).
    /// @details Thread-safe; input size must be a multiple of 188 bytes.
    /// @param[in] tsData Pointer to TS buffer.
    /// @param[in] tsSize Total size in bytes (must be N * 188).
    /// @return True if successfully queued; false on invalid size or overflow.
    [[nodiscard]] bool pushTsPackets(const std::uint8_t* tsData, std::size_t tsSize);

    /// @brief Retrieves current connected client count.
    /// @return Active RTSP client count.
    [[nodiscard]] std::size_t activeClientCount() const noexcept;

    /// @brief Updates estimated bitrate in kbps for dynamic SDP tuning.
    /// @param[in] estBitrateKbps New bitrate estimate.
    void setEstimatedBitrate(unsigned estBitrateKbps);

    /// @brief Explicitly configures SPS and PPS parameter sets for H.264 SDP generation.
    /// @param[in] sps Raw SPS NAL unit buffer (excluding start code).
    /// @param[in] pps Raw PPS NAL unit buffer (excluding start code).
    void setH264SpsPps(const std::vector<std::uint8_t>& sps, const std::vector<std::uint8_t>& pps);

    /// @brief Queries current queue telemetry and metrics snapshot.
    /// @return Frame queue statistics.
    [[nodiscard]] FrameQueueStats queueStats() const;

    /// @brief Retrieves current server configuration.
    /// @return Const reference to configuration struct.
    [[nodiscard]] const RtspServerConfig& config() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Video
