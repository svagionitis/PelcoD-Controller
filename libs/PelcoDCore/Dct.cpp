/// @file Dct.cpp
/// @brief Implementation of 2D 8x8 Discrete Cosine Transform (DCT-II) and IDCT.

#include "Dct.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace PelcoD::Math {

namespace {

    // Orthogonal 8x8 DCT-II basis matrix C: C * C^T = I
    // C[u][x] = (u == 0 ? 1/sqrt(8) : 0.5 * cos((2x+1)*u*pi/16))
    static const double kDctBasis[8][8]
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

void dct8x8(const double input[8][8], double output[8][8]) noexcept
{
    // Step 1: Intermediate row transform T = input * C^T
    double temp[8][8] {};
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            double sum = 0.0;
            for (int k = 0; k < 8; ++k) {
                sum += input[i][k] * kDctBasis[j][k];
            }
            temp[i][j] = sum;
        }
    }

    // Step 2: Column transform output = C * T
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            double sum = 0.0;
            for (int k = 0; k < 8; ++k) {
                sum += kDctBasis[i][k] * temp[k][j];
            }
            output[i][j] = sum;
        }
    }
}

void dct8x8(const uint8_t* block, int stride, double output[8][8]) noexcept
{
    double spatial[8][8] {};
    for (int i = 0; i < 8; ++i) {
        const uint8_t* rowPtr = block + (i * stride);
        for (int j = 0; j < 8; ++j) {
            spatial[i][j] = static_cast<double>(rowPtr[j]);
        }
    }
    dct8x8(spatial, output);
}

void idct8x8(const double input[8][8], double output[8][8]) noexcept
{
    // Inverse transform: output = C^T * input * C
    // Step 1: T = input * C
    double temp[8][8] {};
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            double sum = 0.0;
            for (int k = 0; k < 8; ++k) {
                sum += input[i][k] * kDctBasis[k][j];
            }
            temp[i][j] = sum;
        }
    }

    // Step 2: output = C^T * T
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            double sum = 0.0;
            for (int k = 0; k < 8; ++k) {
                sum += kDctBasis[k][i] * temp[k][j];
            }
            output[i][j] = sum;
        }
    }
}

} // namespace PelcoD::Math
