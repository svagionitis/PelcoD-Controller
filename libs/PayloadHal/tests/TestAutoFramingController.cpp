#include "AutoFramingController.h"
#include "sim/SimulatedPayload.h"

#include <cmath>
#include <gtest/gtest.h>

namespace PayloadHal {
namespace {

    TEST(TestAutoFramingController, DesiredHfovAnalyticalMath)
    {
        // Target: 20 meters long, range = 1000 meters, desired occupancy = 20% (0.20)
        const double rangeMeters = 1000.0;
        const double targetDimMeters = 20.0;
        const double occupancy = 0.20;

        const double hfov = AutoFramingController::calculateDesiredHfov(rangeMeters, targetDimMeters, occupancy);

        // Angular span = 2 * atan2(10, 1000) * 180 / pi = 1.1459 deg
        // Desired HFOV = 1.1459 / 0.20 = 5.7296 deg
        EXPECT_NEAR(hfov, 5.73, 0.05);
    }

    TEST(TestAutoFramingController, NormalizedZoomScaling)
    {
        constexpr double wideHfov = 60.0;
        constexpr double teleHfov = 2.0;
        constexpr double occupancy = 0.30;
        constexpr double targetLength = 15.0;

        // Very close target (30 meters away): requires full wide zoom
        const double zoomClose
            = AutoFramingController::calculateNormalizedZoom(30.0, targetLength, occupancy, wideHfov, teleHfov);
        EXPECT_NEAR(zoomClose, 0.0, 0.05);

        // Extremely distant target (10,000 meters away): requires maximum optical tele
        const double zoomDistant
            = AutoFramingController::calculateNormalizedZoom(10000.0, targetLength, occupancy, wideHfov, teleHfov);
        EXPECT_NEAR(zoomDistant, 1.0, 0.05);

        // Medium distance (1000m): intermediate normalized zoom
        const double zoomMedium
            = AutoFramingController::calculateNormalizedZoom(1000.0, targetLength, occupancy, wideHfov, teleHfov);
        EXPECT_GT(zoomMedium, 0.1);
        EXPECT_LT(zoomMedium, 0.9);
    }

    TEST(TestAutoFramingController, AspectAwareApparentWidthGeometry)
    {
        constexpr double length = 100.0;
        constexpr double beam = 20.0;

        // Broadside (90 deg): apparent width equals length
        const double wBroadside = AutoFramingController::calculateApparentWidth(length, beam, 90.0);
        EXPECT_NEAR(wBroadside, 100.0, 0.01);

        // Head-on (0 deg) and Stern-on (180 deg): apparent width equals beam
        const double wHeadOn = AutoFramingController::calculateApparentWidth(length, beam, 0.0);
        EXPECT_NEAR(wHeadOn, 20.0, 0.01);

        const double wSternOn = AutoFramingController::calculateApparentWidth(length, beam, 180.0);
        EXPECT_NEAR(wSternOn, 20.0, 0.01);

        // Quartering (45 deg): 100 * sin(45) + 20 * cos(45) = 120 / sqrt(2) = ~84.85m
        const double wQuarter = AutoFramingController::calculateApparentWidth(length, beam, 45.0);
        EXPECT_NEAR(wQuarter, 84.85, 0.05);

        // Quadrant symmetry (-45 deg and 135 deg should match 45 deg)
        const double wNegQuarter = AutoFramingController::calculateApparentWidth(length, beam, -45.0);
        EXPECT_NEAR(wNegQuarter, 84.85, 0.05);

        const double wQuarter135 = AutoFramingController::calculateApparentWidth(length, beam, 135.0);
        EXPECT_NEAR(wQuarter135, 84.85, 0.05);
    }

    TEST(TestAutoFramingController, ConstrainedFramingHfovWidthAndHeight)
    {
        constexpr double range = 1000.0;
        constexpr double widthRatio = 0.30;
        constexpr double heightRatio = 0.50;
        constexpr double aspectRatio = 16.0 / 9.0;

        // Scenario A: Tall target (20m wide, 40m tall) -> Governed by height (8.15 deg)
        const double hfovTall = AutoFramingController::calculateFramingHfov(
            range, 20.0, 40.0, widthRatio, heightRatio, aspectRatio);
        EXPECT_NEAR(hfovTall, 8.15, 0.1);

        // Scenario B: Wide target (40m wide, 10m tall) -> Governed by width (7.64 deg)
        const double hfovWide = AutoFramingController::calculateFramingHfov(
            range, 40.0, 10.0, widthRatio, heightRatio, aspectRatio);
        EXPECT_NEAR(hfovWide, 7.64, 0.1);
    }

    TEST(TestAutoFramingController, OpticalLensCurvesLogarithmicVsLinear)
    {
        constexpr double wide = 60.0;
        constexpr double tele = 2.0;

        // Wide boundary: both curves yield 0.0
        EXPECT_NEAR(AutoFramingController::calculateNormalizedZoom(
            60.0, wide, tele, LensZoomCurveType::LinearHfov), 0.0, 1e-4);
        EXPECT_NEAR(AutoFramingController::calculateNormalizedZoom(
            60.0, wide, tele, LensZoomCurveType::LogarithmicFocal), 0.0, 1e-4);

        // Tele boundary: both curves yield 1.0
        EXPECT_NEAR(AutoFramingController::calculateNormalizedZoom(
            2.0, wide, tele, LensZoomCurveType::LinearHfov), 1.0, 1e-4);
        EXPECT_NEAR(AutoFramingController::calculateNormalizedZoom(
            2.0, wide, tele, LensZoomCurveType::LogarithmicFocal), 1.0, 1e-4);

        // Mid-HFOV (e.g. 10 deg): logarithmic zoom concentrates travel towards telephoto
        const double zLinear = AutoFramingController::calculateNormalizedZoom(
            10.0, wide, tele, LensZoomCurveType::LinearHfov);
        const double zLog = AutoFramingController::calculateNormalizedZoom(
            10.0, wide, tele, LensZoomCurveType::LogarithmicFocal);

        EXPECT_GT(zLinear, 0.0);
        EXPECT_LT(zLinear, 1.0);
        EXPECT_GT(zLog, 0.0);
        EXPECT_LT(zLog, 1.0);
        EXPECT_NE(zLinear, zLog);
    }

