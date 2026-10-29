#include "StanagBroadcastEngine.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "KlvTypes.h"
#include "MpegTsKlvMuxer.h"
#include "VideoTypes.h"

using namespace Klv;

TEST(TestStanagBroadcastEngine, LifecycleStartStop)
{
    StanagBroadcastConfig config { };
    config.enableNetworkPublishing = false;
    StanagBroadcastEngine engine(config);

    EXPECT_FALSE(engine.isRunning());

    EXPECT_TRUE(engine.start());
    EXPECT_TRUE(engine.isRunning());

    // Idempotent start
    EXPECT_TRUE(engine.start());
    EXPECT_TRUE(engine.isRunning());

    engine.stop();
    EXPECT_FALSE(engine.isRunning());

    // Idempotent stop
    engine.stop();
    EXPECT_FALSE(engine.isRunning());
}

TEST(TestStanagBroadcastEngine, InProcessCallbackEmission)
{
    StanagBroadcastConfig config { };
    config.enableNetworkPublishing = false;
    StanagBroadcastEngine engine(config);

    std::vector<std::vector<std::uint8_t>> receivedPackets;
    engine.setPacketCallback([&receivedPackets](const std::uint8_t* packet, std::size_t size) {
        ASSERT_EQ(size, MpegTsKlvMuxer::kTsPacketSize);
        receivedPackets.emplace_back(packet, packet + size);
    });

    EXPECT_TRUE(engine.start());

    // Push a video frame
    const std::vector<std::uint8_t> frameData = { 0x00U, 0x00U, 0x00U, 0x01U, 0x65U, 0x88U, 0x84U };
    VideoAccessUnit au { };
    au.codec = VideoCodec::H264;
    au.ptsUs = 1000000ULL;
    au.isKeyframe = true;
    au.data = frameData;

    EXPECT_TRUE(engine.pushVideoAccessUnit(std::move(au)));

    engine.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    EXPECT_FALSE(receivedPackets.empty());
    for (const auto& pkt : receivedPackets) {
        EXPECT_EQ(pkt[0], MpegTsKlvMuxer::kTsSyncByte); // 0x47
    }

    const auto stats = engine.stats();
    EXPECT_GE(stats.videoFramesIngested, 1U);
    EXPECT_GE(stats.tsPacketsEmitted, 1U);

    engine.stop();
}

TEST(TestStanagBroadcastEngine, SynchronizedVideoAndTelemetry)
{
    StanagBroadcastConfig config { };
    config.enableNetworkPublishing = false;
    StanagBroadcastEngine engine(config);

    std::vector<std::size_t> packetSizes;
    engine.setPacketCallback(
        [&packetSizes](const std::uint8_t* /*packet*/, std::size_t size) { packetSizes.push_back(size); });

    EXPECT_TRUE(engine.start());

    constexpr std::uint64_t kBaseTimeUs = 2000000ULL;

    for (std::size_t i = 0U; i < 3U; ++i) {
        const std::uint64_t t = kBaseTimeUs + static_cast<std::uint64_t>(i) * 33333ULL;

        UasDatalinkMessage msg { };
        msg.missionId = "BROADCAST_TEST_" + std::to_string(i);
        msg.precisionTimeStampUs = t;
        EXPECT_TRUE(engine.pushTelemetry(msg));

        VideoAccessUnit au { };
        au.codec = VideoCodec::H264;
        au.ptsUs = t;
        au.isKeyframe = (i == 0U);
        au.data = { 0x00U, 0x00U, 0x00U, 0x01U, 0x41U, 0x9AU };
        EXPECT_TRUE(engine.pushVideoAccessUnit(std::move(au)));
    }

    engine.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    const auto stats = engine.stats();
    EXPECT_GE(stats.videoFramesIngested, 3U);
    EXPECT_GE(stats.telemetryIngested, 3U);
    EXPECT_GE(stats.videoFramesProcessed, 3U);
    EXPECT_GE(stats.telemetryProcessed, 3U);
    EXPECT_FALSE(packetSizes.empty());

    engine.stop();
}

TEST(TestStanagBroadcastEngine, VideoChunkParsing)
{
    StanagBroadcastConfig config { };
    config.enableNetworkPublishing = false;
    StanagBroadcastEngine engine(config);

    EXPECT_TRUE(engine.start());

    // NALU chunk containing H.264 IDR slice with 4-byte start code
    const std::vector<std::uint8_t> chunk = {
        0x00U, 0x00U, 0x00U, 0x01U, 0x67U, 0x42U, 0x00U, 0x1FU, // SPS
        0x00U, 0x00U, 0x00U, 0x01U, 0x68U, 0xCEU, 0x3CU, 0x80U, // PPS
        0x00U, 0x00U, 0x00U, 0x01U, 0x65U, 0x88U, 0x84U // IDR Slice
    };

    EXPECT_TRUE(engine.pushVideoChunk(chunk.data(), chunk.size(), 3000000ULL));

    engine.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    const auto stats = engine.stats();
    EXPECT_GE(stats.videoFramesIngested, 1U);

    engine.stop();
}

