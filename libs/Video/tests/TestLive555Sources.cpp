#include "Live555FrameQueue.h"
#include "Live555FrameTypes.h"
#include "Live555H264LiveSource.h"
#include "Live555Mp2tLiveSource.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include <BasicUsageEnvironment.hh>
#include <UsageEnvironment.hh>
#include <gtest/gtest.h>
#include <liveMedia.hh>

namespace Video {
namespace {

    struct TestDeliveryContext {
        std::atomic<bool> frameDelivered { false };
        std::atomic<bool> sourceClosed { false };
        char watchVariable { 0 };
        unsigned frameSize { 0U };
        unsigned numTruncatedBytes { 0U };
        struct timeval presentationTime {
            0, 0
        };
        unsigned durationInMicroseconds { 0U };
        std::vector<std::uint8_t> buffer {};
    };

    void onFrameDelivered(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
        struct timeval presentationTime, unsigned durationInMicroseconds)
    {
        auto* ctx { static_cast<TestDeliveryContext*>(clientData) };
        if (ctx != nullptr) {
            ctx->frameSize = frameSize;
            ctx->numTruncatedBytes = numTruncatedBytes;
            ctx->presentationTime = presentationTime;
            ctx->durationInMicroseconds = durationInMicroseconds;
            ctx->frameDelivered.store(true);
            ctx->watchVariable = 1;
        }
    }

    void onSourceClosed(void* clientData)
    {
        auto* ctx { static_cast<TestDeliveryContext*>(clientData) };
        if (ctx != nullptr) {
            ctx->sourceClosed.store(true);
            ctx->watchVariable = 1;
        }
    }

    class Live555SourcesTest : public ::testing::Test {
    protected:
        void SetUp() override
        {
            m_scheduler = BasicTaskScheduler::createNew();
            ASSERT_NE(m_scheduler, nullptr);
            m_env = BasicUsageEnvironment::createNew(*m_scheduler);
            ASSERT_NE(m_env, nullptr);
        }

        void TearDown() override
        {
            if (m_env != nullptr) {
                m_env->reclaim();
                m_env = nullptr;
            }
            if (m_scheduler != nullptr) {
                delete m_scheduler;
                m_scheduler = nullptr;
            }
        }

        void stepEventLoop(TestDeliveryContext& ctx, std::int64_t timeoutUs = 50000)
        {
            ctx.watchVariable = 0;
            TaskToken timeoutToken = m_env->taskScheduler().scheduleDelayedTask(
                timeoutUs, [](void* ptr) { *static_cast<char*>(ptr) = 1; }, &ctx.watchVariable);
            m_env->taskScheduler().doEventLoop(&ctx.watchVariable);
            m_env->taskScheduler().unscheduleDelayedTask(timeoutToken);
        }

        void stepEventLoopTimeout(std::int64_t timeoutUs = 5000)
        {
            char watchVariable { 0 };
            m_env->taskScheduler().scheduleDelayedTask(
                timeoutUs, [](void* ptr) { *static_cast<char*>(ptr) = 1; }, &watchVariable);
            m_env->taskScheduler().doEventLoop(&watchVariable);
        }

        TaskScheduler* m_scheduler { nullptr };
        UsageEnvironment* m_env { nullptr };
    };

    TEST_F(Live555SourcesTest, H264SourceImmediateDelivery)
    {
        Live555FrameQueue queue(10U);

        // Push an IDR frame NALU without start code
        const std::vector<std::uint8_t> expectedPayload { 0x65U, 0x88U, 0x84U, 0x21U, 0xFFU };
        EXPECT_TRUE(queue.push(expectedPayload.data(), expectedPayload.size(), 1.5, true));

        auto* source { Live555H264LiveSource::createNew(*m_env, queue, false) };
        ASSERT_NE(source, nullptr);
        EXPECT_GT(source->maxFrameSize(), 100000U);

        TestDeliveryContext ctx {};
        ctx.buffer.resize(1024U, 0U);

        source->getNextFrame(
            ctx.buffer.data(), static_cast<unsigned>(ctx.buffer.size()), onFrameDelivered, &ctx, onSourceClosed, &ctx);

        // Run event loop to execute the scheduled delayed task
        stepEventLoop(ctx);

        EXPECT_TRUE(ctx.frameDelivered.load());
        EXPECT_EQ(ctx.frameSize, expectedPayload.size());
        EXPECT_EQ(ctx.numTruncatedBytes, 0U);
        EXPECT_EQ(ctx.presentationTime.tv_sec, 1);
        EXPECT_EQ(ctx.presentationTime.tv_usec, 500000);
        EXPECT_EQ(std::memcmp(ctx.buffer.data(), expectedPayload.data(), expectedPayload.size()), 0);

        Medium::close(source);
    }

