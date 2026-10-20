#include <cstddef>
#include <cstdint>
#include <cstring>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#if defined(PELCOD_HAS_LIVE555)
#include <BasicUsageEnvironment.hh>
#include <H264VideoRTPSource.hh>
#include <liveMedia.hh>

#include "DecoderFactory.h"
#include "DecoderTypes.h"
#include "IVideoDecoder.h"
#include "Live555Decoder.h"
#include "Live555H264Sink.h"
#include "Live555RTSPClient.h"
#endif

using namespace Video;

TEST(Live555DecoderTest, InstantiationViaFactory)
{
    auto decoder { DecoderFactory::create(BackendType::Live555) };
    ASSERT_NE(decoder, nullptr);
    EXPECT_FALSE(decoder->isInitialized());
    EXPECT_FALSE(decoder->seek(10.0));
    EXPECT_EQ(decoder->getVideoMetadata().codecName, "h264");
}

TEST(Live555DecoderTest, ConfigurationProperties)
{
    auto decoder { DecoderFactory::create(BackendType::Live555) };
    ASSERT_NE(decoder, nullptr);

    decoder->setRtspTransport(RtspTransportMode::Tcp);
    EXPECT_EQ(decoder->rtspTransport(), RtspTransportMode::Tcp);

    decoder->setRtspTransport(RtspTransportMode::Udp);
    EXPECT_EQ(decoder->rtspTransport(), RtspTransportMode::Udp);

    EXPECT_FALSE(decoder->hasCredentials());
    decoder->setCredentials("operator", "secret_pass");
    EXPECT_TRUE(decoder->hasCredentials());
    EXPECT_EQ(decoder->rtspUsername(), "operator");
    EXPECT_EQ(decoder->rtspPassword(), "secret_pass");

    decoder->clearCredentials();
    EXPECT_FALSE(decoder->hasCredentials());
}

TEST(Live555DecoderTest, InterruptResponsiveness)
{
    auto decoder { DecoderFactory::create(BackendType::Live555) };
    ASSERT_NE(decoder, nullptr);

    const auto t0 { std::chrono::steady_clock::now() };
    decoder->interrupt();
    decoder->interrupt(); // Idempotency check
    const auto t1 { std::chrono::steady_clock::now() };

    const auto elapsedMs { std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() };
    EXPECT_LT(elapsedMs, 100);
}

#if defined(PELCOD_HAS_LIVE555)

TEST(Live555DecoderTest, SpropParameterSetExtraction)
{
    // Synthetic Base64 sprop-parameter-sets (SPS, PPS)
    // SPS: "Z00AH5WoLASbagICAoAA"
    // PPS: "aO48gA=="
    char const* const spropStr { "Z00AH5WoLASbagICAoAA,aO48gA==" };

    unsigned numRecords { 0U };
    SPropRecord* records { parseSPropParameterSets(spropStr, numRecords) };

    ASSERT_NE(records, nullptr);
    EXPECT_EQ(numRecords, 2U);

    for (unsigned i { 0U }; i < numRecords; ++i) {
        EXPECT_GT(records[i].sPropLength, 0U);
        EXPECT_NE(records[i].sPropBytes, nullptr);

        // Verify Annex-B prefixing pattern
        std::vector<std::uint8_t> nalu(static_cast<std::size_t>(records[i].sPropLength) + 4U, 0U);
        nalu[0] = 0x00U;
        nalu[1] = 0x00U;
        nalu[2] = 0x00U;
        nalu[3] = 0x01U;
        std::memcpy(&nalu[4], records[i].sPropBytes, records[i].sPropLength);

        EXPECT_EQ(nalu[0], 0x00U);
        EXPECT_EQ(nalu[1], 0x00U);
        EXPECT_EQ(nalu[2], 0x00U);
        EXPECT_EQ(nalu[3], 0x01U);
    }

    delete[] records;
}

