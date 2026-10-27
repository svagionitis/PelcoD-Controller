/// @file Dct.cpp
/// @brief Implementation of 2D 8x8 Discrete Cosine Transform (DCT-II) and IDCT.

#include "Dct.h"

#include <cstddef>
#include <cstdint>

namespace Math {

namespace {

    // Orthogonal 8x8 DCT-II basis matrix C: C * C^T = I
    // C[u][x] = (u == 0 ? 1/sqrt(8) : 0.5 * cos((2x+1)*u*pi/16))
    constexpr double kDctBasis[8][8]
        = { { 0.35355339059327376, 0.35355339059327376, 0.35355339059327376, 0.35355339059327376, 0.35355339059327376,
                0.35355339059327376, 0.35355339059327376, 0.35355339059327376 },
              { 0.49039264020161522, 0.41573480615127261, 0.27778511650980114, 0.09754516100806417,
                  -0.09754516100806417, -0.27778511650980114, -0.41573480615127261, -0.49039264020161522 },
              { 0.46193976625564337, 0.19134171618254492, -0.19134171618254492, -0.46193976625564337,
                  -0.46193976625564337, -0.19134171618254492, 0.19134171618254492, 0.46193976625564337 },
              { 0.41573480615127261, -0.09754516100806417, -0.49039264020161522, -0.27778511650980114,
                  0.27778511650980114, 0.49039264020161522, 0.09754516100806417, -0.41573480615127261 },
              { 0.35355339059327376, -0.35355339059327376, -0.35355339059327376, 0.35355339059327376,
                  0.35355339059327376, -0.35355339059327376, -0.35355339059327376, 0.35355339059327376 },
              { 0.27778511650980114, -0.49039264020161522, 0.09754516100806417, 0.41573480615127261,
                  -0.41573480615127261, -0.09754516100806417, 0.49039264020161522, -0.27778511650980114 },
              { 0.19134171618254492, -0.46193976625564337, 0.46193976625564337, -0.19134171618254492,
                  -0.19134171618254492, 0.46193976625564337, -0.46193976625564337, 0.19134171618254492 },
              { 0.09754516100806417, -0.27778511650980114, 0.41573480615127261, -0.49039264020161522,
                  0.49039264020161522, -0.41573480615127261, 0.27778511650980114, -0.09754516100806417 } };

} // namespace

void dct8x8(const DctMatrix8x8& input, DctMatrix8x8& output) noexcept
{
    // Step 1: Intermediate row transform T = input * C^T
    double temp[8][8] {};
    for (std::size_t i = 0U; i < 8U; ++i) {
        for (std::size_t j = 0U; j < 8U; ++j) {
            double sum = 0.0;
            for (std::size_t k = 0U; k < 8U; ++k) {
                sum += input[i][k] * kDctBasis[j][k];
            }
            temp[i][j] = sum;
        }
    }

    // Step 2: Column transform output = C * T
    for (std::size_t i = 0U; i < 8U; ++i) {
        for (std::size_t j = 0U; j < 8U; ++j) {
            double sum = 0.0;
            for (std::size_t k = 0U; k < 8U; ++k) {
                sum += kDctBasis[i][k] * temp[k][j];
            }
            output[i][j] = sum;
        }
    }
}

void dct8x8(const std::uint8_t* block, int stride, DctMatrix8x8& output) noexcept
{
    if (block == nullptr || stride < 8) {
        for (std::size_t i = 0U; i < 8U; ++i) {
            for (std::size_t j = 0U; j < 8U; ++j) {
                output[i][j] = 0.0;
            }
        }
        return;
    }

    const auto uStride = static_cast<std::size_t>(stride);
    double spatial[8][8] {};
    for (std::size_t i = 0U; i < 8U; ++i) {
        const std::uint8_t* rowPtr = block + (i * uStride);
        for (std::size_t j = 0U; j < 8U; ++j) {
            spatial[i][j] = static_cast<double>(rowPtr[j]);
        }
    }
    dct8x8(spatial, output);
}

void idct8x8(const DctMatrix8x8& input, DctMatrix8x8& output) noexcept
{
    // Inverse transform: output = C^T * input * C
    // Step 1: T = input * C
    double temp[8][8] {};
    for (std::size_t i = 0U; i < 8U; ++i) {
        for (std::size_t j = 0U; j < 8U; ++j) {
            double sum = 0.0;
            for (std::size_t k = 0U; k < 8U; ++k) {
                sum += input[i][k] * kDctBasis[k][j];
            }
            temp[i][j] = sum;
        }
    }

    // Step 2: output = C^T * T
    for (std::size_t i = 0U; i < 8U; ++i) {
        for (std::size_t j = 0U; j < 8U; ++j) {
            double sum = 0.0;
            for (std::size_t k = 0U; k < 8U; ++k) {
                sum += kDctBasis[k][i] * temp[k][j];
            }
            output[i][j] = sum;
        }
    }
}

} // namespace Math
