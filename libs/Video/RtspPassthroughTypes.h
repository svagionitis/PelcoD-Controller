#pragma once

/// @file RtspPassthroughTypes.h
/// @brief Domain value objects, configuration, and statistics for RTSP Passthrough Bridge.

#include <cstddef>
#include <cstdint>

#include <string>
#include <string_view>

#include "Klv/VideoTypes.h"

namespace Video {

/// @enum RtspTransportProtocol
/// @brief Transport protocol used for RTP delivery.
enum class RtspTransportProtocol : std::uint8_t {
    TcpInterleaved = 0U, ///< RFC 2326 Section 10.1 '$' interleaved framing over RTSP TCP connection
    UdpUnicast = 1U ///< RFC 3550 RTP/AVP over independent UDP unicast sockets
};

/// @enum RtspBridgeState
/// @brief State machine operational phase of the RTSP bridge client.
enum class RtspBridgeState : std::uint8_t {
    Disconnected = 0U,
    Connecting = 1U,
    Describing = 2U,
    SettingUp = 3U,
    Playing = 4U,
    Reconnecting = 5U,
    Error = 6U
};

/// @struct RtspPassthroughConfig
/// @brief Configuration settings for RtspPassthroughBridge.
struct RtspPassthroughConfig {
    std::string rtspUrl { };
    std::string username { };
    std::string password { };
    RtspTransportProtocol transport { RtspTransportProtocol::TcpInterleaved };
    std::uint32_t connectTimeoutMs { 5000U };
    std::uint32_t keepaliveIntervalMs { 30000U };
    std::uint32_t reconnectDelayMs { 2000U };
    std::size_t maxAuBufferSize { 10485760U }; ///< 10 MB maximum allocation buffer for keyframes
    bool autoReconnect { true };
    Klv::VideoCodec preferredCodec { Klv::VideoCodec::H264 };
};

/// @struct RtspPassthroughStats
/// @brief Runtime diagnostics and health statistics for RtspPassthroughBridge.
struct RtspPassthroughStats {
    std::size_t rtpPacketsReceived { 0U };
    std::size_t rtpBytesReceived { 0U };
    std::size_t accessUnitsEmitted { 0U };
    std::size_t packetLossCount { 0U };
    std::size_t corruptPackets { 0U };
    double currentBitrateKbps { 0.0 };
    RtspBridgeState state { RtspBridgeState::Disconnected };
};

} // namespace Video
