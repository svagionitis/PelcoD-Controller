#pragma once

/// @file SightlineNetworkBuilder.h
/// @brief Serializer for Sightline network IP parameters and Ethernet video streaming (IDD Network module).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__network.html

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineNetworkBuilder
/// @brief Encodes IP configuration, Ethernet video downsampling, and destination streaming parameters.
class SightlineNetworkBuilder {
public:
    /// @brief Encodes network IP address, subnet, gateway, and UDP ports (Message ID 0x1C).
    /// @param[in] msg Network parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetNetworkParameters(
        const MsgSetNetworkParameters& msg);

    /// @brief Encodes Ethernet video compression and frame rate parameters (Message ID 0x1A).
    /// @param[in] msg Ethernet video parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetEthernetVideo(
        const MsgSetEthernetVideoParameters& msg);

    /// @brief Encodes video destination IP/port and transport protocol (Message ID 0x29).
    /// @param[in] msg Ethernet display parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetEthernetDisplay(
        const MsgSetEthernetDisplayParameters& msg);

    /// @brief Encodes query for active network parameters (Message ID 0x1D).
    /// @param[in] index Network interface index (typically 0).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetNetworkParameters(
        std::uint8_t index = 0U);

    /// @brief Encodes query for active Ethernet video parameters (Message ID 0x1B).
    /// @param[in] displayId Network display mask ID (e.g. 0x0002 for Net0).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetEthernetVideo(
        std::uint16_t displayId = 0x0002U);

    /// @brief Encodes query for active Ethernet display parameters (Message ID 0x39).
    /// @param[in] displayId Network display mask ID (e.g. 0x0002 for Net0).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetEthernetDisplay(
        std::uint16_t displayId = 0x0002U);

    /// @brief Encodes query for network interfaces list (Message ID 0x66).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetNetworkList();
};

} // namespace Sightline
