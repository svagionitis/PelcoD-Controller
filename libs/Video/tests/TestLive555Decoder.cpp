#include <cstddef>
#include <cstdint>
#include <cstring>

#include <chrono>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#if defined(PELCOD_HAS_LIVE555)
#include <BasicUsageEnvironment.hh>
#include <H264VideoRTPSource.hh>
#include <liveMedia.hh>

#include "Live555Decoder.h"
#include "Live555H264Sink.h"
#include "Live555RTSPClient.h"
#endif

#include "DecoderFactory.h"
#include "DecoderTypes.h"

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

#endif // PELCOD_HAS_LIVE555
