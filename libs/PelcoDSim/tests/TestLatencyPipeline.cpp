/// @file TestLatencyPipeline.cpp
/// @brief Unit tests for LatencyPipeline: construction, config get/set, enqueue/delivery,
///        flush, stop, worker thread lifecycle, and packet-drop behavior.

#include "LatencyPipeline.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

using namespace PelcoD;

namespace {

void sleepMs(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

TEST(LatencyPipelineTest, DefaultConfigDisabled)
{
    LatencyPipeline pipeline;
    const auto cfg = pipeline.getConfig();
    EXPECT_FALSE(cfg.enabled);
    EXPECT_EQ(cfg.baseLatencyMs, 0U);
    EXPECT_EQ(cfg.jitterMs, 0U);
    EXPECT_DOUBLE_EQ(cfg.packetDropPercent, 0.0);
}

TEST(LatencyPipelineTest, SetAndGetConfig)
{
    LatencyPipeline pipeline;
    LatencyConfig cfg;
    cfg.enabled = true;
    cfg.baseLatencyMs = 50U;
    cfg.jitterMs = 10U;
    cfg.packetDropPercent = 5.0;
    pipeline.setConfig(cfg);

    const auto got = pipeline.getConfig();
    EXPECT_TRUE(got.enabled);
    EXPECT_EQ(got.baseLatencyMs, 50U);
    EXPECT_EQ(got.jitterMs, 10U);
    EXPECT_DOUBLE_EQ(got.packetDropPercent, 5.0);
}

TEST(LatencyPipelineTest, EnqueueDeliveredWithZeroLatency)
{
    LatencyPipeline pipeline;
    LatencyConfig cfg;
    cfg.enabled = true;
    cfg.baseLatencyMs = 0U;
    cfg.jitterMs = 0U;
    cfg.packetDropPercent = 0.0;
    pipeline.setConfig(cfg);

    std::atomic<int> deliveries { 0 };
    const std::vector<std::uint8_t> pkt { 0x01U, 0x02U, 0x03U };

    pipeline.enqueue(pkt, [&](const std::vector<std::uint8_t>& data) {
        EXPECT_EQ(data, pkt);
        ++deliveries;
    });

    // With 0 latency, should deliver quickly
    sleepMs(100);
    EXPECT_EQ(deliveries.load(), 1);
}

TEST(LatencyPipelineTest, EnqueueMultiplePackets)
{
    LatencyPipeline pipeline;
    LatencyConfig cfg;
    cfg.enabled = true;
    cfg.baseLatencyMs = 0U;
    cfg.packetDropPercent = 0.0;
    pipeline.setConfig(cfg);

    std::atomic<int> count { 0 };
    constexpr int N = 5;
    for (int i = 0; i < N; ++i) {
        pipeline.enqueue({ static_cast<std::uint8_t>(i) }, [&](const auto&) { ++count; });
    }

    sleepMs(200);
    EXPECT_EQ(count.load(), N);
}

TEST(LatencyPipelineTest, FlushDiscardsPackets)
{
    LatencyPipeline pipeline;
    LatencyConfig cfg;
    cfg.enabled = true;
    cfg.baseLatencyMs = 300U; // Long latency so packets are still queued when flush() fires
    cfg.packetDropPercent = 0.0;
    pipeline.setConfig(cfg);

    std::atomic<int> delivered { 0 };
    for (int i = 0; i < 5; ++i) {
        pipeline.enqueue({ 0xAAU }, [&](const auto&) { ++delivered; });
    }
    pipeline.flush();
    sleepMs(500);
    // After flush, delivered count should be < 5 (possibly 0)
    EXPECT_LT(delivered.load(), 5);
}

TEST(LatencyPipelineTest, StopStopsWorker)
{
    LatencyPipeline pipeline;
    LatencyConfig cfg;
    cfg.enabled = true;
    cfg.baseLatencyMs = 0U;
    pipeline.setConfig(cfg);

    pipeline.enqueue({ 0x01U }, [](const auto&) {});
    sleepMs(50);
    pipeline.stop();
    EXPECT_FALSE(pipeline.isWorkerActive());
}

TEST(LatencyPipelineTest, WorkerActiveAfterEnqueue)
{
    LatencyPipeline pipeline;
    LatencyConfig cfg;
    cfg.enabled = true;
    cfg.baseLatencyMs = 50U;
    cfg.packetDropPercent = 0.0;
    pipeline.setConfig(cfg);

    pipeline.enqueue({ 0xFFU }, [](const auto&) {});
    sleepMs(10);
    // Worker should be active while packet is pending
    EXPECT_TRUE(pipeline.isWorkerActive());
    sleepMs(200); // Wait for delivery
}

TEST(LatencyPipelineTest, PacketDropReducesDeliveries)
{
    LatencyPipeline pipeline;
    LatencyConfig cfg;
    cfg.enabled = true;
    cfg.baseLatencyMs = 0U;
    cfg.packetDropPercent = 100.0; // Drop all packets
    pipeline.setConfig(cfg);

    std::atomic<int> delivered { 0 };
    for (int i = 0; i < 10; ++i) {
        pipeline.enqueue({ 0x01U }, [&](const auto&) { ++delivered; });
    }
    sleepMs(300);
    // With 100% drop rate, zero packets should be delivered
    EXPECT_EQ(delivered.load(), 0);
}

} // namespace