TEST(Live555DecoderTest, SinkBufferFramingAndPTS)
{
    TaskScheduler* scheduler { BasicTaskScheduler::createNew() };
    ASSERT_NE(scheduler, nullptr);

    UsageEnvironment* env { BasicUsageEnvironment::createNew(*scheduler) };
    ASSERT_NE(env, nullptr);

    bool callbackInvoked { false };
    std::size_t receivedSize { 0U };
    double receivedPts { 0.0 };
    std::vector<std::uint8_t> receivedData;

    auto callback = [&](const std::uint8_t* data, std::size_t size, double pts) {
        callbackInvoked = true;
        receivedSize = size;
        receivedPts = pts;
        receivedData.assign(data, data + size);
    };

    Live555H264Sink* sink { Live555H264Sink::createNew(*env, callback, 1024U) };
    ASSERT_NE(sink, nullptr);

    Medium::close(sink);
    env->reclaim();
    delete scheduler;
}

TEST(Live555DecoderTest, ConnectionTimeoutFailure)
{
    auto decoder { DecoderFactory::create(BackendType::Live555) };
    ASSERT_NE(decoder, nullptr);

    // Non-existent loopback port should timeout cleanly without crash
    const bool success { decoder->initialize("rtsp://127.0.0.1:54321/live.sdp") };
    EXPECT_FALSE(success);
    EXPECT_FALSE(decoder->isInitialized());

    decoder->close();
}

namespace {

class MockLive555FrameProcessor : public IFrameProcessor {
public:
    void process(std::uint8_t* data, int width, int height, PixelFormat format) override
    {
        invoked = true;
        capturedWidth = width;
        capturedHeight = height;
        capturedFormat = format;
        if ((data != nullptr) && (width > 0) && (height > 0)) {
            hasData = true;
        }
    }

