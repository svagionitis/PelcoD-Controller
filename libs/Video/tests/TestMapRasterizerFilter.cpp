#include "MapRasterizerFilter.h"
#include <gtest/gtest.h>
#include <numeric>
#include <vector>

using namespace Video;
using namespace Video::Filters;

TEST(TestMapRasterizerFilter, DefaultStateAndProperties) {
    MapRasterizerFilter filter(300, 220);

    EXPECT_TRUE(filter.isEnabled());
    EXPECT_EQ(filter.corner(), MapRasterizerFilter::InsetCorner::BottomRight);
    EXPECT_EQ(filter.insetWidth(), 300);
    EXPECT_EQ(filter.insetHeight(), 220);
    EXPECT_NEAR(filter.opacity(), 0.85, 1e-4);

    filter.setCorner(MapRasterizerFilter::InsetCorner::TopLeft);
    EXPECT_EQ(filter.corner(), MapRasterizerFilter::InsetCorner::TopLeft);

    filter.setOpacity(0.5);
    EXPECT_NEAR(filter.opacity(), 0.5, 1e-4);

    filter.setZoom(15.0);
    EXPECT_NEAR(filter.zoom(), 15.0, 1e-4);
}

TEST(TestMapRasterizerFilter, InsetModifiesFrameWhenEnabled) {
    MapRasterizerFilter filter(200, 150);
    filter.setCorner(MapRasterizerFilter::InsetCorner::BottomRight);

    constexpr int kWidth = 640;
    constexpr int kHeight = 480;
    std::vector<std::uint8_t> frame(kWidth * kHeight * 3, 0); // Black frame

    filter.setPlatformTelemetry({ 37.7749, -122.4194 }, 45.0);
    filter.setTargetPosition({ 37.78, -122.41 });

    Klv::FrustumCorners frustum;
    frustum.topLeft = { 37.785, -122.42 };
    frustum.topRight = { 37.785, -122.41 };
    frustum.bottomRight = { 37.775, -122.41 };
    frustum.bottomLeft = { 37.775, -122.42 };
    filter.setFrustum(frustum);

    filter.process(frame.data(), kWidth, kHeight, PixelFormat::RGB24);

    // Bottom-right corner where mini-map was placed should have non-zero pixel data
    const std::size_t sum = std::accumulate(frame.begin(), frame.end(), std::size_t { 0 });
    EXPECT_GT(sum, 0U);

    // Top-left corner (0, 0) should still be black (0)
    EXPECT_EQ(frame[0], 0);
    EXPECT_EQ(frame[1], 0);
    EXPECT_EQ(frame[2], 0);
}

TEST(TestMapRasterizerFilter, DisabledFilterLeavesFrameUntouched) {
    MapRasterizerFilter filter(200, 150);
    filter.setEnabled(false);

    constexpr int kWidth = 640;
    constexpr int kHeight = 480;
    std::vector<std::uint8_t> frame(kWidth * kHeight * 3, 0);

    filter.process(frame.data(), kWidth, kHeight, PixelFormat::RGB24);

    const std::size_t sum = std::accumulate(frame.begin(), frame.end(), std::size_t { 0 });
    EXPECT_EQ(sum, 0U);
}
