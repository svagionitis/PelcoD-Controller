#pragma once

/// @file SightlineIsothermBuilder.h
/// @brief Isotherm false color lookup table (LUT) generator and AGC binder for IR thermal telemetry.
/// @details Implements 256-entry pseudo-color lookup tables, BT.601 RGB-to-YUV matrix transformations,
///          multi-band temperature highlighting, and AGC limit synchronization in compliance with
///          Sightline EAN-Infrared-Temperature Section 8 and EAN-Enhancement Section 5.3.5.
///
/// @section isotherm_ascii_arch Architecture Diagram (ASCII)
/// @verbatim
/// +-------------------------------------------------------------------------------+
/// |                    Sightline Isotherm Color LUT Processing                    |
/// +-------------------------------------------------------------------------------+
/// |                                                                               |
/// |   Physical Scene Temp       Base Palette (256xRGB)      Isotherm Bands        |
/// |  [T_min, T_max] (°C/°F/K)   (WhiteHot, Ironbow, etc.)   (Solid/Gradient/Blend)|
/// |            │                          │                          │            |
/// |            ▼                          ▼                          ▼            |
/// |   ┌──────────────────┐       ┌──────────────────┐       ┌─────────────────┐   |
/// |   │ Temperature Map  │──────>│ LUT Color Raster │<──────│ Band Overwrite/ │   |
/// |   │  idx = f(Temp)   │       │   (256 Entries)  │       │      Blend      │   |
/// |   └────────┬─────────┘       └────────┬─────────┘       └─────────────────┘   |
/// |            │                          │                                       |
/// |            │                          ▼                                       |
/// |            │                 ┌──────────────────┐                             |
/// |            │                 │  RGB -> YUV      │                             |
/// |            │                 │  BT.601 Matrix   │                             |
/// |            │                 └────────┬─────────┘                             |
/// |            │                          │                                       |
/// |            ▼                          ▼                                       |
/// |   ┌──────────────────┐       ┌──────────────────┐                             |
/// |   │ AGC Freeze (0x70)│       │ User LUT (0x72)  │                             |
/// |   │ agHoldmin/max    │       │ 256x3 YUV Bytes  │                             |
/// |   └──────────────────┘       └──────────────────┘                             |
/// +-------------------------------------------------------------------------------+
/// @endverbatim
///
/// @section isotherm_mermaid_arch Architecture Diagram (Mermaid)
/// @verbatim
/// flowchart TD
///     subgraph Input["Input Specifications"]
///         TRange["Temperature Range [Tmin, Tmax]"]
///         Base["Base Palette (Grayscale, Ironbow, etc.)"]
///         Bands["Isotherm Bands (Temp/Index, Color, Blend)"]
///     end
///
///     subgraph Processing["LUT Processing Pipeline"]
///         Init["Initialize Base 256-Entry RGB Table"]
///         Map["Map Temperatures to Indices [0..255]"]
///         Apply["Apply Isotherm Bands (Solid / Linear Gradient)"]
///         YuvConv["Convert RGB to YUV (BT.601 Matrix)"]
///     end
///
///     subgraph Output["Output Targets"]
///         SLA72["MsgUserPalette / SLA 0x72 Packet (768 YUV Bytes)"]
///         SLA70["MsgDigitalCameraParameters / SLA 0x70 (AGC Freeze)"]
///         BinFile["Binary LUT File (256x3 YUV / RGB)"]
///     end
///
///     Base --> Init
///     TRange --> Map
///     Bands --> Map
///     Init --> Apply
///     Map --> Apply
///     Apply --> YuvConv
///     YuvConv --> SLA72
///     YuvConv --> BinFile
///     TRange --> SLA70
/// @endverbatim

#include "SightlineCapture.h"
#include "SightlineNuc.h"
#include "SightlineRadiometry.h"
#include "SightlineTypes.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Sightline {

/// @struct RgbColor
/// @brief 24-bit RGB color representation for thermal false-color palettes.
struct RgbColor {
    std::uint8_t r { 0U }; ///< Red channel intensity [0..255]
    std::uint8_t g { 0U }; ///< Green channel intensity [0..255]
    std::uint8_t b { 0U }; ///< Blue channel intensity [0..255]

    /// @brief Equality comparison operator.
    /// @param[in] other Color to compare with.
    /// @return True if channels match identically.
    [[nodiscard]] constexpr bool operator==(const RgbColor& other) const noexcept {
        return (r == other.r) && (g == other.g) && (b == other.b);
    }

    /// @brief Inequality comparison operator.
    /// @param[in] other Color to compare with.
    /// @return True if any channel differs.
    [[nodiscard]] constexpr bool operator!=(const RgbColor& other) const noexcept {
        return !(*this == other);
    }

