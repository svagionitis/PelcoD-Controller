/// @file TestRttProfiler.cpp
/// @brief Unit and integration tests for PelcoD::RttProfiler.

#include "MockPelcoDDevice.h"
#include "PelcoDDevice.h"
#include "RttProfiler.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

namespace {

/// @brief Verify Welford algorithm accuracy for mean, variance, and standard deviation.
/// @details Feeds a fixed sample vector (10, 20, 30, 40, 50) and checks average, min, max,
///          sample standard deviation, and sample counts.
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
/// @details Records sequence of samples (10, 26, 42) and checks recursive jitter estimates.
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
/// @details Records 4 successful samples and 1 failed timeout probe, validating loss percent is 20%.
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
/// @details Feeds 10 samples into a buffer capped at 5, verifying older entries are dropped
///          and median (p50) reflects the current window.
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
/// @details Ensures exportCsv and exportJson generate expected headers, fields, and values.
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
/// @details Probes mock device configured with simulated latency, checking average RTT reflects simulated delay.
TEST(RttProfilerTest, ActiveBurstWithMockDevice)
{
    constexpr std::uint8_t kAddress { 1U };
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(kAddress);
    PelcoD::LatencyConfig latCfg;
    latCfg.enabled = true;
    latCfg.baseLatencyMs = 25U;
    latCfg.jitterMs = 5U;
    latCfg.packetDropPercent = 0.0;
    mock->setLatencyConfig(latCfg);

    auto device = std::make_shared<PelcoD::PelcoDDevice>(mock, kAddress);
    ASSERT_TRUE(device->start());

    PelcoD::RttProfiler profiler(device);
    PelcoD::RttProfilerConfig cfg;
    cfg.mode = PelcoD::ProfilerMode::ActiveBurst;
    cfg.burstCount = 4U;
    cfg.intervalMs = 40U;
    cfg.timeoutMs = 500U;
    cfg.probeQueryTag = "QueryPan";

    std::atomic<bool> finished { false };
    profiler.setFinishedCallback([&finished](const PelcoD::RttStatistics&) { finished.store(true); });

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

/// @brief Verify statistics calculations on zero and single sample inputs.
/// @details Ensures stddev is 0.0, avg equals the single sample, and no division by zero occurs.
TEST(RttProfilerTest, WelfordZeroAndSingleSample)
{
    PelcoD::RttProfiler profiler;
    PelcoD::RttProfilerConfig cfg;
    cfg.mode = PelcoD::ProfilerMode::Passive;
    ASSERT_TRUE(profiler.start(cfg));

    // Zero samples
    const auto zeroStats = profiler.getStatistics();
    EXPECT_EQ(zeroStats.totalProbes, 0U);
    EXPECT_DOUBLE_EQ(zeroStats.avgRttMs, 0.0);
    EXPECT_DOUBLE_EQ(zeroStats.stdDevMs, 0.0);

    // Single sample
    profiler.recordSampleMs(12.5, "QueryPan", true);
    const auto singleStats = profiler.getStatistics();
    EXPECT_EQ(singleStats.totalProbes, 1U);
    EXPECT_EQ(singleStats.successfulProbes, 1U);
    EXPECT_DOUBLE_EQ(singleStats.minRttMs, 12.5);
    EXPECT_DOUBLE_EQ(singleStats.maxRttMs, 12.5);
    EXPECT_DOUBLE_EQ(singleStats.avgRttMs, 12.5);
    EXPECT_DOUBLE_EQ(singleStats.stdDevMs, 0.0);
}

/// @brief Verify percentile computation when all recorded samples are identical.
/// @details Validates p50, p95, and p99 when 5 identical values of 20ms are recorded.
TEST(RttProfilerTest, PercentileEdgeCases)
{
    PelcoD::RttProfiler profiler;
    PelcoD::RttProfilerConfig cfg;
    cfg.mode = PelcoD::ProfilerMode::Passive;
    cfg.historyCapacity = 10U;
    ASSERT_TRUE(profiler.start(cfg));

    for (int i = 0; i < 5; ++i) {
        profiler.recordSampleMs(20.0, "QueryPan", true);
    }

    const auto stats = profiler.getStatistics();
    EXPECT_NEAR(stats.p50RttMs, 20.0, 1e-5);
    EXPECT_NEAR(stats.p95RttMs, 20.0, 1e-5);
    EXPECT_NEAR(stats.p99RttMs, 20.0, 1e-5);
}

/// @brief Verify reset() clears all statistics, history, and active metrics.
/// @details Populates sample data, invokes reset(), and verifies that all counts and history return to zero.
TEST(RttProfilerTest, ResetProfiler)
{
    PelcoD::RttProfiler profiler;
    PelcoD::RttProfilerConfig cfg;
    cfg.mode = PelcoD::ProfilerMode::Passive;
    ASSERT_TRUE(profiler.start(cfg));

    profiler.recordSampleMs(30.0, "QueryPan", true);
    profiler.recordSampleMs(40.0, "QueryTilt", true);
    EXPECT_EQ(profiler.getStatistics().totalProbes, 2U);
    EXPECT_FALSE(profiler.getHistory().empty());

    profiler.reset();
    EXPECT_EQ(profiler.getStatistics().totalProbes, 0U);
    EXPECT_TRUE(profiler.getHistory().empty());
}

// =============================================================================
// C2 regression tests: worker lifecycle (restart, callbacks, concurrency)
// =============================================================================

/// @brief Poll a predicate until it holds or the timeout expires.
/// @tparam Pred Callable returning bool.
/// @param[in] pred Condition to wait for.
/// @param[in] timeout Maximum time to wait.
/// @return True if the predicate became true before the timeout.
template <typename Pred>
[[nodiscard]] bool waitFor(Pred pred, std::chrono::milliseconds timeout = std::chrono::milliseconds { 3000 })
{
    const auto deadline { std::chrono::steady_clock::now() + timeout };
    bool ok { pred() };
    while (!ok && (std::chrono::steady_clock::now() < deadline)) {
        std::this_thread::sleep_for(std::chrono::milliseconds { 5 });
        ok = pred();
    }
    return ok;
}

/// @brief Create a started PelcoDDevice backed by an in-process MockPelcoDDevice.
/// @return Started device, or nullptr if it failed to start.
[[nodiscard]] std::shared_ptr<PelcoD::PelcoDDevice> makeMockDevice()
{
    constexpr std::uint8_t kAddress { 1U };
    auto mock { std::make_shared<PelcoD::MockPelcoDDevice>(kAddress) };
    auto device { std::make_shared<PelcoD::PelcoDDevice>(mock, kAddress) };
    if (!device->start()) {
        device.reset();
    }
    return device;
}

/// @brief Short ActiveBurst configuration that completes in well under a second.
/// @return Burst configuration with 2 probes, 10 ms interval, and 20 ms settle time.
[[nodiscard]] PelcoD::RttProfilerConfig shortBurst()
{
    PelcoD::RttProfilerConfig cfg {};
    cfg.mode = PelcoD::ProfilerMode::ActiveBurst;
    cfg.burstCount = 2U;
    cfg.intervalMs = 10U;
    cfg.timeoutMs = 20U;
    return cfg;
}

/// @brief Thread-safe recorder of StateChangedCallback transitions.
class StateRecorder {
public:
    /// @brief Append a transition.
    /// @param[in] running New running state.
    void record(bool running)
    {
        std::scoped_lock lock { m_mutex };
        m_states.push_back(running);
    }

    /// @brief Snapshot the recorded transitions.
    /// @return Copy of all transitions in arrival order.
    [[nodiscard]] std::vector<bool> states() const
    {
        std::scoped_lock lock { m_mutex };
        return m_states;
    }

    /// @brief Number of recorded transitions.
    /// @return Transition count.
    [[nodiscard]] std::size_t size() const
    {
        std::scoped_lock lock { m_mutex };
        return m_states.size();
    }

private:
    mutable std::mutex m_mutex {};
    std::vector<bool> m_states {};
};

/// @brief C2a: starting a second burst after the first completed must not terminate.
/// @details The finished worker used to stay joinable and was move-assigned over by start().
TEST(RttProfilerTest, BurstTwiceRestarts)
{
    auto device { makeMockDevice() };
    ASSERT_NE(device, nullptr);

    PelcoD::RttProfiler profiler { device };
    std::atomic<std::uint32_t> finishedCount { 0U };
    profiler.setFinishedCallback([&finishedCount](const PelcoD::RttStatistics&) { finishedCount.fetch_add(1U); });

    ASSERT_TRUE(profiler.start(shortBurst()));
    ASSERT_TRUE(waitFor([&finishedCount] { return finishedCount.load() == 1U; }));
    ASSERT_TRUE(waitFor([&profiler] { return !profiler.isRunning(); }));

    ASSERT_TRUE(profiler.start(shortBurst()));
    ASSERT_TRUE(waitFor([&finishedCount] { return finishedCount.load() == 2U; }));

    profiler.stop();
    EXPECT_FALSE(profiler.isRunning());
    device->stop();
}

/// @brief C2a: many back-to-back burst cycles must all start and complete.
/// @details Restarts immediately after each completion to stress the reap-then-spawn path.
TEST(RttProfilerTest, BurstRestartStress)
{
    auto device { makeMockDevice() };
    ASSERT_NE(device, nullptr);

    PelcoD::RttProfiler profiler { device };
    std::atomic<std::uint32_t> finishedCount { 0U };
    profiler.setFinishedCallback([&finishedCount](const PelcoD::RttStatistics&) { finishedCount.fetch_add(1U); });

    PelcoD::RttProfilerConfig cfg { shortBurst() };
    cfg.burstCount = 1U;
    constexpr std::uint32_t kCycles { 10U };
    for (std::uint32_t i { 0U }; i < kCycles; ++i) {
        ASSERT_TRUE(waitFor([&profiler] { return !profiler.isRunning(); }));
        ASSERT_TRUE(profiler.start(cfg)) << "cycle " << i;
        ASSERT_TRUE(waitFor([&finishedCount, i] { return finishedCount.load() == (i + 1U); })) << "cycle " << i;
    }

    profiler.stop();
    device->stop();
}

/// @brief C2b: start() from the FinishedCallback (worker thread) must be rejected, not terminate.
/// @details After the rejected attempt, a start() from the test thread must succeed.
TEST(RttProfilerTest, StartFromFinishedCbFails)
{
    auto device { makeMockDevice() };
    ASSERT_NE(device, nullptr);

    PelcoD::RttProfiler profiler { device };
    const PelcoD::RttProfilerConfig cfg { shortBurst() };
    std::atomic<std::uint32_t> finishedCount { 0U };
    std::atomic<bool> restartAttempted { false };
    std::atomic<bool> restartResult { true };

    profiler.setFinishedCallback([&](const PelcoD::RttStatistics&) {
        if (!restartAttempted.exchange(true)) {
            restartResult.store(profiler.start(cfg));
        }
        finishedCount.fetch_add(1U);
    });

    ASSERT_TRUE(profiler.start(cfg));
    ASSERT_TRUE(waitFor([&finishedCount] { return finishedCount.load() == 1U; }));
    EXPECT_TRUE(restartAttempted.load());
    EXPECT_FALSE(restartResult.load());

    ASSERT_TRUE(waitFor([&profiler] { return !profiler.isRunning(); }));
    ASSERT_TRUE(profiler.start(cfg));
    ASSERT_TRUE(waitFor([&finishedCount] { return finishedCount.load() == 2U; }));

    profiler.stop();
    device->stop();
}

/// @brief C2c: stop() and setDevice() from the FinishedCallback must not self-join.
/// @details Self-join threw std::system_error on the worker thread, which called std::terminate.
TEST(RttProfilerTest, StopFromFinishedCbIsSafe)
{
    auto device { makeMockDevice() };
    ASSERT_NE(device, nullptr);

    PelcoD::RttProfiler profiler { device };
    std::atomic<std::uint32_t> finishedCount { 0U };
    profiler.setFinishedCallback([&](const PelcoD::RttStatistics&) {
        profiler.stop();
        profiler.setDevice(device);
        finishedCount.fetch_add(1U);
    });

    ASSERT_TRUE(profiler.start(shortBurst()));
    ASSERT_TRUE(waitFor([&finishedCount] { return finishedCount.load() == 1U; }));
    ASSERT_TRUE(waitFor([&profiler] { return !profiler.isRunning(); }));
    EXPECT_EQ(profiler.getDevice(), device);

    ASSERT_TRUE(profiler.start(shortBurst()));
    ASSERT_TRUE(waitFor([&finishedCount] { return finishedCount.load() == 2U; }));

    profiler.stop();
    device->stop();
}

/// @brief StateChangedCallback must report exactly true,false per burst across restarts.
/// @details Guards against duplicate or reordered transitions after the lifecycle rework.
TEST(RttProfilerTest, StateCbOrderAcrossRestarts)
{
    auto device { makeMockDevice() };
    ASSERT_NE(device, nullptr);

    PelcoD::RttProfiler profiler { device };
    StateRecorder recorder {};
    profiler.setStateChangedCallback([&recorder](bool running) { recorder.record(running); });

    ASSERT_TRUE(profiler.start(shortBurst()));
    ASSERT_TRUE(waitFor([&recorder] { return recorder.size() >= 2U; }));
    ASSERT_TRUE(waitFor([&profiler] { return !profiler.isRunning(); }));

    ASSERT_TRUE(profiler.start(shortBurst()));
    ASSERT_TRUE(waitFor([&recorder] { return recorder.size() >= 4U; }));

    profiler.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds { 50 });
    const std::vector<bool> expected { true, false, true, false };
    EXPECT_EQ(recorder.states(), expected);
    device->stop();
}

/// @brief stop() from inside stateCb(true) must prevent the worker from being spawned.
/// @details Previously start() spawned the worker after the callback, producing a second
///          stateCb(false) from a worker that should never have existed.
TEST(RttProfilerTest, StopInStartedCbNoWorker)
{
    auto device { makeMockDevice() };
    ASSERT_NE(device, nullptr);

    PelcoD::RttProfiler profiler { device };
    StateRecorder recorder {};
    std::atomic<std::uint32_t> finishedCount { 0U };
    profiler.setFinishedCallback([&finishedCount](const PelcoD::RttStatistics&) { finishedCount.fetch_add(1U); });
    profiler.setStateChangedCallback([&](bool running) {
        recorder.record(running);
        if (running) {
            profiler.stop();
        }
    });

    ASSERT_TRUE(profiler.start(shortBurst()));
    std::this_thread::sleep_for(std::chrono::milliseconds { 150 });

    EXPECT_FALSE(profiler.isRunning());
    EXPECT_EQ(finishedCount.load(), 0U);
    const std::vector<bool> expected { true, false };
    EXPECT_EQ(recorder.states(), expected);

    profiler.stop();
    device->stop();
}

/// @brief stop() during a long continuous-mode interval must return promptly.
/// @details Guards against a lost wake-up when m_stopRequested is set outside m_mutex.
TEST(RttProfilerTest, StopDuringIntervalIsPrompt)
{
    auto device { makeMockDevice() };
    ASSERT_NE(device, nullptr);

    PelcoD::RttProfiler profiler { device };
    PelcoD::RttProfilerConfig cfg {};
    cfg.mode = PelcoD::ProfilerMode::ActiveContinuous;
    cfg.intervalMs = 5000U;
    cfg.timeoutMs = 20U;

    ASSERT_TRUE(profiler.start(cfg));
    std::this_thread::sleep_for(std::chrono::milliseconds { 50 });

    const auto t0 { std::chrono::steady_clock::now() };
    profiler.stop();
    const auto elapsed { std::chrono::steady_clock::now() - t0 };

    EXPECT_LT(elapsed, std::chrono::milliseconds { 1000 });
    EXPECT_FALSE(profiler.isRunning());
    device->stop();
}

/// @brief Concurrent start()/stop() from two threads must not crash or race on m_worker.
/// @details Primary target for TSan builds; on MSVC it catches double-join and reassign crashes.
TEST(RttProfilerTest, ConcurrentStartStopStress)
{
    auto device { makeMockDevice() };
    ASSERT_NE(device, nullptr);

    PelcoD::RttProfiler profiler { device };
    PelcoD::RttProfilerConfig cfg { shortBurst() };
    cfg.burstCount = 1U;

    constexpr std::uint32_t kIterations { 200U };
    auto hammer = [&profiler, &cfg]() {
        for (std::uint32_t i { 0U }; i < kIterations; ++i) {
            static_cast<void>(profiler.start(cfg));
            if ((i % 3U) == 0U) {
                std::this_thread::sleep_for(std::chrono::milliseconds { 1 });
            }
            profiler.stop();
        }
    };

    std::thread t1 { hammer };
    std::thread t2 { hammer };
    t1.join();
    t2.join();

    profiler.stop();
    EXPECT_FALSE(profiler.isRunning());
    device->stop();
}

/// @brief Mixed Passive / Burst / Continuous cycles must transition cleanly.
/// @details Covers a burst that completes on its own followed by other modes.
TEST(RttProfilerTest, PassiveActiveCycles)
{
    auto device { makeMockDevice() };
    ASSERT_NE(device, nullptr);

    PelcoD::RttProfiler profiler { device };
    std::atomic<std::uint32_t> finishedCount { 0U };
    profiler.setFinishedCallback([&finishedCount](const PelcoD::RttStatistics&) { finishedCount.fetch_add(1U); });

    PelcoD::RttProfilerConfig passive {};
    passive.mode = PelcoD::ProfilerMode::Passive;
    PelcoD::RttProfilerConfig continuous { shortBurst() };
    continuous.mode = PelcoD::ProfilerMode::ActiveContinuous;

    ASSERT_TRUE(profiler.start(passive));
    profiler.stop();

    // Burst completes on its own; the next active start must reap the finished worker.
    ASSERT_TRUE(profiler.start(shortBurst()));
    ASSERT_TRUE(waitFor([&finishedCount] { return finishedCount.load() == 1U; }));
    ASSERT_TRUE(waitFor([&profiler] { return !profiler.isRunning(); }));

    ASSERT_TRUE(profiler.start(continuous));
    std::this_thread::sleep_for(std::chrono::milliseconds { 50 });
    EXPECT_TRUE(profiler.isRunning());
    profiler.stop();
    EXPECT_FALSE(profiler.isRunning());

    ASSERT_TRUE(profiler.start(passive));
    EXPECT_TRUE(profiler.isRunning());
    profiler.stop();
    EXPECT_FALSE(profiler.isRunning());

    device->stop();
}

} // namespace