    TEST_F(Live555SourcesTest, H264SourceStartCodeStripping)
    {
        Live555FrameQueue queue(10U);

        // Frame 1: 4-byte Annex-B start code prefix 0x00000001
        const std::vector<std::uint8_t> spsWith4ByteStart { 0x00U, 0x00U, 0x00U, 0x01U, 0x67U, 0x42U, 0x00U, 0x1EU };
        EXPECT_TRUE(queue.push(spsWith4ByteStart.data(), spsWith4ByteStart.size(), 2.0, true));

        // Frame 2: 3-byte Annex-B start code prefix 0x000001
        const std::vector<std::uint8_t> ppsWith3ByteStart { 0x00U, 0x00U, 0x01U, 0x68U, 0xCEU, 0x3CU, 0x80U };
        EXPECT_TRUE(queue.push(ppsWith3ByteStart.data(), ppsWith3ByteStart.size(), 2.033, false));

        auto* source { Live555H264LiveSource::createNew(*m_env, queue, false) };
        ASSERT_NE(source, nullptr);

        TestDeliveryContext ctx1 {};
        ctx1.buffer.resize(1024U, 0U);
        source->getNextFrame(ctx1.buffer.data(), static_cast<unsigned>(ctx1.buffer.size()), onFrameDelivered, &ctx1,
            onSourceClosed, &ctx1);

        stepEventLoop(ctx1);

        EXPECT_TRUE(ctx1.frameDelivered.load());
        EXPECT_EQ(ctx1.frameSize, 4U); // 8 - 4 = 4 bytes
        const std::vector<std::uint8_t> expectedSps { 0x67U, 0x42U, 0x00U, 0x1EU };
        EXPECT_EQ(std::memcmp(ctx1.buffer.data(), expectedSps.data(), expectedSps.size()), 0);

        // Second read: 3-byte start code
        TestDeliveryContext ctx2 {};
        ctx2.buffer.resize(1024U, 0U);
        source->getNextFrame(ctx2.buffer.data(), static_cast<unsigned>(ctx2.buffer.size()), onFrameDelivered, &ctx2,
            onSourceClosed, &ctx2);

        stepEventLoop(ctx2);

        EXPECT_TRUE(ctx2.frameDelivered.load());
        EXPECT_EQ(ctx2.frameSize, 4U); // 7 - 3 = 4 bytes
        const std::vector<std::uint8_t> expectedPps { 0x68U, 0xCEU, 0x3CU, 0x80U };
        EXPECT_EQ(std::memcmp(ctx2.buffer.data(), expectedPps.data(), expectedPps.size()), 0);

        Medium::close(source);
    }

    TEST_F(Live555SourcesTest, H264SourceTruncation)
    {
        Live555FrameQueue queue(10U);

        // 100-byte NAL unit
        const std::vector<std::uint8_t> largeNalu(100U, 0x41U);
        EXPECT_TRUE(queue.push(largeNalu.data(), largeNalu.size(), 3.0, false));

        auto* source { Live555H264LiveSource::createNew(*m_env, queue, false) };
        ASSERT_NE(source, nullptr);

        TestDeliveryContext ctx {};
        ctx.buffer.resize(32U, 0U); // Buffer capacity is only 32 bytes

        source->getNextFrame(ctx.buffer.data(), 32U, onFrameDelivered, &ctx, onSourceClosed, &ctx);

        stepEventLoop(ctx);

        EXPECT_TRUE(ctx.frameDelivered.load());
        EXPECT_EQ(ctx.frameSize, 32U);
        EXPECT_EQ(ctx.numTruncatedBytes, 68U); // 100 - 32 = 68

        Medium::close(source);
    }

