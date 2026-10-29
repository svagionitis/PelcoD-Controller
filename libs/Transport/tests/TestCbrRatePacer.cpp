#include "CbrRatePacer.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace Transport;

namespace {

std::vector<std::uint8_t> makeSampleTsPacket(
    std::uint16_t pid, std::uint8_t cc, bool withPcr = false, std::uint64_t pcrUs = 0U)
{
    std::vector<std::uint8_t> packet(CbrRatePacer::kTsPacketSize, 0x00U);
    packet[0] = 0x47U;
    packet[1] = static_cast<std::uint8_t>((pid >> 8U) & 0x1FU);
    packet[2] = static_cast<std::uint8_t>(pid & 0xFFU);

    if (withPcr) {
        packet[3] = static_cast<std::uint8_t>(0x30U | (cc & 0x0FU)); // AFC = 11 (AF + payload)
        packet[4] = 0x07U; // AF length: 1 flag byte + 6 PCR bytes
        packet[5] = 0x10U; // PCR flag set

        const std::uint64_t pcrBase = ((pcrUs * 90ULL) / 1000ULL) & 0x1FFFFFFFFULL;
        const auto pcrExt = static_cast<std::uint16_t>(((pcrUs * 27ULL) % 300ULL) & 0x1FFULL);

        packet[6] = static_cast<std::uint8_t>((pcrBase >> 25U) & 0xFFU);
        packet[7] = static_cast<std::uint8_t>((pcrBase >> 17U) & 0xFFU);
        packet[8] = static_cast<std::uint8_t>((pcrBase >> 9U) & 0xFFU);
        packet[9] = static_cast<std::uint8_t>((pcrBase >> 1U) & 0xFFU);
        packet[10] = static_cast<std::uint8_t>(((pcrBase & 1U) << 7U) | 0x7EU | ((pcrExt >> 8U) & 1U));
        packet[11] = static_cast<std::uint8_t>(pcrExt & 0xFFU);
    } else {
        packet[3] = static_cast<std::uint8_t>(0x10U | (cc & 0x0FU)); // AFC = 01 (payload only)
    }

    // Fill remaining bytes with test payload pattern
    for (std::size_t i = (withPcr ? 12U : 4U); i < CbrRatePacer::kTsPacketSize; ++i) {
        packet[i] = static_cast<std::uint8_t>((i + cc) & 0xFFU);
    }

    return packet;
}

} // namespace

TEST(TestCbrRatePacer, NullPacketFormat)
{
    const auto& nullPkt = CbrRatePacer::nullPacket();
    ASSERT_EQ(nullPkt.size(), CbrRatePacer::kTsPacketSize);

    EXPECT_EQ(nullPkt[0], 0x47U); // Sync byte
    EXPECT_EQ(nullPkt[1], 0x1FU); // PID high 5 bits = 0x1F
    EXPECT_EQ(nullPkt[2], 0xFFU); // PID low 8 bits = 0xFF (PID = 0x1FFF)
    EXPECT_EQ(nullPkt[3], 0x10U); // AFC = 01 (payload only), CC = 0

    for (std::size_t i = 4U; i < CbrRatePacer::kTsPacketSize; ++i) {
        EXPECT_EQ(nullPkt[i], 0xFFU) << "Byte mismatch at index " << i;
    }

    EXPECT_FALSE(CbrRatePacer::hasPcr(nullPkt.data()));
}

TEST(TestCbrRatePacer, NullPacketInjectionOnStarvation)
{
    CbrPacerConfig config { };
    config.targetBitrateBps = 4000000U; // 4.0 Mbps
    config.autoStartWorker = false;

    std::vector<std::vector<std::uint8_t>> emitted;
    CbrRatePacer pacer(config, [&emitted](const std::uint8_t* data, std::size_t size) {
        ASSERT_EQ(size, CbrRatePacer::kTsPacketSize);
        emitted.emplace_back(data, data + size);
    });

    // Step 100 milliseconds
    const auto emittedCount = pacer.step(std::chrono::milliseconds(100));

    // 4 Mbps = 4,000,000 bits/sec = 500,000 bytes/sec
    // In 100ms: 50,000 bytes. 50,000 / 188 ≈ 265.95 packets -> 265 or 266 packets
    EXPECT_GE(emittedCount, 265U);
    EXPECT_LE(emittedCount, 267U);
    EXPECT_EQ(emitted.size(), emittedCount);

    // All emitted packets must be null packets
    for (const auto& pkt : emitted) {
        EXPECT_EQ(pkt[0], 0x47U);
        EXPECT_EQ(pkt[1], 0x1FU);
        EXPECT_EQ(pkt[2], 0xFFU);
    }

    EXPECT_EQ(pacer.nullPacketsInjected(), emittedCount);
    EXPECT_EQ(pacer.stats().payloadPacketsOut, 0U);
}

