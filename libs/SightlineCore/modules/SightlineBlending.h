#pragma once

/// @file SightlineBlending.h
/// @brief Sightline SLA Blending Module (Two-channel EO/IR video fusion and false color).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__blend.html

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @struct MsgSetBlendParameters
/// @brief Dual-camera blending and alignment (EO + IR fusion) (Message ID 0x2F).
struct MsgSetBlendParameters {
    std::uint8_t primaryCamera { 0U };
    std::uint8_t secondaryCamera { 1U };
    std::uint8_t blendMode { 0U }; // 0: Alpha, 1: Picture-in-Picture, 2: False Color
    std::uint8_t alphaPercent { 50U };
};

} // namespace Sightline