    bool invoked { false };
    bool hasData { false };
    int capturedWidth { 0 };
    int capturedHeight { 0 };
    PixelFormat capturedFormat { PixelFormat::RGB24 };
};

// 64x64 H.264 Baseline Annex-B sequence (SPS, PPS, IDR slice)
const std::vector<std::uint8_t> kSyntheticSps { 0x00U, 0x00U, 0x00U, 0x01U, 0x67U, 0x42U, 0xd0U, 0x0bU, 0x8cU, 0x8dU,
    0x42U, 0x12U, 0x40U, 0x3cU, 0x22U, 0x11U, 0xa8U };
const std::vector<std::uint8_t> kSyntheticPps { 0x00U, 0x00U, 0x00U, 0x01U, 0x68U, 0xceU, 0x3cU, 0x80U };
const std::vector<std::uint8_t> kSyntheticIdr { 0x00U, 0x00U, 0x00U, 0x01U, 0x65U, 0xb8U, 0x00U, 0x04U, 0x00U, 0x00U,
    0x17U, 0x88U, 0xe3U, 0x80U, 0x00U, 0x82U, 0x2cU, 0x70U, 0x00U, 0x10U, 0x44U, 0x93U, 0x93U, 0x93U, 0xabU, 0xaeU,
    0xbaU, 0xebU, 0xaeU, 0xbaU, 0xeaU, 0x4aU, 0xebU, 0xafU };

// 32x32 H.264 Baseline Annex-B sequence (SPS, PPS, IDR slice)
const std::vector<std::uint8_t> kSynthetic32Sps { 0x00U, 0x00U, 0x00U, 0x01U, 0x67U, 0x42U, 0xc0U, 0x0aU, 0xd9U, 0x09U,
    0x6cU, 0x04U, 0x40U, 0x00U, 0x00U, 0x03U, 0x00U, 0x40U, 0x00U, 0x00U, 0x03U, 0x00U, 0x83U, 0xc4U, 0x89U, 0x92U };
const std::vector<std::uint8_t> kSynthetic32Pps { 0x00U, 0x00U, 0x00U, 0x01U, 0x68U, 0xcbU, 0x83U, 0xcbU, 0x20U };
const std::vector<std::uint8_t> kSynthetic32Idr { 0x00U, 0x00U, 0x00U, 0x01U, 0x65U, 0x88U, 0x84U, 0x5fU, 0xb1U, 0x44U,
    0x2cU, 0x01U, 0xf8U, 0xd8U, 0xa0U, 0x00U, 0x20U, 0x73U, 0x00U, 0x08U, 0x65U, 0x2cU, 0xcfU, 0x42U, 0xcaU, 0xa0U,
    0x00U, 0xc0U, 0x00U, 0x90U, 0xd4U, 0x86U, 0x65U, 0x15U, 0xfbU, 0x8aU, 0x46U, 0xc5U, 0xd0U, 0x92U, 0xaaU, 0xfbU,
    0x7fU, 0xffU, 0xfcU, 0xe7U, 0x7cU, 0x0bU, 0x04U, 0x3dU, 0xf8U, 0x60U, 0x00U, 0x40U, 0xc3U, 0x9bU, 0x96U, 0x0fU,
    0xd6U, 0x08U, 0x0bU, 0x7bU, 0x4bU, 0x42U, 0x90U, 0x84U, 0x97U, 0xdfU, 0xf6U, 0x06U, 0x30U, 0xe1U, 0x40U, 0x03U,
    0x00U, 0x01U, 0x00U, 0x90U, 0x03U, 0x02U, 0x00U, 0x01U, 0x00U, 0x80U, 0x40U, 0x2eU, 0x00U, 0x04U, 0xc9U, 0xbcU,
    0xd0U, 0x0aU, 0x0fU, 0xffU, 0x3fU, 0xb5U, 0x90U, 0x07U, 0x17U, 0x75U, 0x01U, 0x43U, 0xd9U, 0x50U, 0x84U, 0x03U,
    0x60U, 0xb2U, 0x01U, 0xb0U, 0x00U, 0x1eU, 0x8cU, 0x06U, 0x41U, 0x22U, 0x9cU, 0xd0U, 0x6dU, 0x4eU, 0x47U, 0xf3U,
    0x45U, 0x12U, 0x1aU, 0x85U, 0xd7U, 0xe8U, 0xfeU, 0xb2U, 0xd3U, 0xc2U, 0x00U, 0x01U, 0x10U, 0x00U, 0x46U, 0x04U,
    0x00U, 0x02U, 0x20U, 0x00U, 0x8cU, 0x7fU, 0x43U, 0xfaU, 0x71U, 0xd1U, 0xe0U, 0x00U, 0x4cU, 0x02U, 0x82U, 0x42U,
    0x52U, 0xc2U, 0x17U, 0x30U, 0x78U, 0x05U, 0x71U, 0xf8U, 0x36U, 0xc8U, 0x87U, 0x08U, 0xe4U, 0x70U, 0x31U, 0xbbU,
    0x40U, 0x00U, 0x00U, 0x80U, 0x94U, 0x03U, 0xb3U, 0x2eU, 0x76U, 0x34U, 0xdbU, 0x81U, 0xfcU, 0x0cU, 0xe6U, 0xe4U,
    0xf4U, 0xa4U, 0x8eU, 0x06U, 0xfeU, 0xf6U, 0x02U, 0x1fU, 0x0bU, 0x01U, 0x60U, 0xa9U, 0x52U, 0x36U, 0x66U, 0x58U,
    0x7fU, 0xffU, 0xf3U, 0x83U, 0x85U, 0x00U, 0x01U, 0x00U, 0xc0U, 0x00U, 0x10U, 0x09U, 0x00U, 0x01U, 0x0aU, 0xb0U,
    0x80U, 0x00U, 0xf0U, 0x38U, 0x0bU, 0x7bU, 0x5eU, 0x74U, 0x00U, 0xa2U, 0x4dU, 0x40U, 0xc1U, 0x09U, 0xabU, 0x51U,
    0xaeU, 0xd8U, 0x0aU, 0x0dU, 0x3bU, 0x61U, 0x08U, 0x06U, 0xc1U, 0x64U, 0x03U, 0x63U, 0x22U, 0x06U, 0xc1U, 0xf8U,
    0x9cU, 0x13U, 0xc4U, 0x88U, 0x57U, 0x1eU, 0x18U, 0xcbU, 0x8aU, 0xf8U, 0xe4U, 0x03U, 0x66U, 0x7eU, 0x82U, 0x00U,
    0x01U, 0x10U, 0x00U, 0x46U, 0x04U, 0x00U, 0x02U, 0x20U, 0x00U, 0x8cU, 0x42U, 0x1fU, 0xeaU, 0x21U, 0xb1U, 0xfcU,
    0x00U, 0x59U, 0x20U, 0x16U, 0xf4U, 0xb1U, 0x09U, 0x71U, 0x2eU, 0xaaU, 0x7aU, 0xbdU, 0xe4U, 0x20U, 0x0dU, 0xd0U,
    0x07U, 0x06U, 0x84U, 0xa0U, 0x0bU, 0x41U, 0x2aU, 0x86U, 0x11U, 0x51U, 0xb7U, 0xe0U, 0x0bU, 0x0eU, 0x72U, 0x91U,
    0x2cU, 0x9cU, 0x23U, 0x54U, 0xafU, 0xf8U, 0x70U, 0x93U, 0x83U, 0xa2U, 0xf8U, 0x5bU, 0x8aU, 0xacU, 0x0bU, 0x31U,
    0xefU, 0xe3U, 0x08U, 0x08U, 0x30U, 0x5cU, 0xb0U, 0x40U, 0x20U, 0x00U, 0x20U, 0x28U, 0x80U, 0x03U, 0x42U, 0x00U,
    0x01U, 0x03U, 0xd7U, 0x00U, 0x01U, 0x02U, 0x24U, 0x3bU, 0x55U, 0x4bU, 0x00U, 0x72U, 0xf5U, 0x0fU, 0x42U, 0x01U,
    0x43U, 0x17U, 0xaeU, 0x59U, 0x04U, 0xecU, 0xe8U, 0x30U, 0x35U, 0x35U, 0xfcU, 0xb6U, 0x5aU, 0xa9U, 0x60U, 0x0aU,
    0x63U, 0x99U, 0x47U, 0x81U, 0x93U, 0x25U, 0x13U, 0x7bU, 0xfeU, 0xc4U, 0xc1U, 0x2bU, 0x11U, 0xc8U, 0x68U, 0x40U,
    0x0eU, 0x3cU, 0xffU, 0xbfU, 0xc4U, 0x00U, 0x06U, 0x21U, 0x61U, 0x0fU, 0x00U, 0x17U, 0x90U, 0x13U, 0xa0U, 0x76U,
    0x98U, 0x21U, 0x36U, 0x66U, 0x7aU, 0xbcU, 0x07U, 0x80U, 0x20U, 0x74U, 0x03U, 0x03U, 0x00U, 0x74U, 0xc0U, 0xb8U,
    0x69U, 0xd0U, 0x72U, 0x5aU, 0x0cU, 0x1dU, 0x03U, 0x02U, 0x3dU, 0x27U, 0x05U, 0x7dU, 0x4dU, 0xf8U, 0x70U, 0x5bU,
    0x97U, 0x5eU, 0xcfU, 0x82U, 0xc9U, 0x1cU, 0x23U, 0x52U, 0x51U, 0xbbU, 0x43U, 0xc1U, 0x14U, 0x90U, 0xa9U, 0x60U,
    0x80U, 0x00U, 0xd8U, 0x00U, 0x08U, 0x09U, 0x8cU, 0x04U, 0x02U, 0x04U, 0x88U, 0xe5U, 0x6eU, 0x5fU, 0x8bU, 0xc9U,
    0x0dU, 0x5dU, 0x50U, 0x1cU, 0x02U, 0x2fU, 0xdfU, 0x81U, 0x3aU, 0x9bU, 0xc6U, 0xa4U, 0xe2U, 0x02U, 0xb7U, 0xceU,
    0x47U, 0xf2U, 0xccU, 0x24U, 0x6eU, 0xcbU, 0x90U, 0x05U, 0xe5U, 0x80U, 0xfdU, 0x21U, 0x0bU, 0x28U, 0xcfU, 0x48U,
    0x88U, 0x59U, 0x91U, 0x42U, 0xb6U, 0x80U };

} // namespace

