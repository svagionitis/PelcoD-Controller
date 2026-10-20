#include "Live555RtspServer.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#if defined(PELCOD_HAS_LIVE555)
#include "DecoderTypes.h"
#include "Live555Decoder.h"
#include "RtspServerConfig.h"
#endif

namespace Video {
namespace {

#if defined(PELCOD_HAS_LIVE555)

    TEST(Live555RtspServerTest, LifecycleAndEphemeralPortBinding)
    {
        RtspServerConfig config {};
        config.port = 0U; // Request OS-assigned dynamic port
        config.streamPath = "testlive";
        config.streamType = RtspStreamType::H264ElementaryStream;

        Live555RtspServer server(config);
        EXPECT_FALSE(server.isRunning());
        EXPECT_EQ(server.listeningPort(), 0U);

        ASSERT_TRUE(server.start());
        EXPECT_TRUE(server.isRunning());
        EXPECT_GT(server.listeningPort(), 0U);

        const std::string url { server.getUrl("127.0.0.1") };
        EXPECT_NE(url.find("rtsp://127.0.0.1:"), std::string::npos);
        EXPECT_NE(url.find("/testlive"), std::string::npos);

        server.stop();
        EXPECT_FALSE(server.isRunning());
        EXPECT_EQ(server.listeningPort(), 0U);
    }

    TEST(Live555RtspServerTest, StartStopIdempotency)
    {
        RtspServerConfig config {};
        config.port = 0U;
        config.streamPath = "live";

        Live555RtspServer server(config);

        // Multiple stops before start should be safe
        server.stop();
        EXPECT_FALSE(server.isRunning());

        // First start
        ASSERT_TRUE(server.start());
        EXPECT_TRUE(server.isRunning());
        const auto port1 { server.listeningPort() };
        EXPECT_GT(port1, 0U);

        // Second start while already running should be idempotent and return true
        EXPECT_TRUE(server.start());
        EXPECT_TRUE(server.isRunning());
        EXPECT_EQ(server.listeningPort(), port1);

        // Stop
        server.stop();
        EXPECT_FALSE(server.isRunning());
        EXPECT_EQ(server.listeningPort(), 0U);

        // Multiple stops after stopping should be safe
        server.stop();
        EXPECT_FALSE(server.isRunning());

        // Restart should succeed
        ASSERT_TRUE(server.start());
        EXPECT_TRUE(server.isRunning());
        EXPECT_GT(server.listeningPort(), 0U);

        server.stop();
        EXPECT_FALSE(server.isRunning());
    }

    TEST(Live555RtspServerTest, H264NaluIngestAndTelemetry)
    {
        RtspServerConfig config {};
        config.port = 0U;
        config.streamPath = "live";
        config.streamType = RtspStreamType::H264ElementaryStream;
        config.queueCapacity = 20U;

        Live555RtspServer server(config);
        ASSERT_TRUE(server.start());

        // Initial queue stats
        auto stats = server.queueStats();
        EXPECT_EQ(stats.currentDepth, 0U);
        EXPECT_EQ(stats.totalPushed, 0U);

        // Reject null or empty NALU
        EXPECT_FALSE(server.pushH264Nalu(nullptr, 0U));
        EXPECT_FALSE(server.pushH264Nalu(nullptr, 10U));

        // Baseline SPS NAL unit (with Annex-B start code)
        const std::vector<std::uint8_t> sps { 0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xC0, 0x1F, 0x8C, 0x68 };
        EXPECT_TRUE(server.pushH264Nalu(sps.data(), sps.size(), 1.0));

        // PPS NAL unit
        const std::vector<std::uint8_t> pps { 0x00, 0x00, 0x01, 0x68, 0xCE, 0x3C, 0x80 };
        EXPECT_TRUE(server.pushH264Nalu(pps.data(), pps.size(), 1.0));

        // IDR slice NAL unit
        const std::vector<std::uint8_t> idr { 0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x00 };
        EXPECT_TRUE(server.pushH264Nalu(idr.data(), idr.size(), 1.033));

        // Non-IDR P-frame NAL unit
        const std::vector<std::uint8_t> pFrame { 0x00, 0x00, 0x00, 0x01, 0x41, 0x9A, 0x01 };
        EXPECT_TRUE(server.pushH264Nalu(pFrame.data(), pFrame.size(), 1.066));

        stats = server.queueStats();
        EXPECT_EQ(stats.totalPushed, 4U);
        EXPECT_EQ(stats.currentDepth, 4U);

        // Direct SPS/PPS configuration API
        server.setH264SpsPps(sps, pps);

        // Bitrate adjustment API
        server.setEstimatedBitrate(3500U);

        server.stop();
    }

