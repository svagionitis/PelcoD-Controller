/// @file TestRttProfiler.cpp
/// @brief Unit and integration tests for PelcoD::RttProfiler.

#include "MockPelcoDDevice.h"
#include "PelcoDDevice.h"
#include "RttProfiler.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>
#include <vector>

/// @brief Verify Welford algorithm accuracy for mean, variance, and standard deviation.
static void testWelfordStatistics()
{
    PelcoD::RttProfiler profiler;
    PelcoD::RttProfilerConfig cfg;
    cfg.mode = PelcoD::ProfilerMode::Passive;
    cfg.historyCapacity = 50U;
    assert(profiler.start(cfg));

    // Dataset: 10.0, 20.0, 30.0, 40.0, 50.0
    // Expected Mean: 30.0
    // Expected Sample Variance: 250.0
    // Expected Sample StdDev: sqrt(250) = 15.8113883
    const std::vector<double> samples = { 10.0, 20.0, 30.0, 40.0, 50.0 };
    for (double v : samples) {
        profiler.recordSampleMs(v, "QueryPan", true);
    }

    const auto stats = profiler.getStatistics();
    assert(stats.totalProbes == 5U);
    assert(stats.successfulProbes == 5U);
    assert(stats.timedOutProbes == 0U);
    assert(stats.lossPercent == 0.0);
    assert(std::abs(stats.minRttMs - 10.0) < 1e-5);
    assert(std::abs(stats.maxRttMs - 50.0) < 1e-5);
    assert(std::abs(stats.avgRttMs - 30.0) < 1e-5);
    assert(std::abs(stats.stdDevMs - 15.8113883) < 1e-4);
    assert(std::abs(stats.currentRttMs - 50.0) < 1e-5);

    std::cout << "  testWelfordStatistics: PASSED\n";
}

/// @brief Verify RFC 3550 inter-arrival packet delay variation filter.
static void testRfc3550Jitter()
{
    PelcoD::RttProfiler profiler;
    PelcoD::RttProfilerConfig cfg;
    cfg.mode = PelcoD::ProfilerMode::Passive;
    cfg.historyCapacity = 20U;
    assert(profiler.start(cfg));

    // Sample 1: RTT = 10.0 -> J_1 = 0.0
    profiler.recordSampleMs(10.0, "QueryPan", true);
    assert(std::abs(profiler.getStatistics().jitterRfc3550Ms - 0.0) < 1e-5);

    // Sample 2: RTT = 26.0 -> D = |26 - 10| = 16.0 -> J_2 = 0 + (16 - 0)/16 = 1.0
    profiler.recordSampleMs(26.0, "QueryPan", true);
    assert(std::abs(profiler.getStatistics().jitterRfc3550Ms - 1.0) < 1e-5);

    // Sample 3: RTT = 42.0 -> D = |42 - 26| = 16.0 -> J_3 = 1.0 + (16 - 1.0)/16 = 1.9375
    profiler.recordSampleMs(42.0, "QueryPan", true);
    assert(std::abs(profiler.getStatistics().jitterRfc3550Ms - 1.9375) < 1e-5);

    std::cout << "  testRfc3550Jitter: PASSED\n";
}

/// @brief Verify packet loss and timeout metric tracking.
static void testPacketLossMetrics()
{
    PelcoD::RttProfiler profiler;
    PelcoD::RttProfilerConfig cfg;
    cfg.mode = PelcoD::ProfilerMode::Passive;
    cfg.historyCapacity = 20U;
    assert(profiler.start(cfg));

    // 4 successful, 1 timeout -> 20% loss
    profiler.recordSampleMs(15.0, "QueryPan", true);
    profiler.recordSampleMs(16.0, "QueryPan", true);
    profiler.recordSampleMs(14.0, "QueryPan", true);
    profiler.recordSampleMs(15.0, "QueryPan", true);
    profiler.recordSampleMs(0.0, "QueryPan", false);

    const auto stats = profiler.getStatistics();
    assert(stats.totalProbes == 5U);
    assert(stats.successfulProbes == 4U);
    assert(stats.timedOutProbes == 1U);
    assert(std::abs(stats.lossPercent - 20.0) < 1e-5);

    std::cout << "  testPacketLossMetrics: PASSED\n";
}

