#pragma once

/// @file BrailleRenderer.h
/// @brief High-performance terminal video rasterizer using Unicode Braille grids and dithering.

#include <cstdint>
#include <string>
#include <vector>

namespace Video {

/// @enum DitherAlgorithm
/// @brief Quantization dithering strategies for high-contrast character cell rasterization.
enum class DitherAlgorithm : std::uint8_t {
    None,            ///< Simple direct luminance thresholding
    Bayer4x4,        ///< 4x4 Ordered Bayer matrix dithering (fast, zero temporal noise)
    Bayer8x8,        ///< 8x8 Ordered Bayer matrix dithering (fine 64-level tonal gradients)
    FloydSteinberg,  ///< 2D Error diffusion dithering (photographic quality)
    Atkinson         ///< Atkinson 1/8th error diffusion (preserves crisp edges and contrast)
};

/// @enum TuiRenderMode
/// @brief Terminal rendering primitive mode.
enum class TuiRenderMode : std::uint8_t {
    Braille,    ///< 2x4 subpixel Unicode Braille patterns (U+2800..U+28FF)
    HalfBlock   ///< ANSI upper/lower half-blocks (2 vertical subpixels per cell)
};

/// @enum TuiColorPalette
/// @brief Terminal color palette simulation modes.
enum class TuiColorPalette : std::uint8_t {
    TrueColor,        ///< 24-bit TrueColor direct from video frame
    Amber,            ///< Tactical amber phosphor (RGB: 255, 176, 0)
    NightVisionGreen, ///< Tactical military night-vision P43 phosphor (RGB: 0, 255, 100)
    CyanHud,          ///< Cockpit HUD tactical cyan (RGB: 0, 230, 255)
    Monochrome        ///< Clean high-contrast monochrome (RGB: 240, 240, 240)
};

/// @struct BrailleRenderOptions
/// @brief Configuration settings controlling terminal frame rasterization.
struct BrailleRenderOptions {
    TuiRenderMode mode { TuiRenderMode::Braille };
    DitherAlgorithm dither { DitherAlgorithm::Bayer4x4 };
    TuiColorPalette palette { TuiColorPalette::TrueColor };
    double contrast { 1.0 };     ///< Contrast multiplier (0.5 .. 2.0)
    double brightness { 0.0 };   ///< Brightness offset (-100.0 .. +100.0)
    bool invert { false };       ///< Invert luminance values
};

/// @struct TerminalPixelCell
/// @brief Single rendered terminal character cell with UTF-8 payload and 24-bit colors.
struct TerminalPixelCell {
    std::string utf8Text { " " };
    std::uint8_t fgR { 255U };
    std::uint8_t fgG { 255U };
    std::uint8_t fgB { 255U };
    std::uint8_t bgR { 0U };
    std::uint8_t bgG { 0U };
    std::uint8_t bgB { 0U };
    bool hasBg { false };
};

/// @class BrailleRenderer
/// @brief Pure C++17 terminal video rasterizer translating RGB24 frames into Braille grids.
class BrailleRenderer {
public:
    BrailleRenderer() = default;
    ~BrailleRenderer() = default;

    /// @brief Convert an 8-bit Braille dot bitmask into a 3-byte UTF-8 Unicode string (U+2800 + mask).
    /// @param dotMask Bitmask with bits 0-7 corresponding to Braille dots 1-8.
    /// @return 3-byte UTF-8 string representing the Braille character.
    [[nodiscard]] static std::string utf8BrailleChar(std::uint8_t dotMask) noexcept;

    /// @brief Calculate perceived ITU-R BT.601 luminance from 24-bit RGB components.
    [[nodiscard]] static constexpr std::uint8_t calculateLuma(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
    {
        return static_cast<std::uint8_t>((299U * r + 587U * g + 114U * b) / 1000U);
    }

    /// @brief Apply simulated monochrome phosphor or HUD palette tints to an RGB color.
    static void applyPalette(TuiColorPalette palette, std::uint8_t inR, std::uint8_t inG, std::uint8_t inB,
        std::uint8_t& outR, std::uint8_t& outG, std::uint8_t& outB) noexcept;

    /// @brief Rasterize a 24-bit RGB frame into a grid of terminal cells.
    /// @param rgbData Pointer to raw interleaved RGB24 bytes.
    /// @param srcWidth Source video width in pixels.
    /// @param srcHeight Source video height in pixels.
    /// @param targetCols Target terminal column count.
    /// @param targetRows Target terminal row count.
    /// @param options Rendering configuration.
    /// @param outCells Output vector sized to targetCols * targetRows.
    static void renderFrame(const std::uint8_t* rgbData, int srcWidth, int srcHeight, int targetCols, int targetRows,
        const BrailleRenderOptions& options, std::vector<TerminalPixelCell>& outCells);

    /// @brief Return human-readable label for a dither algorithm.
    [[nodiscard]] static const char* ditherName(DitherAlgorithm alg) noexcept;

    /// @brief Return human-readable label for a color palette.
    [[nodiscard]] static const char* paletteName(TuiColorPalette pal) noexcept;

    /// @brief Return human-readable label for a render mode.
    [[nodiscard]] static const char* renderModeName(TuiRenderMode mode) noexcept;

private:
    static void renderBrailleGrid(const std::uint8_t* rgbData, int srcWidth, int srcHeight, int targetCols,
        int targetRows, const BrailleRenderOptions& options, std::vector<TerminalPixelCell>& outCells);

    static void renderHalfBlockGrid(const std::uint8_t* rgbData, int srcWidth, int srcHeight, int targetCols,
        int targetRows, const BrailleRenderOptions& options, std::vector<TerminalPixelCell>& outCells);
};

} // namespace Video
namespace videodecoder = Video;
