/// @file TestStreamHealthMonitor.cpp
/// @brief GoogleTest unit tests for StreamHealthMonitor diagnostic state machine and anomaly detection.

#include "StreamHealthMonitor.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>

namespace {

// Helper to generate a test frame with custom fill pattern
std::vector<std::uint8_t> createTestFrame(int width, int height, std::uint8_t baseVal, int shiftX = 0)
{
    std::vector<std::uint8_t> frame(static_cast<std::size_t>(width * height * 3), baseVal);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto val = static_cast<std::uint8_t>(baseVal + ((x + shiftX * 8 + y) % 64));
            const std::size_t idx = static_cast<std::size_t>((y * width + x) * 3);
            frame[idx + 0] = val;
            frame[idx + 1] = val;
            frame[idx + 2] = val;
        }
    }
    return frame;
}

} // namespace

TEST(TestStreamHealthMonitor, InitialStateAndConfig)
{
    Video::StreamHealthMonitor monitor;

    EXPECT_EQ(monitor.getState(), Video::StreamHealthState::Healthy);
    EXPECT_EQ(monitor.getMetrics().totalFramesAnalyzed, 0U);

    Video::StreamHealthConfig cfg = monitor.getConfig();
    EXPECT_DOUBLE_EQ(cfg.nominalFps, 30.0);
    EXPECT_DOUBLE_EQ(cfg.freezeDurationThresholdSec, 2.5);
    EXPECT_DOUBLE_EQ(cfg.signalLossTimeoutSec, 1.5);

    cfg.freezeDurationThresholdSec = 1.0;
    monitor.setConfig(cfg);
    EXPECT_DOUBLE_EQ(monitor.getConfig().freezeDurationThresholdSec, 1.0);
}

TEST(TestStreamHealthMonitor, HealthyDynamicStreaming)
{
    Video::StreamHealthMonitor monitor;
    const int W = 64;
    const int H = 48;

    // Ingest 30 dynamic frames over 1.0 second (30 fps)
    double t = 0.0;
    for (int i = 0; i < 30; ++i) {
        const std::vector<std::uint8_t> frame = createTestFrame(W, H, 100, i * 2);
        monitor.ingestFrame(frame.data(), W, H, Video::PixelFormat::RGB24, t);
        t += (1.0 / 30.0);
    }

    const Video::StreamHealthMetrics m = monitor.getMetrics();
    EXPECT_EQ(m.state, Video::StreamHealthState::Healthy);
    EXPECT_EQ(m.totalFramesAnalyzed, 30U);
    EXPECT_GT(m.frameDifference, 0.0);
    EXPECT_NEAR(m.measuredFps, 30.0, 3.0);
    EXPECT_GT(m.meanLuminance, 50.0);
    EXPECT_LT(m.meanLuminance, 200.0);
}

TEST(TestStreamHealthMonitor, FreezeDetectionAndRecovery)
{
    Video::StreamHealthConfig cfg {};
    cfg.nominalFps = 30.0;
    cfg.freezeDurationThresholdSec = 1.0; // 1 second threshold for test speed
    cfg.freezeDifferenceThreshold = 0.5;

    Video::StreamHealthMonitor monitor(cfg);
    const int W = 64;
    const int H = 48;

    const std::vector<std::uint8_t> staticFrame = createTestFrame(W, H, 120, 0);

    // Initial frame at t = 0.0
    monitor.ingestFrame(staticFrame.data(), W, H, Video::PixelFormat::RGB24, 0.0);
    EXPECT_EQ(monitor.getState(), Video::StreamHealthState::Healthy);

    // Ingest identical frames up to t = 0.8s (< threshold 1.0s)
    double t = 0.033;
    while (t < 0.8) {
        monitor.ingestFrame(staticFrame.data(), W, H, Video::PixelFormat::RGB24, t);
        t += 0.033;
    }
    EXPECT_EQ(monitor.getState(), Video::StreamHealthState::Healthy);

    // Ingest identical frames up to t = 1.2s (>= threshold 1.0s)
    while (t <= 1.2) {
        monitor.ingestFrame(staticFrame.data(), W, H, Video::PixelFormat::RGB24, t);
        t += 0.033;
    }
    EXPECT_EQ(monitor.getState(), Video::StreamHealthState::Frozen);
    EXPECT_EQ(monitor.getMetrics().freezeCount, 1U);

    // Recovery: Ingest a new, modified frame with motion
    t += 0.033;
    const std::vector<std::uint8_t> movedFrame = createTestFrame(W, H, 120, 20);
    monitor.ingestFrame(movedFrame.data(), W, H, Video::PixelFormat::RGB24, t);

    EXPECT_EQ(monitor.getState(), Video::StreamHealthState::Healthy);
}

