#pragma once

/// @file SightlineEnhancement.h
/// @brief Sightline SLA Enhancement Module (Contrast, CLAHE, Denoise, Kernels, False Color).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__enhance.html

#include "../SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @enum ContrastMode
/// @brief Video contrast enhancement mode algorithm options (Message ID 0x21).
enum class ContrastMode : std::uint8_t {
    None = 0U,
    Clahe = 1U,
    Lap = 2U,
    Clahe9 = 3U,
    Clahe10 = 4U,
    HistogramEq = 5U,
    Gamma = 6U,
    Lap16 = 7U,
    HistEqGamma = 8U
};

/// @enum ScintillationPreset
/// @brief Atmospheric scintillation turbulence mitigation modes (Message ID 0x21).
enum class ScintillationPreset : std::uint8_t {
    Manual = 0U,
    LowMotion = 1U,
    HighMotion = 2U,
    InfraRed = 3U
};

/// @enum EnhancementFlags
/// @brief Bit flags controlling motion masking and histogram weighting (Message ID 0x21).
enum class EnhancementFlags : std::uint8_t {
    None = 0x00U,
    AerialMotionMask = 0x01U,   ///< Bit 0: Aerial de-noise motion mask
    FeatureBasedHist = 0x02U,   ///< Bit 1: Feature-based histogram equalization
    SquareRootHist = 0x04U,     ///< Bit 2: Square-root histogram equalization
    StaringMotionMask = 0x10U   ///< Bit 4: Staring de-noise motion mask
};

/// @enum FalseColorPalette
/// @brief Predefined false color thermal palette modes (Message ID 0x16 byte 9).
enum class FalseColorPalette : std::uint8_t {
    None = 0U,
    NoneAlt = 1U,
    WhiteHot = 2U,
    BlackHot = 3U,
    Rainbow = 4U,
    RainbowInverted = 5U,
    Iron = 6U,
    IronInverted = 7U,
    HotCold = 8U,
    HotColdInverted = 9U,
    Jet = 10U,
    JetInverted = 11U,
    Hot = 12U,
    HotInverted = 13U,
    Hsv = 14U,
    HsvInverted = 15U,
    Clr470_S = 16U,
    Clr470_SInverted = 17U,
    Color1 = 18U,
    Color1Inverted = 19U,
    Color2 = 20U,
    Color2Inverted = 21U,
    Color3 = 22U,
    Color3Inverted = 23U,
    HotIron = 24U,
    HotIronInverted = 25U,
    IceFire = 26U,
    IceFireInverted = 27U,
    IdDef = 28U,
    IdDefInverted = 29U,
    Iron256 = 30U,
    Iron256Inverted = 31U,
    Rain256 = 32U,
    Rain256Inverted = 33U,
    XVolcano = 34U,
    XVolcanoInverted = 35U,
    Red = 36U,
    RedInverted = 37U,
    Green = 38U,
    GreenInverted = 39U,
    Blue = 40U,
    BlueInverted = 41U,
    UserPalette = 127U
};

/// @struct MsgSetVideoEnhancement
/// @brief Contrast, brightness, sharpening, and CLAHE basic/legacy structure (Message ID 0x21).
struct MsgSetVideoEnhancement {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t contrast { 50U };
    std::uint8_t brightness { 50U };
    std::uint8_t sharpening { 0U };
    std::uint8_t claheEnable { 0U };
};

/// @struct MsgSetVideoEnhancementFull
/// @brief Complete Sightline SLA video enhancement parameter block (Message ID 0x21 & 0x4A).
struct MsgSetVideoEnhancementFull {
    std::uint8_t cameraIndex { 0U };
    ContrastMode mode { ContrastMode::None };
    std::uint8_t sharpening { 0U };           ///< 0 (none) - 15 (max)
    std::uint8_t alphaBlend { 255U };         ///< 0..255 (255 default, 200 nominal)
    std::uint8_t enhanceParam { 25U };        ///< CLAHE limit, LAP width (0-18), or Gamma*10 (3-28)
    std::uint8_t denoiseRate { 0U };          ///< 0..255 registered running average
    std::uint8_t flags { 0U };                ///< Combination of EnhancementFlags
    std::uint8_t histAveRate { 0U };          ///< 0 = replace every frame, 255 = max temporal avg
    std::uint8_t histMaxPctBin { 0U };        ///< Max percent of counts in single histogram bin
    std::uint16_t roiRow { 0U };              ///< Sub-region row coordinate (0 = disabled)
    std::uint16_t roiCol { 0U };              ///< Sub-region column coordinate (0 = disabled)
    std::uint16_t roiHigh { 0U };             ///< Sub-region height (0 = full frame)
    std::uint16_t roiWide { 0U };             ///< Sub-region width (0 = full frame)
    std::uint8_t deconvSigma { 0U };          ///< Reserved
    std::uint8_t gaussianBlur { 0U };         ///< 0 = off, 1..6 = spatial Gaussian blur
    std::uint8_t lapMinDiff { 0U };           ///< LAP contour suppression threshold (typical 2)
    std::uint8_t colorEnhance { 0U };         ///< 0 = off, 255 = max saturation/vibrance
    std::uint8_t brightness { 128U };         ///< Histogram brightness offset (128 = neutral)
    std::uint8_t contrast { 128U };           ///< Histogram contrast scale (128 = neutral)
    ScintillationPreset scintillation { ScintillationPreset::Manual };
    std::uint8_t sharpenRadius { 1U };        ///< 1, 2, or 3 pixel kernel radius
    std::vector<std::int8_t> customKernel {}; ///< Custom NxN kernel weights (size 0, 9, 25, 49, 81)
    bool normalizeKernel { false };           ///< Automatic sum normalization
};

/// @struct MsgNoise3D
/// @brief 3D Spatio-temporal noise reduction filter settings (Message ID 0xAF).
struct MsgNoise3D {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t enable { 1U };
    std::uint8_t temporalStrength { 50U };
    std::uint8_t spatialStrength { 30U };
};

} // namespace Sightline

