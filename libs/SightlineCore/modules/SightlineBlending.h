#pragma once

/// @file SightlineBlending.h
/// @brief Sightline SLA Blending Module (Two-channel EO/IR video fusion and false color).
/// @see IDD-SLA-Protocol_3_11_6.pdf (Blending Group)
/// @see EAN-Blending.pdf (Engineering Application Note: Blending IR and EO Images)

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @enum BlendMode
/// @brief Blending algorithms controlling multi-sensor video mixing (EO + IR fusion).
/// @details Defines operational fusion modes described in EAN-Blending Table 3 and IDD 3.11.
enum class BlendMode : std::uint8_t {
    NoChange = 0U,             ///< No change to active blending mode
    FrameBlendWarpEo = 1U,     ///< Alpha blend luminance (Warped EO + Fixed IR)
    ThermalBlendWarpEo = 2U,   ///< False-color hot pixels from Fixed IR over Warped EO
    NightBlendWarpEo = 3U,     ///< Bright visible areas from Warped EO over Fixed IR
    ColorBlendWarpEo = 4U,     ///< Alpha blend luminance and pass Warped EO color to output
    FrameBlendFixedEo = 6U,    ///< Alpha blend luminance (Fixed EO + Warped IR)
    ThermalBlendFixedEo = 7U,  ///< False-color hot pixels from Fixed EO over Warped IR
    NightBlendFixedEo = 8U,    ///< Bright visible areas from Fixed EO over Warped IR
    ColorBlendFixedEo = 9U,    ///< Alpha blend luminance and pass Fixed EO color to output
    ColorIrBlendFixedEo = 10U, ///< Fixed EO blended with user-palette colored Warped IR
    ColorIrBlendWarpEo = 11U,  ///< Warped EO blended with user-palette colored Fixed IR
    ColorEoBlendEo = 12U       ///< Dual color blend: alpha blend two color sensors
};

/// @namespace BlendFlags
/// @brief Bitmask flags modifying blending behavior (SLASetBlendParameters_t byte 12 / payload[8]).
namespace BlendFlags {
    inline constexpr std::uint8_t None { 0x00U };
    inline constexpr std::uint8_t HistogramEq { 0x01U };    ///< Bit 0: IR histogram equalization in thermal blend (legacy/unsupported in >=3.7)
    inline constexpr std::uint8_t UseHueForColor { 0x02U }; ///< Bit 1: 0 = amt controls color, 1 = hue controls color in Color & Color IR blend
} // namespace BlendFlags

/// @namespace AbsOffZoomFlags
/// @brief Bitmask flags controlling offset interpretation and zoom scaling (SLASetBlendParameters_t byte 4 / payload[0]).
namespace AbsOffZoomFlags {
    inline constexpr std::uint8_t IncrementalOffsets { 0x00U }; ///< Bit 0 = 0: incremental pixel shifts
    inline constexpr std::uint8_t AbsoluteOffsets { 0x01U };    ///< Bit 0 = 1: absolute pixel shifts
    inline constexpr std::uint8_t ZoomMultiplierMask { 0x0EU }; ///< Bits 1-3: zoom multiplier (0: 0.9..1.1, 1-7: N*(0.004..0.996))
} // namespace AbsOffZoomFlags

/// @struct MsgSetBlendParameters
/// @brief Dual-camera blending and alignment command (EO + IR fusion) (Message ID 0x2F).
/// @details Conforms to official Sightline SLASetBlendParameters_t (18-byte payload).
struct MsgSetBlendParameters {
    std::uint8_t absOffZoom { AbsOffZoomFlags::IncrementalOffsets }; ///< Offset mode (bit 0) & zoom multiplier (bits 1-3)
    std::int8_t vertical { 0 };                                     ///< Vertical pixel shift (incremental or absolute)
    std::int8_t horizontal { 0 };                                   ///< Horizontal pixel shift (incremental or absolute)
    std::uint8_t rotation { 0U };                                   ///< Warp video rotation: (1..255) maps to (-5..5) degrees; 0 = no change
    std::uint8_t zoom { 0U };                                       ///< Scale factor applied to warp video
    BlendMode mode { BlendMode::FrameBlendWarpEo };                 ///< Blending algorithm
    std::uint8_t amt { 128U };                                      ///< Blend luminance percentage (0 = all IR, 255 = all EO)
    std::uint8_t hue { 0U };                                        ///< Hue adjustment for Night Blend or Color Blend (when UseHueForColor is set)
    std::uint8_t flags { BlendFlags::None };                        ///< Blending flags (bit 0: hist eq, bit 1: use hue for color)
    std::uint8_t reset { 0U };                                      ///< 1: Reset image warp calibration back to defaults
    std::uint8_t reserved { 0U };                                   ///< Reserved byte
    std::uint8_t warpIndex { 0U };                                  ///< Warp camera channel index
    std::uint8_t fixedIndex { 1U };                                 ///< Fixed camera channel index
    std::uint8_t usePresetAlign { 0U };                             ///< 0: Use parameters in message; 1: Use preset alignment
    std::uint8_t presetAlignIndex { 0U };                           ///< Preset index: 0..4 for SLABlendAlign_t, 10..14 for SLAFourAlignPoints_t
    std::uint8_t hzoom { 0U };                                      ///< Horizontal zoom scale (1..255, 0 = no change)
    std::uint8_t hotStart { 0U };                                   ///< Thermal threshold hot start
    std::uint8_t coldEnd { 255U };                                  ///< Thermal threshold cold end
};

