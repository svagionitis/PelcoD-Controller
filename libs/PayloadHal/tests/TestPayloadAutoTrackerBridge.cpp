/// @file TestPayloadAutoTrackerBridge.cpp
/// @brief Unit tests for PayloadAutoTrackerBridge visual auto-tracking controller.

#include "PayloadAutoTrackerBridge.h"
#include "sim/SimulatedPayload.h"

#include <chrono>
#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

namespace PayloadHal {
namespace {

    TEST(TestPayloadAutoTrackerBridge, LifecycleAndEngagement)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        PayloadAutoTrackerBridge bridge(payload);
        EXPECT_FALSE(bridge.isEngaged());

        auto status = bridge.status();
        EXPECT_FALSE(status.engaged);
        EXPECT_EQ(status.trackingState, Tracking::PtzAutoTracker::TrackingState::Idle);

        EXPECT_TRUE(bridge.engage());
        EXPECT_TRUE(bridge.isEngaged());
        EXPECT_TRUE(bridge.status().engaged);

        bridge.disengage();
        EXPECT_FALSE(bridge.isEngaged());
        EXPECT_FALSE(bridge.status().engaged);
    }

    TEST(TestPayloadAutoTrackerBridge, NormalizedVelocityDispatch)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        PayloadAutoTrackerBridge bridge(payload);
        bridge.setDriveMode(TrackerDriveMode::NormalizedVelocity);
        EXPECT_EQ(bridge.driveMode(), TrackerDriveMode::NormalizedVelocity);

        bridge.setDeadbands(0.01, 0.01);
        ASSERT_TRUE(bridge.engage());

        // Target to the right (errorX = +0.5) and below boresight (errorY = +0.3)
        VisualTargetDetection detection;
        detection.errorX = 0.5;
        detection.errorY = 0.3;
        detection.velocityX = 0.0;
        detection.velocityY = 0.0;
        detection.isLocked = true;
        detection.isCoasting = false;

        EXPECT_TRUE(bridge.updateVisual(detection, 0.033));

        auto status = bridge.status();
        EXPECT_EQ(status.updateCount, 1U);
        EXPECT_EQ(status.trackingState, Tracking::PtzAutoTracker::TrackingState::Tracking);
        // Pan should command right (positive velocity)
        EXPECT_GT(status.commandedPanVel, 0.0);
        // Tilt should command downwards (negative velocity)
        EXPECT_LT(status.commandedTiltVel, 0.0);

