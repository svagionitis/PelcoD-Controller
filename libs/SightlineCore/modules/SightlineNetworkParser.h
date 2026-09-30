#pragma once

/// @file SightlineNetworkParser.h
/// @brief Deserializer for Sightline network responses (IDD Network module).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__network.html

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineNetworkParser
/// @brief Parses network settings and Ethernet communications responses.
class SightlineNetworkParser {
public:
    /// @brief Parses current network parameters reply (Message ID 0x49 / 0x1C).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized network parameters structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseNetworkParameters(
        const std::vector<std::uint8_t>& packet, MsgSetNetworkParameters& out);

    /// @brief Parses Ethernet video parameters reply (Message ID 0x48 / 0x1A).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized Ethernet video parameters structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseEthernetVideo(
        const std::vector<std::uint8_t>& packet, MsgSetEthernetVideoParameters& out);

    /// @brief Parses Ethernet display parameters reply (Message ID 0x52 / 0x29).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized Ethernet display parameters structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseEthernetDisplay(
        const std::vector<std::uint8_t>& packet, MsgSetEthernetDisplayParameters& out);

    /// @brief Parses network interfaces list reply (Message ID 0x67).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized network list structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseNetworkList(
        const std::vector<std::uint8_t>& packet, MsgCurrentNetworkList& out);
};

} // namespace Sightline
