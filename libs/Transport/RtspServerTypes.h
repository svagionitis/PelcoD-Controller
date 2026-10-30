#pragma once

/// @file RtspServerTypes.h
/// @brief Domain value objects, configuration structures, and metrics for RtspServerPublisher.

#include <cstddef>
#include <cstdint>

#include <string>
#include <string_view>

namespace Transport {

/// @enum RtspTransportMode
/// @brief Underlying delivery transport protocol negotiated during RTSP SETUP.
enum class RtspTransportMode : std::uint8_t {
    TcpInterleaved = 0U, ///< RFC 2326 §10.1 interleaved binary data over TCP control connection
    UdpUnicast = 1U ///< RFC 3550 RTP/AVP over independent UDP unicast datagrams
};

/// @enum RtspSessionState
/// @brief Lifecycle state machine for an active RTSP client session.
enum class RtspSessionState : std::uint8_t {
    Init = 0U, ///< Connection established, awaiting DESCRIBE / SETUP
    Ready = 1U, ///< SETUP complete, transport allocated, awaiting PLAY
    Streaming = 2U, ///< PLAY active, transmitting RTP packets
    Teardown = 3U, ///< TEARDOWN requested, releasing resources
    Closed = 4U ///< Disconnected and marked for immediate destruction
};

/// @class RtspPort
/// @brief Strongly-typed TCP/UDP network port value object.
class RtspPort {
public:
    constexpr explicit RtspPort(std::uint16_t port = 8554U) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] constexpr std::uint16_t value() const noexcept
    {
        return m_port;
    }

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return m_port > 0U;
    }

private:
    std::uint16_t m_port { 8554U };
};

/// @struct RtspServerConfig
/// @brief Comprehensive configuration settings for RtspServerPublisher.
struct RtspServerConfig {
    /// @brief Server listen interface IPv4 address ("0.0.0.0" for all interfaces).
    std::string listenAddress { "0.0.0.0" };

    /// @brief Server listen TCP port for RTSP control connections (default 8554).
    RtspPort listenPort { 8554U };

    /// @brief URL resource path serving the STANAG 4609 stream (e.g. "/live" or "/stanag4609").
    std::string streamPath { "/live" };

    /// @brief Human-readable SDP session title.
    std::string sessionName { "STANAG 4609 Video & Telemetry Stream" };

    /// @brief Maximum concurrent connected RTSP clients permitted (prevents resource exhaustion).
    std::size_t maxClients { 32U };

    /// @brief Maximum client session inactivity timeout in seconds (default 60s).
    std::uint32_t sessionTimeoutSeconds { 60U };

    /// @brief Maximum 188-byte TS packets grouped into a single RTP packet (default 7 = 1316 B payload).
    std::size_t packetsPerDatagram { 7U };

    /// @brief Per-client outbound packet queue capacity before dropping for slow clients.
    std::size_t maxClientQueuePackets { 256U };

    /// @brief Base UDP port for ephemeral RTP port allocation in UDP unicast mode.
    std::uint16_t udpPortRangeStart { 30000U };

    /// @brief End UDP port for ephemeral RTP port allocation in UDP unicast mode.
    std::uint16_t udpPortRangeEnd { 35000U };

    /// @brief 32-bit RTP SSRC identifier.
    std::uint32_t rtpSsrc { 0x4D503254U }; // "MP2T"
};

/// @struct RtspServerStats
/// @brief Operational telemetry and throughput metrics for RtspServerPublisher.
struct RtspServerStats {
    /// @brief Number of active RTSP client sessions currently connected.
    std::size_t activeClients { 0U };

    /// @brief Number of clients actively receiving media in Streaming state.
    std::size_t streamingClients { 0U };

    /// @brief Total lifetime RTSP client connections accepted.
    std::uint64_t totalConnectionsAccepted { 0U };

    /// @brief Total 188-byte TS packets ingested into the server.
    std::uint64_t tsPacketsIngested { 0U };

    /// @brief Total RFC 3550 RTP packets dispatched across all clients.
    std::uint64_t rtpPacketsDispatched { 0U };

    /// @brief Total bytes transmitted across TCP and UDP client channels.
    std::uint64_t bytesTransmitted { 0U };

    /// @brief Total RTP packets dropped due to slow client queue saturation.
    std::uint64_t droppedPacketsSlowClients { 0U };

    /// @brief Current calculated outbound bitrate in kilobits per second (kbps).
    double currentBitrateKbps { 0.0 };
};

} // namespace Transport