    [[nodiscard]] static constexpr RgbColor black() noexcept { return { 0U, 0U, 0U }; }
    [[nodiscard]] static constexpr RgbColor white() noexcept { return { 255U, 255U, 255U }; }
    [[nodiscard]] static constexpr RgbColor red() noexcept { return { 255U, 0U, 0U }; }
    [[nodiscard]] static constexpr RgbColor green() noexcept { return { 0U, 255U, 0U }; }
    [[nodiscard]] static constexpr RgbColor blue() noexcept { return { 0U, 0U, 255U }; }
    [[nodiscard]] static constexpr RgbColor yellow() noexcept { return { 255U, 255U, 0U }; }
    [[nodiscard]] static constexpr RgbColor cyan() noexcept { return { 0U, 255U, 255U }; }
    [[nodiscard]] static constexpr RgbColor magenta() noexcept { return { 255U, 0U, 255U }; }
    [[nodiscard]] static constexpr RgbColor orange() noexcept { return { 255U, 128U, 0U }; }
};

/// @struct YuvColor
/// @brief 24-bit YUV (Y-Cb-Cr) color representation conforming to ITU-R BT.601 standard.
struct YuvColor {
    std::uint8_t y { 128U }; ///< Luma component [0..255]
    std::uint8_t u { 128U }; ///< Chroma blue difference [0..255] (centered at 128)
    std::uint8_t v { 128U }; ///< Chroma red difference [0..255] (centered at 128)

    /// @brief Equality comparison operator.
    /// @param[in] other Color to compare with.
    /// @return True if components match identically.
    [[nodiscard]] constexpr bool operator==(const YuvColor& other) const noexcept {
        return (y == other.y) && (u == other.u) && (v == other.v);
    }

    /// @brief Inequality comparison operator.
    /// @param[in] other Color to compare with.
    /// @return True if any component differs.
    [[nodiscard]] constexpr bool operator!=(const YuvColor& other) const noexcept {
        return !(*this == other);
    }
};

/// @enum BasePaletteType
/// @brief Foundation color scheme prior to applying isotherm overlay bands.
enum class BasePaletteType : std::uint8_t {
    GrayscaleWhiteHot = 0U, ///< Linear monochrome gradient: index 0 (black) to 255 (white)
    GrayscaleBlackHot = 1U, ///< Inverted monochrome gradient: index 0 (white) to 255 (black)
    Ironbow           = 2U, ///< Thermal multi-hue curve: black -> purple -> red -> yellow -> white
    Rainbow           = 3U, ///< High-contrast spectrum: blue -> cyan -> green -> yellow -> red
    Sepia             = 4U  ///< Warm sepia monochrome per Sightline EAN-Enhancement Fig. 21
};

/// @enum IsothermBlendMode
/// @brief Compositing mode for rasterizing isotherm bands onto the base palette.
enum class IsothermBlendMode : std::uint8_t {
    Replace = 0U, ///< Replaces the underlying base color completely
    Blend   = 1U  ///< Blends the isotherm color with base using an alpha weight
};

/// @struct IsothermBand
/// @brief Defined temperature or index interval with designated color highlighting.
struct IsothermBand {
    std::uint8_t minIndex { 0U };                               ///< Lower 8-bit table index [0..255]
    std::uint8_t maxIndex { 255U };                             ///< Upper 8-bit table index [0..255]
    RgbColor startColor {};                                     ///< Color at lower bound
    RgbColor endColor {};                                       ///< Color at upper bound (equals start for solid)
    IsothermBlendMode blendMode { IsothermBlendMode::Replace }; ///< Compositing rule
    float alpha { 1.0F };                                       ///< Opacity [0.0 = transparent, 1.0 = opaque]
};

/// @struct IsothermColorLut
/// @brief Complete 256-entry dual RGB and YUV lookup table ready for transmission or export.
struct IsothermColorLut {
    std::array<RgbColor, 256U> rgb {}; ///< 256 RGB entries
    std::array<YuvColor, 256U> yuv {}; ///< 256 YUV entries (BT.601)

    /// @brief Converts table to 768-byte binary YUV stream matching SLA Message ID 0x72.
    /// @details Format: Y[0], U[0], V[0], Y[1], U[1], V[1], ..., Y[255], U[255], V[255].
    /// @return 768-byte byte vector.
    [[nodiscard]] std::vector<std::uint8_t> toYuvBytes() const;

    /// @brief Converts table to 768-byte binary RGB stream (R[0], G[0], B[0], ...).
    /// @return 768-byte byte vector.
    [[nodiscard]] std::vector<std::uint8_t> toRgbBytes() const;

