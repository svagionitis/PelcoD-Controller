#pragma once

/// @file RgbWarpUtility.h
/// @brief High-throughput bilinear image warping for packed RGB24 video frames.

#include <cstddef>
#include <cstdint>

#include "StabilizerTypes.h"
#include "WarpEngine.h"

namespace StabilizerApp {

/// @class RgbWarpUtility
/// @brief Static utilities for warping packed RGB24 frames with SIMD-ready algorithms.
class RgbWarpUtility {
public:
    /// @brief Warps an entire packed RGB24 frame using an affine transformation.
    /// @param src Raw input packed RGB24 buffer.
    /// @param dst Pre-allocated destination RGB24 buffer.
    /// @param width Frame width in pixels.
    /// @param height Frame height in pixels.
    /// @param stride Row stride in bytes (typically width * 3).
    /// @param transform Forward stabilizing affine transformation.
    /// @param config Warp configuration (border mode, auto-crop ratio).
    /// @return True on successful warp; false on invalid dimensions or null pointers.
    [[nodiscard]] static bool warpRgb24(const std::uint8_t* src, std::uint8_t* dst, std::uint32_t width,
        std::uint32_t height, std::uint32_t stride, const Stabilizer::AffineTransform2D& transform,
        const Stabilizer::WarpConfig& config) noexcept;

    /// @brief Extracts grayscale luma (Y) plane from packed RGB24 buffer.
    /// @details Uses integer approximation Y = (77*R + 150*G + 29*B + 128) >> 8.
    /// @param rgbSrc Pointer to packed RGB24 source buffer.
    /// @param lumaDst Pointer to destination grayscale buffer (width * height bytes).
    /// @param width Frame width in pixels.
    /// @param height Frame height in pixels.
    static void extractLuma(
        const std::uint8_t* rgbSrc, std::uint8_t* lumaDst, std::uint32_t width, std::uint32_t height) noexcept;
};

} // namespace StabilizerApp
