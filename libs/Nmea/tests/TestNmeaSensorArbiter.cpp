#include "NmeaChecksum.h"
#include "arbiter/NmeaSensorArbiter.h"

#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <vector>

namespace Nmea::Arbiter {

TEST(TestNmeaSensorArbiter, PrimaryHealthySelection)
{
    ArbiterConfig cfg {};
    cfg.policy = FailoverPolicy::PrimarySecondaryAutoRevert;
    cfg.minimumHealthyScore = 50.0;

    NmeaSensorArbiter arbiter(cfg);

    // Primary: RTK Fixed, 12 sats, HDOP 0.8
    arbiter.updateGpsCoordinates(
        GpsSourceId::Primary, 37.7749, -122.4194, 15.0, NmeaFixQuality::RtkFixed, 12U, 0.8, 10.0, 90.0);

    // Secondary: Standard GPS Fix, 6 sats, HDOP 1.5
    arbiter.updateGpsCoordinates(
        GpsSourceId::Secondary, 37.7749, -122.4194, 15.0, NmeaFixQuality::GpsFix, 6U, 1.5, 9.8, 91.0);

    arbiter.evaluate();

    EXPECT_EQ(arbiter.activeGpsSource(), GpsSourceId::Primary);
    EXPECT_GE(arbiter.gpsStatus(GpsSourceId::Primary).healthScore, 95.0);
    EXPECT_LT(arbiter.gpsStatus(GpsSourceId::Secondary).healthScore, 75.0);
}

TEST(TestNmeaSensorArbiter, FailoverOnTimeoutAndAutoRevert)
{
    ArbiterConfig cfg {};
    cfg.policy = FailoverPolicy::PrimarySecondaryAutoRevert;
    cfg.gpsTimeout = std::chrono::milliseconds(50); // fast timeout for test

    NmeaSensorArbiter arbiter(cfg);

    struct FailoverEvent {
        GpsSourceId oldSrc;
        GpsSourceId newSrc;
        std::string reason;
    };
    std::vector<FailoverEvent> events {};
    arbiter.setGpsFailoverCallback([&](GpsSourceId oldSrc, GpsSourceId newSrc, const std::string& reason) {
        events.push_back({ oldSrc, newSrc, reason });
    });

    // Both online initially
    arbiter.updateGpsCoordinates(GpsSourceId::Primary, 37.7749, -122.4194, 10.0, NmeaFixQuality::RtkFixed, 10U, 1.0);
    arbiter.updateGpsCoordinates(GpsSourceId::Secondary, 37.7749, -122.4194, 10.0, NmeaFixQuality::GpsFix, 8U, 1.2);

    arbiter.evaluate();
    EXPECT_EQ(arbiter.activeGpsSource(), GpsSourceId::Primary);

    // Wait past timeout for Primary, but keep Secondary fresh
    std::this_thread::sleep_for(std::chrono::milliseconds(70));
    arbiter.updateGpsCoordinates(GpsSourceId::Secondary, 37.7749, -122.4194, 10.0, NmeaFixQuality::GpsFix, 8U, 1.2);

    arbiter.evaluate();

    // Primary should have timed out -> failed over to Secondary
    EXPECT_EQ(arbiter.activeGpsSource(), GpsSourceId::Secondary);
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.back().oldSrc, GpsSourceId::Primary);
    EXPECT_EQ(events.back().newSrc, GpsSourceId::Secondary);

    // Now Primary recovers
    arbiter.updateGpsCoordinates(GpsSourceId::Primary, 37.7749, -122.4194, 10.0, NmeaFixQuality::RtkFixed, 10U, 1.0);
    arbiter.evaluate();

    // Auto-reverts to Primary
    EXPECT_EQ(arbiter.activeGpsSource(), GpsSourceId::Primary);
    EXPECT_EQ(events.back().oldSrc, GpsSourceId::Secondary);
    EXPECT_EQ(events.back().newSrc, GpsSourceId::Primary);
}