TEST(TestStreamHealthMonitor, SignalLossTimeoutAndRecovery)
{
    Video::StreamHealthConfig cfg {};
    cfg.signalLossTimeoutSec = 0.5; // 500 ms threshold

    Video::StreamHealthMonitor monitor(cfg);
    const int W = 32;
    const int H = 32;
    const std::vector<std::uint8_t> frame = createTestFrame(W, H, 100, 0);

    // Ingest frame at t = 0.0
    monitor.ingestFrame(frame.data(), W, H, Video::PixelFormat::RGB24, 0.0);
    EXPECT_EQ(monitor.getState(), Video::StreamHealthState::Healthy);

    // Check timeout before deadline (t = 0.3s)
    monitor.checkTimeout(0.3);
    EXPECT_EQ(monitor.getState(), Video::StreamHealthState::Healthy);

    // Check timeout after deadline (t = 0.6s)
    monitor.checkTimeout(0.6);
    EXPECT_EQ(monitor.getState(), Video::StreamHealthState::SignalLoss);
    EXPECT_EQ(monitor.getMetrics().signalLossCount, 1U);

    // Recovery: Ingest new frame
    monitor.ingestFrame(frame.data(), W, H, Video::PixelFormat::RGB24, 0.7);
    EXPECT_EQ(monitor.getState(), Video::StreamHealthState::Healthy);
}

TEST(TestStreamHealthMonitor, BlackoutDetection)
{
    Video::StreamHealthMonitor monitor;
    const int W = 32;
    const int H = 32;

    // Create dark frame (blackout)
    const std::vector<std::uint8_t> blackFrame(static_cast<std::size_t>(W * H * 3), 2U);

    monitor.ingestFrame(blackFrame.data(), W, H, Video::PixelFormat::RGB24, 0.0);
    EXPECT_EQ(monitor.getState(), Video::StreamHealthState::Blackout);
    EXPECT_EQ(monitor.getMetrics().blackoutCount, 1U);
    EXPECT_LT(monitor.getMetrics().meanLuminance, 5.0);
}

TEST(TestStreamHealthMonitor, WhiteoutDetection)
{
    Video::StreamHealthMonitor monitor;
    const int W = 32;
    const int H = 32;

    // Create saturated frame (whiteout)
    const std::vector<std::uint8_t> whiteFrame(static_cast<std::size_t>(W * H * 3), 254U);

    monitor.ingestFrame(whiteFrame.data(), W, H, Video::PixelFormat::RGB24, 0.0);
    EXPECT_EQ(monitor.getState(), Video::StreamHealthState::Whiteout);
    EXPECT_EQ(monitor.getMetrics().whiteoutCount, 1U);
    EXPECT_GT(monitor.getMetrics().meanLuminance, 250.0);
}

