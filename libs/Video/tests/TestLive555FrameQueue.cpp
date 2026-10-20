#include "Live555FrameQueue.h"
#include "Live555FrameTypes.h"
#include "RtspServerConfig.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace Video;

TEST(RtspServerConfigTest, DefaultConfigIsValid)
{
    const RtspServerConfig config {};
    EXPECT_TRUE(config.validate());
    EXPECT_EQ(config.port, 8554U);
    EXPECT_EQ(config.streamPath, "live");
    EXPECT_EQ(config.streamType, RtspStreamType::H264ElementaryStream);
    EXPECT_EQ(config.transport, RtspTransportProtocol::All);
}

TEST(RtspServerConfigTest, StreamPathValidation)
{
    RtspServerConfig config {};

    config.streamPath = "";
    EXPECT_FALSE(config.validate());

    config.streamPath = "live stream";
    EXPECT_FALSE(config.validate());

    config.streamPath = "live?token=123";
    EXPECT_FALSE(config.validate());

    config.streamPath = "live#section";
    EXPECT_FALSE(config.validate());

    config.streamPath = "live_stream_01";
    EXPECT_TRUE(config.validate());

    config.streamPath = "stanag4609.ts";
    EXPECT_TRUE(config.validate());
}

TEST(RtspServerConfigTest, ClientAndQueueBoundaries)
{
    RtspServerConfig config {};

    config.maxClients = 0U;
    EXPECT_FALSE(config.validate());

    config.maxClients = 257U;
    EXPECT_FALSE(config.validate());

    config.maxClients = 16U;
    EXPECT_TRUE(config.validate());

    config.queueCapacity = 1U;
    EXPECT_FALSE(config.validate());

    config.queueCapacity = 1001U;
    EXPECT_FALSE(config.validate());

    config.queueCapacity = 60U;
    EXPECT_TRUE(config.validate());
}

TEST(RtspServerConfigTest, CredentialsValidation)
{
    RtspServerConfig config {};

    config.username = "admin";
    config.password = std::nullopt;
    EXPECT_FALSE(config.validate());

    config.username = std::nullopt;
    config.password = "secret";
    EXPECT_FALSE(config.validate());

    config.username = "";
    config.password = "secret";
    EXPECT_FALSE(config.validate());

    config.username = "admin";
    config.password = "secret";
    EXPECT_TRUE(config.validate());
}

TEST(RtspServerConfigTest, MulticastValidation)
{
    RtspServerConfig config {};
    config.enableMulticast = true;

    // Valid Class D multicast
    config.multicastAddress = "239.255.0.1";
    config.multicastPort = 5004U;
    config.multicastTtl = 16U;
    EXPECT_TRUE(config.validate());

    // Port 0 invalid
    config.multicastPort = 0U;
    EXPECT_FALSE(config.validate());
    config.multicastPort = 5004U;

    // TTL 0 invalid
    config.multicastTtl = 0U;
    EXPECT_FALSE(config.validate());
    config.multicastTtl = 16U;

    // Unicast address invalid for multicast
    config.multicastAddress = "192.168.1.100";
    EXPECT_FALSE(config.validate());

    config.multicastAddress = "127.0.0.1";
    EXPECT_FALSE(config.validate());

    // Malformed address
    config.multicastAddress = "239.255.0";
    EXPECT_FALSE(config.validate());

    config.multicastAddress = "239.255.0.300";
    EXPECT_FALSE(config.validate());

    // Upper boundary of Class D (239.255.255.255)
    config.multicastAddress = "239.255.255.255";
    EXPECT_TRUE(config.validate());

    // Lower boundary of Class D (224.0.0.0)
    config.multicastAddress = "224.0.0.1";
    EXPECT_TRUE(config.validate());
}

TEST(Live555FrameQueueTest, PushInputValidation)
{
    Live555FrameQueue queue(10U);
    EXPECT_EQ(queue.capacity(), 10U);

    // Null buffer or size 0 rejected
    EXPECT_FALSE(queue.push(nullptr, 100U, 0.0, false));
    const std::uint8_t dummyByte { 0x47U };
    EXPECT_FALSE(queue.push(&dummyByte, 0U, 0.0, false));

    // Empty shared vector rejected
    auto emptyVec { std::make_shared<std::vector<std::uint8_t>>() };
    EXPECT_FALSE(queue.push(emptyVec, 0.0, false));
    EXPECT_FALSE(queue.push(nullptr, 0.0, false));

    // Valid push
    const std::vector<std::uint8_t> validData { 0x00U, 0x00U, 0x00U, 0x01U, 0x67U };
    EXPECT_TRUE(queue.push(validData.data(), validData.size(), 1.0, true));

    const auto stats { queue.getStats() };
    EXPECT_EQ(stats.totalPushed, 1U);
    EXPECT_EQ(stats.currentDepth, 1U);
    EXPECT_EQ(stats.latestSequence, 1U);
    EXPECT_EQ(stats.oldestSequence, 1U);
}