/// @struct MsgCurrentBlendParameters
/// @brief Active dual-camera blending telemetry snapshot (Message ID 0x4D).
/// @details Conforms to official Sightline SLACurrentBlendParameters_t (19-byte payload).
struct MsgCurrentBlendParameters {
    std::uint8_t absOffZoom { 0U };                 ///< Bit 1: zoom mode (0: 0.9..1.1, 1: 0.004..0.996)
    std::uint8_t up { 0U };                         ///< Number of pixels warp image is shifted up
    std::uint8_t right { 0U };                      ///< Number of pixels warp image is shifted right
    std::uint8_t down { 0U };                       ///< Number of pixels warp image is shifted down
    std::uint8_t left { 0U };                       ///< Number of pixels warp image is shifted left
    std::uint8_t rotation { 0U };                   ///< Rotation of warp image: (1..255) maps to (-5..5) degrees
    std::uint8_t zoom { 0U };                       ///< Scale factor applied to warp image
    BlendMode mode { BlendMode::FrameBlendWarpEo }; ///< Active blending mode
    std::uint8_t amt { 128U };                      ///< Blend luminance percentage
    std::uint8_t hue { 0U };                        ///< Active color hue
    std::uint8_t flags { 0U };                      ///< Active blending flags
    std::uint8_t reserved { 0U };                   ///< Reserved byte
    std::uint8_t warpIndex { 0U };                  ///< Warp camera channel index
    std::uint8_t fixedIndex { 1U };                 ///< Fixed camera channel index
    std::uint8_t usePresetAlign { 0U };             ///< 1 if preset alignment is active
    std::uint8_t presetAlignIndex { 0U };           ///< Active preset alignment index
    std::uint8_t hzoom { 0U };                      ///< Horizontal zoom scale
    std::uint8_t hotStart { 0U };                   ///< Thermal threshold hot start
    std::uint8_t coldEnd { 255U };                  ///< Thermal threshold cold end
};

/// @struct FourAlignCornerPair
/// @brief Point coordinates for a single corner feature pair in dual-camera homography.
struct FourAlignCornerPair {
    std::int16_t leftCol { 0 };  ///< Column (x) coordinate in left camera
    std::int16_t leftRow { 0 };  ///< Row (y) coordinate in left camera
    std::int16_t rightCol { 0 }; ///< Column (x) coordinate in right camera
    std::int16_t rightRow { 0 }; ///< Row (y) coordinate in right camera
};

/// @typedef AlignPointPair
/// @brief Backward-compatible alias for FourAlignCornerPair.
using AlignPointPair = FourAlignCornerPair;

/// @struct MsgFourAlignPoints
/// @brief 4-point projective homography calibration for dual-sensor co-boresighting (Message ID 0x95).
/// @details Conforms to official Sightline SLAFourAlignPoints_t (33-byte payload).
///          Negative values are set to 0. All zeros resets alignment.
struct MsgFourAlignPoints {
    std::uint8_t index { 0U };        ///< Alignment table slot index [0..4] (maps to presetAlignIndex 10..14)
    FourAlignCornerPair points[4] {}; ///< Corner feature points A, B, C, D
};

/// @struct MsgBlendAlign
/// @brief Fine-tune alignment offsets and scaling for a dual camera setup (Message ID 0xB9).
/// @details Conforms to official Sightline SLABlendAlign_t (11-byte payload).
struct MsgBlendAlign {
    std::uint8_t index { 0U };     ///< Alignment slot index [0..4]
    std::int16_t vertical { 0 };   ///< Vertical offset in pixels
    std::int16_t horizontal { 0 }; ///< Horizontal offset in pixels
    std::uint16_t rotate { 0U };   ///< Rotation angle in degrees (0 to 360) * 128
    std::uint16_t zoom { 4096U };  ///< Vertical & horizontal zoom: (0.01x to 15.99x) * 4096 (4096 = 1.0x)
    std::uint16_t hzoom { 4096U }; ///< Horizontal-only zoom: (0.01x to 15.99x) * 4096 (4096 = 1.0x)
};

/// @struct MultipleAlignmentEntry
/// @brief Single camera alignment slot for SetMultipleAlignment (0x74).
struct MultipleAlignmentEntry {
    std::uint8_t vertical { 0U };   ///< Vertical offset in pixels
    std::uint8_t horizontal { 0U }; ///< Horizontal offset in pixels
    std::uint8_t rotate { 0U };     ///< Rotation (see SLASetBlendParameters_t)
    std::uint8_t zoom { 0U };       ///< Zoom (see SLASetBlendParameters_t)
    std::uint8_t hzoom { 0U };      ///< Horizontal zoom scale
};

/// @struct MsgSetMultipleAlignment
/// @brief Multi-camera alignment parameters (Message ID 0x74 / 0x75).
/// @details Conforms to official Sightline SLASetMultipleAlignment_t and SLACurrentMultipleAlignment_t.
struct MsgSetMultipleAlignment {
    std::uint8_t nAlignments { 0U };        ///< Number of valid alignments present (up to 5)
    MultipleAlignmentEntry alignment[5] {}; ///< Alignment slots [0..4]
};

/// @typedef MsgCurrentMultipleAlignment
/// @brief Format-identical telemetry representation for CurrentMultipleAlignment (0x75).
using MsgCurrentMultipleAlignment = MsgSetMultipleAlignment;

} // namespace Sightline