TEST(TestStreamHealthMonitor, DegradedFpsDetection)
{
    Video::StreamHealthConfig cfg {};
    cfg.nominalFps = 30.0;
    cfg.degradedFpsRatio = 0.5; // Alert if FPS < 15

    Video::StreamHealthMonitor monitor(cfg);
    const int W = 32;
    const int H = 32;

    // Ingest dynamic frames at 10 fps (interval = 0.1s)
    double t = 0.0;
    for (int i = 0; i < 10; ++i) {
        const std::vector<std::uint8_t> frame = createTestFrame(W, H, 100, i * 3);
        monitor.ingestFrame(frame.data(), W, H, Video::PixelFormat::RGB24, t);
        t += 0.1;
    }

    EXPECT_EQ(monitor.getState(), Video::StreamHealthState::Degraded);
    EXPECT_NEAR(monitor.getMetrics().measuredFps, 10.0, 2.0);
}

TEST(TestStreamHealthMonitor, StateTransitionCallbacksAndAutoReconnect)
{
    Video::StreamHealthConfig cfg {};
    cfg.freezeDurationThresholdSec = 0.5;
    cfg.autoReconnectOnFailure = true;

    Video::StreamHealthMonitor monitor(cfg);
    const int W = 32;
    const int H = 32;

    std::atomic<int> callbackCount { 0 };
    std::atomic<bool> reconnectInvoked { false };
    Video::StreamHealthState lastNewState { Video::StreamHealthState::Healthy };

    monitor.setHealthCallback([&](Video::StreamHealthState /*oldState*/, Video::StreamHealthState newState,
                                  const Video::StreamHealthMetrics& /*metrics*/) {
        callbackCount++;
        lastNewState = newState;
    });

    monitor.setReconnectCallback([&]() -> bool {
        reconnectInvoked = true;
        return true;
    });

    const std::vector<std::uint8_t> frame = createTestFrame(W, H, 100, 0);

    // Initial frame
    monitor.ingestFrame(frame.data(), W, H, Video::PixelFormat::RGB24, 0.0);

    // Freeze stream past 0.5s threshold
    monitor.ingestFrame(frame.data(), W, H, Video::PixelFormat::RGB24, 0.3);
    monitor.ingestFrame(frame.data(), W, H, Video::PixelFormat::RGB24, 0.6);

    EXPECT_EQ(monitor.getState(), Video::StreamHealthState::Frozen);
    EXPECT_EQ(lastNewState, Video::StreamHealthState::Frozen);
    EXPECT_TRUE(reconnectInvoked.load());
    EXPECT_GE(callbackCount.load(), 1);
}

TEST(TestStreamHealthMonitor, IFrameProcessorIntegration)
{
    Video::StreamHealthMonitor monitor;
    const int W = 32;
    const int H = 32;

    std::vector<std::uint8_t> frame = createTestFrame(W, H, 100, 5);
    const std::vector<std::uint8_t> original = frame;

    // Process via IFrameProcessor interface
    monitor.process(frame.data(), W, H, Video::PixelFormat::RGB24);

    // Monitor must not corrupt or modify the image buffer
    EXPECT_EQ(frame, original);
    EXPECT_EQ(monitor.getMetrics().totalFramesAnalyzed, 1U);
}

TEST(TestStreamHealthMonitor, StateToString)
{
    EXPECT_EQ(Video::StreamHealthMonitor::stateToString(Video::StreamHealthState::Healthy), "Healthy");
    EXPECT_EQ(Video::StreamHealthMonitor::stateToString(Video::StreamHealthState::Degraded), "Degraded");
    EXPECT_EQ(Video::StreamHealthMonitor::stateToString(Video::StreamHealthState::Frozen), "Frozen");
    EXPECT_EQ(Video::StreamHealthMonitor::stateToString(Video::StreamHealthState::SignalLoss), "SignalLoss");
    EXPECT_EQ(Video::StreamHealthMonitor::stateToString(Video::StreamHealthState::Blackout), "Blackout");
    EXPECT_EQ(Video::StreamHealthMonitor::stateToString(Video::StreamHealthState::Whiteout), "Whiteout");
}
