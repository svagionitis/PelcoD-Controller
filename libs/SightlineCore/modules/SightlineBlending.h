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

/// @struct AlignPointPair
/// @brief Coordinate pair mapping fixed and warp camera points for homography alignment.
struct AlignPointPair {
    std::uint16_t warpCol { 0U };
    std::uint16_t warpRow { 0U };
    std::uint16_t fixedCol { 0U };
    std::uint16_t fixedRow { 0U };
};

/// @struct MsgFourAlignPoints
/// @brief 4-point projective homography calibration for dual-sensor co-boresighting (Message ID 0x95).
struct MsgFourAlignPoints {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t warpIndex { 1U };
    AlignPointPair points[4] {};
};

/// @struct MsgBlendAlign
/// @brief Fine-tune alignment offsets and automated registration parameters (Message ID 0xB9).
struct MsgBlendAlign {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t mode { 0U }; ///< 0: Manual, 1: Feature-based auto-align, 2: Edge-based
    std::int16_t offsetX { 0 }; ///< Horizontal pixel shift
    std::int16_t offsetY { 0 }; ///< Vertical pixel shift
    std::int16_t rotation { 0 }; ///< Rotation angle in 1/100 degrees
    std::uint16_t scale { 1000U }; ///< Scaling factor (1000 = 1.0x)
};

} // namespace Sightline
