#include <gtest/gtest.h>

#include "CameraStreamBinder.h"
#include "DemRayCaster.h"
#include "GeoreferenceUtils.h"
#include "IPanTiltUnit.h"
#include "PayloadHal.h"
#include "PayloadKlvGenerator.h"
#include "sim/SimulatedPayload.h"

#include <chrono>
#include <cmath>
#include <memory>
#include <thread>

using namespace PayloadHal;

// =============================================================================
// 1. Horizon Leveling Kinematics Tests
// =============================================================================

TEST(TestGimbalRollAndHorizonLeveling, KinematicsLevelingRoll)
{
    // Level flight, looking straight forward: leveling roll = 0.0
    EXPECT_NEAR(GeoreferenceUtils::computeLevelingRoll(0.0, 0.0, 0.0, 0.0), 0.0, 1e-4);

    // Bank right 15°, looking forward: counter-roll should be -15.0°
    EXPECT_NEAR(GeoreferenceUtils::computeLevelingRoll(15.0, 0.0, 0.0, 0.0), -15.0, 1e-4);

    // Bank left 20°, looking forward: counter-roll should be +20.0°
    EXPECT_NEAR(GeoreferenceUtils::computeLevelingRoll(-20.0, 0.0, 0.0, 0.0), 20.0, 1e-4);

    // Bank right 15°, looking 90° right: platform roll does not tilt horizon in image
    EXPECT_NEAR(GeoreferenceUtils::computeLevelingRoll(15.0, 0.0, 90.0, 0.0), 0.0, 1e-3);

    // Pitch up 10°, looking 90° right: platform pitch acts as roll in image frame
    EXPECT_NEAR(GeoreferenceUtils::computeLevelingRoll(0.0, 10.0, 90.0, 0.0), -10.0, 1e-3);

    // Pitch down 12°, looking 90° left (-90° pan): counter-roll should be -12.0°
    EXPECT_NEAR(GeoreferenceUtils::computeLevelingRoll(0.0, -12.0, -90.0, 0.0), -12.0, 1e-3);

    // Looking straight down (nadir, tilt = -90°): returns 0.0 safely without singularity
    EXPECT_NEAR(GeoreferenceUtils::computeLevelingRoll(10.0, 5.0, 0.0, -90.0), 0.0, 1e-3);
}

// =============================================================================
// 2. Frustum Corners with Roll Axis
// =============================================================================

TEST(TestGimbalRollAndHorizonLeveling, FrustumCornersRollRotation)
{
    const Klv::GeoPoint3D platform { 45.0, 10.0, 1000.0 };
    const double heading = 0.0;
    const double pan = 0.0;
    const double tilt = -45.0;
    const double hfov = 30.0;
    const double vfov = 20.0;

    // Zero roll frustum
    const auto unrolled = GeoreferenceUtils::computeFrustumCorners(platform, heading, pan, tilt, hfov, vfov, 0.0, 0.0);
    ASSERT_TRUE(unrolled.has_value());

    // In level flight looking North (heading 0), top-left and top-right have the same latitude
    EXPECT_NEAR(unrolled->topLeft.latitudeDeg, unrolled->topRight.latitudeDeg, 1e-4);
    // And top-left is west of top-right
    EXPECT_LT(unrolled->topLeft.longitudeDeg, unrolled->topRight.longitudeDeg);

    // With +30° optical roll, the frame rotates clockwise
    const auto rolled = GeoreferenceUtils::computeFrustumCorners(platform, heading, pan, tilt, hfov, vfov, 0.0, 30.0);
    ASSERT_TRUE(rolled.has_value());

    // Because of clockwise rotation, top-left corner dips downwards (lower latitude)
    // and top-right corner rises upwards (higher latitude)
    EXPECT_LT(rolled->topLeft.latitudeDeg, unrolled->topLeft.latitudeDeg);
    EXPECT_GT(rolled->topRight.latitudeDeg, unrolled->topRight.latitudeDeg);

    // Test with DEM provider
    ProceduralDemProvider dem([](double /*lat*/, double /*lon*/) {
        return 50.0;
    }, 50.0, 50.0);

    const auto demCorners = GeoreferenceUtils::computeFrustumCorners(
        dem, platform, heading, pan, tilt, hfov, vfov, 25.0);
    ASSERT_TRUE(demCorners.has_value());
    EXPECT_GT(demCorners->topLeft.latitudeDeg, demCorners->bottomLeft.latitudeDeg);
}

