/// @file TestPtzSlewToCueBridge.cpp
/// @brief Comprehensive unit tests for PtzSlewToCueBridge.

#include "PtzAutoTracker.h"
#include "PtzSlewToCueBridge.h"
#include "PelcoDCore/PelcoDFrame.h"
#include <gtest/gtest.h>
#include <cmath>

using namespace Tracking;

class PtzSlewToCueBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_tracker = std::make_unique<PtzAutoTracker>();
        m_config.coarseThresholdDeg = 4.0;
        m_config.settleToleranceDeg = 0.25;
        m_config.enableAutoZoom = true;
        m_config.pelcoAddress = 1U;
        m_bridge = std::make_unique<PtzSlewToCueBridge>(*m_tracker, m_config);
    }

    std::unique_ptr<PtzAutoTracker> m_tracker;
    SlewBridgeConfig m_config{};
    std::unique_ptr<PtzSlewToCueBridge> m_bridge;
};

TEST_F(PtzSlewToCueBridgeTest, InitialStateIsIdle) {
    EXPECT_EQ(m_bridge->getState(), SlewState::Idle);
    EXPECT_FALSE(m_bridge->hasCue());

    const auto batch = m_bridge->update(0.0, 0.0, 30.0, 0.05);
    EXPECT_EQ(batch.state, SlewState::Idle);
    EXPECT_TRUE(batch.coarsePanCmd.empty());
    EXPECT_TRUE(batch.coarseTiltCmd.empty());
    EXPECT_TRUE(batch.rateMotionCmd.empty());
}

TEST_F(PtzSlewToCueBridgeTest, ShortestAngleDeltaWrapping) {
    // Basic deltas
    EXPECT_NEAR(PtzSlewToCueBridge::shortestAngleDelta(10.0, 5.0), 5.0, 1e-6);
    EXPECT_NEAR(PtzSlewToCueBridge::shortestAngleDelta(5.0, 10.0), -5.0, 1e-6);

    // Cross 0/360 wrap: target 1 deg, current 359 deg -> delta is +2 deg
    EXPECT_NEAR(PtzSlewToCueBridge::shortestAngleDelta(1.0, 359.0), 2.0, 1e-6);

    // Cross 0/360 wrap: target 359 deg, current 1 deg -> delta is -2 deg
    EXPECT_NEAR(PtzSlewToCueBridge::shortestAngleDelta(359.0, 1.0), -2.0, 1e-6);

    // Exactly 180 degrees
    EXPECT_NEAR(std::abs(PtzSlewToCueBridge::shortestAngleDelta(180.0, 0.0)), 180.0, 1e-6);
}

TEST_F(PtzSlewToCueBridgeTest, StationaryMastDueNorth) {
    PlatformNavState platform{};
    platform.position = Klv::GeoPoint3D { 0.0, 0.0, 100.0 };
    platform.headingDeg = 0.0;
    platform.pitchDeg = 0.0;
    platform.rollDeg = 0.0;

    m_bridge->updatePlatform(platform);

    // Target 0.01 deg North (approx 1113 meters), same elevation
    TargetCue cue{};
    cue.position = Klv::GeoPoint3D { 0.01, 0.0, 100.0 };
    cue.targetRadiusM = 2.5;
    m_bridge->updateCue(cue);

    EXPECT_TRUE(m_bridge->hasCue());
    EXPECT_EQ(m_bridge->getState(), SlewState::CoarseAcquisition);

    double panDeg{0.0};
    double tiltDeg{0.0};
    double slantM{0.0};
    EXPECT_TRUE(PtzSlewToCueBridge::solveGimbalAngles(platform, cue.position, panDeg, tiltDeg, slantM));

    EXPECT_NEAR(panDeg, 0.0, 0.1);
    EXPECT_NEAR(tiltDeg, 0.0, 0.1);
    EXPECT_GT(slantM, 1100.0);
    EXPECT_LT(slantM, 1120.0);
}

TEST_F(PtzSlewToCueBridgeTest, PlatformAttitudeCompensation) {
    PlatformNavState platform{};
    platform.position = Klv::GeoPoint3D { 37.0, -120.0, 500.0 };
    // Host vehicle heading 090 (East), pitched 5 deg up, rolled 0 deg
    platform.headingDeg = 90.0;
    platform.pitchDeg = 5.0;
    platform.rollDeg = 0.0;

    // Target is due East (same as platform heading), same altitude
    Klv::GeoPoint3D target{ 37.0, -119.99, 500.0 };

    double panDeg{0.0};
    double tiltDeg{0.0};
    double slantM{0.0};
    EXPECT_TRUE(PtzSlewToCueBridge::solveGimbalAngles(platform, target, panDeg, tiltDeg, slantM));

    // Because vehicle heading is East (90 deg), target due East should be at Gimbal Pan = 0 deg
    EXPECT_NEAR(PtzSlewToCueBridge::shortestAngleDelta(panDeg, 0.0), 0.0, 0.2);
    // Because vehicle is pitched 5 deg nose up, target on horizon is 5 deg below vehicle nose (Tilt = -5 deg)
    EXPECT_NEAR(tiltDeg, -5.0, 0.2);
}

