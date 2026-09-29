#include "VesselAttitudeCompensator.h"
#include "GeoLockController.h"
#include "Nmea/NmeaChecksum.h"
#include "Nmea/NmeaSentenceParser.h"
#include "Nmea/NmeaTypes.h"
#include "Nmea/n2k/N2kTypes.h"
#include "sim/SimulatedPayload.h"

#include <gtest/gtest.h>
#include <chrono>
#include <cmath>

namespace PayloadHal {
namespace {

    constexpr double kPi = 3.14159265358979323846;

    TEST(TestVesselAttitudeCompensator, PurePitchCompensation)
    {
        VesselAttitudeCompensator comp;
        const auto now = std::chrono::steady_clock::now();

        // Platform at 0 altitude, bow pitched UP by +5.0 degrees, heading 000 (North), roll 0
        comp.updateAttitude(0.0, 5.0, 0.0, 0.0, now);

        EXPECT_TRUE(comp.hasValidAttitude(now));
        const auto st = comp.attitudeState();
        EXPECT_NEAR(st.pitchDeg, 5.0, 1e-4);
        EXPECT_NEAR(st.rollDeg, 0.0, 1e-4);
        EXPECT_NEAR(st.headingDeg, 0.0, 1e-4);

        Klv::GeoPoint3D platformPos { 37.0, -122.0, 0.0 };
        // Target 1 km directly North (heading 000) at 0 altitude
        Klv::GeoPoint3D targetPos { 37.009, -122.0, 0.0 };

        PlatformLeverArmConfig config {}; // No lever arm, upright mount

        const auto look = comp.compensateLookAngles(platformPos, targetPos, config, now);

        // When bow is pitched UP by 5 degrees, gimbal must pitch DOWN by -5 degrees to hold target
        EXPECT_NEAR(look.panAngleDeg, 0.0, 0.1);
        EXPECT_NEAR(look.tiltAngleDeg, -5.0, 0.2);
    }

    TEST(TestVesselAttitudeCompensator, PureRollCompensation)
    {
        VesselAttitudeCompensator comp;
        const auto now = std::chrono::steady_clock::now();

        // Platform rolled starboard down by +10.0 degrees, heading 000, pitch 0
        comp.updateAttitude(0.0, 0.0, 10.0, 0.0, now);

        PlatformLeverArmConfig config {};
        Klv::GeoPoint3D platformPos { 37.0, -122.0, 0.0 };

        // Target 1 km directly East (90 deg to starboard)
        Klv::GeoPoint3D targetStarboard { 37.0, -121.9887, 0.0 };
        const auto lookStarboard = comp.compensateLookAngles(platformPos, targetStarboard, config, now);

        // Starboard deck is dipped down by +10 deg, so a horizon target to starboard is elevated relative to the deck (+10 deg tilt)
        EXPECT_NEAR(lookStarboard.panAngleDeg, 90.0, 0.2);
        EXPECT_NEAR(lookStarboard.tiltAngleDeg, 10.0, 0.3);

        // Target 1 km directly West (90 deg to port / -90 deg relative)
        Klv::GeoPoint3D targetPort { 37.0, -122.0113, 0.0 };
        const auto lookPort = comp.compensateLookAngles(platformPos, targetPort, config, now);

        // Port deck is elevated up by 10 deg, so a horizon target to port is depressed relative to the deck (-10 deg tilt)
        EXPECT_NEAR(std::abs(lookPort.panAngleDeg), 90.0, 0.2);
        EXPECT_NEAR(lookPort.tiltAngleDeg, -10.0, 0.3);
    }

    TEST(TestVesselAttitudeCompensator, CombinedAttitudeWaveMotion)
    {
        VesselAttitudeCompensator comp;
        const auto now = std::chrono::steady_clock::now();

        // Platform with combined pitch +3.0 deg, roll -4.0 deg, heading 045.0 deg
        comp.updateAttitude(45.0, 3.0, -4.0, 0.0, now);

        PlatformLeverArmConfig config {};
        Klv::GeoPoint3D platformPos { 37.0, -122.0, 10.0 };
        Klv::GeoPoint3D targetPos { 37.01, -121.99, 0.0 };

        const auto compLook = comp.compensateLookAngles(platformPos, targetPos, config, now);

        // Compare against direct PlatformLeverArmCompensator reference
        PlatformPose pose { platformPos, 45.0, 3.0, -4.0 };
        PlatformLeverArmCompensator refComp(config);
        const auto refLook = refComp.computeLookAnglesToTarget(pose, targetPos);

        EXPECT_NEAR(compLook.panAngleDeg, refLook.panAngleDeg, 1e-4);
        EXPECT_NEAR(compLook.tiltAngleDeg, refLook.tiltAngleDeg, 1e-4);
        EXPECT_NEAR(compLook.slantRangeMeters, refLook.slantRangeMeters, 1e-3);
    }

