#include "PayloadHal.h"
#include "TargetKinematicsFilter.h"
#include "sim/SimulatedPayload.h"
#include <gtest/gtest.h>
#include <chrono>
#include <cmath>

namespace PayloadHal {
namespace {

constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;

TEST(TestTargetKinematicsFilter, StationaryTargetConvergence)
{
    TargetKinematicsFilter filter;
    EXPECT_EQ(filter.trackState(), TargetTrackState::Unacquired);
    EXPECT_FALSE(filter.isTracking());

    auto now = std::chrono::steady_clock::now();
    const double trueRange = 1000.0;
    const double trueAz = 0.0;
    const double trueEl = 0.0;

    // Feed 30 measurements at 20 Hz (1.5 seconds)
    for (int i = 0; i < 30; ++i) {
        now += std::chrono::milliseconds(50);
        filter.updateFullMeasurement(trueAz, trueEl, trueRange, {0.0, 0.0, 0.0}, now);
    }

    EXPECT_EQ(filter.trackState(), TargetTrackState::Tracking);
    EXPECT_TRUE(filter.isTracking());

    TargetKinematics3D kin = filter.currentKinematics();
    EXPECT_NEAR(kin.positionNedMeters.x, 1000.0, 1.0);
    EXPECT_NEAR(kin.positionNedMeters.y, 0.0, 1.0);
    EXPECT_NEAR(kin.positionNedMeters.z, 0.0, 1.0);

    // Speed should be virtually zero
    EXPECT_LT(kin.speedMps, 0.2);
    EXPECT_NEAR(kin.slantRangeMeters, 1000.0, 1.0);
    EXPECT_NEAR(kin.azimuthDeg, 0.0, 0.1);
    EXPECT_NEAR(kin.elevationDeg, 0.0, 0.1);
}

TEST(TestTargetKinematicsFilter, ConstantVelocityTrack)
{
    TargetKinematicsFilter filter;
    auto now = std::chrono::steady_clock::now();

    // Target starts at North = 1000m, moves East at 20.0 m/s
    const double vEastTrue = 20.0;
    double t = 0.0;
    const double dt = 0.05; // 20 Hz

    for (int i = 0; i < 60; ++i) { // 3 seconds
        t += dt;
        now += std::chrono::milliseconds(50);

        const double pN = 1000.0;
        const double pE = vEastTrue * t;
        const double pD = 0.0;

        const double r = std::sqrt(pN * pN + pE * pE + pD * pD);
        const double az = std::atan2(pE, pN) * kRadToDeg;
        const double el = 0.0;

        filter.updateFullMeasurement(az, el, r, {0.0, 0.0, 0.0}, now);
    }

    EXPECT_EQ(filter.trackState(), TargetTrackState::Tracking);

    TargetKinematics3D kin = filter.currentKinematics();
    EXPECT_NEAR(kin.velocityNedMps.y, vEastTrue, 1.0);
    EXPECT_NEAR(kin.velocityNedMps.x, 0.0, 1.0);
    EXPECT_NEAR(kin.velocityNedMps.z, 0.0, 1.0);

    EXPECT_NEAR(kin.groundSpeedMps, vEastTrue, 1.0);
    EXPECT_NEAR(kin.courseDeg, 90.0, 3.0); // Heading East = 90 deg
}

TEST(TestTargetKinematicsFilter, AcceleratedManeuverTrack)
{
    TargetKinematicsFilter filter;
    auto now = std::chrono::steady_clock::now();

    // Target starts at 500m North and accelerates North at 2.0 m/s^2
    const double aNorthTrue = 2.0;
    double t = 0.0;
    const double dt = 0.05;

    for (int i = 0; i < 80; ++i) { // 4 seconds
        t += dt;
        now += std::chrono::milliseconds(50);

        const double pN = 500.0 + 0.5 * aNorthTrue * t * t;
        const double r = pN;
        const double az = 0.0;
        const double el = 0.0;

        filter.updateFullMeasurement(az, el, r, {0.0, 0.0, 0.0}, now);
    }

    EXPECT_EQ(filter.trackState(), TargetTrackState::Tracking);

    TargetKinematics3D kin = filter.currentKinematics();
    // Velocity should be ~ a * t = 2.0 * 4.0 = 8.0 m/s
    EXPECT_NEAR(kin.velocityNedMps.x, 8.0, 1.5);
    EXPECT_NEAR(kin.accelerationNedMps2.x, aNorthTrue, 1.0);
}

TEST(TestTargetKinematicsFilter, PredictiveLeadLatency)
{
    TargetKinematicsFilter filter;
    auto now = std::chrono::steady_clock::now();

    // Target at 1000m North moving East at 30 m/s
    const double vEast = 30.0;
    double t = 0.0;
    const double dt = 0.05;

    for (int i = 0; i < 40; ++i) {
        t += dt;
        now += std::chrono::milliseconds(50);
        const double pN = 1000.0;
        const double pE = vEast * t;
        const double r = std::sqrt(pN * pN + pE * pE);
        const double az = std::atan2(pE, pN) * kRadToDeg;
        filter.updateFullMeasurement(az, 0.0, r, {0.0, 0.0, 0.0}, now);
    }

    // Lead for 100ms latency (0.1s)
    const double latencySec = 0.100;
    PredictiveLeadSolution sol = filter.computeLeadAngles(latencySec, 0.0);

    EXPECT_TRUE(sol.valid);
    EXPECT_DOUBLE_EQ(sol.totalLeadTimeSec, latencySec);
    EXPECT_DOUBLE_EQ(sol.timeOfFlightSec, 0.0);

    // In 0.1s at 30 m/s, target advances 3.0 meters East.
    // Since target is East of North, lead azimuth must be greater than current azimuth!
    EXPECT_GT(sol.leadAzimuthDeg, sol.currentAzimuthDeg);
    EXPECT_GT(sol.deltaAzimuthDeg, 0.0);

    // Delta azimuth should be roughly atan2(3m, 1000m) * 180 / pi ~ 0.17 degrees
    EXPECT_NEAR(sol.deltaAzimuthDeg, 0.17, 0.05);
}

TEST(TestTargetKinematicsFilter, ProjectileTimeOfFlight)
{
    TargetKinematicsFilter filter;
    auto now = std::chrono::steady_clock::now();

    // Target at ~1600m moving East at 20 m/s
    const double vEast = 20.0;
    double t = 0.0;
    const double dt = 0.05;

    for (int i = 0; i < 40; ++i) {
        t += dt;
        now += std::chrono::milliseconds(50);
        const double pN = 1600.0;
        const double pE = vEast * t;
        const double r = std::sqrt(pN * pN + pE * pE);
        const double az = std::atan2(pE, pN) * kRadToDeg;
        filter.updateFullMeasurement(az, 0.0, r, {0.0, 0.0, 0.0}, now);
    }

    // Projectile speed = 800 m/s
    // TOF should be approximately 1600 / 800 = 2.0 seconds
    PredictiveLeadSolution sol = filter.computeLeadAngles(0.0, 800.0);

    EXPECT_TRUE(sol.valid);
    EXPECT_NEAR(sol.timeOfFlightSec, 2.0, 0.1);
    EXPECT_NEAR(sol.totalLeadTimeSec, 2.0, 0.1);

    // Target moves ~ 40 meters East during TOF (2.0s * 20 m/s)
    EXPECT_GT(sol.deltaAzimuthDeg, 1.0);
    EXPECT_NEAR(sol.predictedPositionNed.x, 1600.0, 5.0);
    EXPECT_GT(sol.predictedPositionNed.y, filter.currentKinematics().positionNedMeters.y);
}

TEST(TestTargetKinematicsFilter, OcclusionCoastingAndTimeout)
{
    TargetKinematicsFilter filter;
    TargetKinematicsConfig cfg = filter.config();
    cfg.maxCoastDurationSec = 2.0; // 2 second coast timeout
    filter.setConfig(cfg);

    auto now = std::chrono::steady_clock::now();

    // Target moving at 15 m/s North
    for (int i = 0; i < 30; ++i) {
        now += std::chrono::milliseconds(50);
        const double pN = 1000.0 + 15.0 * (i * 0.05);
        filter.updateFullMeasurement(0.0, 0.0, pN, {0.0, 0.0, 0.0}, now);
    }

    EXPECT_EQ(filter.trackState(), TargetTrackState::Tracking);

    // Occlusion: 0.5s passes with no measurements
    now += std::chrono::milliseconds(500);
    filter.predict(now);

    // Must transition to Coasting
    EXPECT_EQ(filter.trackState(), TargetTrackState::Coasting);
    EXPECT_TRUE(filter.isTracking());

    // Kinematics should still predict forward motion
    TargetKinematics3D kinCoast = filter.currentKinematics();
    EXPECT_GT(kinCoast.positionNedMeters.x, 1000.0);

    // Exceed max coast duration (total 2.5s no measurements)
    now += std::chrono::milliseconds(2000);
    filter.predict(now);

    // Must transition to Lost
    EXPECT_EQ(filter.trackState(), TargetTrackState::Lost);
    EXPECT_FALSE(filter.isTracking());
}

TEST(TestTargetKinematicsFilter, ReacquisitionAfterCoasting)
{
    TargetKinematicsFilter filter;
    auto now = std::chrono::steady_clock::now();

    for (int i = 0; i < 30; ++i) {
        now += std::chrono::milliseconds(50);
        filter.updateFullMeasurement(45.0, -5.0, 1000.0, {0.0, 0.0, 0.0}, now);
    }
    EXPECT_EQ(filter.trackState(), TargetTrackState::Tracking);

    // Coast for 0.5s
    now += std::chrono::milliseconds(500);
    filter.predict(now);
    EXPECT_EQ(filter.trackState(), TargetTrackState::Coasting);

    // Target reappears
    now += std::chrono::milliseconds(50);
    filter.updateFullMeasurement(45.0, -5.0, 1000.0, {0.0, 0.0, 0.0}, now);

    // Successfully reacquired back to Tracking
    EXPECT_EQ(filter.trackState(), TargetTrackState::Tracking);
    EXPECT_TRUE(filter.isTracking());
}

TEST(TestTargetKinematicsFilter, SimulatedPayloadIntegration)
{
    SimulatedPayload payload;
    ASSERT_TRUE(payload.connect());

    auto filter = payload.targetKinematics();
    ASSERT_NE(filter, nullptr);

    EXPECT_EQ(filter->trackState(), TargetTrackState::Unacquired);

    // Feed a measurement through the payload's filter
    filter->updateFullMeasurement(90.0, 0.0, 500.0);
    EXPECT_EQ(filter->trackState(), TargetTrackState::Acquiring);

    payload.disconnect();
}

} // namespace
} // namespace PayloadHal