    TEST_F(Live555SourcesTest, H264SourceAsyncEventTrigger)
    {
        Live555FrameQueue queue(10U);

        auto* source { Live555H264LiveSource::createNew(*m_env, queue, false) };
        ASSERT_NE(source, nullptr);

        // Connect queue event trigger to Live555 task scheduler
        const auto triggerId { source->eventTriggerId() };
        EXPECT_NE(triggerId, 0U);
        queue.setEventTrigger(
            [scheduler = m_scheduler, triggerId, source]() { scheduler->triggerEvent(triggerId, source); });

        TestDeliveryContext ctx {};
        ctx.buffer.resize(512U, 0U);

        // Request frame while queue is empty
        source->getNextFrame(
            ctx.buffer.data(), static_cast<unsigned>(ctx.buffer.size()), onFrameDelivered, &ctx, onSourceClosed, &ctx);

        EXPECT_TRUE(source->isAwaitingData());
        EXPECT_FALSE(ctx.frameDelivered.load());

        // Single step with no events queued yet
        stepEventLoopTimeout(2000);
        EXPECT_FALSE(ctx.frameDelivered.load());

        // Producer asynchronously pushes frame
        const std::vector<std::uint8_t> framePayload { 0x65U, 0xAAU, 0xBBU };
        EXPECT_TRUE(queue.push(framePayload.data(), framePayload.size(), 4.0, true));

        // Event loop handles the triggered event
        stepEventLoop(ctx);

        EXPECT_TRUE(ctx.frameDelivered.load());
        EXPECT_EQ(ctx.frameSize, 3U);
        EXPECT_EQ(std::memcmp(ctx.buffer.data(), framePayload.data(), framePayload.size()), 0);

        Medium::close(source);
    }

    TEST_F(Live555SourcesTest, Mp2tSourceSingleBundleDelivery)
    {
        Live555FrameQueue queue(10U);

        // Build 7 TS packets (7 * 188 = 1316 bytes)
        constexpr std::size_t kNumPackets { 7U };
        constexpr std::size_t kPacketSize { 188U };
        std::vector<std::uint8_t> tsBlock(kNumPackets * kPacketSize, 0x00U);
        for (std::size_t i { 0U }; i < kNumPackets; ++i) {
            tsBlock[i * kPacketSize] = 0x47U; // Sync byte
            tsBlock[(i * kPacketSize) + 1U] = static_cast<std::uint8_t>(i);
        }

        EXPECT_TRUE(queue.push(tsBlock.data(), tsBlock.size(), 10.0, true));

        auto* source { Live555Mp2tLiveSource::createNew(*m_env, queue, false) };
        ASSERT_NE(source, nullptr);
        EXPECT_EQ(source->maxFrameSize(), 1316U);

        TestDeliveryContext ctx {};
        ctx.buffer.resize(2000U, 0U);

        source->getNextFrame(
            ctx.buffer.data(), static_cast<unsigned>(ctx.buffer.size()), onFrameDelivered, &ctx, onSourceClosed, &ctx);

        stepEventLoop(ctx);

        EXPECT_TRUE(ctx.frameDelivered.load());
        EXPECT_EQ(ctx.frameSize, 1316U);
        EXPECT_EQ(ctx.numTruncatedBytes, 0U);
        EXPECT_FALSE(source->hasBufferedData());
        EXPECT_EQ(std::memcmp(ctx.buffer.data(), tsBlock.data(), tsBlock.size()), 0);

        Medium::close(source);
    }

    TEST_F(Live555SourcesTest, Mp2tSourceMultiChunkSlicing)
    {
        Live555FrameQueue queue(10U);

        // Build 21 TS packets (21 * 188 = 3948 bytes) -> should slice into 3 deliveries of 7 packets
        constexpr std::size_t kTotalPackets { 21U };
        constexpr std::size_t kPacketSize { 188U };
        constexpr std::size_t kBundleSize { 7U * kPacketSize }; // 1316 bytes
        std::vector<std::uint8_t> largeTsBlock(kTotalPackets * kPacketSize, 0x00U);

        for (std::size_t i { 0U }; i < kTotalPackets; ++i) {
            largeTsBlock[i * kPacketSize] = 0x47U; // Sync byte
            largeTsBlock[(i * kPacketSize) + 1U] = static_cast<std::uint8_t>(i + 1U);
        }

        EXPECT_TRUE(queue.push(largeTsBlock.data(), largeTsBlock.size(), 20.0, true));

        auto* source { Live555Mp2tLiveSource::createNew(*m_env, queue, false) };
        ASSERT_NE(source, nullptr);

        // Slice 1: Packets 0..6
        TestDeliveryContext ctx1 {};
        ctx1.buffer.resize(2000U, 0U);
        source->getNextFrame(ctx1.buffer.data(), static_cast<unsigned>(ctx1.buffer.size()), onFrameDelivered, &ctx1,
            onSourceClosed, &ctx1);
        stepEventLoop(ctx1);

        EXPECT_TRUE(ctx1.frameDelivered.load());
        EXPECT_EQ(ctx1.frameSize, kBundleSize);
        EXPECT_TRUE(source->hasBufferedData());
        EXPECT_EQ(std::memcmp(ctx1.buffer.data(), largeTsBlock.data(), kBundleSize), 0);

        // Slice 2: Packets 7..13
        TestDeliveryContext ctx2 {};
        ctx2.buffer.resize(2000U, 0U);
        source->getNextFrame(ctx2.buffer.data(), static_cast<unsigned>(ctx2.buffer.size()), onFrameDelivered, &ctx2,
            onSourceClosed, &ctx2);
        stepEventLoop(ctx2);

        EXPECT_TRUE(ctx2.frameDelivered.load());
        EXPECT_EQ(ctx2.frameSize, kBundleSize);
        EXPECT_TRUE(source->hasBufferedData());
        EXPECT_EQ(std::memcmp(ctx2.buffer.data(), largeTsBlock.data() + kBundleSize, kBundleSize), 0);

        // Slice 3: Packets 14..20 (final slice)
        TestDeliveryContext ctx3 {};
        ctx3.buffer.resize(2000U, 0U);
        source->getNextFrame(ctx3.buffer.data(), static_cast<unsigned>(ctx3.buffer.size()), onFrameDelivered, &ctx3,
            onSourceClosed, &ctx3);
        stepEventLoop(ctx3);

        EXPECT_TRUE(ctx3.frameDelivered.load());
        EXPECT_EQ(ctx3.frameSize, kBundleSize);
        EXPECT_FALSE(source->hasBufferedData());
        EXPECT_EQ(std::memcmp(ctx3.buffer.data(), largeTsBlock.data() + (2U * kBundleSize), kBundleSize), 0);

        Medium::close(source);
    }