    /// @brief Packages LUT into MsgUserPalette structure for Sightline device transmission.
    /// @param[in] paletteIndex Palette slot index (0..3).
    /// @return Populated MsgUserPalette object.
    [[nodiscard]] MsgUserPalette toUserPalette(std::uint8_t paletteIndex = 0U) const;

    /// @brief Exports 768-byte binary LUT file conforming to EAN-Enhancement Section 5.3.5.
    /// @param[in] path File destination path.
    /// @param[in] asYuv True for standard Sightline YUV, false for RGB format.
    /// @return True on successful file write.
    [[nodiscard]] bool saveToFile(const std::string& path, bool asYuv = true) const;

    /// @brief Loads 768-byte binary LUT file from disk and populates structure.
    /// @param[in] path File source path.
    /// @param[out] out Destination LUT structure.
    /// @param[in] asYuv True if file is YUV, false if RGB format.
    /// @return True on successful file read.
    [[nodiscard]] static bool loadFromFile(
        const std::string& path, IsothermColorLut& out, bool asYuv = true);
};

/// @class SightlineIsothermBuilder
/// @brief Fluent builder for constructing calibrated isotherm palettes and coordinating AGC limits.
class SightlineIsothermBuilder {
public:
    SightlineIsothermBuilder() noexcept;

    // --- Base Palette Configuration ---

    /// @brief Sets the foundational base palette scheme.
    /// @param[in] type Predefined base palette type.
    /// @return Reference to this builder.
    SightlineIsothermBuilder& setBasePalette(BasePaletteType type) noexcept;

    /// @brief Provides a fully customized 256-entry base RGB palette.
    /// @param[in] base Array of 256 RGB colors.
    /// @return Reference to this builder.
    SightlineIsothermBuilder& setCustomBase(const std::array<RgbColor, 256U>& base) noexcept;

    // --- AGC Scene Range Calibration ---

    /// @brief Defines the full thermal span mapped across 8-bit output counts [0..255].
    /// @details Aligns with Sightline EAN-Infrared-Temperature Section 8.1 Step 1.
    /// @param[in] minTemp Scene minimum temperature (mapped to 0).
    /// @param[in] maxTemp Scene maximum temperature (mapped to 255).
    /// @param[in] scale Temperature scale (°C, °F, or K).
    /// @return Reference to this builder.
    SightlineIsothermBuilder& setAgcRange(
        float minTemp, float maxTemp, TemperatureScale scale) noexcept;

    // --- Isotherm Band Definitions ---

    /// @brief Appends an arbitrary isotherm band specification.
    /// @param[in] band Configured isotherm band.
    /// @return Reference to this builder.
    SightlineIsothermBuilder& addBand(const IsothermBand& band);

    /// @brief Adds a solid color highlight over an 8-bit index interval.
    /// @param[in] minIdx Lower 8-bit index [0..255].
    /// @param[in] maxIdx Upper 8-bit index [0..255].
    /// @param[in] color Highlight color.
    /// @param[in] mode Compositing rule.
    /// @param[in] alpha Opacity multiplier [0.0..1.0].
    /// @return Reference to this builder.
    SightlineIsothermBuilder& addBandIndex(
        std::uint8_t minIdx, std::uint8_t maxIdx, RgbColor color,
        IsothermBlendMode mode = IsothermBlendMode::Replace, float alpha = 1.0F);

    /// @brief Adds a linear gradient highlight over an 8-bit index interval.
    /// @param[in] minIdx Lower 8-bit index [0..255].
    /// @param[in] maxIdx Upper 8-bit index [0..255].
    /// @param[in] start Color at lower index.
    /// @param[in] end Color at upper index.
    /// @param[in] mode Compositing rule.
    /// @param[in] alpha Opacity multiplier [0.0..1.0].
    /// @return Reference to this builder.
    SightlineIsothermBuilder& addGradientIndex(
        std::uint8_t minIdx, std::uint8_t maxIdx, RgbColor start, RgbColor end,
        IsothermBlendMode mode = IsothermBlendMode::Replace, float alpha = 1.0F);

    /// @brief Adds a solid color highlight over a physical temperature span.
    /// @details Requires prior call to setAgcRange().
    /// @param[in] minTemp Band start temperature.
    /// @param[in] maxTemp Band end temperature.
    /// @param[in] scale Temperature scale (°C, °F, or K).
    /// @param[in] color Highlight color.
    /// @param[in] mode Compositing rule.
    /// @param[in] alpha Opacity multiplier [0.0..1.0].
    /// @return Reference to this builder.
    SightlineIsothermBuilder& addBandTemp(
        float minTemp, float maxTemp, TemperatureScale scale, RgbColor color,
        IsothermBlendMode mode = IsothermBlendMode::Replace, float alpha = 1.0F);

