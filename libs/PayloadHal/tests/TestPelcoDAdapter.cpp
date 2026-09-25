#include "Klv/KlvTypes.h"
#include "MockPelcoDDevice.h"
#include "PayloadFactory.h"
#include "PelcoDCore/PelcoDDevice.h"
#include "adapters/PelcoDPtzAdapter.h"
#include <gtest/gtest.h>

namespace PayloadHal {
namespace {

    TEST(TestPelcoDAdapter, LifecycleAndStabilizationGating)
    {
        auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
        auto pelcoDevice = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, 1U);
        PelcoDPtzAdapter adapter(pelcoDevice);

        EXPECT_FALSE(adapter.supportsStabilization());
        EXPECT_TRUE(adapter.setStabilizationMode(StabilizationMode::Disabled));
        EXPECT_FALSE(adapter.setStabilizationMode(StabilizationMode::RateStabilized));
        EXPECT_FALSE(adapter.setStabilizationMode(StabilizationMode::GeoHold));
        EXPECT_EQ(adapter.stabilizationMode(), StabilizationMode::Disabled);
        EXPECT_FALSE(adapter.zeroGyroDrift());

        double minPan { 0.0 };
        double maxPan { 0.0 };
        double minTilt { 0.0 };
        double maxTilt { 0.0 };
        EXPECT_TRUE(adapter.getLimits(minPan, maxPan, minTilt, maxTilt));
        EXPECT_DOUBLE_EQ(minPan, 0.0);
        EXPECT_DOUBLE_EQ(maxPan, 360.0);
        EXPECT_DOUBLE_EQ(minTilt, -90.0);
        EXPECT_DOUBLE_EQ(maxTilt, 90.0);
    }

    TEST(TestPelcoDAdapter, MotionCommands)
    {
        auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
        auto pelcoDevice = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, 1U);
        PelcoDPtzAdapter adapter(pelcoDevice);

        ASSERT_TRUE(adapter.connect());
        EXPECT_TRUE(adapter.isConnected());

        // Normalized velocity commands
        EXPECT_TRUE(adapter.setNormalizedVelocity(1.0f, -0.5f));
        EXPECT_TRUE(adapter.stopMotion());

        // Absolute angles
        EXPECT_TRUE(adapter.setAbsoluteAngles(180.0, -25.0));
        const GimbalTelemetry telem = adapter.currentTelemetry();
        EXPECT_DOUBLE_EQ(telem.panAngleDeg, 180.0);
        EXPECT_DOUBLE_EQ(telem.tiltAngleDeg, -25.0);

        // Relative nudge
        EXPECT_TRUE(adapter.setRelativeNudge(5.0, 2.0));
        const GimbalTelemetry nudgedTelem = adapter.currentTelemetry();
        EXPECT_DOUBLE_EQ(nudgedTelem.panAngleDeg, 185.0);
        EXPECT_DOUBLE_EQ(nudgedTelem.tiltAngleDeg, -23.0);

        // Presets
        EXPECT_TRUE(adapter.savePreset(1));
        EXPECT_TRUE(adapter.recallPreset(1));

        adapter.disconnect();
        EXPECT_FALSE(adapter.isConnected());
    }

    TEST(TestPelcoDAdapter, CompositePayloadGeoreferenceCoordinates)
    {
        auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
        auto pelcoDevice = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, 1U);
        auto composite = PayloadFactory::createPelcoDPayload(pelcoDevice);
        ASSERT_NE(composite, nullptr);
        ASSERT_TRUE(composite->connect());

        auto ptu = composite->panTilt();
        ASSERT_NE(ptu, nullptr);

        // Aim south (pan 180°), down 30°
        EXPECT_TRUE(ptu->setAbsoluteAngles(180.0, -30.0));
        const GimbalTelemetry telem = ptu->currentTelemetry();
        EXPECT_DOUBLE_EQ(telem.panAngleDeg, 180.0);
        EXPECT_DOUBLE_EQ(telem.tiltAngleDeg, -30.0);

        // Platform at (38.0, 24.0, 500m MSL), heading 0°
        const Klv::GeoPoint2D platformGps { 38.0, 24.0 };
        auto target = composite->calculateTargetCoordinates(platformGps, 0.0, 500.0);
        ASSERT_TRUE(target.has_value());

        // Target must be south of platform (latitude < 38.0)
        EXPECT_LT(target->latitudeDeg, 38.0);
        EXPECT_NEAR(target->longitudeDeg, 24.0, 1e-4);

        // Test camera optics synchronous telemetry
        auto cam = composite->primaryCamera();
        ASSERT_NE(cam, nullptr);
        const CameraTelemetry camTelem = cam->currentTelemetry();
        EXPECT_GE(camTelem.opticalZoomFactor, 1.0);
    }

} // namespace
} // namespace PayloadHal