    TEST(Live555RtspServerTest, Mp2tPacketsIngestAndAlignment)
    {
        RtspServerConfig config {};
        config.port = 0U;
        config.streamPath = "stanag4609.ts";
        config.streamType = RtspStreamType::Mpeg2TransportStream;
        config.queueCapacity = 15U;

        Live555RtspServer server(config);
        ASSERT_TRUE(server.start());

        // Invalid sizes (must be non-zero and multiple of 188)
        const std::vector<std::uint8_t> invalidChunk(100U, 0x47U);
        EXPECT_FALSE(server.pushTsPackets(invalidChunk.data(), invalidChunk.size()));
        EXPECT_FALSE(server.pushTsPackets(nullptr, 188U));

        // Valid 188-byte PAT packet
        std::vector<std::uint8_t> patPacket(188U, 0xFFU);
        patPacket[0] = 0x47U;
        patPacket[1] = 0x40U; // PUSI + PID 0 (PAT)
        patPacket[2] = 0x00U;
        patPacket[3] = 0x10U;

        EXPECT_TRUE(server.pushTsPackets(patPacket.data(), patPacket.size()));

        // Valid multi-packet buffer (7 * 188 = 1316 bytes)
        std::vector<std::uint8_t> multiPacket(1316U, 0x00U);
        for (std::size_t i { 0U }; i < 7U; ++i) {
            multiPacket[i * 188U] = 0x47U;
        }
        EXPECT_TRUE(server.pushTsPackets(multiPacket.data(), multiPacket.size()));

        const auto stats { server.queueStats() };
        EXPECT_EQ(stats.totalPushed, 2U);
        EXPECT_EQ(stats.currentDepth, 2U);

        server.stop();
    }

    TEST(Live555RtspServerTest, AuthenticationConfiguration)
    {
        RtspServerConfig config {};
        config.port = 0U;
        config.streamPath = "secure";
        config.username = "operator";
        config.password = "pass123";
        config.realm = "PelcoD_Security";

        Live555RtspServer server(config);
        ASSERT_TRUE(server.start());
        EXPECT_TRUE(server.isRunning());
        EXPECT_GT(server.listeningPort(), 0U);

        EXPECT_EQ(server.config().username.value_or(""), "operator");
        EXPECT_EQ(server.config().password.value_or(""), "pass123");
        EXPECT_EQ(server.config().realm, "PelcoD_Security");

        server.stop();
    }

    TEST(Live555RtspServerTest, H264LoopbackStreamingWithDecoder)
    {
        RtspServerConfig config {};
        config.port = 0U;
        config.streamPath = "live";
        config.streamType = RtspStreamType::H264ElementaryStream;

        Live555RtspServer server(config);
        ASSERT_TRUE(server.start());

        // Provide baseline SPS/PPS
        const std::vector<std::uint8_t> sps { 0x67, 0x42, 0xC0, 0x1F, 0x8C, 0x68, 0x05, 0x00, 0x5B, 0xA1, 0x00, 0x00,
            0x03, 0x00, 0x01, 0x00, 0x00, 0x03, 0x00, 0x3C, 0x8F, 0x18, 0x32, 0x48 };
        const std::vector<std::uint8_t> pps { 0x68, 0xCE, 0x3C, 0x80 };
        server.setH264SpsPps(sps, pps);

        // Push initial keyframe
        std::vector<std::uint8_t> idrNalu { 0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x00 };
        ASSERT_TRUE(server.pushH264Nalu(idrNalu.data(), idrNalu.size(), 1.0));

        // Connect Live555Decoder
        const std::string rtspUrl { server.getUrl("127.0.0.1") };
        Live555Decoder decoder;
        decoder.setRtspTransport(RtspTransportMode::Tcp);

        // Initial connection attempt
        const bool initOk { decoder.initialize(rtspUrl) };
        EXPECT_TRUE(initOk);

        if (initOk) {
            EXPECT_TRUE(decoder.isInitialized());

            // Stream several frames concurrently while decoder is connected
            for (int i { 0 }; i < 5; ++i) {
                std::vector<std::uint8_t> nalu { 0x00, 0x00, 0x00, 0x01, 0x41, 0x9A, static_cast<std::uint8_t>(i) };
                EXPECT_TRUE(server.pushH264Nalu(nalu.data(), nalu.size(), 1.0 + (i * 0.033)));
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            decoder.interrupt();
        }

        server.stop();
    }

    TEST(Live555RtspServerTest, MultiClientConnectionAndActiveCount)
    {
        RtspServerConfig config {};
        config.port = 0U;
        config.streamPath = "multiclient";
        config.streamType = RtspStreamType::H264ElementaryStream;

        Live555RtspServer server(config);
        ASSERT_TRUE(server.start());

        const std::vector<std::uint8_t> sps { 0x67, 0x42, 0xC0, 0x1F, 0x8C, 0x68, 0x05, 0x00, 0x5B, 0xA1 };
        const std::vector<std::uint8_t> pps { 0x68, 0xCE, 0x3C, 0x80 };
        server.setH264SpsPps(sps, pps);

        const std::string rtspUrl { server.getUrl("127.0.0.1") };

        // Connect Client 1
        Live555Decoder decoder1;
        decoder1.setRtspTransport(RtspTransportMode::Tcp);
        const bool dec1Ok { decoder1.initialize(rtspUrl) };

        // Connect Client 2
        Live555Decoder decoder2;
        decoder2.setRtspTransport(RtspTransportMode::Tcp);
        const bool dec2Ok { decoder2.initialize(rtspUrl) };

        if (dec1Ok && dec2Ok) {
            EXPECT_GE(server.activeClientCount(), 1U);

            // Push frames to verify multi-client wake dispatching
            for (int i { 0 }; i < 3; ++i) {
                std::vector<std::uint8_t> nalu { 0x00, 0x00, 0x00, 0x01, 0x41, 0x00, static_cast<std::uint8_t>(i) };
                EXPECT_TRUE(server.pushH264Nalu(nalu.data(), nalu.size(), 1.0 + (i * 0.033)));
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            decoder1.interrupt();
            decoder2.interrupt();
        }

        server.stop();
    }

#endif // PELCOD_HAS_LIVE555

} // namespace
} // namespace Video