    /// @brief Adds a linear gradient highlight over a physical temperature span.
    /// @details Requires prior call to setAgcRange().
    /// @param[in] minTemp Band start temperature.
    /// @param[in] maxTemp Band end temperature.
    /// @param[in] scale Temperature scale (°C, °F, or K).
    /// @param[in] start Color at band start temperature.
    /// @param[in] end Color at band end temperature.
    /// @param[in] mode Compositing rule.
    /// @param[in] alpha Opacity multiplier [0.0..1.0].
    /// @return Reference to this builder.
    SightlineIsothermBuilder& addGradientTemp(
        float minTemp, float maxTemp, TemperatureScale scale, RgbColor start, RgbColor end,
        IsothermBlendMode mode = IsothermBlendMode::Replace, float alpha = 1.0F);

    /// @brief Removes all configured isotherm bands.
    /// @return Reference to this builder.
    SightlineIsothermBuilder& clearBands() noexcept;

    // --- Index Mapping and Artifact Generation ---

    /// @brief Computes 8-bit palette index for a given temperature within calibrated AGC range.
    /// @param[in] temp Physical temperature.
    /// @param[in] scale Scale of input temperature.
    /// @return Clamped 8-bit index [0..255].
    [[nodiscard]] std::uint8_t mapTempToIndex(float temp, TemperatureScale scale) const noexcept;

    /// @brief Rasterizes the configured base palette and isotherm bands into dual RGB/YUV tables.
    /// @return Fully synthesized IsothermColorLut.
    [[nodiscard]] IsothermColorLut buildLut() const;

    /// @brief Synthesizes a MsgUserPalette message containing 768-byte YUV data.
    /// @param[in] paletteIdx Target palette index (0..3).
    /// @return Populated MsgUserPalette object.
    [[nodiscard]] MsgUserPalette buildUserPalette(std::uint8_t paletteIdx = 0U) const;

    /// @brief Builds a complete framed SLA Message ID 0x72 (SetUserPalette) packet.
    /// @param[in] paletteIdx Target palette index (0..3).
    /// @return Binary SLA packet ready for transmission.
    [[nodiscard]] std::vector<std::uint8_t> buildPacket(std::uint8_t paletteIdx = 0U) const;

    /// @brief Builds MsgDigitalCameraParameters (0x70) to freeze AGC to the calibrated thermal span.
    /// @details In accordance with EAN-Infrared-Temperature Section 8.1, sets mode to Manual Gain
    ///          and calculates agHoldmin / agHoldmax detector counts.
    /// @param[in] sensor Calibration model of target IR sensor.
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @param[in] scaleA Custom linear slope multiplier (if sensor == CustomLinear).
    /// @param[in] offsetB Custom linear offset in Kelvin (if sensor == CustomLinear).
    /// @return Populated MsgDigitalCameraParameters structure.
    [[nodiscard]] MsgDigitalCameraParameters buildAgcConfig(
        RadiometricSensor sensor, std::uint8_t cameraIndex = 0U,
        float scaleA = 1.0F, float offsetB = 0.0F) const noexcept;

    // --- Static Color Transformations ---

    /// @brief Converts an RGB color to YUV using ITU-R BT.601 matrix transformations.
    /// @param[in] rgb Source RGB color.
    /// @return Clamped YUV color.
    [[nodiscard]] static YuvColor toYuv(RgbColor rgb) noexcept;

    /// @brief Converts a YUV color to RGB using inverse ITU-R BT.601 matrix transformations.
    /// @param[in] yuv Source YUV color.
    /// @return Clamped RGB color.
    [[nodiscard]] static RgbColor toRgb(YuvColor yuv) noexcept;

private:
    BasePaletteType m_baseType { BasePaletteType::GrayscaleWhiteHot };
    std::array<RgbColor, 256U> m_customBase {};
    bool m_hasCustomBase { false };

    float m_minTemp { 0.0F };
    float m_maxTemp { 100.0F };
    TemperatureScale m_scale { TemperatureScale::Celsius };
    bool m_hasAgcRange { false };

    std::vector<IsothermBand> m_bands {};

    [[nodiscard]] static std::array<RgbColor, 256U> makeBasePalette(BasePaletteType type) noexcept;
    [[nodiscard]] static RgbColor interpolate(RgbColor c1, RgbColor c2, float t) noexcept;
};

} // namespace Sightline
