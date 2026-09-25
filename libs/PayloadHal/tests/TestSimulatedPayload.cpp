#include "Klv/KlvGeodesy.h"
#include "PayloadHal.h"
#include <gtest/gtest.h>

namespace PayloadHal {
namespace {

    TEST(TestSimulatedPayload, FactoryCreationAndSubsystems)
    {
        auto payload = PayloadFactory::createFromUri("sim://");
        ASSERT_NE(payload, nullptr);

        EXPECT_TRUE(payload->connect());
        EXPECT_TRUE(payload->isConnected());
        EXPECT_EQ(payload->state(), DeviceState::Ready);

        auto ptu = payload->panTilt();
        ASSERT_NE(ptu, nullptr);
        EXPECT_TRUE(ptu->supportsStabilization());
        EXPECT_TRUE(ptu->setStabilizationMode(StabilizationMode::GeoHold));
        EXPECT_EQ(ptu->stabilizationMode(), StabilizationMode::GeoHold);

        auto eoCam = payload->primaryCamera();
        ASSERT_NE(eoCam, nullptr);
        EXPECT_EQ(eoCam->spectrum(), CameraSpectrum::DaylightVisible);

        auto irCam = payload->secondaryCamera();
        ASSERT_NE(irCam, nullptr);
        EXPECT_EQ(irCam->spectrum(), CameraSpectrum::ThermalLWIR);

        auto lrf = payload->lrf();
        ASSERT_NE(lrf, nullptr);
        EXPECT_FALSE(lrf->isArmed());

        payload->disconnect();
        EXPECT_FALSE(payload->isConnected());
    }

    TEST(TestSimulatedPayload, MultiSensorTargetingCalculation)
    {
        auto simPayload = std::dynamic_pointer_cast<SimulatedPayload>(PayloadFactory::createSimulatedPayload());
        ASSERT_NE(simPayload, nullptr);

        ASSERT_TRUE(simPayload->connect());
        auto ptu = simPayload->panTilt();
        auto lrf = simPayload->lrf();
        ASSERT_NE(ptu, nullptr);
        ASSERT_NE(lrf, nullptr);

        // Aim gimbal: Pan 90° (East), Tilt -30° (30° depression)
        EXPECT_TRUE(ptu->setAbsoluteAngles(90.0, -30.0));

        // Arm and configure LRF with 1000m slant range
        EXPECT_TRUE(lrf->armLaser());
        simPayload->setSimulatedSlantRange(1000.0);
        EXPECT_TRUE(lrf->triggerSingleMeasurement());

        // Platform at (37.0, 23.0, 500m MSL), heading 0° (North)
        const Klv::GeoPoint2D platformGps { 37.0, 23.0 };
        auto target = simPayload->calculateTargetCoordinates(platformGps, 0.0, 500.0);
        ASSERT_TRUE(target.has_value());

        // Bearing = heading (0°) + pan (90°) = 90° (Due East)
        // Target should have same latitude and higher longitude (East)
        EXPECT_NEAR(target->latitudeDeg, 37.0, 1e-3);
        EXPECT_GT(target->longitudeDeg, 23.0);

        // Ground distance should be 1000 * cos(-30°) = 866.025 m
        const double expectedGroundDist = 1000.0 * std::cos(30.0 * 3.14159265358979323846 / 180.0);
        const double actualDist = Klv::KlvGeodesy::distanceMeters(platformGps, *target);
        EXPECT_NEAR(actualDist, expectedGroundDist, 0.5);
    }

    TEST(TestSimulatedPayload, SynchronousTelemetryAccess)
    {
        auto simPayload = std::dynamic_pointer_cast<SimulatedPayload>(PayloadFactory::createSimulatedPayload());
        ASSERT_NE(simPayload, nullptr);
        ASSERT_TRUE(simPayload->connect());

        auto ptu = simPayload->panTilt();
        auto eoCam = simPayload->primaryCamera();
        auto lrf = simPayload->lrf();

        ASSERT_NE(ptu, nullptr);
        ASSERT_NE(eoCam, nullptr);
        ASSERT_NE(lrf, nullptr);

        // Initial LRF measurement must be std::nullopt before any shot
        EXPECT_FALSE(lrf->lastMeasurement().has_value());

        // Update gimbal orientation and query synchronously
        EXPECT_TRUE(ptu->setAbsoluteAngles(120.0, -15.0));
        const GimbalTelemetry ptuTelem = ptu->currentTelemetry();
        EXPECT_DOUBLE_EQ(ptuTelem.panAngleDeg, 120.0);
        EXPECT_DOUBLE_EQ(ptuTelem.tiltAngleDeg, -15.0);

        // Update camera zoom and query synchronously
        EXPECT_TRUE(eoCam->setZoomNormalized(0.5));
        const CameraTelemetry camTelem = eoCam->currentTelemetry();
        EXPECT_DOUBLE_EQ(camTelem.normalizedZoom, 0.5);
        EXPECT_GT(camTelem.opticalZoomFactor, 1.0);
        EXPECT_LT(camTelem.horizontalFovDeg, 60.0);

        // Fire laser and query synchronously
        EXPECT_TRUE(lrf->armLaser());
        simPayload->setSimulatedSlantRange(750.0);
        EXPECT_TRUE(lrf->triggerSingleMeasurement());

        const auto lrfResult = lrf->lastMeasurement();
        ASSERT_TRUE(lrfResult.has_value());
        EXPECT_TRUE(lrfResult->valid);
        EXPECT_DOUBLE_EQ(lrfResult->slantRangeMeters, 750.0);
        EXPECT_GE(lrfResult->pulseCounter, 1U);
    }

    TEST(TestSimulatedPayload, ContinuousFocusAndIrisControls)
    {
        auto simPayload = std::dynamic_pointer_cast<SimulatedPayload>(PayloadFactory::createSimulatedPayload());
        ASSERT_NE(simPayload, nullptr);
        ASSERT_TRUE(simPayload->connect());

        auto cam = simPayload->primaryCamera();
        ASSERT_NE(cam, nullptr);

        // Continuous focus
        EXPECT_TRUE(cam->setFocusNormalized(0.2));
        EXPECT_NEAR(cam->currentTelemetry().focusDistanceNormalized, 0.2, 1e-3);
        EXPECT_TRUE(cam->focusContinuous(0.8f));
        EXPECT_GT(cam->currentTelemetry().focusDistanceNormalized, 0.2);
        EXPECT_TRUE(cam->focusStop());

        // Iris controls
        EXPECT_TRUE(cam->setIrisAuto(true));
        EXPECT_TRUE(cam->currentTelemetry().autoIrisActive);
        EXPECT_TRUE(cam->setIrisNormalized(0.65));
        EXPECT_FALSE(cam->currentTelemetry().autoIrisActive);
        EXPECT_NEAR(cam->currentTelemetry().irisNormalized, 0.65, 1e-3);
        EXPECT_TRUE(cam->irisContinuous(0.5f));
        EXPECT_GT(cam->currentTelemetry().irisNormalized, 0.65);
        EXPECT_TRUE(cam->irisStop());

        // Illuminator access
        auto illuminator = simPayload->illuminator();
        ASSERT_NE(illuminator, nullptr);
        EXPECT_TRUE(illuminator->armLaser());
        EXPECT_TRUE(illuminator->isArmed());
    }

} // namespace
} // namespace PayloadHal
