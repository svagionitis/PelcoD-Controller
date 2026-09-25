/// @file TestGeoLockController.cpp
/// @brief Unit tests for Click-to-Point (slewToGeoTarget) and GeoLockController closed loop.

#include "GeoLockController.h"
#include "GeoreferenceUtils.h"
#include "sim/SimulatedPayload.h"
#include <chrono>
#include <cmath>
#include <gtest/gtest.h>
#include <thread>

namespace PayloadHal {
namespace {

    TEST(TestGeoLockController, ClickToPointSlewToGeoTarget)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        // Host platform: 1000m altitude, heading North (0 deg)
        const Klv::GeoPoint3D platform { 37.0, -122.0, 1000.0 };
        const double heading = 0.0;

        // Target directly North of platform on the ground (approx 1.1 km North)
        const Klv::GeoPoint3D targetNorth { 37.01, -122.0, 0.0 };

        EXPECT_TRUE(payload->slewToGeoTarget(platform, heading, targetNorth));

        auto telem = payload->panTilt()->currentTelemetry();
        // Azimuth should be ~0 degrees (due North)
        EXPECT_NEAR(telem.panAngleDeg, 0.0, 0.5);
        // Tilt should be depression angle (~ -42 degrees)
        EXPECT_LT(telem.tiltAngleDeg, -30.0);
        EXPECT_GT(telem.tiltAngleDeg, -60.0);

        // Now slew to target East of platform
        const Klv::GeoPoint3D targetEast { 37.0, -121.988, 0.0 };
        EXPECT_TRUE(payload->slewToGeoTarget(platform, heading, targetEast));

        telem = payload->panTilt()->currentTelemetry();
        // Azimuth should be ~90 degrees (East)
        EXPECT_NEAR(telem.panAngleDeg, 90.0, 1.0);
        EXPECT_LT(telem.tiltAngleDeg, -30.0);
    }

    TEST(TestGeoLockController, EngagementLifecycle)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        GeoLockController controller(payload);
        EXPECT_FALSE(controller.isEngaged());
        EXPECT_FALSE(controller.currentTarget().has_value());
        EXPECT_FALSE(payload->isGeoLocked());

        const Klv::GeoPoint3D target { 37.5, -122.3, 50.0 };
        EXPECT_TRUE(controller.engage(target));
        EXPECT_TRUE(controller.isEngaged());
        ASSERT_TRUE(controller.currentTarget().has_value());
        EXPECT_NEAR(controller.currentTarget()->latitudeDeg, 37.5, 1e-6);
        EXPECT_TRUE(payload->isGeoLocked());

        controller.disengage();
        EXPECT_FALSE(controller.isEngaged());
        EXPECT_FALSE(controller.currentTarget().has_value());
        EXPECT_FALSE(payload->isGeoLocked());
    }

    TEST(TestGeoLockController, DeadbandSuppression)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        GeoLockController controller(payload);
        controller.setDeadbandDeg(0.2); // 0.2 degrees deadband
        EXPECT_NEAR(controller.deadbandDeg(), 0.2, 1e-6);

        const Klv::GeoPoint3D platform { 37.0, -122.0, 1000.0 };
        const Klv::GeoPoint3D target { 37.01, -122.0, 0.0 };

        EXPECT_TRUE(controller.engage(target));

        // Initial update dispatches slew command
        EXPECT_TRUE(controller.updatePlatform(platform, 0.0));
        auto status1 = controller.status();
        EXPECT_EQ(status1.updateCount, 1u);
        const double initialPan = status1.commandedPanDeg;
        const double initialTilt = status1.commandedTiltDeg;

        // Negligible platform movement (e.g. 0.01m displacement)
        const Klv::GeoPoint3D microMove { 37.0000001, -122.0, 1000.0 };
        EXPECT_TRUE(controller.updatePlatform(microMove, 0.0));
        auto status2 = controller.status();
        EXPECT_EQ(status2.updateCount, 2u);
        // Commanded angles should be suppressed by deadband
        EXPECT_NEAR(status2.commandedPanDeg, initialPan, 1e-6);
        EXPECT_NEAR(status2.commandedTiltDeg, initialTilt, 1e-6);

        // Significant heading rotation (5 degrees yaw turn)
        EXPECT_TRUE(controller.updatePlatform(microMove, 5.0));
        auto status3 = controller.status();
        EXPECT_EQ(status3.updateCount, 3u);
        // Pan command must compensate for the 5° heading rotation
        EXPECT_NEAR(status3.commandedPanDeg, -5.0, 0.5);
    }

    TEST(TestGeoLockController, BackgroundTrackingLoop)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        GeoLockController controller(payload);
        const Klv::GeoPoint3D target { 37.02, -122.0, 0.0 };
        ASSERT_TRUE(controller.engage(target));

        std::atomic<double> heading { 0.0 };
        controller.setPlatformNavProvider([&]() -> std::optional<std::pair<Klv::GeoPoint3D, double>> {
            return std::make_pair(Klv::GeoPoint3D { 37.0, -122.0, 1000.0 }, heading.load());
        });

        EXPECT_FALSE(controller.isTrackingLoopRunning());
        EXPECT_TRUE(controller.startTrackingLoop(50.0)); // 50 Hz
        EXPECT_TRUE(controller.isTrackingLoopRunning());

        // Allow background thread to process several cycles
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        auto status = controller.status();
        EXPECT_GT(status.updateCount, 0u);

        // Change heading
        heading.store(15.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(60));

        status = controller.status();
        EXPECT_NEAR(status.commandedPanDeg, -15.0, 1.0);

        controller.stopTrackingLoop();
        EXPECT_FALSE(controller.isTrackingLoopRunning());
    }

} // namespace
} // namespace PayloadHal