        // Reset
        bridge.reset();
        status = bridge.status();
        EXPECT_EQ(status.trackingState, Tracking::PtzAutoTracker::TrackingState::Idle);
        EXPECT_NEAR(status.commandedPanVel, 0.0, 1e-6);
        EXPECT_NEAR(status.commandedTiltVel, 0.0, 1e-6);
    }

    TEST(TestPayloadAutoTrackerBridge, PhysicalRateDispatch)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        PayloadAutoTrackerBridge bridge(payload);
        bridge.setDriveMode(TrackerDriveMode::PhysicalRate);
        EXPECT_EQ(bridge.driveMode(), TrackerDriveMode::PhysicalRate);

        bridge.setMaxPhysicalRates(60.0, 30.0);
        bridge.setDeadbands(0.01, 0.01);
        ASSERT_TRUE(bridge.engage());

        VisualTargetDetection detection;
        detection.errorX = 0.4;
        detection.errorY = 0.0;
        detection.isLocked = true;

        EXPECT_TRUE(bridge.updateVisual(detection, 0.033));
        auto status = bridge.status();
        EXPECT_GT(status.commandedPanVel, 1.0); // Scaled into deg/sec range (> 1.0 deg/s)
        EXPECT_LE(status.commandedPanVel, 60.0);
    }

    TEST(TestPayloadAutoTrackerBridge, AutoZoomFraming)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        PayloadAutoTrackerBridge bridge(payload);
        bridge.setAutoZoomEnabled(true);
        bridge.setTargetFramingHeight(0.20, 0.04);
        bridge.setDeadbands(0.01, 0.01);
        ASSERT_TRUE(bridge.engage());

        // Target well centered but tiny (8% viewport -> needs zoom Tele / In)
        VisualTargetDetection smallTarget;
        smallTarget.errorX = 0.0;
        smallTarget.errorY = 0.0;
        smallTarget.targetNormHeight = 0.08;
        smallTarget.isLocked = true;

        EXPECT_TRUE(bridge.updateVisual(smallTarget, 0.033));
        auto status = bridge.status();
        EXPECT_GT(status.commandedZoomVel, 0.0); // Positive indicates Zoom Tele

        // Target well centered but huge (45% viewport -> needs zoom Wide / Out)
        VisualTargetDetection largeTarget;
        largeTarget.errorX = 0.0;
        largeTarget.errorY = 0.0;
        largeTarget.targetNormHeight = 0.45;
        largeTarget.isLocked = true;

        EXPECT_TRUE(bridge.updateVisual(largeTarget, 0.033));
        status = bridge.status();
        EXPECT_LT(status.commandedZoomVel, 0.0); // Negative indicates Zoom Wide

        // Target inside framing deadband (20% viewport -> zoom stop)
        VisualTargetDetection goodTarget;
        goodTarget.errorX = 0.0;
        goodTarget.errorY = 0.0;
        goodTarget.targetNormHeight = 0.20;
        goodTarget.isLocked = true;

        EXPECT_TRUE(bridge.updateVisual(goodTarget, 0.033));
        status = bridge.status();
        EXPECT_NEAR(status.commandedZoomVel, 0.0, 1e-6);
    }

    TEST(TestPayloadAutoTrackerBridge, BoundingBoxIngestion)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        PayloadAutoTrackerBridge bridge(payload);
        bridge.setDeadbands(0.01, 0.01);
        ASSERT_TRUE(bridge.engage());

        // Bounding box at bottom-right of viewport: normX=0.7, normY=0.7, normW=0.1, normH=0.1
        // Center is (0.75, 0.75), which translates to errorX = +0.5, errorY = +0.5
        EXPECT_TRUE(bridge.updateBoundingBox(0.7, 0.7, 0.1, 0.1, true, 0.033));

        auto status = bridge.status();
        EXPECT_EQ(status.updateCount, 1U);
        EXPECT_NEAR(status.lastErrorX, 0.5, 1e-6);
        EXPECT_NEAR(status.lastErrorY, 0.5, 1e-6);
        EXPECT_GT(status.commandedPanVel, 0.0);
        EXPECT_LT(status.commandedTiltVel, 0.0);
    }

    TEST(TestPayloadAutoTrackerBridge, BackgroundTrackingLoop)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        PayloadAutoTrackerBridge bridge(payload);
        bridge.setDeadbands(0.01, 0.01);
        ASSERT_TRUE(bridge.engage());

        std::atomic<double> errX { 0.2 };
        bridge.setTargetProvider([&]() -> std::optional<VisualTargetDetection> {
            VisualTargetDetection d;
            d.errorX = errX.load();
            d.errorY = 0.0;
            d.isLocked = true;
            return d;
        });

        EXPECT_FALSE(bridge.isTrackingLoopRunning());
        EXPECT_TRUE(bridge.startTrackingLoop(50.0));
        EXPECT_TRUE(bridge.isTrackingLoopRunning());

        std::this_thread::sleep_for(std::chrono::milliseconds(80));

        auto status = bridge.status();
        EXPECT_GT(status.updateCount, 0U);
        EXPECT_GT(status.commandedPanVel, 0.0);

        bridge.stopTrackingLoop();
        EXPECT_FALSE(bridge.isTrackingLoopRunning());
    }

} // namespace
} // namespace PayloadHal