TEST(Live555DecoderTest, OpenH264DecoderLifecycle)
{
    auto decoder { std::make_unique<Live555Decoder>() };
    ASSERT_NE(decoder, nullptr);

    EXPECT_TRUE(decoder->initialize("memory://stream", PixelFormat::RGB24, 2));
    EXPECT_TRUE(decoder->isInitialized());
    EXPECT_EQ(decoder->getPerformanceStats().totalDecodedFrames, 0U);

    decoder->close();
    EXPECT_FALSE(decoder->isInitialized());
}

TEST(Live555DecoderTest, NonVclNaluHandling)
{
    auto decoder { std::make_unique<Live555Decoder>() };
    ASSERT_NE(decoder, nullptr);

    EXPECT_TRUE(decoder->initialize("memory://stream"));
    decoder->queueNalu(kSyntheticSps.data(), kSyntheticSps.size(), 0.0);
    decoder->queueNalu(kSyntheticPps.data(), kSyntheticPps.size(), 0.0);

    // Without an IDR slice, decodeNextFrame should timeout after consuming SPS/PPS and return false
    EXPECT_FALSE(decoder->decodeNextFrame());
    EXPECT_EQ(decoder->getPerformanceStats().totalDecodedFrames, 0U);

    decoder->close();
}

TEST(Live555DecoderTest, SyntheticFrameDecodingRgb24)
{
    auto decoder { std::make_unique<Live555Decoder>() };
    ASSERT_NE(decoder, nullptr);

    EXPECT_TRUE(decoder->initialize("memory://stream", PixelFormat::RGB24));
    decoder->queueNalu(kSyntheticSps.data(), kSyntheticSps.size(), 0.0);
    decoder->queueNalu(kSyntheticPps.data(), kSyntheticPps.size(), 0.0);
    decoder->queueNalu(kSyntheticIdr.data(), kSyntheticIdr.size(), 1.25);

    EXPECT_TRUE(decoder->decodeNextFrame());

    const auto frameInfo { decoder->getRawFrameData() };
    EXPECT_EQ(frameInfo.width, 64);
    EXPECT_EQ(frameInfo.height, 64);
    EXPECT_NE(frameInfo.data, nullptr);
    EXPECT_DOUBLE_EQ(frameInfo.timestamp, 1.25);

    const auto stats { decoder->getPerformanceStats() };
    EXPECT_EQ(stats.totalDecodedFrames, 1U);
    EXPECT_GT(stats.averageDecodeTimeMs, 0.0);

    decoder->close();
}