TEST(Live555FrameQueueTest, SingleConsumerSequentialRead)
{
    Live555FrameQueue queue(20U);
    auto reader { queue.createReader(false) };
    ASSERT_NE(reader, nullptr);

    bool dropped { false };
    EXPECT_FALSE(reader->getNextFrame(dropped).has_value());
    EXPECT_FALSE(dropped);

    // Push 5 sequential frames
    for (std::size_t i { 1U }; i <= 5U; ++i) {
        std::vector<std::uint8_t> nalu { 0x00U, 0x00U, 0x01U, static_cast<std::uint8_t>(i) };
        const double pts { static_cast<double>(i) * 0.04 };
        const bool isKey { i == 1U };
        EXPECT_TRUE(queue.push(nalu.data(), nalu.size(), pts, isKey));
    }

    // Read all 5 frames sequentially
    for (std::size_t i { 1U }; i <= 5U; ++i) {
        auto frameOpt { reader->getNextFrame(dropped) };
        ASSERT_TRUE(frameOpt.has_value());
        EXPECT_FALSE(dropped);
        EXPECT_EQ(frameOpt->sequenceNumber, i);
        EXPECT_DOUBLE_EQ(frameOpt->ptsSeconds, static_cast<double>(i) * 0.04);
        ASSERT_NE(frameOpt->data, nullptr);
        EXPECT_EQ(frameOpt->data->size(), 4U);
        EXPECT_EQ((*frameOpt->data)[3], static_cast<std::uint8_t>(i));
        EXPECT_EQ(frameOpt->isKeyFrame, (i == 1U));
    }

    // 6th read should yield std::nullopt (reader caught up)
    EXPECT_FALSE(reader->getNextFrame(dropped).has_value());
    EXPECT_FALSE(dropped);
}

TEST(Live555FrameQueueTest, MultiReaderFanOut)
{
    Live555FrameQueue queue(50U);
    auto reader1 { queue.createReader(false) };
    auto reader2 { queue.createReader(false) };
    auto reader3 { queue.createReader(false) };

    const auto statsInit { queue.getStats() };
    EXPECT_EQ(statsInit.activeReaders, 3U);

    // Push 30 frames
    for (std::size_t i { 1U }; i <= 30U; ++i) {
        const std::vector<std::uint8_t> payload(64U, static_cast<std::uint8_t>(i));
        EXPECT_TRUE(queue.push(payload.data(), payload.size(), static_cast<double>(i), (i % 10U == 1U)));
    }

    // All three readers independently read all 30 frames
    for (std::size_t i { 1U }; i <= 30U; ++i) {
        bool dropped1 { false };
        bool dropped2 { false };
        bool dropped3 { false };

        auto f1 { reader1->getNextFrame(dropped1) };
        auto f2 { reader2->getNextFrame(dropped2) };
        auto f3 { reader3->getNextFrame(dropped3) };

        ASSERT_TRUE(f1.has_value());
        ASSERT_TRUE(f2.has_value());
        ASSERT_TRUE(f3.has_value());

        EXPECT_FALSE(dropped1);
        EXPECT_FALSE(dropped2);
        EXPECT_FALSE(dropped3);

        EXPECT_EQ(f1->sequenceNumber, i);
        EXPECT_EQ(f2->sequenceNumber, i);
        EXPECT_EQ(f3->sequenceNumber, i);

        // Verify all 3 readers share the exact same underlying buffer pointer (zero-copy fanout)
        EXPECT_EQ(f1->data.get(), f2->data.get());
        EXPECT_EQ(f2->data.get(), f3->data.get());
    }
}

TEST(Live555FrameQueueTest, KeyframeSyncOnAttach)
{
    Live555FrameQueue queue(20U);

    // Push frames 1..4 (P-frames)
    for (std::size_t i { 1U }; i <= 4U; ++i) {
        const std::vector<std::uint8_t> pFrame { 0x41U };
        EXPECT_TRUE(queue.push(pFrame.data(), pFrame.size(), static_cast<double>(i), false));
    }

    // Frame 5 is IDR keyframe
    const std::vector<std::uint8_t> idrFrame { 0x65U };
    EXPECT_TRUE(queue.push(idrFrame.data(), idrFrame.size(), 5.0, true));

    // Frames 6..8 are P-frames
    for (std::size_t i { 6U }; i <= 8U; ++i) {
        const std::vector<std::uint8_t> pFrame { 0x41U };
        EXPECT_TRUE(queue.push(pFrame.data(), pFrame.size(), static_cast<double>(i), false));
    }

    // New client attaches with syncToKeyframe = true
    auto lateReader { queue.createReader(true) };
    bool dropped { false };

    // First read should jump directly to Keyframe #5!
    auto firstFrame { lateReader->getNextFrame(dropped) };
    ASSERT_TRUE(firstFrame.has_value());
    EXPECT_FALSE(dropped);
    EXPECT_EQ(firstFrame->sequenceNumber, 5U);
    EXPECT_TRUE(firstFrame->isKeyFrame);

    // Subsequent reads continue sequentially from #6
    auto secondFrame { lateReader->getNextFrame(dropped) };
    ASSERT_TRUE(secondFrame.has_value());
    EXPECT_EQ(secondFrame->sequenceNumber, 6U);
    EXPECT_FALSE(secondFrame->isKeyFrame);
}