TEST(TestStanagBroadcastEngine, QueueBackpressureAndDrop)
{
    StanagBroadcastConfig config { };
    config.enableNetworkPublishing = false;
    config.maxVideoQueueSize = 2U;
    config.maxTelemetryQueueSize = 2U;
    StanagBroadcastEngine engine(config);

    // Engine is NOT started, so queues will not be drained
    VideoAccessUnit au { };
    au.codec = VideoCodec::H264;
    au.ptsUs = 1000ULL;
    au.data = { 0x00U, 0x00U, 0x00U, 0x01U, 0x41U };

    EXPECT_TRUE(engine.pushVideoAccessUnit(au));
    EXPECT_TRUE(engine.pushVideoAccessUnit(au));
    // 3rd push must overflow and drop
    EXPECT_FALSE(engine.pushVideoAccessUnit(au));

    UasDatalinkMessage msg { };
    msg.precisionTimeStampUs = 1000ULL;
    EXPECT_TRUE(engine.pushTelemetry(msg));
    EXPECT_TRUE(engine.pushTelemetry(msg));
    // 3rd push must overflow and drop
    EXPECT_FALSE(engine.pushTelemetry(msg));

    const auto stats = engine.stats();
    EXPECT_EQ(stats.droppedVideoFrames, 1U);
    EXPECT_EQ(stats.droppedTelemetry, 1U);
}

TEST(TestStanagBroadcastEngine, RawKlvIngest)
{
    StanagBroadcastConfig config { };
    config.enableNetworkPublishing = false;
    StanagBroadcastEngine engine(config);

    std::size_t packetCount = 0U;
    engine.setPacketCallback([&packetCount](const std::uint8_t* /*packet*/, std::size_t /*size*/) { packetCount++; });

    EXPECT_TRUE(engine.start());

    // Standard 16-byte MISB ST 0601 Universal Label prefix + 2-byte length + Tag 1 CRC
    const std::vector<std::uint8_t> rawKlv = { 0x06U, 0x0EU, 0x2BU, 0x34U, 0x02U, 0x0BU, 0x01U, 0x01U, 0x0EU, 0x01U,
        0x03U, 0x01U, 0x01U, 0x00U, 0x00U, 0x00U, 0x04U, 0x01U, 0x02U, 0x12U, 0x34U };

    EXPECT_TRUE(engine.pushRawKlv(rawKlv.data(), rawKlv.size(), 4000000ULL));

    VideoAccessUnit au { };
    au.codec = VideoCodec::H264;
    au.ptsUs = 4000000ULL;
    au.data = { 0x00U, 0x00U, 0x00U, 0x01U, 0x41U };
    EXPECT_TRUE(engine.pushVideoAccessUnit(std::move(au)));

    engine.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    const auto stats = engine.stats();
    EXPECT_GE(stats.telemetryIngested, 1U);
    EXPECT_GE(packetCount, 1U);

    engine.stop();
}

TEST(TestStanagBroadcastEngine, NetworkPublisherLoopback)
{
    StanagBroadcastConfig config { };
    config.enableNetworkPublishing = true;
    config.networkConfig.destinationIp = "127.0.0.1";
    config.networkConfig.destinationPort = 59876U;
    config.networkConfig.protocol = Transport::TsNetworkProtocol::RawUdp;

    StanagBroadcastEngine engine(config);

    EXPECT_TRUE(engine.start());
    EXPECT_TRUE(engine.stats().isNetworkOpen);

    VideoAccessUnit au { };
    au.codec = VideoCodec::H264;
    au.ptsUs = 5000000ULL;
    au.data = { 0x00U, 0x00U, 0x00U, 0x01U, 0x65U, 0x01U };
    EXPECT_TRUE(engine.pushVideoAccessUnit(std::move(au)));

    engine.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    EXPECT_GT(engine.stats().tsPacketsEmitted, 0U);

    engine.stop();
    EXPECT_FALSE(engine.stats().isNetworkOpen);
}

TEST(TestStanagBroadcastEngine, CbrPacingIntegration)
{
    StanagBroadcastConfig config { };
    config.enableNetworkPublishing = false;
    config.enableCbrPacing = true;
    config.cbrConfig.targetBitrateBps = 2000000U; // 2.0 Mbps
    config.cbrConfig.autoStartWorker = true;

    StanagBroadcastEngine engine(config);

    std::atomic<std::size_t> packetCount { 0U };
    engine.setPacketCallback(
        [&packetCount](const std::uint8_t*, std::size_t) { packetCount.fetch_add(1U, std::memory_order_relaxed); });

    EXPECT_TRUE(engine.start());
    EXPECT_TRUE(engine.isRunning());

    // Push 1 video frame
    VideoAccessUnit au { };
    au.codec = VideoCodec::H264;
    au.ptsUs = 1000000ULL;
    au.data = { 0x00U, 0x00U, 0x00U, 0x01U, 0x65U, 0x01U, 0x02U };
    EXPECT_TRUE(engine.pushVideoAccessUnit(std::move(au)));

    // Let CBR pacer run for 50 ms
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const auto stats = engine.stats();
    EXPECT_TRUE(stats.isCbrActive);
    EXPECT_GT(stats.tsPacketsEmitted, 0U);
    EXPECT_GT(stats.nullPacketsInjected, 0U);
    EXPECT_GT(packetCount.load(std::memory_order_relaxed), 0U);

    engine.stop();
    EXPECT_FALSE(engine.isRunning());
}