// =============================================================================
// 3. Simulated PTU 3-Axis Motion & Travel Limits
// =============================================================================

TEST(TestGimbalRollAndHorizonLeveling, SimPtu3AxisControlAndLimits)
{
    SimulatedPayload payload;
    ASSERT_TRUE(payload.connect());

    const auto ptu = payload.panTilt();
    ASSERT_NE(ptu, nullptr);

    EXPECT_TRUE(ptu->hasRollAxis());
    EXPECT_TRUE(ptu->supportsHorizonLeveling());

    double minRoll = 0.0;
    double maxRoll = 0.0;
    EXPECT_TRUE(ptu->getRollLimits(minRoll, maxRoll));
    EXPECT_DOUBLE_EQ(minRoll, -60.0);
    EXPECT_DOUBLE_EQ(maxRoll, 60.0);

    // Roll angle slew within limits
    EXPECT_TRUE(ptu->setRollAngle(35.0));
    auto telem = ptu->currentTelemetry();
    EXPECT_NEAR(telem.rollAngleDeg, 35.0, 1e-4);
    EXPECT_FALSE(telem.isMoving);

    // Roll angle clamped to maximum limit (+60°)
    EXPECT_TRUE(ptu->setRollAngle(95.0));
    telem = ptu->currentTelemetry();
    EXPECT_DOUBLE_EQ(telem.rollAngleDeg, 60.0);

    // Roll angle clamped to minimum limit (-60°)
    EXPECT_TRUE(ptu->setRollAngle(-80.0));
    telem = ptu->currentTelemetry();
    EXPECT_DOUBLE_EQ(telem.rollAngleDeg, -60.0);

    // 3-Axis coordinated slew
    EXPECT_TRUE(ptu->setAbsoluteAngles3Axis(180.0, -30.0, 15.0));
    telem = ptu->currentTelemetry();
    EXPECT_NEAR(telem.panAngleDeg, 180.0, 1e-4);
    EXPECT_NEAR(telem.tiltAngleDeg, -30.0, 1e-4);
    EXPECT_NEAR(telem.rollAngleDeg, 15.0, 1e-4);

    // 3-Axis coordinated velocity
    EXPECT_TRUE(ptu->setRate3Axis(10.0, -5.0, 3.0));
    telem = ptu->currentTelemetry();
    EXPECT_DOUBLE_EQ(telem.panRateDegPerSec, 10.0);
    EXPECT_DOUBLE_EQ(telem.tiltRateDegPerSec, -5.0);
    EXPECT_DOUBLE_EQ(telem.rollRateDegPerSec, 3.0);
    EXPECT_TRUE(telem.isMoving);

    // Stop motion stops roll rate as well
    EXPECT_TRUE(ptu->stopMotion());
    telem = ptu->currentTelemetry();
    EXPECT_DOUBLE_EQ(telem.panRateDegPerSec, 0.0);
    EXPECT_DOUBLE_EQ(telem.tiltRateDegPerSec, 0.0);
    EXPECT_DOUBLE_EQ(telem.rollRateDegPerSec, 0.0);
    EXPECT_FALSE(telem.isMoving);
}

// =============================================================================
// 4. Simulated Horizon Leveling Mode
// =============================================================================

