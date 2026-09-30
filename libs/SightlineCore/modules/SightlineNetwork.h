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

/// @struct MsgSetEthernetDisplayParameters
/// @brief Destination IP, UDP port, and video streaming transport protocol (Message ID 0x29).
/// @details Conforms to official Sightline SLASetEthernetDisplayParameters_t.
struct MsgSetEthernetDisplayParameters {
    std::uint8_t protocol { 1U }; ///< Transport protocol (1: MPEG2-TS H.264, 5: RTP H.264, 8: MPEG2-TS H.265)
    std::uint32_t ipAddress { 0U }; ///< Destination IPv4 address
    std::uint16_t port { 15004U }; ///< Destination UDP port (default 15004 for MPEG2-TS)
    std::uint16_t displayId { 0x0002U }; ///< Network Display ID (0x0002 = Net0)
    std::uint16_t maxPacket { 1400U }; ///< Maximum network packet size in bytes
    std::uint16_t maxRawPacket { 1400U }; ///< Maximum raw packet size in bytes
};

/// @struct MsgCurrentNetworkList
/// @brief Available network interfaces on the SLA board (Message ID 0x67).
struct MsgCurrentNetworkList {
    std::uint8_t numInterfaces { 0U };
    std::vector<std::string> interfaceNames {};
};

} // namespace Sightline