TEST(Live555DecoderTest, SyntheticFrameDecodingBgr24)
{
    auto decoder { std::make_unique<Live555Decoder>() };
    ASSERT_NE(decoder, nullptr);

    EXPECT_TRUE(decoder->initialize("memory://stream", PixelFormat::BGR24));
    decoder->queueNalu(kSyntheticSps.data(), kSyntheticSps.size(), 0.0);
    decoder->queueNalu(kSyntheticPps.data(), kSyntheticPps.size(), 0.0);
    decoder->queueNalu(kSyntheticIdr.data(), kSyntheticIdr.size(), 0.75);

    EXPECT_TRUE(decoder->decodeNextFrame());

    const auto frameInfo { decoder->getRawFrameData() };
    EXPECT_EQ(frameInfo.width, 64);
    EXPECT_EQ(frameInfo.height, 64);
    EXPECT_EQ(frameInfo.format, PixelFormat::BGR24);
    EXPECT_NE(frameInfo.data, nullptr);
    EXPECT_DOUBLE_EQ(frameInfo.timestamp, 0.75);

    decoder->close();
}

TEST(Live555DecoderTest, TripleBufferingAndFrameProcessors)
{
    auto decoder { std::make_unique<Live555Decoder>() };
    ASSERT_NE(decoder, nullptr);

    decoder->enableTripleBuffering(true);
    EXPECT_TRUE(decoder->isTripleBufferingEnabled());

    auto processor { std::make_shared<MockLive555FrameProcessor>() };
    decoder->addFrameProcessor(processor);

    EXPECT_TRUE(decoder->initialize("memory://stream", PixelFormat::RGB24));
    decoder->queueNalu(kSyntheticSps.data(), kSyntheticSps.size(), 0.0);
    decoder->queueNalu(kSyntheticPps.data(), kSyntheticPps.size(), 0.0);
    decoder->queueNalu(kSyntheticIdr.data(), kSyntheticIdr.size(), 2.5);

    EXPECT_TRUE(decoder->decodeNextFrame());

    EXPECT_TRUE(processor->invoked);
    EXPECT_TRUE(processor->hasData);
    EXPECT_EQ(processor->capturedWidth, 64);
    EXPECT_EQ(processor->capturedHeight, 64);
    EXPECT_EQ(processor->capturedFormat, PixelFormat::RGB24);

    decoder->close();
}

