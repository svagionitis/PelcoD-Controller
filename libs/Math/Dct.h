#pragma once

/// @file Dct.h
/// @brief 2D 8x8 Discrete Cosine Transform (DCT-II) and Inverse DCT routines.

#include <cstdint>

namespace Math {

/// @brief 8x8 matrix type for Discrete Cosine Transform operations.
using DctMatrix8x8 = double[8][8];

/// @brief Computes the 2D 8x8 Discrete Cosine Transform (DCT-II) using separable row-column decomposition.
/// @details Projects spatial luminance values into frequency coefficients using precomputed orthogonal cosine bases:
///          X[u,v] = 1/4 * C(u) * C(v) * sum_{x=0}^7 sum_{y=0}^7 f[x,y] * cos((2x+1)u*pi/16) * cos((2y+1)v*pi/16).
/// @param[in] input 8x8 matrix of spatial domain real values.
/// @param[out] output 8x8 matrix of frequency coefficients (output[0][0] = DC).
/// @note Pure function; thread-safe with no dynamic memory allocations.
void dct8x8(const DctMatrix8x8& input, DctMatrix8x8& output) noexcept;

/// @brief Computes 2D 8x8 DCT-II directly from a raw std::uint8_t image buffer with pitch stride.
/// @details Safely zeroes output and returns early if block is nullptr or stride < 8.
/// @param[in] block Pointer to the top-left pixel byte of the 8x8 block.
/// @param[in] stride Row pitch / stride in bytes of the source image buffer (must be >= 8).
/// @param[out] output 8x8 matrix of frequency coefficients.
/// @note Thread-safe with zero heap allocations.
void dct8x8(const std::uint8_t* block, int stride, DctMatrix8x8& output) noexcept;

/// @brief Computes the 2D 8x8 Inverse Discrete Cosine Transform (IDCT-II).
/// @details Reconstructs spatial domain values from 8x8 DCT-II frequency coefficients via C^T * X * C.
/// @param[in] input 8x8 matrix of frequency coefficients.
/// @param[out] output 8x8 matrix of reconstructed spatial values.
/// @note Thread-safe with zero heap allocations.
void idct8x8(const DctMatrix8x8& input, DctMatrix8x8& output) noexcept;

} // namespace Math
