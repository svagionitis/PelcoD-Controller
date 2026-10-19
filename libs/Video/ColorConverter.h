#pragma once

/// @file ColorConverter.h
/// @brief High-performance color space conversion utilities for raw video frames.

#include "DecoderTypes.h"

#include <cstddef>
#include <cstdint>

namespace Video {

/// @class ColorConverter
/// @brief Static conversion utilities converting planar YUV formats to packed RGB/BGR.
class ColorConverter {
public:
    /// @brief Converts planar YUV420P frames into packed 24-bit RGB or BGR buffers.
    /// @param[in] yPlane Pointer to the luma (Y) plane buffer.
    /// @param[in] uPlane Pointer to the chroma-blue (U) plane buffer.
    /// @param[in] vPlane Pointer to the chroma-red (V) plane buffer.
    /// @param[in] width Frame width in pixels (must be even).
    /// @param[in] height Frame height in pixels (must be even).
    /// @param[in] yStride Row stride in bytes for the luma plane.
    /// @param[in] uStride Row stride in bytes for the U plane.
    /// @param[in] vStride Row stride in bytes for the V plane.
    /// @param[out] dst Pointer to destination buffer (must hold at least width * height * 3 bytes).
    /// @param[in] format Target pixel format (RGB24 or BGR24).
    /// @return True if conversion succeeded, false on invalid dimensions or null pointers.
    static bool yuv420ToRgb(const std::uint8_t* yPlane, const std::uint8_t* uPlane, const std::uint8_t* vPlane,
        int width, int height, int yStride, int uStride, int vStride, std::uint8_t* dst,
        PixelFormat format = PixelFormat::RGB24);
};

} // namespace Video