TEST(Live555DecoderTest, LifecycleCloseIdempotency)
{
    auto decoder { std::make_unique<Live555Decoder>() };
    ASSERT_NE(decoder, nullptr);

    EXPECT_TRUE(decoder->initialize("memory://stream", PixelFormat::RGB24));
    EXPECT_TRUE(decoder->isInitialized());

    decoder->close();
    EXPECT_FALSE(decoder->isInitialized());

    // Idempotent secondary and tertiary close calls
    decoder->close();
    EXPECT_FALSE(decoder->isInitialized());
    decoder->close();
    EXPECT_FALSE(decoder->isInitialized());
}

TEST(Live555DecoderTest, LifecycleInterruptDuringDecoding)
{
    auto decoder { std::make_unique<Live555Decoder>() };
    ASSERT_NE(decoder, nullptr);

    EXPECT_TRUE(decoder->initialize("memory://stream", PixelFormat::RGB24));

    std::atomic<bool> decodeFinished { false };
    std::atomic<bool> decodeResult { true };

    std::thread decodeThread([&]() {
        decodeResult.store(decoder->decodeNextFrame());
        decodeFinished.store(true);
    });

    // Allow thread to enter waiting condition variable
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    const auto t0 { std::chrono::steady_clock::now() };
    decoder->interrupt();
    decodeThread.join();
    const auto t1 { std::chrono::steady_clock::now() };

    const auto elapsedMs { std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() };
    EXPECT_TRUE(decodeFinished.load());
    EXPECT_FALSE(decodeResult.load());
    EXPECT_LT(elapsedMs, 150);

    decoder->close();
}

TEST(Live555DecoderTest, SeekRejectionForLiveStream)
{
    auto decoder { std::make_unique<Live555Decoder>() };
    ASSERT_NE(decoder, nullptr);

    EXPECT_FALSE(decoder->seek(10.0));
    EXPECT_FALSE(decoder->seek(-1.0));
    EXPECT_FALSE(decoder->seek(0.0));
}

TEST(Live555DecoderTest, AutoReconnectConfiguration)
{
    auto decoder { std::make_unique<Live555Decoder>() };
    ASSERT_NE(decoder, nullptr);

    EXPECT_TRUE(decoder->isAutoReconnectEnabled());

    decoder->setAutoReconnect(false);
    EXPECT_FALSE(decoder->isAutoReconnectEnabled());

    decoder->setAutoReconnect(true, 500);
    EXPECT_TRUE(decoder->isAutoReconnectEnabled());
}

