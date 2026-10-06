#pragma once

/// @file SightlinePalette.h
/// @brief Sightline SLA user pseudo-colour palette messages (0x72 / 0x73).
/// @details Moved out of SightlineNuc.h: palettes are not part of NUC / DPR
///          (see docs/protocols/Sightline/EAN-NUC-and-DPR.pdf for that scope).

#include "../SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @struct MsgUserPalette
/// @brief Ingests or queries custom pseudo-color lookup table (LUT) for thermal sensors (Message ID 0x72 / 0x73).
struct MsgUserPalette {
    std::uint8_t paletteIndex { 0U }; ///< Palette slot index (0..3)
    std::vector<std::uint8_t> lutData {}; ///< Raw RGB or YUV palette table data
};

} // namespace Sightline