TEST(TestCbrRatePacer, BurstAbsorptionAndDrain)
{
    CbrPacerConfig config { };
    config.targetBitrateBps = 4000000U; // 4.0 Mbps (~265 packets per 100ms)
    config.autoStartWorker = false;

    std::vector<std::vector<std::uint8_t>> emitted;
    CbrRatePacer pacer(
        config, [&emitted](const std::uint8_t* data, std::size_t size) { emitted.emplace_back(data, data + size); });

    // Submit 50 payload packets
    std::vector<std::uint8_t> payloadBurst;
    for (std::uint8_t i = 0U; i < 50U; ++i) {
        const auto pkt = makeSampleTsPacket(0x0101U, i);
        payloadBurst.insert(payloadBurst.end(), pkt.begin(), pkt.end());
    }

    const std::size_t queued = pacer.submitTsPackets(payloadBurst.data(), 50U);
    EXPECT_EQ(queued, 50U);
    EXPECT_EQ(pacer.stats().currentQueueSize, 50U);

    // Step 100 milliseconds (~265 packets slot)
    const auto count = pacer.step(std::chrono::milliseconds(100));
    EXPECT_GE(count, 265U);
    EXPECT_LE(count, 267U);

    // The first 50 packets must be the submitted payload packets in FIFO order
    for (std::size_t i = 0U; i < 50U; ++i) {
        EXPECT_EQ(emitted[i][1], 0x01U);
        EXPECT_EQ(emitted[i][2], 0x01U);
        EXPECT_EQ(emitted[i][3] & 0x0FU, static_cast<std::uint8_t>(i & 0x0FU));
    }

    // The remaining packets must be null packets
    for (std::size_t i = 50U; i < count; ++i) {
        EXPECT_EQ(emitted[i][1], 0x1FU);
        EXPECT_EQ(emitted[i][2], 0xFFU);
    }

    EXPECT_EQ(pacer.stats().payloadPacketsOut, 50U);
    EXPECT_EQ(pacer.nullPacketsInjected(), count - 50U);
    EXPECT_EQ(pacer.stats().currentQueueSize, 0U);
}

TEST(TestCbrRatePacer, PcrInspectionAndRestamping)
{
    CbrPacerConfig config { };
    config.targetBitrateBps = 4000000U;
    config.autoStartWorker = false;
    config.enablePcrRestamping = true;

    std::vector<std::vector<std::uint8_t>> emitted;
    CbrRatePacer pacer(
        config, [&emitted](const std::uint8_t* data, std::size_t size) { emitted.emplace_back(data, data + size); });

    // Step 10 ms first (~26 packets)
    static_cast<void>(pacer.step(std::chrono::milliseconds(10)));
    emitted.clear();

    const auto pcrPkt = makeSampleTsPacket(0x0101U, 0U, true, 1000000ULL);
    EXPECT_TRUE(CbrRatePacer::hasPcr(pcrPkt.data()));

    EXPECT_EQ(pacer.submitTsPackets(pcrPkt.data(), 1U), 1U);

    // Step 1 ms (~2-3 packets)
    const auto pcrStepCount = pacer.step(std::chrono::milliseconds(1));
    EXPECT_GT(pcrStepCount, 0U);

    ASSERT_FALSE(emitted.empty());
    // Packet 0 is our payload packet with PCR
    EXPECT_TRUE(CbrRatePacer::hasPcr(emitted[0].data()));

    // Verify PCR base and ext were preserved or restamped validly
    const std::uint64_t base = (static_cast<std::uint64_t>(emitted[0][6]) << 25U)
        | (static_cast<std::uint64_t>(emitted[0][7]) << 17U) | (static_cast<std::uint64_t>(emitted[0][8]) << 9U)
        | (static_cast<std::uint64_t>(emitted[0][9]) << 1U) | (static_cast<std::uint64_t>(emitted[0][10] >> 7U) & 1U);
    const auto ext = static_cast<std::uint16_t>(((emitted[0][10] & 1U) << 8U) | emitted[0][11]);

    EXPECT_LE(ext, 299U);
    EXPECT_GT(base + ext, 0ULL);
}

