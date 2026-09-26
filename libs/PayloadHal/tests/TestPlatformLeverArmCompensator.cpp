#include "PayloadHal.h"
#include "PlatformLeverArmCompensator.h"
#include "sim/SimulatedPayload.h"
#include <gtest/gtest.h>
#include <cmath>

namespace PayloadHal {
namespace {

TEST(TestPlatformLeverArmCompensator, ZeroLeverArmIdentity)
{
    PlatformLeverArmConfig config {};
    config.gpsToGimbalBodyM = { 0.0, 0.0, 0.0 };
    config.gimbalToSensorM = { 0.0, 0.0, 0.0 };
    config.mountingType = GimbalMountingType::Upright;

    PlatformLeverArmCompensator comp(config);

    PlatformPose pose {};
    pose.gpsPosition = { 38.0, 24.0, 500.0 };
    pose.rollDeg = 0.0;
    pose.pitchDeg = 0.0;
    pose.headingDeg = 0.0;

    // Sensor position should equal GPS position exactly
    const auto sensorPos = comp.computeSensorPosition(pose);
    EXPECT_NEAR(sensorPos.latitudeDeg, 38.0, 1e-7);
    EXPECT_NEAR(sensorPos.longitudeDeg, 24.0, 1e-7);
    EXPECT_NEAR(sensorPos.altitudeM, 500.0, 1e-4);

    const auto sensorNed = comp.computeSensorPositionNed(pose);
    EXPECT_NEAR(sensorNed.x, 0.0, 1e-6);
    EXPECT_NEAR(sensorNed.y, 0.0, 1e-6);
    EXPECT_NEAR(sensorNed.z, 0.0, 1e-6);

    // LOS: pan=0, tilt=0 heading=0 should point North (1, 0, 0)
    const auto losNorth = comp.computeLineOfSightNed(pose, 0.0, 0.0);
    EXPECT_NEAR(losNorth.x, 1.0, 1e-6);
    EXPECT_NEAR(losNorth.y, 0.0, 1e-6);
    EXPECT_NEAR(losNorth.z, 0.0, 1e-6);

    // LOS: pan=90, tilt=0 heading=0 should point East (0, 1, 0)
    const auto losEast = comp.computeLineOfSightNed(pose, 90.0, 0.0);
    EXPECT_NEAR(losEast.x, 0.0, 1e-6);
    EXPECT_NEAR(losEast.y, 1.0, 1e-6);
    EXPECT_NEAR(losEast.z, 0.0, 1e-6);

    // LOS: pan=0, tilt=-90 (nadir/down) should point Nadir (0, 0, 1) in NED
    const auto losNadir = comp.computeLineOfSightNed(pose, 0.0, -90.0);
    EXPECT_NEAR(losNadir.x, 0.0, 1e-6);
    EXPECT_NEAR(losNadir.y, 0.0, 1e-6);
    EXPECT_NEAR(losNadir.z, 1.0, 1e-6);
}

TEST(TestPlatformLeverArmCompensator, VerticalMastOffset)
{
    // Gimbal mounted on a mast 3 meters above GPS antenna (Body Z is -3.0m in aircraft/vehicle coordinates)
    PlatformLeverArmConfig config {};
    config.gpsToGimbalBodyM = { 0.0, 0.0, -3.0 };
    config.gimbalToSensorM = { 0.0, 0.0, 0.0 };
    config.mountingType = GimbalMountingType::Upright;

    PlatformLeverArmCompensator comp(config);

    PlatformPose pose {};
    pose.gpsPosition = { 38.0, 24.0, 100.0 };
    pose.rollDeg = 0.0;
    pose.pitchDeg = 0.0;
    pose.headingDeg = 0.0;

    const auto sensorPos = comp.computeSensorPosition(pose);
    // Sensor should be at 100m + 3m = 103m altitude
    EXPECT_NEAR(sensorPos.altitudeM, 103.0, 1e-3);
    EXPECT_NEAR(sensorPos.latitudeDeg, 38.0, 1e-7);
    EXPECT_NEAR(sensorPos.longitudeDeg, 24.0, 1e-7);

    const auto sensorNed = comp.computeSensorPositionNed(pose);
    EXPECT_NEAR(sensorNed.x, 0.0, 1e-6);
    EXPECT_NEAR(sensorNed.y, 0.0, 1e-6);
    EXPECT_NEAR(sensorNed.z, -3.0, 1e-6); // -3m in NED (upwards)
}

TEST(TestPlatformLeverArmCompensator, PlatformRollAndPitchDisplacement)
{
    // Gimbal mounted 2 meters forward, 4 meters above GPS: (2.0, 0.0, -4.0)
    PlatformLeverArmConfig config {};
    config.gpsToGimbalBodyM = { 2.0, 0.0, -4.0 };
    config.gimbalToSensorM = { 0.0, 0.0, 0.0 };
    config.mountingType = GimbalMountingType::Upright;

    PlatformLeverArmCompensator comp(config);

    // Platform rolled 90 deg right wing down, level pitch, heading 0 (North)
    PlatformPose pose {};
    pose.gpsPosition = { 38.0, 24.0, 100.0 };
    pose.rollDeg = 90.0;
    pose.pitchDeg = 0.0;
    pose.headingDeg = 0.0;

    // Body offset (2, 0, -4) with roll 90 deg:
    // NED X = 2 (forward unchanged)
    // NED Y = 0*cos(90) - (-4)*sin(90) = +4.0 (displaced East)
    // NED Z = 0*sin(90) + (-4)*cos(90) = 0.0 (altitude at GPS level now)
    const auto sensorNed = comp.computeSensorPositionNed(pose);
    EXPECT_NEAR(sensorNed.x, 2.0, 1e-5);
    EXPECT_NEAR(sensorNed.y, 4.0, 1e-5);
    EXPECT_NEAR(sensorNed.z, 0.0, 1e-5);

    const auto sensorPos = comp.computeSensorPosition(pose);
    EXPECT_NEAR(sensorPos.altitudeM, 100.0, 1e-3);
}

TEST(TestPlatformLeverArmCompensator, InvertedMountingKinematics)
{
    // Gimbal mounted under aircraft belly (Inverted)
    PlatformLeverArmConfig config {};
    config.gpsToGimbalBodyM = { 0.0, 0.0, 1.5 }; // 1.5m below GPS
    config.gimbalToSensorM = { 0.0, 0.0, 0.0 };
    config.mountingType = GimbalMountingType::Inverted;

    PlatformLeverArmCompensator comp(config);

    PlatformPose pose {};
    pose.gpsPosition = { 38.0, 24.0, 1000.0 };
    pose.rollDeg = 0.0;
    pose.pitchDeg = 0.0;
    pose.headingDeg = 0.0;

    // Inverted mount: pan=0, tilt=0 should still point forward (North)
    const auto losNorth = comp.computeLineOfSightNed(pose, 0.0, 0.0);
    EXPECT_NEAR(losNorth.x, 1.0, 1e-5);
    EXPECT_NEAR(losNorth.y, 0.0, 1e-5);
    EXPECT_NEAR(losNorth.z, 0.0, 1e-5);

    // Inverted mount: positive tilt in gimbal coordinates (looking up relative to gimbal base,
    // which is mounted upside down) points downwards towards ground in aircraft frame
    const auto losTiltUp = comp.computeLineOfSightNed(pose, 0.0, 30.0);
    EXPECT_GT(losTiltUp.z, 0.0); // positive Z is Nadir/downward
}

TEST(TestPlatformLeverArmCompensator, TargetSlantRangeAndGroundIntersection)
{
    PlatformLeverArmConfig config {};
    config.gpsToGimbalBodyM = { 0.0, 0.0, 0.0 };
    config.gimbalToSensorM = { 0.0, 0.0, 0.0 };
    config.mountingType = GimbalMountingType::Upright;

    PlatformLeverArmCompensator comp(config);

    PlatformPose pose {};
    pose.gpsPosition = { 37.0, 24.0, 1000.0 }; // 1000m MSL
    pose.rollDeg = 0.0;
    pose.pitchDeg = 0.0;
    pose.headingDeg = 0.0; // Heading North

    // Looking North, tilt -30° (downward 30°), slant range 2000m
    // Vertical drop: 2000 * sin(30°) = 1000m -> Target Alt = 0m MSL
    // Horizontal distance: 2000 * cos(30°) = 1732.0508m North
    const auto targetSlant = comp.computeTargetFromSlantRange(pose, 0.0, -30.0, 2000.0);
    ASSERT_TRUE(targetSlant.has_value());
    EXPECT_NEAR(targetSlant->altitudeM, 0.0, 1.0);
    EXPECT_GT(targetSlant->latitudeDeg, 37.0);
    EXPECT_NEAR(targetSlant->longitudeDeg, 24.0, 1e-5);

    // Intersection with ground plane at 0m MSL should produce the identical target!
    const auto targetGround = comp.computeTargetFromGroundIntersection(pose, 0.0, -30.0, 0.0);
    ASSERT_TRUE(targetGround.has_value());
    EXPECT_NEAR(targetGround->latitudeDeg, targetSlant->latitudeDeg, 1e-6);
    EXPECT_NEAR(targetGround->longitudeDeg, targetSlant->longitudeDeg, 1e-6);
    EXPECT_NEAR(targetGround->altitudeM, targetSlant->altitudeM, 1e-2);
}

TEST(TestPlatformLeverArmCompensator, InverseLookAnglesRoundTrip)
{
    // Mast lever arm: 5m above GPS
    PlatformLeverArmConfig config {};
    config.gpsToGimbalBodyM = { 1.0, 0.5, -5.0 };
    config.gimbalToSensorM = { 0.1, 0.0, 0.05 };
    config.mountingType = GimbalMountingType::Upright;

    PlatformLeverArmCompensator comp(config);

    PlatformPose pose {};
    pose.gpsPosition = { 38.0, 24.0, 500.0 };
    pose.rollDeg = 5.0;
    pose.pitchDeg = -2.0;
    pose.headingDeg = 45.0;

    // Target located roughly 3 km North-East at sea level (alt 0.0)
    Klv::GeoPoint3D targetGeo { 38.02, 24.02, 0.0 };

    // Compute look angles from sensor to target
    const auto look = comp.computeLookAnglesToTarget(pose, targetGeo);
    EXPECT_LT(look.tiltAngleDeg, 0.0); // Looking downward

    // Project forward from sensor with computed range and look angles
    const auto projectedTarget = comp.computeTargetFromSlantRange(pose, look.panAngleDeg, look.tiltAngleDeg, look.slantRangeMeters);
    ASSERT_TRUE(projectedTarget.has_value());

    // Round-trip check: reconstructed target must match original target within sub-millimeter/centimeter accuracy
    EXPECT_NEAR(projectedTarget->latitudeDeg, targetGeo.latitudeDeg, 1e-6);
    EXPECT_NEAR(projectedTarget->longitudeDeg, targetGeo.longitudeDeg, 1e-6);
    EXPECT_NEAR(projectedTarget->altitudeM, targetGeo.altitudeM, 0.05);
}

TEST(TestPlatformLeverArmCompensator, SimulatedPayloadIntegration)
{
    auto payload = PayloadFactory::createSimulatedPayload();
    ASSERT_NE(payload, nullptr);

    auto comp = payload->leverArmCompensator();
    ASSERT_NE(comp, nullptr);

    const auto cfg = comp->config();
    EXPECT_NEAR(cfg.gpsToGimbalBodyM.z, 5.0, 0.01); // default simulated mast
    EXPECT_EQ(cfg.mountingType, GimbalMountingType::Upright);

    PlatformPose pose {};
    pose.gpsPosition = { 37.9838, 23.7275, 100.0 };
    pose.rollDeg = 0.0;
    pose.pitchDeg = 0.0;
    pose.headingDeg = 0.0;

    const auto sensorPos = comp->computeSensorPosition(pose);
    EXPECT_NEAR(sensorPos.altitudeM, 95.0, 0.01); // 5m below GPS antenna
}

} // namespace
} // namespace PayloadHal
