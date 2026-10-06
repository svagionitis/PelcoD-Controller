#pragma once

/// @file SightlinePaletteBuilder.h
/// @brief Serializer for Sightline user palette commands (0x72).

#include "SightlineFraming.h"
#include "SightlinePalette.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlinePaletteBuilder
/// @brief Encodes user palette commands. Stateless and thread-safe.
class SightlinePaletteBuilder {
public:
    /// @brief Encodes custom pseudo-color thermal palette table (Message ID 0x72).
    /// @details A 768-byte LUT for slot 0 is sent without the index byte.
    /// @param[in] msg User palette table.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetUserPalette(const MsgUserPalette& msg);

    /// @brief Encodes query for active user palette (Message ID 0x28 query 0x72).
    /// @details The IDD getter takes no arguments; the index byte is kept for backward
    ///          compatibility with existing callers (review note, not changed by the move).
    /// @param[in] paletteIndex Palette slot index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetUserPalette(std::uint8_t paletteIndex = 0U);
};

} // namespace Sightline
