#pragma once

/// @file SightlineOverlayParser.h
/// @brief Deserializer for Sightline overlay responses (IDD Overlays module).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__overlay.html

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineOverlayParser
/// @brief Parses current overlay configuration replies.
class SightlineOverlayParser {
public:
    /// @brief Parses current overlay mode reply (Message ID 0x42 / 0x06).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized overlay mode structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseOverlayMode(
        const std::vector<std::uint8_t>& packet, MsgSetOverlayMode& out);
};

} // namespace Sightline
