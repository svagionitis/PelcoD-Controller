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
/// @brief Network video streaming protocol, destination, and payload (Message ID 0x1A).
struct MsgSetEthernetVideoParameters {
    std::uint8_t streamIndex { 0U };
    std::uint32_t destIpAddress { 0U };
    std::uint16_t destPort { 15004U }; // Default MPEG2-TS port
    std::uint8_t protocol { 0U }; // 0: MPEG2-TS UDP, 1: RTP H.264, 2: RTSP
    std::uint16_t ttl { 64U };
};

} // namespace Sightline