/// @brief Verify circular ring buffer capacity capping and percentile calculations.
static void testRingBufferAndPercentiles()
{
    PelcoD::RttProfiler profiler;
    PelcoD::RttProfilerConfig cfg;
    cfg.mode = PelcoD::ProfilerMode::Passive;
    cfg.historyCapacity = 5U;
    assert(profiler.start(cfg));

    // Feed 10 samples (10ms through 100ms)
    for (int i = 1; i <= 10; ++i) {
        profiler.recordSampleMs(static_cast<double>(i * 10), "QueryPan", true);
    }

    const auto history = profiler.getHistory();
    assert(history.size() == 5U);
    assert(history.front().sequenceNumber == 6U);
    assert(std::abs(history.front().rttMs - 60.0) < 1e-5);
    assert(history.back().sequenceNumber == 10U);
    assert(std::abs(history.back().rttMs - 100.0) < 1e-5);

    const auto stats = profiler.getStatistics();
    // Rolling buffer has: 60, 70, 80, 90, 100
    // Median (p50) = 80
    assert(std::abs(stats.p50RttMs - 80.0) < 1e-5);

    std::cout << "  testRingBufferAndPercentiles: PASSED\n";
}

/// @brief Verify CSV and JSON serialization output streams.
static void testExportCsvAndJson()
{
    PelcoD::RttProfiler profiler;
    PelcoD::RttProfilerConfig cfg;
    cfg.mode = PelcoD::ProfilerMode::Passive;
    cfg.historyCapacity = 10U;
    assert(profiler.start(cfg));

    profiler.recordSampleMs(25.5, "QueryPan", true);
    profiler.recordSampleMs(0.0, "QueryTilt", false);

    // CSV test
    std::ostringstream csvOss;
    assert(profiler.exportCsv(csvOss));
    const std::string csv = csvOss.str();
    assert(csv.find("Sequence,RTT_ms,Status,QueryTag") != std::string::npos);
    assert(csv.find("25.500,OK,QueryPan") != std::string::npos);
    assert(csv.find("TIMEOUT,QueryTilt") != std::string::npos);

    // JSON test
    std::ostringstream jsonOss;
    assert(profiler.exportJson(jsonOss));
    const std::string json = jsonOss.str();
    assert(json.find("\"totalProbes\": 2") != std::string::npos);
    assert(json.find("\"successfulProbes\": 1") != std::string::npos);
    assert(json.find("\"timedOutProbes\": 1") != std::string::npos);
    assert(json.find("\"samples\": [") != std::string::npos);

    std::cout << "  testExportCsvAndJson: PASSED\n";
}

/// @brief Verify active burst probing against MockPelcoDDevice with LatencyPipeline.
static void testActiveBurstWithMockDevice()
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::LatencyConfig latCfg;
    latCfg.enabled = true;
    latCfg.baseLatencyMs = 25U;
    latCfg.jitterMs = 5U;
    latCfg.packetDropPercent = 0.0;
    mock->setLatencyConfig(latCfg);

    auto device = std::make_shared<PelcoD::PelcoDDevice>(mock, 1U);
    assert(device->start());

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

    assert(profiler.start(cfg));

    // Wait up to 3 seconds for burst to complete
    const auto startTime = std::chrono::steady_clock::now();
    while (!finished.load() && std::chrono::steady_clock::now() - startTime < std::chrono::seconds(3)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    assert(finished.load());
    assert(!profiler.isRunning());

    const auto stats = profiler.getStatistics();
    assert(stats.totalProbes >= 4U);
    assert(stats.successfulProbes >= 3U);
    // Latency was simulated at 25ms + jitter, so avg should be >= 20ms
    assert(stats.avgRttMs >= 15.0);

    device->stop();
    std::cout << "  testActiveBurstWithMockDevice: PASSED\n";
}

int main()
{
    std::cout << "Running TestRttProfiler suite...\n";

    testWelfordStatistics();
    testRfc3550Jitter();
    testPacketLossMetrics();
    testRingBufferAndPercentiles();
    testExportCsvAndJson();
    testActiveBurstWithMockDevice();

    std::cout << "All TestRttProfiler tests passed successfully!\n";
    return 0;
}