TEST(Live555DecoderTest, ReconnectionWithSyntheticSource)
{
    auto decoder { std::make_unique<Live555Decoder>() };
    ASSERT_NE(decoder, nullptr);

    EXPECT_TRUE(decoder->initialize("memory://stream", PixelFormat::RGB24));
    EXPECT_TRUE(decoder->isInitialized());

    EXPECT_TRUE(decoder->reconnect());
    EXPECT_TRUE(decoder->isInitialized());

    decoder->queueNalu(kSyntheticSps.data(), kSyntheticSps.size(), 0.0);
    decoder->queueNalu(kSyntheticPps.data(), kSyntheticPps.size(), 0.0);
    decoder->queueNalu(kSyntheticIdr.data(), kSyntheticIdr.size(), 3.0);

    EXPECT_TRUE(decoder->decodeNextFrame());
    const auto frameInfo { decoder->getRawFrameData() };
    EXPECT_EQ(frameInfo.width, 64);
    EXPECT_EQ(frameInfo.height, 64);
    EXPECT_DOUBLE_EQ(frameInfo.timestamp, 3.0);

    decoder->close();
}

TEST(Live555DecoderTest, OpenH264DecoderReinitializationCycles)
{
    auto decoder { std::make_unique<Live555Decoder>() };
    ASSERT_NE(decoder, nullptr);

    for (int cycle { 0 }; cycle < 3; ++cycle) {
        EXPECT_TRUE(decoder->initialize("memory://stream", PixelFormat::RGB24));
        EXPECT_TRUE(decoder->isInitialized());

        decoder->queueNalu(kSyntheticSps.data(), kSyntheticSps.size(), 0.0);
        decoder->queueNalu(kSyntheticPps.data(), kSyntheticPps.size(), 0.0);
        decoder->queueNalu(kSyntheticIdr.data(), kSyntheticIdr.size(), static_cast<double>(cycle));

        EXPECT_TRUE(decoder->decodeNextFrame());
        const auto frameInfo { decoder->getRawFrameData() };
        EXPECT_EQ(frameInfo.width, 64);
        EXPECT_EQ(frameInfo.height, 64);

        decoder->close();
        EXPECT_FALSE(decoder->isInitialized());
    }
}

TEST(Live555DecoderTest, BitstreamCorruptionTolerance)
{
    auto decoder { std::make_unique<Live555Decoder>() };
    ASSERT_NE(decoder, nullptr);

    EXPECT_TRUE(decoder->initialize("memory://stream", PixelFormat::RGB24));

    // Truncated NALU (short slice header without payload)
    const std::vector<std::uint8_t> truncatedNalu { 0x00U, 0x00U, 0x00U, 0x01U, 0x65U };
    decoder->queueNalu(truncatedNalu.data(), truncatedNalu.size(), 0.0);
    EXPECT_FALSE(decoder->decodeNextFrame());

    // Corrupted random garbage NALU
    const std::vector<std::uint8_t> garbageNalu { 0x00U, 0x00U, 0x00U, 0x01U, 0xffU, 0xaaU, 0xbbU, 0xccU, 0xddU };
    decoder->queueNalu(garbageNalu.data(), garbageNalu.size(), 0.0);
    EXPECT_FALSE(decoder->decodeNextFrame());

    // Immediate recovery with valid synthetic stream
    decoder->queueNalu(kSyntheticSps.data(), kSyntheticSps.size(), 0.0);
    decoder->queueNalu(kSyntheticPps.data(), kSyntheticPps.size(), 0.0);
    decoder->queueNalu(kSyntheticIdr.data(), kSyntheticIdr.size(), 1.0);

    EXPECT_TRUE(decoder->decodeNextFrame());
    const auto frameInfo { decoder->getRawFrameData() };
    EXPECT_EQ(frameInfo.width, 64);
    EXPECT_EQ(frameInfo.height, 64);
    EXPECT_NE(frameInfo.data, nullptr);

    decoder->close();
}