TEST(TestGimbalRollAndHorizonLeveling, SimHorizonLevelingLifecycle)
{
    SimulatedPayload payload;
    ASSERT_TRUE(payload.connect());
    const auto ptu = payload.panTilt();
    ASSERT_NE(ptu, nullptr);

    EXPECT_FALSE(ptu->isHorizonLevelingEnabled());

    // Enable horizon leveling
    EXPECT_TRUE(ptu->setHorizonLeveling(true));
    EXPECT_TRUE(ptu->isHorizonLevelingEnabled());
    EXPECT_EQ(ptu->stabilizationMode(), StabilizationMode::HorizonLevel);
    EXPECT_TRUE(ptu->currentTelemetry().isHorizonLeveled);

    // Center pan/tilt
    EXPECT_TRUE(ptu->setAbsoluteAngles(0.0, 0.0));

    // Platform banks right 20°
    payload.setSimulatedPlatformAttitude(0.0, 20.0);
    auto telem = ptu->currentTelemetry();
    EXPECT_NEAR(telem.rollAngleDeg, -20.0, 1e-3);
    EXPECT_TRUE(telem.isHorizonLeveled);

    // Pan to 90° right, platform pitches up 15°
    EXPECT_TRUE(ptu->setAbsoluteAngles(90.0, 0.0));
    payload.setSimulatedPlatformAttitude(15.0, 0.0);
    telem = ptu->currentTelemetry();
    EXPECT_NEAR(telem.rollAngleDeg, -15.0, 1e-3);

    // Disable horizon leveling
    EXPECT_TRUE(ptu->setHorizonLeveling(false));
    EXPECT_FALSE(ptu->isHorizonLevelingEnabled());
    EXPECT_NE(ptu->stabilizationMode(), StabilizationMode::HorizonLevel);

    // Re-enable via stabilization mode
    EXPECT_TRUE(ptu->setStabilizationMode(StabilizationMode::HorizonLevel));
    EXPECT_TRUE(ptu->isHorizonLevelingEnabled());
    EXPECT_TRUE(ptu->currentTelemetry().isHorizonLeveled);
}

// =============================================================================
// 5. 2-Axis PTU Backward Compatibility Fallbacks
// =============================================================================

class MockTwoAxisPtu : public IPanTiltUnit {
public:
    bool connect() override { return true; }
    void disconnect() override {}
    [[nodiscard]] bool isConnected() const noexcept override { return true; }
    [[nodiscard]] DeviceState state() const noexcept override { return DeviceState::Ready; }
    [[nodiscard]] DeviceInfo info() const noexcept override { return {}; }
    void registerStateCallback(StateCallback) override {}

    bool setRate(double pan, double tilt) override
    {
        m_panRate = pan;
        m_tiltRate = tilt;
        return true;
    }
    bool setNormalizedVelocity(float, float) override { return true; }
    bool setAbsoluteAngles(double pan, double tilt) override
    {
        m_pan = pan;
        m_tilt = tilt;
        return true;
    }
    bool setRelativeNudge(double, double) override { return true; }
    bool stopMotion() override { return true; }
    [[nodiscard]] bool supportsStabilization() const noexcept override { return false; }
    bool setStabilizationMode(StabilizationMode) override { return false; }
    [[nodiscard]] StabilizationMode stabilizationMode() const noexcept override { return StabilizationMode::Disabled; }
    bool zeroGyroDrift() override { return false; }
    bool getLimits(double&, double&, double&, double&) const override { return false; }
    bool savePreset(uint8_t, const std::string&) override { return false; }
    bool recallPreset(uint8_t) override { return false; }
    void registerTelemetryCallback(TelemetryCallback) override {}
    [[nodiscard]] GimbalTelemetry currentTelemetry() const override
    {
        GimbalTelemetry t {};
        t.panAngleDeg = m_pan;
        t.tiltAngleDeg = m_tilt;
        t.panRateDegPerSec = m_panRate;
        t.tiltRateDegPerSec = m_tiltRate;
        return t;
    }

    double m_pan { 0.0 };
    double m_tilt { 0.0 };
    double m_panRate { 0.0 };
    double m_tiltRate { 0.0 };
};

