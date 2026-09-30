#pragma once

/// @file SightlineBlending.h
/// @brief Sightline SLA Blending Module (Two-channel EO/IR video fusion and false color).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__blend.html

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @struct MsgSetBlendParameters
/// @brief Dual-camera blending and alignment (EO + IR fusion) (Message ID 0x2F).
/// @details Conforms to official Sightline SLASetBlendParameters_t.
struct MsgSetBlendParameters {
    std::uint8_t absOffZoom { 0U }; ///< 0: incremental offsets, 1: absolute offsets
    std::int8_t vertical { 0 }; ///< Vertical pixel shift
    std::int8_t horizontal { 0 }; ///< Horizontal pixel shift
    std::uint8_t rotation { 0U }; ///< Rotation angle adjustment
    std::uint8_t zoom { 0U }; ///< Video scaling factor
    std::uint8_t mode { 1U }; ///< 1: Frame blend, 2: Thermal blend, 3: Night blend, 4: Color blend
    std::uint8_t amt { 128U }; ///< Blend percentage / weight (0 to 255)
    std::uint8_t hue { 0U }; ///< False color hue
    std::uint8_t flags { 0U }; ///< Blending flags
    std::uint8_t reset { 0U }; ///< Reset alignment / parameters
    std::uint8_t reserved { 0U }; ///< Reserved byte
    std::uint8_t warpIndex { 0U }; ///< Warp camera channel index
    std::uint8_t fixedIndex { 1U }; ///< Fixed camera channel index
    std::uint8_t usePresetAlign { 0U }; ///< Enable preset alignment
    std::uint8_t presetAlignIndex { 0U }; ///< Preset alignment slot index
    std::uint8_t hzoom { 0U }; ///< Horizontal zoom factor
    std::uint8_t hotStart { 0U }; ///< Thermal threshold hot start
    std::uint8_t coldEnd { 255U }; ///< Thermal threshold cold end
};

} // namespace Sightline