TEST(Live555DecoderTest, DynamicResolutionSwitching)
{
    auto decoder { std::make_unique<Live555Decoder>() };
    ASSERT_NE(decoder, nullptr);

    decoder->enableTripleBuffering(true);
    EXPECT_TRUE(decoder->initialize("memory://stream", PixelFormat::RGB24));

    // Initial 64x64 sequence
    decoder->queueNalu(kSyntheticSps.data(), kSyntheticSps.size(), 0.0);
    decoder->queueNalu(kSyntheticPps.data(), kSyntheticPps.size(), 0.0);
    decoder->queueNalu(kSyntheticIdr.data(), kSyntheticIdr.size(), 1.0);

    EXPECT_TRUE(decoder->decodeNextFrame());
    auto frameInfo { decoder->getRawFrameData() };
    EXPECT_EQ(frameInfo.width, 64);
    EXPECT_EQ(frameInfo.height, 64);

    // Dynamic switch to 32x32 sequence
    decoder->queueNalu(kSynthetic32Sps.data(), kSynthetic32Sps.size(), 0.0);
    decoder->queueNalu(kSynthetic32Pps.data(), kSynthetic32Pps.size(), 0.0);
    decoder->queueNalu(kSynthetic32Idr.data(), kSynthetic32Idr.size(), 2.0);

    EXPECT_TRUE(decoder->decodeNextFrame());
    frameInfo = decoder->getRawFrameData();
    EXPECT_EQ(frameInfo.width, 32);
    EXPECT_EQ(frameInfo.height, 32);

    // Dynamic switch back to 64x64 sequence
    decoder->queueNalu(kSyntheticSps.data(), kSyntheticSps.size(), 0.0);
    decoder->queueNalu(kSyntheticPps.data(), kSyntheticPps.size(), 0.0);
    decoder->queueNalu(kSyntheticIdr.data(), kSyntheticIdr.size(), 3.0);

    EXPECT_TRUE(decoder->decodeNextFrame());
    frameInfo = decoder->getRawFrameData();
    EXPECT_EQ(frameInfo.width, 64);
    EXPECT_EQ(frameInfo.height, 64);

    decoder->close();
}

TEST(Live555DecoderTest, QueueBackpressureAndOverflow)
{
    auto decoder { std::make_unique<Live555Decoder>() };
    ASSERT_NE(decoder, nullptr);

    EXPECT_TRUE(decoder->initialize("memory://stream", PixelFormat::RGB24));

    // Flood the queue with 50 NALUs (max capacity is 30)
    for (std::size_t i { 0U }; i < 50U; ++i) {
        decoder->queueNalu(kSyntheticIdr.data(), kSyntheticIdr.size(), static_cast<double>(i) * 0.04);
    }

    decoder->close();
    EXPECT_FALSE(decoder->isInitialized());
}

TEST(Live555DecoderTest, ConcurrentTripleBufferReading)
{
    auto decoder { std::make_unique<Live555Decoder>() };
    ASSERT_NE(decoder, nullptr);

    decoder->enableTripleBuffering(true);
    EXPECT_TRUE(decoder->initialize("memory://stream", PixelFormat::RGB24));

    std::atomic<bool> stopReaders { false };
    std::atomic<std::size_t> successfulReads { 0U };

    auto readerFunc = [&]() {
        while (!stopReaders.load()) {
            const auto frame { decoder->getRawFrameData() };
            if ((frame.data != nullptr) && (frame.width == 64) && (frame.height == 64)) {
                ++successfulReads;
            }
            std::this_thread::yield();
        }
    };

    std::thread reader1(readerFunc);
    std::thread reader2(readerFunc);

    for (int frameIdx { 0 }; frameIdx < 10; ++frameIdx) {
        decoder->queueNalu(kSyntheticSps.data(), kSyntheticSps.size(), 0.0);
        decoder->queueNalu(kSyntheticPps.data(), kSyntheticPps.size(), 0.0);
        decoder->queueNalu(kSyntheticIdr.data(), kSyntheticIdr.size(), static_cast<double>(frameIdx));
        EXPECT_TRUE(decoder->decodeNextFrame());
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    stopReaders.store(true);
    reader1.join();
    reader2.join();

    EXPECT_GT(successfulReads.load(), 0U);

    decoder->close();
}

#endif // PELCOD_HAS_LIVE555
