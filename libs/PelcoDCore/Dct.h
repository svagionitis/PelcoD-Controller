#pragma once

/// @file Dct.h
/// @brief 2D 8x8 Discrete Cosine Transform (DCT-II) and Inverse DCT routines.

#include <cstddef>
#include <cstdint>

namespace PelcoD::Math {

/// @brief Computes the 2D 8x8 Discrete Cosine Transform (DCT-II) using separable row-column decomposition.
/// @details Projects spatial luminance values into frequency coefficients using precomputed orthogonal cosine bases:
///          X[u,v] = 1/4 * C(u) * C(v) * sum_{x=0}^7 sum_{y=0}^7 f[x,y] * cos((2x+1)u*pi/16) * cos((2y+1)v*pi/16).
/// @param[in] input 8x8 matrix of spatial domain real values.
/// @param[out] output 8x8 matrix of frequency coefficients (output[0][0] = DC).
/// @note Pure function; thread-safe with no dynamic memory allocations.
void dct8x8(const double input[8][8], double output[8][8]) noexcept;

/// @brief Computes 2D 8x8 DCT-II directly from a raw std::uint8_t image buffer with pitch stride.
/// @param[in] block Pointer to the top-left pixel byte of the 8x8 block.
/// @param[in] stride Row pitch / stride in bytes of the source image buffer.
/// @param[out] output 8x8 matrix of frequency coefficients.
/// @note Thread-safe with zero heap allocations.
void dct8x8(const std::uint8_t* block, int stride, double output[8][8]) noexcept;

/// @brief Computes the 2D 8x8 Inverse Discrete Cosine Transform (IDCT-II).
/// @param[in] input 8x8 matrix of frequency coefficients.
/// @param[out] output 8x8 matrix of reconstructed spatial values.
/// @note Thread-safe with zero heap allocations.
void idct8x8(const double input[8][8], double output[8][8]) noexcept;

} // namespace PelcoD::Math