TEST(Live555FrameQueueTest, OverrunDetectionAndKeyframeRecovery)
{
    // Small ring buffer capacity = 5
    Live555FrameQueue queue(5U);
    auto reader { queue.createReader(false) };

    bool dropped { false };

    // Push Frame 1 (Keyframe)
    const std::vector<std::uint8_t> key1 { 0x65U };
    EXPECT_TRUE(queue.push(key1.data(), key1.size(), 1.0, true));

    // Reader consumes Frame 1
    auto f1 { reader->getNextFrame(dropped) };
    ASSERT_TRUE(f1.has_value());
    EXPECT_EQ(f1->sequenceNumber, 1U);

    // Reader now stalls. Producer pushes 10 more frames (frames 2..11).
    // Buffer has capacity 5, so frames 1..6 are overwritten!
    // Set frame 8 as a keyframe.
    for (std::size_t i { 2U }; i <= 11U; ++i) {
        const std::vector<std::uint8_t> data { static_cast<std::uint8_t>(i) };
        const bool isKey { i == 8U };
        EXPECT_TRUE(queue.push(data.data(), data.size(), static_cast<double>(i), isKey));
    }

    // Next read on reader must detect overrun
    auto recoveredFrame { reader->getNextFrame(dropped) };
    ASSERT_TRUE(recoveredFrame.has_value());
    EXPECT_TRUE(dropped);

    // Should have snapped to Keyframe 8 (which is in the valid buffer window 7..11)
    EXPECT_EQ(recoveredFrame->sequenceNumber, 8U);
    EXPECT_TRUE(recoveredFrame->isKeyFrame);

    // Telemetry reflects the drop
    const auto stats { queue.getStats() };
    EXPECT_GT(stats.totalDropped, 0U);
}

TEST(Live555FrameQueueTest, EventTriggerFiring)
{
    Live555FrameQueue queue(10U);
    std::atomic<std::size_t> triggerCount { 0U };

    queue.setEventTrigger([&triggerCount]() { ++triggerCount; });

    const std::vector<std::uint8_t> frame { 0x01U };
    for (std::size_t i { 0U }; i < 7U; ++i) {
        EXPECT_TRUE(queue.push(frame.data(), frame.size(), 0.0, false));
    }

    EXPECT_EQ(triggerCount.load(), 7U);
}

TEST(Live555FrameQueueTest, ConcurrentProducerAndReadersStress)
{
    Live555FrameQueue queue(50U);
    constexpr std::size_t kTotalFrames { 500U };
    std::atomic<bool> producerDone { false };
    std::atomic<std::size_t> totalFramesReadByAll { 0U };

    // 1 Producer Thread
    std::thread producerThread([&]() {
        for (std::size_t i { 1U }; i <= kTotalFrames; ++i) {
            std::vector<std::uint8_t> payload(128U, static_cast<std::uint8_t>(i & 0xFFU));
            static_cast<void>(
                queue.push(payload.data(), payload.size(), static_cast<double>(i) * 0.001, (i % 25U == 0U)));
            std::this_thread::yield();
        }
        producerDone.store(true);
    });

    // 3 Consumer Threads
    auto consumerTask = [&]() {
        auto reader { queue.createReader(false) };
        std::size_t readCount { 0U };
        while (!producerDone.load() || (readCount < kTotalFrames)) {
            bool dropped { false };
            auto frame { reader->getNextFrame(dropped) };
            if (frame.has_value()) {
                ++readCount;
                ++totalFramesReadByAll;
            } else {
                std::this_thread::yield();
            }
        }
    };

    std::thread c1(consumerTask);
    std::thread c2(consumerTask);
    std::thread c3(consumerTask);

    producerThread.join();
    c1.join();
    c2.join();
    c3.join();

    EXPECT_GT(totalFramesReadByAll.load(), 0U);
    const auto stats { queue.getStats() };
    EXPECT_EQ(stats.totalPushed, kTotalFrames);
}
