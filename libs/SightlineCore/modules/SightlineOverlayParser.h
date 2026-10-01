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
/// @brief Parses overlay configuration, graphic primitives, logo parameters, and user fonts.
class SightlineOverlayParser {
public:
    /// @brief Parses current overlay mode reply (Message ID 0x42 / 0x06).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized overlay mode structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseOverlayMode(ByteView packet, MsgSetOverlayMode& out);

    /// @brief Parses single custom graphic object command (Message ID 0x3B).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized graphic object structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseDrawObject(ByteView packet, MsgDrawObject& out);

    /// @brief Parses batch overlay graphic primitives (Message ID 0x9C).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized batch overlay structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseDrawOverlay(ByteView packet, MsgDrawOverlay& out);

    /// @brief Parses logo watermark parameters reply (Message ID 0x9B).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized logo parameters structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseLogoParameters(ByteView packet, MsgLogoParameters& out);

    /// @brief Parses dynamic ancillary text metadata overlay (Message ID 0xAC).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized ancillary text metadata.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseAncillaryTextMetadata(ByteView packet, MsgAncillaryTextMetadata& out);

    /// @brief Parses custom raster font glyph table (Message ID 0xAE).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized user font structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseUserFont(ByteView packet, MsgUserFont& out);
};

} // namespace Sightline
