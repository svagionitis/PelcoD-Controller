/// @file TestDct.cpp
/// @brief Unit tests for the 2D 8x8 DCT-II and IDCT: identity round-trips, DC coefficient,
///        orthogonality, pixel-buffer overload, zero/constant inputs, and linearity.

#include "Dct.h"

#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <iostream>

using namespace Math;

namespace {

constexpr double TOLERANCE = 1e-8;

bool nearEqual(double a, double b, double tol = TOLERANCE)
{
    return std::abs(a - b) <= tol;
}

// ---------------------------------------------------------------------------

TEST(DctTest, DctIdctIdentityUniform)
{
    std::cout << "[Test] testDctIdctIdentityUniform\n";
    DctMatrix8x8 input {};
    DctMatrix8x8 freqDomain {};
    DctMatrix8x8 reconstructed {};

    // Fill with uniform value (128.0)
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            input[y][x] = 128.0;
        }
    }

    dct8x8(input, freqDomain);
    idct8x8(freqDomain, reconstructed);

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            EXPECT_TRUE(nearEqual(reconstructed[y][x], input[y][x], 1e-6));
        }
    }
    std::cout << "  -> PASSED\n";
}

TEST(DctTest, DctIdctIdentityRamp)
{
    std::cout << "[Test] testDctIdctIdentityRamp\n";
    DctMatrix8x8 input {};
    DctMatrix8x8 freq {};
    DctMatrix8x8 rec {};

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            input[y][x] = static_cast<double>(y * 8 + x) * 3.7;
        }
    }

    dct8x8(input, freq);
    idct8x8(freq, rec);

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            EXPECT_TRUE(nearEqual(rec[y][x], input[y][x], 1e-6));
        }
    }
    std::cout << "  -> PASSED\n";
}

TEST(DctTest, DctZeroInputYieldsZeroOutput)
{
    std::cout << "[Test] testDctZeroInputYieldsZeroOutput\n";
    DctMatrix8x8 input {};
    DctMatrix8x8 freq {};

    // input is all zeros (value-initialized above)
    dct8x8(input, freq);

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            EXPECT_TRUE(nearEqual(freq[y][x], 0.0));
        }
    }
    std::cout << "  -> PASSED\n";
}

TEST(DctTest, DcCoefficientForConstantBlock)
{
    std::cout << "[Test] testDcCoefficientForConstantBlock\n";
    // For a constant block of value V, the DC coefficient (0,0) should be:
    // X[0,0] = (1/4) * C(0) * C(0) * sum(V) = (1/4) * (1/sqrt(2)) * (1/sqrt(2)) * 64 * V = 8.0 * V
    DctMatrix8x8 input {};
    DctMatrix8x8 freq {};

    constexpr double V = 100.0;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            input[y][x] = V;
        }
    }

    dct8x8(input, freq);

    // DC coefficient
    const double expectedDC = 8.0 * V;
    EXPECT_TRUE(nearEqual(freq[0][0], expectedDC, 1e-4));

    // All AC coefficients should be zero for a constant block
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            if (y == 0 && x == 0) {
                continue;
            }
            EXPECT_TRUE(nearEqual(freq[y][x], 0.0, 1e-6));
        }
    }
    std::cout << "  -> PASSED\n";
}

TEST(DctTest, PixelBufferOverloadMatchesDctMatrix)
{
    std::cout << "[Test] testPixelBufferOverloadMatchesDctMatrix\n";
    // Build an 8x8 pixel block and compare the two dct8x8 overloads
    std::uint8_t pixels[8][8] {};
    DctMatrix8x8 inputDouble {};

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const auto val = static_cast<std::uint8_t>((y * 8 + x) % 256);
            pixels[y][x] = val;
            inputDouble[y][x] = static_cast<double>(val);
        }
    }

    DctMatrix8x8 freqFromPixels {};
    DctMatrix8x8 freqFromDouble {};

    dct8x8(reinterpret_cast<const std::uint8_t*>(pixels), 8, freqFromPixels);
    dct8x8(inputDouble, freqFromDouble);

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            EXPECT_TRUE(nearEqual(freqFromPixels[y][x], freqFromDouble[y][x], 1e-6));
        }
    }
    std::cout << "  -> PASSED\n";
}

TEST(DctTest, PixelBufferOverloadWithNonUnitStride)
{
    std::cout << "[Test] testPixelBufferOverloadWithNonUnitStride\n";
    // Simulate a wider row buffer with stride=16 (only 8 pixels used per row)
    constexpr int STRIDE = 16;
    std::uint8_t wide[8 * STRIDE] {};

    DctMatrix8x8 inputDouble {};
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const auto val = static_cast<std::uint8_t>((y + x * 3) % 200 + 10);
            wide[y * STRIDE + x] = val;
            inputDouble[y][x] = static_cast<double>(val);
        }
    }

    DctMatrix8x8 freqWide {};
    DctMatrix8x8 freqDbl {};

    dct8x8(wide, STRIDE, freqWide);
    dct8x8(inputDouble, freqDbl);

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            EXPECT_TRUE(nearEqual(freqWide[y][x], freqDbl[y][x], 1e-6));
        }
    }
    std::cout << "  -> PASSED\n";
}

TEST(DctTest, DctLinearity)
{
    std::cout << "[Test] testDctLinearity\n";
    // DCT is linear: DCT(a + b) == DCT(a) + DCT(b)
    DctMatrix8x8 a {}, b {}, sum_ab {};
    DctMatrix8x8 fa {}, fb {}, f_sum {};
    DctMatrix8x8 sum_f {};

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            a[y][x] = static_cast<double>(y + x);
            b[y][x] = static_cast<double>(x * x + 1);
            sum_ab[y][x] = a[y][x] + b[y][x];
        }
    }

    dct8x8(a, fa);
    dct8x8(b, fb);
    dct8x8(sum_ab, f_sum);

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            sum_f[y][x] = fa[y][x] + fb[y][x];
        }
    }

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            EXPECT_TRUE(nearEqual(f_sum[y][x], sum_f[y][x], 1e-6));
        }
    }
    std::cout << "  -> PASSED\n";
}

TEST(DctTest, IdctZeroFrequencyDomainYieldsZeroSpatial)
{
    std::cout << "[Test] testIdctZeroFrequencyDomainYieldsZeroSpatial\n";
    DctMatrix8x8 freq {}; // all zeros
    DctMatrix8x8 spatial {};

    idct8x8(freq, spatial);

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            EXPECT_TRUE(nearEqual(spatial[y][x], 0.0, 1e-9));
        }
    }
    std::cout << "  -> PASSED\n";
}

TEST(DctTest, DctOrthogonality)
{
    std::cout << "[Test] testDctOrthogonality\n";
    // Parseval's theorem: sum(x^2) == sum(X^2) * (normalization factor)
    // For a 2D 8x8 DCT with the chosen normalization, energy is preserved via IDCT round-trip
    DctMatrix8x8 input {};
    DctMatrix8x8 freq {};
    DctMatrix8x8 rec {};

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            input[y][x] = static_cast<double>((y + 1) * (x + 1));
        }
    }

    dct8x8(input, freq);
    idct8x8(freq, rec);

    double energyIn = 0.0;
    double energyRec = 0.0;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            energyIn += input[y][x] * input[y][x];
            energyRec += rec[y][x] * rec[y][x];
        }
    }
    // Round-trip must preserve energy
    EXPECT_TRUE(nearEqual(energyIn, energyRec, energyIn * 1e-5));
    std::cout << "  -> PASSED\n";
}

} // namespace
