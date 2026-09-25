#include "MockPelcoDDevice.h"
#include "PelcoDFujinon/FujinonSX800Device.h"
#include "adapters/PelcoDFujinonPayloadAdapter.h"
#include <gtest/gtest.h>

namespace PayloadHal {
namespace {

    TEST(TestPelcoDFujinonAdapter, SubsystemsAccessAndLifecycle)
    {
        auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
        auto fujinonDevice = std::make_shared<PelcoD::FujinonSX800Device>(mockTransport, 1U);
        PelcoDFujinonPayloadAdapter adapter(fujinonDevice);

        auto ptu = adapter.panTilt();
        ASSERT_NE(ptu, nullptr);

        auto cam = adapter.primaryCamera();
        ASSERT_NE(cam, nullptr);
        EXPECT_EQ(cam->spectrum(), CameraSpectrum::DaylightVisible);

        EXPECT_EQ(adapter.secondaryCamera(), nullptr);
        EXPECT_EQ(adapter.lrf(), nullptr);

        EXPECT_TRUE(adapter.connect());
        EXPECT_TRUE(adapter.isConnected());

        adapter.disconnect();
        EXPECT_FALSE(adapter.isConnected());
    }

    TEST(TestPelcoDFujinonAdapter, OpticsAndEnhancementControls)
    {
        auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
        auto fujinonDevice = std::make_shared<PelcoD::FujinonSX800Device>(mockTransport, 1U);
        PelcoDFujinonPayloadAdapter adapter(fujinonDevice);

        ASSERT_TRUE(adapter.connect());

        // Optical zoom
        auto cam = adapter.primaryCamera();
        ASSERT_NE(cam, nullptr);
        EXPECT_TRUE(cam->setZoomNormalized(0.75));
        EXPECT_TRUE(cam->zoomContinuous(0.8f));
        EXPECT_TRUE(cam->zoomStop());

        // Autofocus
        EXPECT_TRUE(cam->setFocusAuto(true));
        EXPECT_TRUE(cam->triggerOnePushFocus());
        EXPECT_TRUE(cam->focusContinuous(0.6f));
        EXPECT_TRUE(cam->focusContinuous(-0.6f));
        EXPECT_TRUE(cam->focusStop());

        // Iris controls
        EXPECT_TRUE(cam->setIrisAuto(true));
        EXPECT_TRUE(cam->setIrisAuto(false));
        EXPECT_TRUE(cam->setIrisNormalized(0.7));
        EXPECT_TRUE(cam->irisContinuous(0.5f));
        EXPECT_TRUE(cam->irisContinuous(-0.5f));
        EXPECT_TRUE(cam->irisStop());

        // Extended Fujinon enhancements
        EXPECT_TRUE(adapter.setOISMode(PelcoD::FujinonOISMode::OisOn));
        EXPECT_TRUE(adapter.setDefogLevel(PelcoD::FujinonDefogLevel::Level2));
        EXPECT_TRUE(adapter.setHeatHaze(PelcoD::FujinonHeatHazeLevel::Level1));
        EXPECT_TRUE(adapter.setWDR(PelcoD::FujinonWDRLevel::Level3));
        EXPECT_TRUE(adapter.setVLCFilter(true));

        // Pan Tilt controls
        auto ptu = adapter.panTilt();
        ASSERT_NE(ptu, nullptr);
        EXPECT_TRUE(ptu->setNormalizedVelocity(0.5f, 0.2f));
        EXPECT_TRUE(ptu->stopMotion());
        EXPECT_TRUE(ptu->setAbsoluteAngles(90.0, -10.0));

        // Synchronous telemetry queries
        const GimbalTelemetry ptuTelem = ptu->currentTelemetry();
        EXPECT_DOUBLE_EQ(ptuTelem.panAngleDeg, 90.0);
        EXPECT_DOUBLE_EQ(ptuTelem.tiltAngleDeg, -10.0);

        const CameraTelemetry camTelem = cam->currentTelemetry();
        EXPECT_GE(camTelem.opticalZoomFactor, 1.0);
    }

    TEST(TestPelcoDFujinonAdapter, GroundIntersectionTargetCoordinates)
    {
        auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
        auto fujinonDevice = std::make_shared<PelcoD::FujinonSX800Device>(mockTransport, 1U);
        PelcoDFujinonPayloadAdapter adapter(fujinonDevice);

        ASSERT_TRUE(adapter.connect());
        auto ptu = adapter.panTilt();
        ASSERT_NE(ptu, nullptr);

        // Set camera looking south (180° pan relative) tilted down (-30°)
        EXPECT_TRUE(ptu->setAbsoluteAngles(180.0, -30.0));

        // Platform at (35.0, 25.0, 500m MSL), heading 0°
        const Klv::GeoPoint2D platformGps { 35.0, 25.0 };
        auto target = adapter.calculateTargetCoordinates(platformGps, 0.0, 500.0);
        ASSERT_TRUE(target.has_value());

        // Target should be located south of platform (latitude < 35.0)
        EXPECT_LT(target->latitudeDeg, 35.0);
        EXPECT_NEAR(target->longitudeDeg, 25.0, 1e-4);
    }

} // namespace
} // namespace PayloadHal
