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

    /// @brief Parses single user graphic overlay object command (Message ID 0x9C).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized overlay object structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseDrawOverlay(ByteView packet, MsgDrawOverlay& out);

    /// @brief Parses legacy graphic object command (Message ID 0x3B).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized graphic object structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseDrawObject(ByteView packet, MsgDrawObject& out);

    /// @brief Parses logo watermark parameters reply (Message ID 0x9B).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized logo parameters structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseLogoParameters(ByteView packet, MsgLogoParameters& out);

    /// @brief Parses TrueType user font assignment reply (Message ID 0xAE).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized user font structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseUserFont(ByteView packet, MsgUserFont& out);

    /// @brief Parses active overlay object IDs bitmask reply (Message ID 0x68).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized active object bitmask.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseOverlayObjectsIds(ByteView packet, MsgCurrentOverlayObjectsIds& out);

    /// @brief Parses graphic overlay object parameters reply (Message ID 0x6B).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized object parameters structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseOverlayObjectParams(ByteView packet, MsgCurrentOverlayObjectParameters& out);

    /// @brief Parses dynamic ancillary text metadata packet (Message ID 0xAC).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized ancillary text metadata.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseAncillaryTextMetadata(ByteView packet, MsgAncillaryTextMetadata& out);
};

} // namespace Sightline
