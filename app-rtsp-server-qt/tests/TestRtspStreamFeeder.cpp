#include "RtspStreamFeeder.h"

#include <cstddef>
#include <cstdint>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "Live555Decoder.h"
#include "Live555RtspServer.h"
#include "RtspServerConfig.h"

namespace RtspServerApp {
namespace {

    TEST(RtspStreamFeederTest, NullServerRejection)
    {
        RtspStreamFeeder feeder;
        FeederConfig config {};
        EXPECT_FALSE(feeder.start(config, nullptr));
        EXPECT_FALSE(feeder.isRunning());
    }

    TEST(RtspStreamFeederTest, SyntheticPacingLifecycle)
    {
        Video::RtspServerConfig serverConfig {};
        serverConfig.port = 0U; // Ephemeral
        serverConfig.streamPath = "synthetic_feed";
        serverConfig.streamType = Video::RtspStreamType::H264ElementaryStream;

        Video::Live555RtspServer server(serverConfig);
        ASSERT_TRUE(server.start());

        RtspStreamFeeder feeder;
        FeederConfig feederConfig {};
        feederConfig.mode = FeederSourceMode::SyntheticPattern;
        feederConfig.targetFps = 60U;

        ASSERT_TRUE(feeder.start(feederConfig, &server));
        EXPECT_TRUE(feeder.isRunning());

        // Allow worker to stream several frames
        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        const auto metrics = feeder.metrics();
        EXPECT_GT(metrics.framesStreamed, 0U);
        EXPECT_GT(metrics.bytesStreamed, 0U);

        feeder.stop();
        EXPECT_FALSE(feeder.isRunning());

        server.stop();
    }

    TEST(RtspStreamFeederTest, MpegTsStreamingPacing)
    {
        Video::RtspServerConfig serverConfig {};
        serverConfig.port = 0U;
        serverConfig.streamPath = "ts_feed";
        serverConfig.streamType = Video::RtspStreamType::Mpeg2TransportStream;

        Video::Live555RtspServer server(serverConfig);
        ASSERT_TRUE(server.start());

        RtspStreamFeeder feeder;
        FeederConfig feederConfig {};
        feederConfig.mode = FeederSourceMode::MpegTsFile;
        feederConfig.filePath = "sample-videos/mpegts-klv-day-flight.ts";
        feederConfig.targetFps = 60U;
        feederConfig.loopPlayback = true;

        ASSERT_TRUE(feeder.start(feederConfig, &server));
        EXPECT_TRUE(feeder.isRunning());

        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        const auto metrics = feeder.metrics();
        EXPECT_GT(metrics.bytesStreamed, 0U);

        feeder.stop();
        EXPECT_FALSE(feeder.isRunning());

        server.stop();
    }

    TEST(RtspStreamFeederTest, SyntheticStreamingLoopbackDecoder)
    {
        Video::RtspServerConfig serverConfig {};
        serverConfig.port = 0U; // Ephemeral
        serverConfig.streamPath = "synthetic_loopback";
        serverConfig.streamType = Video::RtspStreamType::H264ElementaryStream;

        Video::Live555RtspServer server(serverConfig);
        ASSERT_TRUE(server.start());

        RtspStreamFeeder feeder;
        FeederConfig feederConfig {};
        feederConfig.mode = FeederSourceMode::SyntheticPattern;
        feederConfig.targetFps = 30U;

        ASSERT_TRUE(feeder.start(feederConfig, &server));
        EXPECT_TRUE(feeder.isRunning());

        const std::string url = server.getUrl("127.0.0.1");
        Video::Live555Decoder decoder;
        decoder.setRtspTransport(Video::RtspTransportMode::Tcp);

        const bool initOk = decoder.initialize(url);
        EXPECT_TRUE(initOk);

        if (initOk) {
            EXPECT_TRUE(decoder.isInitialized());
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            decoder.interrupt();
        }

        feeder.stop();
        server.stop();
    }

} // namespace
} // namespace RtspServerApp
