#pragma once

/// @file SpectrogramColorMap.h
/// @brief High-contrast spectral colormaps (Inferno, Viridis, Tactical Green, Jet) for GUI and TUI waterfalls.

#include <cstdint>
#include <string>
#include <tuple>

namespace PelcoD {

/// @struct RgbColor
/// @brief 24-bit RGB color representation.
struct RgbColor {
    std::uint8_t r { 0U };
    std::uint8_t g { 0U };
    std::uint8_t b { 0U };

    constexpr bool operator==(const RgbColor& o) const noexcept
    {
        return r == o.r && g == o.g && b == o.b;
    }
};

/// @class SpectrogramColorMap
/// @brief Translates normalized spectral power or dB values to 24-bit RGB or ANSI truecolor terminal escapes.
class SpectrogramColorMap {
public:
    /// @enum Preset
    /// @brief Curated visual color palettes.
    enum class Preset : std::uint8_t {
        Inferno, ///< Perceptually uniform dark-purple -> red-orange -> bright yellow (industry thermal/spectral standard).
        Viridis, ///< Perceptually uniform deep-purple -> teal -> emerald -> yellow.
        TacticalGreen, ///< Tactical monochrome phosphor HUD green gradient (black -> dark green -> neon mint).
        Jet ///< Classic rainbow spectrum (blue -> cyan -> green -> yellow -> red).
    };

    /// @brief Maps normalized value [0.0, 1.0] to an RGB color.
    /// @param[in] value Normalized intensity [0.0 = minimum, 1.0 = maximum].
    /// @param[in] preset Colormap palette.
    /// @return 24-bit RgbColor triplet.
    [[nodiscard]] static RgbColor mapNormalized(double value, Preset preset = Preset::Inferno) noexcept;

    /// @brief Maps decibel power [minDb, maxDb] to an RGB color.
    /// @param[in] db Logarithmic spectral power in decibels.
    /// @param[in] minDb Floor decibel cutoff (mapped to 0.0).
    /// @param[in] maxDb Ceiling decibel cutoff (mapped to 1.0).
    /// @param[in] preset Colormap palette.
    /// @return 24-bit RgbColor triplet.
    [[nodiscard]] static RgbColor mapDb(double db, double minDb = -60.0, double maxDb = 0.0,
        Preset preset = Preset::Inferno) noexcept;

    /// @brief Generates an ANSI 24-bit truecolor escape sequence rendering two vertical bins in one character cell.
    /// @details Uses the Unicode upper half-block glyph (▀): the top half is colored with normTop (foreground),
    ///          and the bottom half is colored with normBottom (background).
    /// @param[in] normTop Normalized intensity for top frequency bin [0.0, 1.0].
    /// @param[in] normBottom Normalized intensity for bottom frequency bin [0.0, 1.0].
    /// @param[in] preset Colormap palette.
    /// @return String containing ANSI color escape sequences and upper half-block character.
    [[nodiscard]] static std::string mapHalfBlockAnsi(double normTop, double normBottom,
        Preset preset = Preset::Inferno);
};

} // namespace PelcoD
