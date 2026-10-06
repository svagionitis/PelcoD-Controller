#pragma once

/// @file SightlinePaletteParser.h
/// @brief Deserializer for Sightline user palette packets (0x72 / 0x73).

#include "SightlineFraming.h"
#include "SightlinePalette.h"
#include "SightlineTypes.h"

namespace Sightline {

/// @class SightlinePaletteParser
/// @brief Decodes user palette packets. Stateless and thread-safe.
class SightlinePaletteParser {
public:
    /// @brief Parses custom thermal pseudo-color palette (Message ID 0x72 / 0x73).
    /// @details A 768-byte payload is a slot-0 LUT without index; otherwise byte 0 is the slot.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized user palette structure.
    /// @return True on successful parse.
    /// @retval false Wrong message ID or empty payload.
    [[nodiscard]] static bool parseUserPalette(ByteView packet, MsgUserPalette& out);
};

} // namespace Sightline
