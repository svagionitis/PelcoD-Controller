/// @file TestLatencyPipeline.cpp
/// @brief Unit tests for LatencyPipeline: construction, config get/set, enqueue/delivery,
///        flush, stop, worker thread lifecycle, and packet-drop behavior.

#include "LatencyPipeline.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

using namespace PelcoD;

namespace {

void sleepMs(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// ---------------------------------------------------------------------------

void testDefaultConfigDisabled()
{
    std::cout << "[Test] testDefaultConfigDisabled\n";
    LatencyPipeline pipeline;
    const auto cfg = pipeline.getConfig();
    assert(!cfg.enabled);
    assert(cfg.baseLatencyMs == 0U);
    assert(cfg.jitterMs == 0U);
    assert(cfg.packetDropPercent == 0.0);
    std::cout << "  -> PASSED\n";
}

void testSetAndGetConfig()
{
    std::cout << "[Test] testSetAndGetConfig\n";
    LatencyPipeline pipeline;
    LatencyConfig cfg;
    cfg.enabled = true;
    cfg.baseLatencyMs = 50U;
    cfg.jitterMs = 10U;
    cfg.packetDropPercent = 5.0;
    pipeline.setConfig(cfg);

    const auto got = pipeline.getConfig();
    assert(got.enabled);
    assert(got.baseLatencyMs == 50U);
    assert(got.jitterMs == 10U);
    assert(got.packetDropPercent == 5.0);
    std::cout << "  -> PASSED\n";
}

void testEnqueueDeliveredWithZeroLatency()
{
    std::cout << "[Test] testEnqueueDeliveredWithZeroLatency\n";
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
        assert(data == pkt);
        ++deliveries;
    });

    // With 0 latency, should deliver quickly
    sleepMs(100);
    assert(deliveries.load() == 1);
    std::cout << "  -> PASSED\n";
}

void testEnqueueMultiplePackets()
{
    std::cout << "[Test] testEnqueueMultiplePackets\n";
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
    assert(count.load() == N);
    std::cout << "  -> PASSED\n";
}

void testFlushDiscardsPackets()
{
    std::cout << "[Test] testFlushDiscardsPackets\n";
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
    assert(delivered.load() < 5);
    std::cout << "  delivered after flush=" << delivered.load() << "\n";
    std::cout << "  -> PASSED\n";
}

void testStopStopsWorker()
{
    std::cout << "[Test] testStopStopsWorker\n";
    LatencyPipeline pipeline;
    LatencyConfig cfg;
    cfg.enabled = true;
    cfg.baseLatencyMs = 0U;
    pipeline.setConfig(cfg);

    pipeline.enqueue({ 0x01U }, [](const auto&) {});
    sleepMs(50);
    pipeline.stop();
    assert(!pipeline.isWorkerActive());
    std::cout << "  -> PASSED\n";
}

void testWorkerActiveAfterEnqueue()
{
    std::cout << "[Test] testWorkerActiveAfterEnqueue\n";
    LatencyPipeline pipeline;
    LatencyConfig cfg;
    cfg.enabled = true;
    cfg.baseLatencyMs = 50U;
    cfg.packetDropPercent = 0.0;
    pipeline.setConfig(cfg);

    pipeline.enqueue({ 0xFFU }, [](const auto&) {});
    sleepMs(10);
    // Worker should be active while packet is pending
    assert(pipeline.isWorkerActive());
    sleepMs(200); // Wait for delivery
    std::cout << "  -> PASSED\n";
}

void testPacketDropReducesDeliveries()
{
    std::cout << "[Test] testPacketDropReducesDeliveries\n";
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
    assert(delivered.load() == 0);
    std::cout << "  delivered=" << delivered.load() << " (expected 0)\n";
    std::cout << "  -> PASSED\n";
}

} // namespace

int main()
{
    std::cout << "Running TestLatencyPipeline Test Suite\n";
    testDefaultConfigDisabled();
    testSetAndGetConfig();
    testEnqueueDeliveredWithZeroLatency();
    testEnqueueMultiplePackets();
    testFlushDiscardsPackets();
    testStopStopsWorker();
    testWorkerActiveAfterEnqueue();
    testPacketDropReducesDeliveries();
    std::cout << "All TestLatencyPipeline Tests Passed!\n";
    return 0;
}
