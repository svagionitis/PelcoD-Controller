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

#endif // PELCOD_HAS_LIVE555
