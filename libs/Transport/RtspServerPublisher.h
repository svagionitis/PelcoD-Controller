#pragma once

/// @file RtspServerPublisher.h
/// @brief Embedded RTSP 1.0/2.0 streaming server for STANAG 4609 MPEG-TS egress.

#include <cstddef>
#include <cstdint>

#include <memory>
#include <string>
#include <string_view>

#include "RtspServerTypes.h"

namespace Transport {

/// @class RtspServerPublisher
/// @brief Pure C++17 embedded RTSP server streaming MPEG-TS/KLV over TCP interleaved or UDP.
/// @details Listens on a configurable TCP port (default 8554), handles RTSP control handshakes,
///          and broadcasts 188-byte MPEG-2 Transport Stream packets to connected clients.
class RtspServerPublisher {
public:
    /// @brief TS packet size constant (188 bytes).
    static constexpr std::size_t kTsPacketSize { 188U };

    /// @brief Maximum TS packets per RTP payload (7 * 188 = 1316 bytes).
    static constexpr std::size_t kMaxPacketsPerDatagram { 7U };

    /// @brief Constructs server publisher with specified configuration.
    /// @param[in] config Server configuration settings.
    explicit RtspServerPublisher(RtspServerConfig config = {});

    /// @brief Destructor. Shuts down listen socket, terminates client sessions, stops worker thread.
    ~RtspServerPublisher();

    // Non-copyable, non-movable
    RtspServerPublisher(const RtspServerPublisher&) = delete;
    RtspServerPublisher& operator=(const RtspServerPublisher&) = delete;
    RtspServerPublisher(RtspServerPublisher&&) = delete;
    RtspServerPublisher& operator=(RtspServerPublisher&&) = delete;

    /// @brief Binds listen socket and launches background network I/O worker thread.
    /// @return True if socket was bound and worker thread started successfully.
    [[nodiscard]] bool start();

    /// @brief Shuts down worker thread, disconnects clients, and closes sockets.
    void stop();

    /// @brief Queries whether the server is actively listening and processing client I/O.
    /// @return True if background worker thread is active.
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Ingests a single 188-byte MPEG-TS packet for fanout to active streaming clients.
    /// @param[in] packet Pointer to 188-byte TS packet.
    /// @param[in] size Size of packet (must be exactly 188 bytes).
    /// @param[in] ptsUs Presentation timestamp in microseconds (for 90kHz RTP timestamp generation).
    /// @return True if ingested and buffered successfully.
    bool pushPacket(const std::uint8_t* packet, std::size_t size, std::uint64_t ptsUs = 0U);

    /// @brief Ingests a contiguous buffer of multiple 188-byte TS packets.
    /// @param[in] buffer Pointer to buffer of TS packets.
    /// @param[in] size Buffer size in bytes (must be multiple of 188 bytes).
    /// @param[in] ptsUs Presentation timestamp in microseconds.
    /// @return Number of TS packets successfully processed.
    std::size_t pushPackets(const std::uint8_t* buffer, std::size_t size, std::uint64_t ptsUs = 0U);

    /// @brief Flushes any partially accumulated TS packets immediately to all clients.
    void flush();

    /// @brief Retrieves the active server configuration.
    /// @return Active configuration copy.
    [[nodiscard]] RtspServerConfig config() const;

    /// @brief Retrieves real-time runtime diagnostics and client metrics.
    /// @return Diagnostic statistics snapshot.
    [[nodiscard]] RtspServerStats stats() const;

    /// @brief Resets operational diagnostic counters.
    void resetStats() noexcept;

    /// @brief Resolves the complete RTSP stream URL (e.g. "rtsp://127.0.0.1:8554/live").
    /// @param[in] hostname Optional host override (defaults to listenAddress or "127.0.0.1").
    /// @return Formatted RTSP URL string.
    [[nodiscard]] std::string streamUrl(std::string_view hostname = {}) const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Transport