    TEST(TestVesselAttitudeCompensator, ActiveHorizonLevelingRoll)
    {
        VesselAttitudeCompensator comp;
        const auto now = std::chrono::steady_clock::now();

        // Roll 8 deg starboard down, pitch 0 deg
        comp.updateAttitude(0.0, 0.0, 8.0, 0.0, now);

        // When looking straight ahead (pan 0, tilt 0), counter-roll must be -8.0 deg
        const double rollBoresight = comp.computeHorizonRoll(0.0, 0.0, now);
        EXPECT_NEAR(rollBoresight, -8.0, 0.1);

        // When looking straight starboard (pan 90, tilt 0) with roll 8 deg, pitch 0 deg:
        // camera right axis is along vessel longitudinal axis, which has 0 pitch, so leveling roll is ~0
        const double rollStarboard = comp.computeHorizonRoll(90.0, 0.0, now);
        EXPECT_NEAR(rollStarboard, 0.0, 0.2);
    }

    TEST(TestVesselAttitudeCompensator, AttitudeTimeoutAndGracefulDegradation)
    {
        VesselAttitudeCompensator comp;
        comp.setAttitudeTimeout(std::chrono::milliseconds(200));
        EXPECT_EQ(comp.attitudeTimeout().count(), 200);

        const auto t0 = std::chrono::steady_clock::now();
        comp.updateAttitude(045.0, 5.0, 10.0, 0.0, t0);
        EXPECT_TRUE(comp.hasValidAttitude(t0));

        // 100 ms later: still valid
        const auto t1 = t0 + std::chrono::milliseconds(100);
        EXPECT_TRUE(comp.hasValidAttitude(t1));

        // 300 ms later: expired!
        const auto t2 = t0 + std::chrono::milliseconds(300);
        EXPECT_FALSE(comp.hasValidAttitude(t2));

        // When expired, horizon roll returns 0.0
        EXPECT_NEAR(comp.computeHorizonRoll(0.0, 0.0, t2), 0.0, 1e-6);

        // When expired, compensateLookAngles gracefully degrades to 2D model (pitch=0, roll=0)
        PlatformLeverArmConfig config {};
        Klv::GeoPoint3D platformPos { 37.0, -122.0, 0.0 };
        Klv::GeoPoint3D targetPos { 37.01, -122.0, 0.0 };

        const auto expiredLook = comp.compensateLookAngles(platformPos, targetPos, config, t2);
        EXPECT_FALSE(std::isnan(expiredLook.panAngleDeg));
        EXPECT_FALSE(std::isnan(expiredLook.tiltAngleDeg));
        EXPECT_NEAR(expiredLook.tiltAngleDeg, 0.0, 0.1);
    }

