#pragma once

/// @file SightlineNetwork.h
/// @brief Sightline SLA Network Module (Network communications, IP setup, Ethernet video).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__network.html

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @struct MsgSetNetworkParameters
/// @brief IP address, subnet mask, gateway, and DHCP configuration (Message ID 0x1C).
struct MsgSetNetworkParameters {
    std::uint32_t ipAddress { 0U };
    std::uint32_t subnetMask { 0U };
    std::uint32_t gateway { 0U };
    std::uint8_t dhcpEnable { 1U };
    std::uint16_t commandPort { DefaultHardwareCommandPort };
    std::uint16_t replyPort { DefaultClientReplyPort };
};

/// @struct MsgSetEthernetVideoParameters
/// @brief Video frame quality, downsample, and frame rate over Ethernet (Message ID 0x1A).
/// @details Conforms to official Sightline SLASetEthernetVideoParameters_t.
struct MsgSetEthernetVideoParameters {
    std::uint8_t quality { 80U }; ///< 0-100: MJPEG image quality (default 80)
    std::uint8_t foveal { 0U }; ///< 0-100: Quality reduction away from center (MJPEG)
    std::uint8_t frameStep { 1U }; ///< Frame skip divisor (1 = full rate, 2 = 1/2 rate)
    std::uint8_t frameSize { 0U }; ///< Output frame size enum (0 = input size, 11 = custom)
    std::uint16_t displayId { 0x0002U }; ///< Network Display ID (Net0 = 0x0002)
    std::uint16_t customWide { 0U }; ///< Width in pixels for custom frameSize (multiple of 32)
    std::uint16_t customHigh { 0U }; ///< Height in pixels for custom frameSize (multiple of 8)
};

/// @enum EthernetDisplayProtocol
/// @brief Transport stream and elementary video protocol types (Message ID 0x29 / 0x52).
enum class EthernetDisplayProtocol : std::uint8_t {
    Disabled = 0U,
    Mpeg2TsH264 = 1U,    ///< MPEG2-TS with H.264 video and KLV metadata
    Mjpeg = 2U,          ///< Motion JPEG (1500-OEM legacy)
    Mpeg4 = 3U,          ///< MPEG-4 Part 2 (1500-OEM legacy)
    Raw = 4U,            ///< Raw uncompressed frames
    RtpH264 = 5U,        ///< Direct RTP H.264 (RFC 6184)
    RtpMpeg2TsH264 = 6U, ///< RTP encapsulating MPEG2-TS H.264
    KlvOnly = 7U,        ///< MPEG2-TS KLV metadata only without video (SW 3.3+)
    Mpeg2TsH265 = 8U,    ///< MPEG2-TS with H.265 (HEVC) video (17xx/4000/41xx)
    RtpH265 = 9U,        ///< Direct RTP H.265 (RFC 7798)
    RtpMpeg2TsH265 = 10U ///< RTP encapsulating MPEG2-TS H.265
};

/// @enum NetworkDisplayId
/// @brief Logical network channel display IDs for multi-stream output.
enum class NetworkDisplayId : std::uint16_t {
    Net0 = 0x0002U, ///< Network display channel 0 (Primary)
    Net1 = 0x0080U, ///< Network display channel 1 (Secondary)
    Net2 = 0x0200U  ///< Network display channel 2 (Tertiary, 4100/4110)
};

/// @brief Checks whether the given protocol utilizes RTP framing.
/// @param[in] protocol Protocol integer or enum value.
/// @return True if protocol is RTP-based.
[[nodiscard]] constexpr bool isRtpProtocol(std::uint8_t protocol) noexcept
{
    return (protocol == static_cast<std::uint8_t>(EthernetDisplayProtocol::RtpH264)
        || protocol == static_cast<std::uint8_t>(EthernetDisplayProtocol::RtpMpeg2TsH264)
        || protocol == static_cast<std::uint8_t>(EthernetDisplayProtocol::RtpH265)
        || protocol == static_cast<std::uint8_t>(EthernetDisplayProtocol::RtpMpeg2TsH265));
}

/// @brief Validates destination port according to RFC 3550 Section 11.
/// @details RTP protocols require an even destination port number.
/// @param[in] port Destination UDP port.
/// @param[in] protocol Selected transport protocol.
/// @return True if port complies with RFC 3550.
[[nodiscard]] constexpr bool isValidTransportPort(std::uint16_t port, std::uint8_t protocol) noexcept
{
    if (isRtpProtocol(protocol)) {
        return (port % 2U) == 0U;
    }
    return port > 0U;
}

/// @struct MsgSetEthernetDisplayParameters
/// @brief Destination IP, UDP port, and video streaming transport protocol (Message ID 0x29).
/// @details Conforms to official Sightline SLASetEthernetDisplayParameters_t.
struct MsgSetEthernetDisplayParameters {
    std::uint8_t protocol { 1U }; ///< Transport protocol (see EthernetDisplayProtocol)
    std::uint32_t ipAddress { 0U }; ///< Destination IPv4 address
    std::uint16_t port { 15004U }; ///< Destination UDP port (default 15004 for MPEG2-TS)
    std::uint16_t displayId { 0x0002U }; ///< Network Display ID (Net0 = 0x0002, Net1 = 0x0080, Net2 = 0x0200)
    std::uint16_t maxPacket { 1400U }; ///< Maximum network packet size in bytes (MPEG2-TS)
    std::uint16_t maxRawPacket { 1400U }; ///< Maximum raw packet size in bytes (RTP)
};

/// @struct MsgCurrentNetworkList
/// @brief Available network interfaces on the SLA board (Message ID 0x67).
struct MsgCurrentNetworkList {
    std::uint8_t numInterfaces { 0U };
    std::vector<std::string> interfaceNames {};
};

} // namespace Sightline