TEST(TestGimbalRollAndHorizonLeveling, TwoAxisPtuFallbackSafety)
{
    MockTwoAxisPtu ptu;

    // 2-axis PTU defaults from interface
    EXPECT_FALSE(ptu.hasRollAxis());
    EXPECT_FALSE(ptu.supportsHorizonLeveling());
    EXPECT_FALSE(ptu.isHorizonLevelingEnabled());
    EXPECT_FALSE(ptu.setRollAngle(10.0));
    EXPECT_FALSE(ptu.setRollRate(5.0));
    EXPECT_FALSE(ptu.setHorizonLeveling(true));
    EXPECT_FALSE(ptu.updateHorizonLeveling(10.0, 5.0));

    double minR = 0.0;
    double maxR = 0.0;
    EXPECT_FALSE(ptu.getRollLimits(minR, maxR));

    // setAbsoluteAngles3Axis with 0.0 roll succeeds and forwards to setAbsoluteAngles
    EXPECT_TRUE(ptu.setAbsoluteAngles3Axis(45.0, -10.0, 0.0));
    EXPECT_DOUBLE_EQ(ptu.m_pan, 45.0);
    EXPECT_DOUBLE_EQ(ptu.m_tilt, -10.0);

    // setAbsoluteAngles3Axis with non-zero roll fails safely
    EXPECT_FALSE(ptu.setAbsoluteAngles3Axis(45.0, -10.0, 5.0));

    // setRate3Axis with 0.0 roll rate succeeds
    EXPECT_TRUE(ptu.setRate3Axis(12.0, 6.0, 0.0));
    EXPECT_DOUBLE_EQ(ptu.m_panRate, 12.0);
    EXPECT_DOUBLE_EQ(ptu.m_tiltRate, 6.0);

    // setRate3Axis with non-zero roll rate fails safely
    EXPECT_FALSE(ptu.setRate3Axis(12.0, 6.0, 2.0));
}

// =============================================================================
// 6. MISB ST 0601 Tag 20 Sensor Relative Roll Telemetry
// =============================================================================

TEST(TestGimbalRollAndHorizonLeveling, KlvTag20SensorRelativeRoll)
{
    auto payload = std::make_shared<SimulatedPayload>();
    ASSERT_TRUE(payload->connect());

    const auto ptu = payload->panTilt();
    ASSERT_NE(ptu, nullptr);

    // Slew roll axis to +22.5°
    EXPECT_TRUE(ptu->setRollAngle(22.5));

    PayloadKlvGenerator generator(payload);
    PlatformNavData nav {};
    nav.position = { 38.0, 23.0, 1500.0 };
    nav.headingDeg = 45.0;

    const auto msg = generator.buildMessage(nav);
    ASSERT_TRUE(msg.sensorRelRollDeg.has_value());
    EXPECT_NEAR(*msg.sensorRelRollDeg, 22.5, 1e-4);

    // Negative roll (-18.0°) normalized to [0, 360) -> 342.0°
    EXPECT_TRUE(ptu->setRollAngle(-18.0));
    const auto msgNeg = generator.buildMessage(nav);
    ASSERT_TRUE(msgNeg.sensorRelRollDeg.has_value());
    EXPECT_NEAR(*msgNeg.sensorRelRollDeg, 342.0, 1e-4);
}

// =============================================================================
// 7. Video Stream Binder Roll Interpolation
// =============================================================================

TEST(TestGimbalRollAndHorizonLeveling, StreamBinderRollInterpolation)
{
    auto payload = std::make_shared<SimulatedPayload>();
    ASSERT_TRUE(payload->connect());

    CameraStreamBinder binder(payload, VideoStreamProfile::Primary);

    const auto t0 = std::chrono::system_clock::now();
    const auto t1 = t0 + std::chrono::milliseconds(100);
    const auto tMid = t0 + std::chrono::milliseconds(50);

    GimbalTelemetry telem1 {};
    telem1.timestamp = t0;
    telem1.panAngleDeg = 10.0;
    telem1.tiltAngleDeg = -20.0;
    telem1.rollAngleDeg = 10.0;
    telem1.rollRateDegPerSec = 2.0;
    telem1.isHorizonLeveled = false;

    GimbalTelemetry telem2 {};
    telem2.timestamp = t1;
    telem2.panAngleDeg = 20.0;
    telem2.tiltAngleDeg = -10.0;
    telem2.rollAngleDeg = 20.0;
    telem2.rollRateDegPerSec = 4.0;
    telem2.isHorizonLeveled = true;

    binder.recordGimbalTelemetry(telem1);
    binder.recordGimbalTelemetry(telem2);

    RawVideoFrame frame {};
    frame.timestamp = tMid;
    frame.width = 1920;
    frame.height = 1080;

    const auto syncd = binder.bindFrame(std::move(frame));
    EXPECT_NEAR(syncd.gimbalTelemetry.rollAngleDeg, 15.0, 1e-3);
    EXPECT_NEAR(syncd.gimbalTelemetry.rollRateDegPerSec, 3.0, 1e-3);
    EXPECT_TRUE(syncd.gimbalTelemetry.isHorizonLeveled);
}