TEST(TestNmeaSensorArbiter, KinematicJumpAntiSpoofing)
{
    ArbiterConfig cfg {};
    cfg.maxSanitySpeedMps = 40.0; // ~77 knots

    NmeaSensorArbiter arbiter(cfg);

    // Initial position
    arbiter.updateGpsCoordinates(GpsSourceId::Primary, 37.7749, -122.4194, 10.0, NmeaFixQuality::RtkFixed, 10U, 1.0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Sudden unphysical position leap: 5 km away in 100 ms (speed ~50,000 m/s)
    arbiter.updateGpsCoordinates(GpsSourceId::Primary, 37.8200, -122.4194, 10.0, NmeaFixQuality::RtkFixed, 10U, 1.0);

    const auto status = arbiter.gpsStatus(GpsSourceId::Primary);
    EXPECT_TRUE(status.kinematicJumpDetected);
    // Score penalized by 50 points
    EXPECT_LE(status.healthScore, 50.0);
}

TEST(TestNmeaSensorArbiter, CrossSensorDivergenceMonitoring)
{
    ArbiterConfig cfg {};
    cfg.maxPositionDivergenceMeters = 25.0;
    cfg.maxHeadingDivergenceDeg = 4.0;

    NmeaSensorArbiter arbiter(cfg);

    int divergenceAlertCount { 0 };
    arbiter.setDivergenceCallback([&](const DivergenceStatus&) { ++divergenceAlertCount; });

    // Identical coordinates: 0 divergence
    arbiter.updateGpsCoordinates(GpsSourceId::Primary, 37.774900, -122.419400, 10.0, NmeaFixQuality::GpsFix, 8U, 1.0);
    arbiter.updateGpsCoordinates(GpsSourceId::Secondary, 37.774900, -122.419400, 10.0, NmeaFixQuality::GpsFix, 8U, 1.0);

    EXPECT_FALSE(arbiter.divergenceStatus().positionDiverged);
    EXPECT_NEAR(arbiter.divergenceStatus().positionDeltaMeters, 0.0, 1.0);

    // Move Secondary ~80 meters away (0.0007 degrees latitude ~ 78 meters)
    arbiter.updateGpsCoordinates(GpsSourceId::Secondary, 37.775600, -122.419400, 10.0, NmeaFixQuality::GpsFix, 8U, 1.0);

    EXPECT_TRUE(arbiter.divergenceStatus().positionDiverged);
    EXPECT_GT(arbiter.divergenceStatus().positionDeltaMeters, 25.0);
    EXPECT_GT(divergenceAlertCount, 0);

    // Test Heading Divergence
    arbiter.updateHeading(HeadingSourceId::Primary, 100.0);
    arbiter.updateHeading(HeadingSourceId::Secondary, 102.0); // 2 deg diff <= 4 deg
    EXPECT_FALSE(arbiter.divergenceStatus().headingDiverged);

    arbiter.updateHeading(HeadingSourceId::Secondary, 108.0); // 8 deg diff > 4 deg
    EXPECT_TRUE(arbiter.divergenceStatus().headingDiverged);
    EXPECT_NEAR(arbiter.divergenceStatus().headingDeltaDeg, 8.0, 0.1);
}

TEST(TestNmeaSensorArbiter, SeamlessNavSnapshot)
{
    NmeaSensorArbiter arbiter {};

    arbiter.updateGpsCoordinates(
        GpsSourceId::Primary, 54.1234, 12.5678, 22.5, NmeaFixQuality::RtkFixed, 14U, 0.9, 15.4, 270.0);
    arbiter.updateHeading(HeadingSourceId::Primary, 269.5);
    arbiter.updateAttitude(HeadingSourceId::Primary, 1.5, -2.0);

    const auto snapshot = arbiter.activeNavSnapshot();
    EXPECT_TRUE(snapshot.hasPosition);
    EXPECT_TRUE(snapshot.hasHeading);
    EXPECT_TRUE(snapshot.hasAttitude);
    EXPECT_NEAR(snapshot.position.latitudeDeg, 54.1234, 1e-5);
    EXPECT_NEAR(snapshot.position.longitudeDeg, 12.5678, 1e-5);
    EXPECT_NEAR(snapshot.altitudeMeters, 22.5, 0.1);
    EXPECT_NEAR(snapshot.sogKnots, 15.4, 0.1);
    EXPECT_NEAR(snapshot.cogDegrees, 270.0, 0.1);
    EXPECT_NEAR(snapshot.trueHeadingDegrees, 269.5, 0.1);
    EXPECT_NEAR(snapshot.pitchDegrees, 1.5, 0.1);
    EXPECT_NEAR(snapshot.rollDegrees, -2.0, 0.1);
}

} // namespace Nmea::Arbiter