    TEST(TestAutoFramingController, AisTargetEnvelopeEstimation)
    {
        AutoFramingController framing;

        // Valid AIS dimensions: toBow=70, toStern=30, toPort=10, toStarboard=10 -> Length=100m, Beam=20m
        Nmea::AisDimensions dims;
        dims.toBow = 70U;
        dims.toStern = 30U;
        dims.toPort = 10U;
        dims.toStarboard = 10U;

        const auto envDims = framing.estimateTargetEnvelope(dims);
        EXPECT_DOUBLE_EQ(envDims.lengthMeters, 100.0);
        EXPECT_DOUBLE_EQ(envDims.beamMeters, 20.0);

        // Fallback with ship type: Cargo (type 70)
        Nmea::AisDimensions emptyDims {};
        const auto envCargo = framing.estimateTargetEnvelope(emptyDims, static_cast<std::uint8_t>(70));
        EXPECT_DOUBLE_EQ(envCargo.lengthMeters, 140.0);
        EXPECT_DOUBLE_EQ(envCargo.beamMeters, 22.0);

        // Fallback with ship type: Tug (type 52)
        const auto envTug = framing.estimateTargetEnvelope(emptyDims, static_cast<std::uint8_t>(52));
        EXPECT_DOUBLE_EQ(envTug.lengthMeters, 32.0);
        EXPECT_DOUBLE_EQ(envTug.beamMeters, 10.0);
    }

    TEST(TestAutoFramingController, ZoomSchedulingAndVelocityProfiler)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());
        auto camera = payload->primaryCamera();
        ASSERT_NE(camera, nullptr);

        AutoFramingConfig cfg;
        cfg.maxZoomVelocityPerSec = 0.50; // 50% per second
        cfg.zoomConvergenceTolerance01 = 0.02;

        AutoFramingController framing(cfg);

        TargetPhysicalEnvelope env { 50.0, 10.0, 8.0 };
        // Schedule framing at 2000m
        const bool scheduled = framing.scheduleFraming(*camera, 2000.0, env, 90.0, 0.0);
        EXPECT_TRUE(scheduled);
        EXPECT_FALSE(framing.isZoomConverged());
        EXPECT_GT(framing.targetZoom(), 0.1);

        const double initialTarget = framing.targetZoom();

        // Step by 500ms -> zoom should travel approx 25% (0.50 * 0.5)
        framing.update(*camera, std::chrono::milliseconds(500));
        EXPECT_GT(framing.currentZoom(), 0.1);
        EXPECT_LE(framing.currentZoom(), initialTarget);

        // Step by 2000ms -> should fully converge to targetZoom
        framing.update(*camera, std::chrono::milliseconds(2000));
        EXPECT_TRUE(framing.isZoomConverged());
        EXPECT_NEAR(framing.currentZoom(), initialTarget, 0.02);
    }

    TEST(TestAutoFramingController, HysteresisDeadbandRejection)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());
        auto camera = payload->primaryCamera();
        ASSERT_NE(camera, nullptr);

        AutoFramingConfig cfg;
        cfg.rangeHysteresisRatio = 0.06; // 6%
        cfg.zoomDeadband01 = 0.025;      // 2.5%

        AutoFramingController framing(cfg);
        TargetPhysicalEnvelope env { 30.0, 8.0, 5.0 };

        // Initial target at 1000m
        EXPECT_TRUE(framing.scheduleFraming(*camera, 1000.0, env, 90.0, 0.0));
        const double targetZoom1 = framing.targetZoom();

        // Settle zoom
        framing.update(*camera, std::chrono::milliseconds(4000));
        EXPECT_TRUE(framing.isZoomConverged());

        // Range jitter: 1030m (3% change, below 6% threshold)
        EXPECT_TRUE(framing.scheduleFraming(*camera, 1030.0, env, 90.0, 0.0));
        // Target zoom should NOT be updated, remains settled
        EXPECT_DOUBLE_EQ(framing.targetZoom(), targetZoom1);
        EXPECT_TRUE(framing.isZoomConverged());

        // Significant range change: 1250m (25% change, exceeds 6% threshold)
        EXPECT_TRUE(framing.scheduleFraming(*camera, 1250.0, env, 90.0, 0.0));
        // New target zoom scheduled, zoom convergence reset
        EXPECT_FALSE(framing.isZoomConverged());
        EXPECT_NE(framing.targetZoom(), targetZoom1);
    }

    TEST(TestAutoFramingController, FrameTargetCameraIntegration)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        auto camera = payload->primaryCamera();
        ASSERT_NE(camera, nullptr);

        AutoFramingController framing;

        // Frame target at 1500m
        const bool framed = framing.frameTarget(*camera, 1500.0, 25.0);
        EXPECT_TRUE(framed);

        const auto telemetry = camera->currentTelemetry();
        EXPECT_GT(telemetry.normalizedZoom, 0.0);
        EXPECT_LE(telemetry.normalizedZoom, 1.0);
    }

} // namespace
} // namespace PayloadHal