TEST(TestCbrRatePacer, DeterministicStepPacing)
{
    CbrPacerConfig config { };
    config.targetBitrateBps = 2000000U; // 2.0 Mbps
    config.autoStartWorker = false;

    std::size_t totalPackets = 0U;
    CbrRatePacer pacer(config, [&totalPackets](const std::uint8_t* /*data*/, std::size_t size) {
        if (size == CbrRatePacer::kTsPacketSize) {
            ++totalPackets;
        }
    });

    // 100 steps of 10 ms each = 1.0 second total
    for (int i = 0; i < 100; ++i) {
        const auto pcrStepCount = pacer.step(std::chrono::milliseconds(10));
        EXPECT_GT(pcrStepCount, 0U);
    }

    // 2.0 Mbps = 250,000 bytes/sec -> 250,000 / 188 ≈ 1329.78 packets -> 1329 or 1330 packets
    EXPECT_GE(totalPackets, 1328U);
    EXPECT_LE(totalPackets, 1331U);
    EXPECT_EQ(pacer.nullPacketsInjected(), totalPackets);
}

TEST(TestCbrRatePacer, DynamicReconfiguration)
{
    CbrPacerConfig config { };
    config.targetBitrateBps = 2000000U; // 2 Mbps
    config.autoStartWorker = false;

    std::size_t packetCount = 0U;
    CbrRatePacer pacer(config, [&packetCount](const std::uint8_t*, std::size_t) { ++packetCount; });

    static_cast<void>(pacer.step(std::chrono::milliseconds(100))); // ~132 packets
    const auto firstPass = packetCount;
    EXPECT_GE(firstPass, 132U);
    EXPECT_LE(firstPass, 134U);

    // Double bitrate to 4 Mbps
    config.targetBitrateBps = 4000000U;
    EXPECT_TRUE(pacer.setConfig(config));

    packetCount = 0U;
    static_cast<void>(pacer.step(std::chrono::milliseconds(100))); // ~265 packets
    EXPECT_GE(packetCount, 264U);
    EXPECT_LE(packetCount, 267U);
}

TEST(TestCbrRatePacer, QueueOverflowHandling)
{
    CbrPacerConfig config { };
    config.maxQueuePackets = 10U;
    config.autoStartWorker = false;

    CbrRatePacer pacer(config, [](const std::uint8_t*, std::size_t) { });

    std::vector<std::uint8_t> packets;
    for (std::uint8_t i = 0U; i < 25U; ++i) {
        const auto p = makeSampleTsPacket(0x0101U, i);
        packets.insert(packets.end(), p.begin(), p.end());
    }

    const std::size_t queued = pacer.submitTsPackets(packets.data(), 25U);
    EXPECT_EQ(queued, 10U);
    EXPECT_EQ(pacer.stats().droppedPackets, 15U);
    EXPECT_EQ(pacer.stats().currentQueueSize, 10U);
}

TEST(TestCbrRatePacer, FlushAndReset)
{
    CbrPacerConfig config { };
    config.autoStartWorker = false;

    std::size_t emittedCount = 0U;
    CbrRatePacer pacer(config, [&emittedCount](const std::uint8_t*, std::size_t) { ++emittedCount; });

    std::vector<std::uint8_t> packets;
    for (std::uint8_t i = 0U; i < 5U; ++i) {
        const auto p = makeSampleTsPacket(0x0101U, i);
        packets.insert(packets.end(), p.begin(), p.end());
    }

    EXPECT_EQ(pacer.submitTsPackets(packets.data(), 5U), 5U);
    EXPECT_EQ(pacer.stats().currentQueueSize, 5U);

    pacer.flush();
    EXPECT_EQ(emittedCount, 5U);
    EXPECT_EQ(pacer.stats().currentQueueSize, 0U);

    pacer.reset();
    EXPECT_EQ(pacer.stats().payloadPacketsIn, 0U);
    EXPECT_EQ(pacer.stats().payloadPacketsOut, 0U);
    EXPECT_EQ(pacer.nullPacketsInjected(), 0U);
}

TEST(TestCbrRatePacer, RealTimeWorkerLifecycle)
{
    CbrPacerConfig config { };
    config.targetBitrateBps = 2000000U; // 2.0 Mbps
    config.autoStartWorker = false;

    std::atomic<std::size_t> packetCount { 0U };
    CbrRatePacer pacer(config,
        [&packetCount](const std::uint8_t*, std::size_t) { packetCount.fetch_add(1U, std::memory_order_relaxed); });

    EXPECT_FALSE(pacer.isRunning());
    EXPECT_TRUE(pacer.startWorker());
    EXPECT_TRUE(pacer.isRunning());

    // Run live for 50 ms (~66 packets)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    pacer.stopWorker();
    EXPECT_FALSE(pacer.isRunning());

    const auto count = packetCount.load(std::memory_order_relaxed);
    // Allow wide margin for Windows scheduler timer quantum
    EXPECT_GE(count, 30U);
    EXPECT_LE(count, 120U);
}
