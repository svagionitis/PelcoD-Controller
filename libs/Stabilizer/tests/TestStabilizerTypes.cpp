/// @file TestStabilizerTypes.cpp
/// @brief Unit tests for StabilizerTypes domain objects and affine algebra.
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include "StabilizerTypes.h"

using namespace Stabilizer;

TEST(TestStabilizerTypes, FrameDimensionsValidation)
{
    FrameDimensions invalidSmall { 8U, 8U };
    EXPECT_FALSE(invalidSmall.isValid());

    FrameDimensions invalidLarge { 8000U, 5000U };
    EXPECT_FALSE(invalidLarge.isValid());

    FrameDimensions validHd { 1920U, 1080U };
    EXPECT_TRUE(validHd.isValid());
    EXPECT_EQ(validHd.width(), 1920U);
    EXPECT_EQ(validHd.height(), 1080U);
    EXPECT_EQ(validHd.stride(), 1920U);
    EXPECT_EQ(validHd.totalBytes(), 1920U * 1080U);

    FrameDimensions withStride { 1920U, 1080U, 2048U };
    EXPECT_TRUE(withStride.isValid());
    EXPECT_EQ(withStride.stride(), 2048U);
    EXPECT_EQ(withStride.totalBytes(), 2048U * 1080U);

    FrameDimensions invalidStride { 1920U, 1080U, 1000U };
    EXPECT_FALSE(invalidStride.isValid());
}

TEST(TestStabilizerTypes, CropRatioClamping)
{
    CropRatio negative { -0.1F };
    EXPECT_FLOAT_EQ(negative.value(), 0.0F);

    CropRatio excessive { 0.5F };
    EXPECT_FLOAT_EQ(excessive.value(), 0.30F);

    CropRatio valid { 0.12F };
    EXPECT_FLOAT_EQ(valid.value(), 0.12F);
}

TEST(TestStabilizerTypes, SmoothingFactorClamping)
{
    SmoothingFactor tooLow { 0.001F };
    EXPECT_FLOAT_EQ(tooLow.value(), 0.01F);

    SmoothingFactor tooHigh { 1.5F };
    EXPECT_FLOAT_EQ(tooHigh.value(), 1.0F);

    SmoothingFactor normal { 0.85F };
    EXPECT_FLOAT_EQ(normal.value(), 0.85F);
}

TEST(TestStabilizerTypes, SubPixelPointDistance)
{
    SubPixelPoint p1 { 10.0F, 20.0F };
    SubPixelPoint p2 { 13.0F, 24.0F };

    EXPECT_FLOAT_EQ(p1.distanceSquared(p2), 25.0F);
    EXPECT_TRUE(p1.isValid());

    SubPixelPoint invalid { -1.0F, 5.0F };
    EXPECT_FALSE(invalid.isValid());
}

TEST(TestStabilizerTypes, AffineIdentityAndTranslation)
{
    AffineTransform2D id = AffineTransform2D::identity();
    SubPixelPoint pt { 50.0F, 100.0F };
    SubPixelPoint transformed = id.transform(pt);

    EXPECT_FLOAT_EQ(transformed.x, 50.0F);
    EXPECT_FLOAT_EQ(transformed.y, 100.0F);

    AffineTransform2D trans = AffineTransform2D::translation(5.0F, -10.0F);
    SubPixelPoint ptMoved = trans.transform(pt);
    EXPECT_FLOAT_EQ(ptMoved.x, 55.0F);
    EXPECT_FLOAT_EQ(ptMoved.y, 90.0F);
}

TEST(TestStabilizerTypes, AffineInversionAndComposition)
{
    AffineTransform2D t1 = AffineTransform2D::translation(12.5F, -8.2F);
    AffineTransform2D inv { };
    ASSERT_TRUE(t1.invert(inv));

    AffineTransform2D composed = t1.compose(inv);
    EXPECT_NEAR(composed.a11, 1.0F, 1e-5F);
    EXPECT_NEAR(composed.a12, 0.0F, 1e-5F);
    EXPECT_NEAR(composed.tx, 0.0F, 1e-5F);
    EXPECT_NEAR(composed.a21, 0.0F, 1e-5F);
    EXPECT_NEAR(composed.a22, 1.0F, 1e-5F);
    EXPECT_NEAR(composed.ty, 0.0F, 1e-5F);

    // Singular matrix test
    AffineTransform2D singular { 0.0F, 0.0F, 10.0F, 0.0F, 0.0F, 20.0F };
    AffineTransform2D invSingular { };
    EXPECT_FALSE(singular.invert(invSingular));
}
