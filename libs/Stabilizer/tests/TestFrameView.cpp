/// @file TestFrameView.cpp
/// @brief Unit tests for FrameView and FrameBuffer memory structures.
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "FrameBuffer.h"
#include "FrameView.h"
#include "StabilizerTypes.h"

using namespace Stabilizer;

TEST(TestFrameView, PixelClampedAccess)
{
    std::vector<std::uint8_t> data(100U * 100U, 42U);
    data[0] = 10U; // Top-left
    data[99] = 20U; // Top-right
    data[99U * 100U] = 30U; // Bottom-left
    data[(99U * 100U) + 99U] = 40U; // Bottom-right

    FrameDimensions dims { 100U, 100U };
    FrameView view { data.data(), dims };

    EXPECT_FALSE(view.isEmpty());
    EXPECT_EQ(view.width(), 100U);
    EXPECT_EQ(view.height(), 100U);

    // In-bounds
    EXPECT_EQ(view.pixelClamped(0, 0), 10U);
    EXPECT_EQ(view.pixelClamped(99, 0), 20U);
    EXPECT_EQ(view.pixelClamped(0, 99), 30U);
    EXPECT_EQ(view.pixelClamped(99, 99), 40U);
    EXPECT_EQ(view.pixelClamped(50, 50), 42U);

    // Negative out-of-bounds clamped to edge
    EXPECT_EQ(view.pixelClamped(-5, -5), 10U);
    EXPECT_EQ(view.pixelClamped(200, -5), 20U);
    EXPECT_EQ(view.pixelClamped(-5, 200), 30U);
    EXPECT_EQ(view.pixelClamped(200, 200), 40U);
}

TEST(TestFrameView, SubViewSlicing)
{
    std::vector<std::uint8_t> data(200U * 100U, 0U);
    FrameDimensions dims { 200U, 100U };
    FrameView view { data.data(), dims };

    FrameView sub = view.subView(10U, 20U, 50U, 30U);
    EXPECT_FALSE(sub.isEmpty());
    EXPECT_EQ(sub.width(), 50U);
    EXPECT_EQ(sub.height(), 30U);
    EXPECT_EQ(sub.stride(), 200U);

    // Out of bounds subview
    FrameView invalidSub = view.subView(180U, 20U, 50U, 30U);
    EXPECT_TRUE(invalidSub.isEmpty());
}

TEST(TestFrameView, FrameBufferLifecycle)
{
    FrameDimensions dims { 128U, 128U };
    FrameBuffer buffer { dims };

    ASSERT_TRUE(buffer.isValid());
    EXPECT_NE(buffer.data(), nullptr);
    EXPECT_TRUE(buffer.view().isAligned(64U));

    buffer.fillZero();
    EXPECT_EQ(buffer.view().pixelClamped(10, 10), 0U);

    buffer.rowPtr(10)[10] = 255U;
    EXPECT_EQ(buffer.view().pixelClamped(10, 10), 255U);

    // Move construction
    FrameBuffer moved { std::move(buffer) };
    EXPECT_TRUE(moved.isValid());
    EXPECT_FALSE(buffer.isValid());
    EXPECT_EQ(moved.view().pixelClamped(10, 10), 255U);
}
