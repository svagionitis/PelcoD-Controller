/// @file TestRttProfiler.cpp
/// @brief Unit and integration tests for PelcoD::RttProfiler.

#include "MockPelcoDDevice.h"
#include "PelcoDDevice.h"
#include "RttProfiler.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <sstream>
#include <thread>
#include <vector>

namespace {

/// @brief Verify Welford algorithm accuracy for mean, variance, and standard deviation.
TEST(RttProfilerTest, WelfordStatistics)
{
    PelcoD::RttProfiler profiler;
    PelcoD::RttProfilerConfig cfg;
    cfg.mode = PelcoD::ProfilerMode::Passive;
    cfg.historyCapacity = 50U;
    ASSERT_TRUE(profiler.start(cfg));

    // Dataset: 10.0, 20.0, 30.0, 40.0, 50.0
    // Expected Mean: 30.0
    // Expected Sample Variance: 250.0
    // Expected Sample StdDev: sqrt(250) = 15.8113883
    const std::vector<double> samples = { 10.0, 20.0, 30.0, 40.0, 50.0 };
    for (double v : samples) {
        profiler.recordSampleMs(v, "QueryPan", true);
    }

    const auto stats = profiler.getStatistics();
    EXPECT_EQ(stats.totalProbes, 5U);
    EXPECT_EQ(stats.successfulProbes, 5U);
    EXPECT_EQ(stats.timedOutProbes, 0U);
    EXPECT_DOUBLE_EQ(stats.lossPercent, 0.0);
    EXPECT_NEAR(stats.minRttMs, 10.0, 1e-5);
    EXPECT_NEAR(stats.maxRttMs, 50.0, 1e-5);
    EXPECT_NEAR(stats.avgRttMs, 30.0, 1e-5);
    EXPECT_NEAR(stats.stdDevMs, 15.8113883, 1e-4);
    EXPECT_NEAR(stats.currentRttMs, 50.0, 1e-5);
}

/// @brief Verify RFC 3550 inter-arrival packet delay variation filter.
TEST(RttProfilerTest, Rfc3550Jitter)
{
    PelcoD::RttProfiler profiler;
    PelcoD::RttProfilerConfig cfg;
    cfg.mode = PelcoD::ProfilerMode::Passive;
    cfg.historyCapacity = 20U;
    ASSERT_TRUE(profiler.start(cfg));

    // Sample 1: RTT = 10.0 -> J_1 = 0.0
    profiler.recordSampleMs(10.0, "QueryPan", true);
    EXPECT_NEAR(profiler.getStatistics().jitterRfc3550Ms, 0.0, 1e-5);

    // Sample 2: RTT = 26.0 -> D = |26 - 10| = 16.0 -> J_2 = 0 + (16 - 0)/16 = 1.0
    profiler.recordSampleMs(26.0, "QueryPan", true);
    EXPECT_NEAR(profiler.getStatistics().jitterRfc3550Ms, 1.0, 1e-5);

    // Sample 3: RTT = 42.0 -> D = |42 - 26| = 16.0 -> J_3 = 1.0 + (16 - 1.0)/16 = 1.9375
    profiler.recordSampleMs(42.0, "QueryPan", true);
    EXPECT_NEAR(profiler.getStatistics().jitterRfc3550Ms, 1.9375, 1e-5);
}

/// @brief Verify packet loss and timeout metric tracking.
TEST(RttProfilerTest, PacketLossMetrics)
{
    PelcoD::RttProfiler profiler;
    PelcoD::RttProfilerConfig cfg;
    cfg.mode = PelcoD::ProfilerMode::Passive;
    cfg.historyCapacity = 20U;
    ASSERT_TRUE(profiler.start(cfg));

    // 4 successful, 1 timeout -> 20% loss
    profiler.recordSampleMs(15.0, "QueryPan", true);
    profiler.recordSampleMs(16.0, "QueryPan", true);
    profiler.recordSampleMs(14.0, "QueryPan", true);
    profiler.recordSampleMs(15.0, "QueryPan", true);
    profiler.recordSampleMs(0.0, "QueryPan", false);

    const auto stats = profiler.getStatistics();
    EXPECT_EQ(stats.totalProbes, 5U);
    EXPECT_EQ(stats.successfulProbes, 4U);
    EXPECT_EQ(stats.timedOutProbes, 1U);
    EXPECT_NEAR(stats.lossPercent, 20.0, 1e-5);
}

/// @brief Verify circular ring buffer capacity capping and percentile calculations.
TEST(RttProfilerTest, RingBufferAndPercentiles)
{
    PelcoD::RttProfiler profiler;
    PelcoD::RttProfilerConfig cfg;
    cfg.mode = PelcoD::ProfilerMode::Passive;
    cfg.historyCapacity = 5U;
    ASSERT_TRUE(profiler.start(cfg));

    // Feed 10 samples (10ms through 100ms)
    for (int i = 1; i <= 10; ++i) {
        profiler.recordSampleMs(static_cast<double>(i * 10), "QueryPan", true);
    }

    const auto history = profiler.getHistory();
    ASSERT_EQ(history.size(), 5U);
    EXPECT_EQ(history.front().sequenceNumber, 6U);
    EXPECT_NEAR(history.front().rttMs, 60.0, 1e-5);
    EXPECT_EQ(history.back().sequenceNumber, 10U);
    EXPECT_NEAR(history.back().rttMs, 100.0, 1e-5);

    const auto stats = profiler.getStatistics();
    // Rolling buffer has: 60, 70, 80, 90, 100
    // Median (p50) = 80
    EXPECT_NEAR(stats.p50RttMs, 80.0, 1e-5);
}

/// @brief Verify CSV and JSON serialization output streams.
TEST(RttProfilerTest, ExportCsvAndJson)
{
    PelcoD::RttProfiler profiler;
    PelcoD::RttProfilerConfig cfg;
    cfg.mode = PelcoD::ProfilerMode::Passive;
    cfg.historyCapacity = 10U;
    ASSERT_TRUE(profiler.start(cfg));

    profiler.recordSampleMs(25.5, "QueryPan", true);
    profiler.recordSampleMs(0.0, "QueryTilt", false);

    // CSV test
    std::ostringstream csvOss;
    EXPECT_TRUE(profiler.exportCsv(csvOss));
    const std::string csv = csvOss.str();
    EXPECT_NE(csv.find("Sequence,RTT_ms,Status,QueryTag"), std::string::npos);
    EXPECT_NE(csv.find("25.500,OK,QueryPan"), std::string::npos);
    EXPECT_NE(csv.find("TIMEOUT,QueryTilt"), std::string::npos);

    // JSON test
    std::ostringstream jsonOss;
    EXPECT_TRUE(profiler.exportJson(jsonOss));
    const std::string json = jsonOss.str();
    EXPECT_NE(json.find("\"totalProbes\": 2"), std::string::npos);
    EXPECT_NE(json.find("\"successfulProbes\": 1"), std::string::npos);
    EXPECT_NE(json.find("\"timedOutProbes\": 1"), std::string::npos);
    EXPECT_NE(json.find("\"samples\": ["), std::string::npos);
}

/// @brief Verify active burst probing against MockPelcoDDevice with LatencyPipeline.
TEST(RttProfilerTest, ActiveBurstWithMockDevice)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::LatencyConfig latCfg;
    latCfg.enabled = true;
    latCfg.baseLatencyMs = 25U;
    latCfg.jitterMs = 5U;
    latCfg.packetDropPercent = 0.0;
    mock->setLatencyConfig(latCfg);