    TEST(TestVesselAttitudeCompensator, MultiProtocolIngestionN2kPashrPfecXdr)
    {
        VesselAttitudeCompensator comp;
        const auto now = std::chrono::steady_clock::now();

        // 1. N2K PGN 127257
        Nmea::N2k::Attitude n2k {};
        n2k.yawDegrees = 90.0;
        n2k.pitchDegrees = 2.5;
        n2k.rollDegrees = -1.5;
        n2k.hasYaw = true;
        n2k.hasPitch = true;
        n2k.hasRoll = true;
        comp.updateFromN2k(n2k, now);

        auto st = comp.attitudeState();
        EXPECT_TRUE(st.isValid);
        EXPECT_NEAR(st.headingDeg, 90.0, 1e-2);
        EXPECT_NEAR(st.pitchDeg, 2.5, 1e-2);
        EXPECT_NEAR(st.rollDeg, -1.5, 1e-2);

        // 2. NMEA 0183 PASHR
        Nmea::PashrData pashr {};
        pashr.headingDegrees = 135.0;
        pashr.isTrueHeading = true;
        pashr.pitchDegrees = -3.0;
        pashr.rollDegrees = 4.2;
        pashr.heaveMeters = 0.55;
        pashr.valid = true;
        comp.updateFromPashr(pashr, now);

        st = comp.attitudeState();
        EXPECT_NEAR(st.headingDeg, 135.0, 1e-2);
        EXPECT_NEAR(st.pitchDeg, -3.0, 1e-2);
        EXPECT_NEAR(st.rollDeg, 4.2, 1e-2);
        EXPECT_NEAR(st.heaveMeters, 0.55, 1e-2);

        // 3. FLIR PFEC GPatt
        Nmea::PfecAttitudeData pfec {};
        pfec.yawDegrees = 270.0;
        pfec.pitchDegrees = 1.0;
        pfec.rollDegrees = -2.0;
        pfec.valid = true;
        comp.updateFromPfec(pfec, now);

        st = comp.attitudeState();
        EXPECT_NEAR(st.headingDeg, 270.0, 1e-2);
        EXPECT_NEAR(st.pitchDeg, 1.0, 1e-2);
        EXPECT_NEAR(st.rollDeg, -2.0, 1e-2);

        // 4. XDR Transducers
        Nmea::XdrData xdr {};
        xdr.transducers.push_back({ 'A', 0.8, 'D', "PITCH" });
        xdr.transducers.push_back({ 'A', -1.2, 'D', "ROLL" });
        xdr.valid = true;
        comp.updateFromXdr(xdr, now);

        st = comp.attitudeState();
        EXPECT_NEAR(st.pitchDeg, 0.8, 1e-2);
        EXPECT_NEAR(st.rollDeg, -1.2, 1e-2);

        // 5. Unified AttitudeData
        Nmea::AttitudeData att {};
        att.headingDegrees = 315.0;
        att.pitchDegrees = -0.5;
        att.rollDegrees = 1.8;
        att.heaveMeters = 0.2;
        att.hasHeading = true;
        att.valid = true;
        comp.updateFromAttitude(att, now);

        st = comp.attitudeState();
        EXPECT_NEAR(st.headingDeg, 315.0, 1e-2);
        EXPECT_NEAR(st.pitchDeg, -0.5, 1e-2);
        EXPECT_NEAR(st.rollDeg, 1.8, 1e-2);
        EXPECT_NEAR(st.heaveMeters, 0.2, 1e-2);
    }

    TEST(TestVesselAttitudeCompensator, WaveRateSmoothingAndJitterFiltering)
    {
        VesselAttitudeCompensator comp;
        comp.setSmoothing(true, 0.5);
        EXPECT_TRUE(comp.isSmoothingEnabled());

        const auto t0 = std::chrono::steady_clock::now();
        comp.updateAttitude(0.0, 0.0, 0.0, 0.0, t0);

        // 1 second later: jump to pitch 10.0
        const auto t1 = t0 + std::chrono::seconds(1);
        comp.updateAttitude(0.0, 10.0, 0.0, 0.0, t1);

        const auto st = comp.attitudeState();
        // With alpha = 0.5, pitch should be 0.0 + 0.5*(10.0 - 0.0) = 5.0
        EXPECT_NEAR(st.pitchDeg, 5.0, 1e-2);
        // Pitch rate should be 10 deg/s
        EXPECT_NEAR(st.pitchRateDegPerSec, 10.0, 1e-2);
    }

    TEST(TestVesselAttitudeCompensator, GeoLockControllerAttitudeStabilization)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        GeoLockController geoLock(payload);
        auto attComp = std::make_shared<VesselAttitudeCompensator>();

        geoLock.setAttitudeCompensator(attComp);
        EXPECT_EQ(geoLock.attitudeCompensator(), attComp);
        EXPECT_FALSE(geoLock.isAttitudeStabilized()); // No attitude data yet

        const auto now = std::chrono::steady_clock::now();
        attComp->updateAttitude(0.0, 4.0, 0.0, 0.0, now);
        EXPECT_TRUE(geoLock.isAttitudeStabilized());

        Klv::GeoPoint3D target { 37.01, -122.0, 0.0 };
        ASSERT_TRUE(geoLock.engage(target));

        Klv::GeoPoint3D platformPos { 37.0, -122.0, 0.0 };
        // updatePlatform should use the pitch 4.0 from attComp
        EXPECT_TRUE(geoLock.updatePlatform(platformPos, 0.0));

        const auto status = geoLock.status();
        EXPECT_TRUE(status.engaged);
        // Tilt should compensate for +4 deg pitch (commanded around -4 deg)
        EXPECT_NEAR(status.commandedTiltDeg, -4.0, 0.5);
    }

} // namespace
} // namespace PayloadHal
