#include "PayloadHal.h"
#include "SensorParallaxCompensator.h"
#include "sim/SimulatedPayload.h"
#include <gtest/gtest.h>
#include <cmath>

namespace PayloadHal {
namespace {

TEST(TestSensorParallaxCompensator, ZeroBaselineInfinityConvergence)
{
    SensorOffset3D zeroBaseline { 0.0, 0.0, 0.0 };
    BoresightCalibration zeroBoresight { 0.0, 0.0, 0.0 };
    SensorParallaxCompensator comp(zeroBaseline, zeroBoresight);

    // At 50m, 500m, 50000m
    const auto disp50 = comp.computeDisparity(50.0, 30.0, 20.0);
    EXPECT_NEAR(disp50.azimuthDisparityDeg, 0.0, 1e-6);
    EXPECT_NEAR(disp50.elevationDisparityDeg, 0.0, 1e-6);
    EXPECT_NEAR(disp50.pixelDeltaX, 0.0, 1e-6);
    EXPECT_NEAR(disp50.pixelDeltaY, 0.0, 1e-6);

    const auto disp5000 = comp.computeDisparity(5000.0, 10.0, 6.0);
    EXPECT_NEAR(disp5000.azimuthDisparityDeg, 0.0, 1e-6);
    EXPECT_NEAR(disp5000.elevationDisparityDeg, 0.0, 1e-6);
}

TEST(TestSensorParallaxCompensator, StaticBoresightCalibration)
{
    SensorOffset3D zeroBaseline { 0.0, 0.0, 0.0 };
    BoresightCalibration boresight { 0.25, -0.10, 0.0 };
    SensorParallaxCompensator comp(zeroBaseline, boresight);

    // At any range, angular disparity should equal boresight offset
    const auto disp100 = comp.computeDisparity(100.0, 30.0, 20.0);
    EXPECT_NEAR(disp100.azimuthDisparityDeg, 0.25, 1e-6);
    EXPECT_NEAR(disp100.elevationDisparityDeg, -0.10, 1e-6);

    const auto disp1000 = comp.computeDisparity(1000.0, 30.0, 20.0);
    EXPECT_NEAR(disp1000.azimuthDisparityDeg, 0.25, 1e-6);
    EXPECT_NEAR(disp1000.elevationDisparityDeg, -0.10, 1e-6);
}

TEST(TestSensorParallaxCompensator, FiniteRangeParallaxScaling)
{
    // 20 cm lateral baseline (bx = 0.20m)
    SensorOffset3D baseline { 0.20, 0.0, 0.0 };
    SensorParallaxCompensator comp(baseline);

    // Expected at 10m: atan2(0.2, 10) * 180 / pi ~ 1.145763°
    const double expected10 = std::atan2(0.20, 10.0) * (180.0 / 3.14159265358979323846);
    const auto disp10 = comp.computeDisparity(10.0, 30.0, 20.0);
    EXPECT_NEAR(disp10.azimuthDisparityDeg, expected10, 0.001);

    // Expected at 100m: atan2(0.2, 100) * 180 / pi ~ 0.11459°
    const double expected100 = std::atan2(0.20, 100.0) * (180.0 / 3.14159265358979323846);
    const auto disp100 = comp.computeDisparity(100.0, 30.0, 20.0);
    EXPECT_NEAR(disp100.azimuthDisparityDeg, expected100, 0.0005);

    // Expected at 1000m: atan2(0.2, 1000) * 180 / pi ~ 0.011459°
    const double expected1000 = std::atan2(0.20, 1000.0) * (180.0 / 3.14159265358979323846);
    const auto disp1000 = comp.computeDisparity(1000.0, 30.0, 20.0);
    EXPECT_NEAR(disp1000.azimuthDisparityDeg, expected1000, 0.0001);

    // Disparity must strictly decrease as range increases
    EXPECT_GT(disp10.azimuthDisparityDeg, disp100.azimuthDisparityDeg);
    EXPECT_GT(disp100.azimuthDisparityDeg, disp1000.azimuthDisparityDeg);
}

TEST(TestSensorParallaxCompensator, PixelAndNormalizedScreenDisparity)
{
    SensorOffset3D baseline { 0.15, -0.05, 0.0 }; // 15cm right, 5cm down
    SensorParallaxCompensator comp(baseline);

    const auto disp = comp.computeDisparity(50.0, 15.0, 8.5, 1920, 1080);

    // Azimuth is positive (right) -> normalized screen X should be positive
    EXPECT_GT(disp.azimuthDisparityDeg, 0.0);
    EXPECT_GT(disp.normalizedScreenDeltaX, 0.0);
    EXPECT_GT(disp.pixelDeltaX, 0.0);

    // Elevation is negative (down) -> normalized screen Y should be positive (screen Y points down)
    EXPECT_LT(disp.elevationDisparityDeg, 0.0);
    EXPECT_GT(disp.normalizedScreenDeltaY, 0.0);
    EXPECT_GT(disp.pixelDeltaY, 0.0);
}

TEST(TestSensorParallaxCompensator, CrossSpectrumBoundingBoxTransfer)
{
    SensorOffset3D baseline { 0.20, 0.0, 0.0 };
    SensorParallaxCompensator comp(baseline);

    ScreenRect2D primBox { 500.0, 400.0, 100.0, 80.0 };

    // Primary HFOV = 10°, Secondary HFOV = 20° (Secondary has 2x wider FOV, so box width should be half)
    const auto secBox = comp.transformBoundingBox(primBox, 100.0, 10.0, 6.0, 20.0, 12.0);

    // Scale should be 10 / 20 = 0.5
    EXPECT_NEAR(secBox.width, 50.0, 0.01);
    EXPECT_NEAR(secBox.height, 40.0, 0.01);

    // X position should be shifted by pixel disparity
    const auto disp = comp.computeDisparity(100.0, 10.0, 6.0);
    EXPECT_NEAR(secBox.x, (500.0 * 0.5) + disp.pixelDeltaX, 0.01);
}

TEST(TestSensorParallaxCompensator, ReticleConvergenceAndPointMapping)
{
    SensorOffset3D baseline { 0.10, 0.05, 0.0 };
    SensorParallaxCompensator comp(baseline);

    const auto reticle = comp.computeReticleOffset(80.0, 25.0, 15.0);
    EXPECT_NE(reticle.first, 0.0);
    EXPECT_NE(reticle.second, 0.0);

    // Center point (0, 0) mapped from primary to secondary
    const auto mapped = comp.mapPointPrimaryToSecondary(0.0, 0.0, 80.0, 25.0, 15.0, 25.0, 15.0);
    EXPECT_NEAR(mapped.first, -reticle.first, 0.005);
}

TEST(TestSensorParallaxCompensator, LrfBeamConvergence)
{
    SensorOffset3D baseline { 0.0, -0.15, 0.0 }; // LRF 15cm below optical center
    SensorParallaxCompensator comp(baseline);

    // At 100m, laser points slightly upward to hit optical aimpoint
    const auto lrfCorrection = comp.computeLrfConvergenceAngles(100.0);
    EXPECT_NEAR(lrfCorrection.first, 0.0, 1e-5); // no lateral offset

    const double expectedEl = std::atan2(-0.15, 100.0) * (180.0 / 3.14159265358979323846);
    EXPECT_NEAR(lrfCorrection.second, expectedEl, 0.001);
}

TEST(TestSensorParallaxCompensator, SimulatedPayloadIntegration)
{
    auto payload = PayloadFactory::createSimulatedPayload();
    ASSERT_NE(payload, nullptr);

    auto comp = payload->parallaxCompensator();
    ASSERT_NE(comp, nullptr);

    const auto offset = comp->baselineOffset();
    EXPECT_NEAR(offset.lateralOffsetM, 0.15, 0.01); // default 15cm bench baseline

    const auto disp = comp->computeDisparity(100.0, 30.0, 20.0);
    EXPECT_GT(disp.azimuthDisparityDeg, 0.0);
}

} // namespace
} // namespace PayloadHal
