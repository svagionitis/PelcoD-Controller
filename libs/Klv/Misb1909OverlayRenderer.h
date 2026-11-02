#pragma once

/// @file Misb1909OverlayRenderer.h
/// @brief Non-destructive tactical HUD layout generator conforming to MISB ST 1909.1.
/// @details Generates normalized graphical primitives and standardized metadata layout groups
///          from MISB ST 0601 / ST 0102 telemetry for client-side viewport rendering. Zero Qt dependencies.

#include <cstddef>
#include <cstdint>

#include <optional>
#include <string>

#include "KlvTypes.h"
#include "Misb1909Types.h"

namespace Klv {

/// @class Misb1909OverlayRenderer
/// @brief Pure C++17 stateless generator synthesizing normalized MISB ST 1909.1 HUD layouts.
class Misb1909OverlayRenderer {
public:
    /// @brief Generates a complete tactical HUD layout from parsed UAS Datalink telemetry.
    /// @param[in] message Fully or partially populated MISB ST 0601 telemetry packet.
    /// @param[in] config Operational configuration (density, theme, instrument flags).
    /// @return Immutable normalized layout ready for client viewport rasterization.
    [[nodiscard]] static TacticalOverlayLayout buildLayout(
        const UasDatalinkMessage& message, const Misb1909Config& config = Misb1909Config {}) noexcept;

    /// @brief Formats an angle per MISB ST 1909.1 (ST 1909-01, ST 1909.1-95).
    /// @param[in] deg Angle in degrees, or nullopt if unavailable.
    /// @param[in] digitsBefore Minimum width of integer part padded with leading spaces.
    /// @param[in] digitsAfter Fixed decimal fractional precision.
    /// @return Padded angle string with asterisk degree indicator, or "N/A".
    [[nodiscard]] static std::string formatSt1909Angle(std::optional<double> deg, int digitsBefore, int digitsAfter);

    /// @brief Formats integer meters per MISB ST 1909.1 (ST 1909-01).
    /// @param[in] meters Distance or altitude in meters, or nullopt.
    /// @param[in] digits Total integer field width padded with leading spaces.
    /// @return Padded string with 'm' suffix, or "N/A".
    [[nodiscard]] static std::string formatSt1909Meters(std::optional<double> meters, int digits);

    /// @brief Formats a microsecond timestamp per ISO 8601-1 CCYY-MM-DDThh:mm:ss.sZ (ST 1909-46).
    /// @param[in] epochUs Microseconds since UNIX epoch, or nullopt.
    /// @return Formatted ISO 8601-1 string with tenth-second precision, or "N/A".
    [[nodiscard]] static std::string formatSt1909IsoTime(std::optional<std::uint64_t> epochUs);

    /// @brief Converts WGS-84 geodetic coordinates to Military Grid Reference System (MGRS).
    /// @param[in] latDeg Latitude in degrees [-90.0, +90.0].
    /// @param[in] lonDeg Longitude in degrees [-180.0, +180.0].
    /// @return Standard 10-figure MGRS string (e.g. "11SMT 82060 91440"), or "N/A".
    [[nodiscard]] static std::string toMgrsString(std::optional<double> latDeg, std::optional<double> lonDeg);

    /// @brief Resolves color RGBA channels from a tactical color palette enum.
    /// @param[in] palette Operational theme choice.
    /// @return Normalized RGBA color channels.
    [[nodiscard]] static constexpr HudColorRgba getPaletteColor(TacticalColorPalette palette) noexcept
    {
        switch (palette) {
        case TacticalColorPalette::TacticalGreen:
            return HudColorRgba { 0.0F, 1.0F, 0.25F, 1.0F }; // #00FF41 NVG Green
        case TacticalColorPalette::Amber:
            return HudColorRgba { 1.0F, 0.69F, 0.0F, 1.0F }; // #FFB000 FLIR Amber
        case TacticalColorPalette::ElectricCyan:
            return HudColorRgba { 0.0F, 0.94F, 1.0F, 1.0F }; // #00F0FF Glass Cyan
        case TacticalColorPalette::CombatRed:
            return HudColorRgba { 1.0F, 0.20F, 0.20F, 1.0F }; // #FF3333 Threat Red
        case TacticalColorPalette::AviationWhite:
            return HudColorRgba { 1.0F, 1.0F, 1.0F, 1.0F }; // #FFFFFF High-Contrast White
        }
        return HudColorRgba { 0.0F, 1.0F, 0.25F, 1.0F };
    }
};

} // namespace Klv