    TEST_F(Live555SourcesTest, Mp2tSourceSmallPacketDelivery)
    {
        Live555FrameQueue queue(10U);

        // Push 2 TS packets (2 * 188 = 376 bytes)
        constexpr std::size_t kPacketCount { 2U };
        constexpr std::size_t kPacketSize { 188U };
        std::vector<std::uint8_t> smallTsBlock(kPacketCount * kPacketSize, 0x47U);

        EXPECT_TRUE(queue.push(smallTsBlock.data(), smallTsBlock.size(), 30.0, false));

        auto* source { Live555Mp2tLiveSource::createNew(*m_env, queue, false) };
        ASSERT_NE(source, nullptr);

        TestDeliveryContext ctx {};
        ctx.buffer.resize(1500U, 0U);

        source->getNextFrame(
            ctx.buffer.data(), static_cast<unsigned>(ctx.buffer.size()), onFrameDelivered, &ctx, onSourceClosed, &ctx);

        stepEventLoop(ctx);

        EXPECT_TRUE(ctx.frameDelivered.load());
        EXPECT_EQ(ctx.frameSize, 376U);
        EXPECT_FALSE(source->hasBufferedData());

        Medium::close(source);
    }

    TEST_F(Live555SourcesTest, CleanLifecycleTeardownWhileAwaiting)
    {
        Live555FrameQueue queue(10U);

        auto* h264Source { Live555H264LiveSource::createNew(*m_env, queue, false) };
        auto* mp2tSource { Live555Mp2tLiveSource::createNew(*m_env, queue, false) };

        ASSERT_NE(h264Source, nullptr);
        ASSERT_NE(mp2tSource, nullptr);

        TestDeliveryContext h264Ctx {};
        h264Ctx.buffer.resize(512U, 0U);
        h264Source->getNextFrame(h264Ctx.buffer.data(), static_cast<unsigned>(h264Ctx.buffer.size()), onFrameDelivered,
            &h264Ctx, onSourceClosed, &h264Ctx);

        TestDeliveryContext mp2tCtx {};
        mp2tCtx.buffer.resize(1500U, 0U);
        mp2tSource->getNextFrame(mp2tCtx.buffer.data(), static_cast<unsigned>(mp2tCtx.buffer.size()), onFrameDelivered,
            &mp2tCtx, onSourceClosed, &mp2tCtx);

        EXPECT_TRUE(h264Source->isAwaitingData());
        EXPECT_TRUE(mp2tSource->isAwaitingData());

        // Close both sources while waiting
        Medium::close(h264Source);
        Medium::close(mp2tSource);

        // Step scheduler - must not crash or invoke dangling callbacks
        stepEventLoopTimeout(2000);

        EXPECT_FALSE(h264Ctx.frameDelivered.load());
        EXPECT_FALSE(mp2tCtx.frameDelivered.load());
    }

} // namespace
} // namespace Video