    auto device = std::make_shared<PelcoD::PelcoDDevice>(mock, 1U);
    ASSERT_TRUE(device->start());

    PelcoD::RttProfiler profiler(device);
    PelcoD::RttProfilerConfig cfg;
    cfg.mode = PelcoD::ProfilerMode::ActiveBurst;
    cfg.burstCount = 4U;
    cfg.intervalMs = 40U;
    cfg.timeoutMs = 500U;
    cfg.probeQueryTag = "QueryPan";

    std::atomic<bool> finished { false };
    profiler.setFinishedCallback([&finished](const PelcoD::RttStatistics&) {
        finished.store(true);
    });

    ASSERT_TRUE(profiler.start(cfg));

    // Wait up to 3 seconds for burst to complete
    const auto startTime = std::chrono::steady_clock::now();
    while (!finished.load() && std::chrono::steady_clock::now() - startTime < std::chrono::seconds(3)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    ASSERT_TRUE(finished.load());
    EXPECT_FALSE(profiler.isRunning());

    const auto stats = profiler.getStatistics();
    EXPECT_GE(stats.totalProbes, 4U);
    EXPECT_GE(stats.successfulProbes, 3U);
    // Latency was simulated at 25ms + jitter, so avg should be >= 15ms
    EXPECT_GE(stats.avgRttMs, 15.0);

    device->stop();
}

} // namespace
