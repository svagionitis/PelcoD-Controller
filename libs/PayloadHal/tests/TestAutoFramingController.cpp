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