TEST_F(PtzSlewToCueBridgeTest, CoarseSlewTriggersAbsolutePositionCommands) {
    PlatformNavState platform{};
    platform.position = Klv::GeoPoint3D { 0.0, 0.0, 50.0 };
    m_bridge->updatePlatform(platform);

    // Target 45 degrees East of North
    TargetCue cue{};
    cue.position = Klv::GeoPoint3D { 0.01, 0.01, 50.0 };
    m_bridge->updateCue(cue);

    // Gimbal currently looking North (0 deg)
    const auto batch = m_bridge->update(0.0, 0.0, 30.0, 0.05);

    EXPECT_EQ(batch.state, SlewState::CoarseAcquisition);
    // Coarse commands should be populated with absolute pan/tilt
    ASSERT_FALSE(batch.coarsePanCmd.empty());
    ASSERT_FALSE(batch.coarseTiltCmd.empty());
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(batch.coarsePanCmd));
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(batch.coarseTiltCmd));

    // Verify command opcode 0x4B (Set Pan Position)
    EXPECT_EQ(batch.coarsePanCmd[3], 0x4BU);
    // Verify command opcode 0x4D (Set Tilt Position)
    EXPECT_EQ(batch.coarseTiltCmd[3], 0x4DU);
}

TEST_F(PtzSlewToCueBridgeTest, FineTrackingAndSettlingTransitions) {
    PlatformNavState platform{};
    platform.position = Klv::GeoPoint3D { 0.0, 0.0, 50.0 };
    m_bridge->updatePlatform(platform);

    TargetCue cue{};
    cue.position = Klv::GeoPoint3D { 0.01, 0.0, 50.0 }; // Target at Azimuth 0.0 deg
    m_bridge->updateCue(cue);

    // 1. Current Pan at 2.0 deg (within coarseThreshold of 4.0 deg) -> FineTracking
    auto batch = m_bridge->update(2.0, 0.0, 30.0, 0.05);
    EXPECT_EQ(batch.state, SlewState::FineTracking);
    EXPECT_TRUE(batch.coarsePanCmd.empty());
    ASSERT_FALSE(batch.rateMotionCmd.empty());
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(batch.rateMotionCmd));

    // Target is at 0 deg, current is at 2 deg -> need Pan Left (error is -2 deg)
    EXPECT_LT(batch.errorAzimuthDeg, 0.0);

    // 2. Gimbal settles within settleTolerance (0.1 deg error) -> Settled
    batch = m_bridge->update(0.1, 0.0, 30.0, 0.05);
    EXPECT_EQ(batch.state, SlewState::Settled);
}

TEST_F(PtzSlewToCueBridgeTest, AutoZoomFramingAdjustment) {
    PlatformNavState platform{};
    platform.position = Klv::GeoPoint3D { 0.0, 0.0, 100.0 };
    m_bridge->updatePlatform(platform);

    TargetCue cue{};
    cue.position = Klv::GeoPoint3D { 0.01, 0.0, 100.0 }; // Slant range ~1113 meters
    cue.targetRadiusM = 5.0; // 5 meter target
    m_bridge->updateCue(cue);

    // Desired HFOV = 2 * atan2(5.0 * 1.5, 1113) rad = 2 * atan2(7.5, 1113) rad ~ 0.77 deg
    // Clamped to minHfovDeg = 2.0 deg
    // If current camera HFOV is 30.0 deg (wide), bridge should command Zoom Tele (In)
    const auto batch = m_bridge->update(0.0, 0.0, 30.0, 0.05);

    ASSERT_FALSE(batch.zoomCmd.empty());
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(batch.zoomCmd));
    // Pelco-D Command 2 byte for Zoom Tele is bit 5 (0x20)
    EXPECT_EQ(batch.zoomCmd[3], 0x20U);
}

TEST_F(PtzSlewToCueBridgeTest, ClearCueTransitionsToIdle) {
    PlatformNavState platform{};
    platform.position = Klv::GeoPoint3D { 0.0, 0.0, 100.0 };
    m_bridge->updatePlatform(platform);

    TargetCue cue{};
    cue.position = Klv::GeoPoint3D { 0.01, 0.0, 100.0 };
    m_bridge->updateCue(cue);
    EXPECT_TRUE(m_bridge->hasCue());

    m_bridge->clearCue();
    EXPECT_FALSE(m_bridge->hasCue());
    EXPECT_EQ(m_bridge->getState(), SlewState::Idle);

    const auto batch = m_bridge->update(0.0, 0.0, 30.0, 0.05);
    EXPECT_EQ(batch.state, SlewState::Idle);
    EXPECT_TRUE(batch.coarsePanCmd.empty());
    EXPECT_TRUE(batch.rateMotionCmd.empty());
}
