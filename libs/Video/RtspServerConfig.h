#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <string>

namespace Video {

/// @enum RtspStreamType
/// @brief Payload encapsulation format for RTSP sessions.
enum class RtspStreamType : std::uint8_t {
    H264ElementaryStream = 0U, ///< RFC 6184 H.264 elementary stream (video/H264)
    Mpeg2TransportStream = 1U ///< RFC 2250 MPEG-2 TS carrying STANAG 4609 (video/MP2T)
};

/// @enum RtspTransportProtocol
/// @brief Allowed network delivery protocols for client sessions.
enum class RtspTransportProtocol : std::uint8_t {
    All = 0U, ///< Permit both UDP Unicast and TCP Interleaved
    TcpOnly = 1U, ///< Restrict to TCP Interleaved (firewall/NAT traversal)
    UdpOnly = 2U ///< Restrict to UDP Unicast (lowest latency LAN)
};

/// @struct RtspServerConfig
/// @brief Complete configuration parameters for Live555RtspServer.
struct RtspServerConfig {
    std::uint16_t port { 8554U }; ///< RTSP server listen port (0 for ephemeral test port)
    std::string streamPath { "live" }; ///< URL mount point path (e.g. "live" -> rtsp://host:port/live)
    std::string streamName { "Live Stream" }; ///< Descriptive stream name for SDP
    std::string streamInfo { "PelcoD RTSP Server" }; ///< Session description text for SDP
    RtspStreamType streamType { RtspStreamType::H264ElementaryStream };
    RtspTransportProtocol transport { RtspTransportProtocol::All };
    std::size_t maxClients { 16U }; ///< Maximum simultaneous client connections
    std::size_t queueCapacity { 30U }; ///< In-memory ring buffer frame capacity
    std::optional<std::string> username {}; ///< Optional RTSP authentication username
    std::optional<std::string> password {}; ///< Optional RTSP authentication password
    std::string realm { "PelcoD_RTSP_Server" }; ///< Authentication realm for Digest/Basic
    bool enableMulticast { false }; ///< Enable multicast RTP egress
    std::string multicastAddress { "239.255.0.1" }; ///< Multicast group destination IP
    std::uint16_t multicastPort { 5004U }; ///< Multicast group destination RTP port
    std::uint8_t multicastTtl { 16U }; ///< Multicast TTL hop limit

    /// @brief Validates configuration fields for sanity and bounds.
    /// @return True if configuration is valid and ready to bind.
    [[nodiscard]] bool validate() const noexcept;
};

} // namespace Video
