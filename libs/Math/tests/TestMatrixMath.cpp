/// @file TestMatrixMath.cpp
/// @brief Unit tests for MatrixMath.h: fixed-size Matrix and Vector templates, arithmetic, transpose,
///        Gauss-Jordan inversion (invertible and singular), Cholesky decomposition, and Vector products.

#include <cmath>
#include <cstddef>

#include <gtest/gtest.h>

#include "MatrixMath.h"

using namespace Math;

namespace {

constexpr double kTol = 1e-9;

TEST(MatrixMathTest, DefaultZeroAndIdentityConstruction)
{
    const Matrix<3U, 3U> zero {};
    const Matrix<3U, 3U> ident = Matrix<3U, 3U>::identity();

    for (std::size_t r = 0U; r < 3U; ++r) {
        for (std::size_t c = 0U; c < 3U; ++c) {
            EXPECT_NEAR(zero(r, c), 0.0, kTol);
            const double expected = (r == c) ? 1.0 : 0.0;
            EXPECT_NEAR(ident(r, c), expected, kTol);
        }
    }
}

TEST(MatrixMathTest, InitializerListAndPadding)
{
    const Matrix<2U, 3U> m { { 1.0, 2.0 }, { 3.0, 4.0, 5.0 } };
    EXPECT_NEAR(m(0U, 0U), 1.0, kTol);
    EXPECT_NEAR(m(0U, 1U), 2.0, kTol);
    EXPECT_NEAR(m(0U, 2U), 0.0, kTol); // zero-padded
    EXPECT_NEAR(m(1U, 0U), 3.0, kTol);
    EXPECT_NEAR(m(1U, 1U), 4.0, kTol);
    EXPECT_NEAR(m(1U, 2U), 5.0, kTol);
}

TEST(MatrixMathTest, AdditionSubtractionAndScalarMultiply)
{
    const Matrix<2U, 2U> a { { 1.0, 2.0 }, { 3.0, 4.0 } };
    const Matrix<2U, 2U> b { { 5.0, 6.0 }, { 7.0, 8.0 } };

    const auto sum = a + b;
    const auto diff = b - a;
    const auto scaledRight = a * 2.5;
    const auto scaledLeft = 2.5 * a;

    EXPECT_NEAR(sum(0U, 0U), 6.0, kTol);
    EXPECT_NEAR(sum(1U, 1U), 12.0, kTol);
    EXPECT_NEAR(diff(0U, 0U), 4.0, kTol);
    EXPECT_NEAR(diff(1U, 1U), 4.0, kTol);
    EXPECT_NEAR(scaledRight(0U, 1U), 5.0, kTol);
    EXPECT_NEAR(scaledLeft(1U, 0U), 7.5, kTol);
}

TEST(MatrixMathTest, RectangularMultiplicationAndTranspose)
{
    const Matrix<2U, 3U> a { { 1.0, 2.0, 3.0 }, { 4.0, 5.0, 6.0 } };
    const Matrix<3U, 2U> aT = a.transpose();

    EXPECT_NEAR(aT(0U, 1U), 4.0, kTol);
    EXPECT_NEAR(aT(2U, 0U), 3.0, kTol);

    const Matrix<2U, 2U> prod = a * aT;
    // Row 0 dot Row 0 = 1 + 4 + 9 = 14
    // Row 0 dot Row 1 = 4 + 10 + 18 = 32
    // Row 1 dot Row 1 = 16 + 25 + 36 = 77
    EXPECT_NEAR(prod(0U, 0U), 14.0, kTol);
    EXPECT_NEAR(prod(0U, 1U), 32.0, kTol);
    EXPECT_NEAR(prod(1U, 0U), 32.0, kTol);
    EXPECT_NEAR(prod(1U, 1U), 77.0, kTol);
}

TEST(MatrixMathTest, InverseAndTryInverse)
{
    const Matrix<3U, 3U> a { { 4.0, 7.0, 2.0 }, { 3.0, 6.0, 1.0 }, { 2.0, 5.0, 3.0 } };
    const auto maybeInv = a.tryInverse();
    ASSERT_TRUE(maybeInv.has_value());

    const Matrix<3U, 3U> inv = a.inverse();
    const Matrix<3U, 3U> ident = a * inv;

    for (std::size_t r = 0U; r < 3U; ++r) {
        for (std::size_t c = 0U; c < 3U; ++c) {
            const double expected = (r == c) ? 1.0 : 0.0;
            EXPECT_NEAR(ident(r, c), expected, 1e-9);
        }
    }

    // Singular matrix: row 1 is 2x row 0
    const Matrix<2U, 2U> singular { { 1.0, 2.0 }, { 2.0, 4.0 } };
    EXPECT_FALSE(singular.tryInverse().has_value());

    // Fallback inverse() returns identity on singular matrix
    const Matrix<2U, 2U> fallback = singular.inverse();
    EXPECT_NEAR(fallback(0U, 0U), 1.0, kTol);
    EXPECT_NEAR(fallback(0U, 1U), 0.0, kTol);
    EXPECT_NEAR(fallback(1U, 0U), 0.0, kTol);
    EXPECT_NEAR(fallback(1U, 1U), 1.0, kTol);
}

TEST(MatrixMathTest, CholeskyDecomposition)
{
    // Symmetric positive-definite matrix: A = [4 12 -16; 12 37 -43; -16 -43 98]
    // Exact lower Cholesky factor L = [2 0 0; 6 1 0; -8 5 3]
    const Matrix<3U, 3U> spd { { 4.0, 12.0, -16.0 }, { 12.0, 37.0, -43.0 }, { -16.0, -43.0, 98.0 } };
    const Matrix<3U, 3U> L = spd.cholesky();

    EXPECT_NEAR(L(0U, 0U), 2.0, kTol);
    EXPECT_NEAR(L(1U, 0U), 6.0, kTol);
    EXPECT_NEAR(L(1U, 1U), 1.0, kTol);
    EXPECT_NEAR(L(2U, 0U), -8.0, kTol);
    EXPECT_NEAR(L(2U, 1U), 5.0, kTol);
    EXPECT_NEAR(L(2U, 2U), 3.0, kTol);

    const Matrix<3U, 3U> reconstructed = L * L.transpose();
    for (std::size_t r = 0U; r < 3U; ++r) {
        for (std::size_t c = 0U; c < 3U; ++c) {
            EXPECT_NEAR(reconstructed(r, c), spd(r, c), 1e-8);
        }
    }
}

TEST(MatrixMathTest, VectorOperations)
{
    const Vector<3U> u { 3.0, 4.0, 0.0 };
    const Vector<3U> v { 1.0, 2.0, 3.0 };

    EXPECT_NEAR(u.norm(), 5.0, kTol);
    EXPECT_NEAR(u.dot(v), 11.0, kTol);

    const Vector<2U> w { 2.0, -1.0 };
    const Matrix<3U, 2U> outerProd = u.outer(w);
    EXPECT_NEAR(outerProd(0U, 0U), 6.0, kTol);
    EXPECT_NEAR(outerProd(0U, 1U), -3.0, kTol);
    EXPECT_NEAR(outerProd(1U, 0U), 8.0, kTol);
    EXPECT_NEAR(outerProd(1U, 1U), -4.0, kTol);
    EXPECT_NEAR(outerProd(2U, 0U), 0.0, kTol);
}

} // namespace
