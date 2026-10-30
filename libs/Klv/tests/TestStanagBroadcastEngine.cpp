#include "StanagBroadcastEngine.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "KlvTypes.h"
#include "MpegTsKlvMuxer.h"
#include "SocketUtils.h"
#include "VideoTypes.h"

using namespace Klv;

TEST(TestStanagBroadcastEngine, LifecycleStartStop)
{
    StanagBroadcastConfig config {};
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
    StanagBroadcastConfig config {};
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
    VideoAccessUnit au {};
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
    StanagBroadcastConfig config {};
    config.enableNetworkPublishing = false;
    StanagBroadcastEngine engine(config);

    std::vector<std::size_t> packetSizes;
    engine.setPacketCallback(
        [&packetSizes](const std::uint8_t* /*packet*/, std::size_t size) { packetSizes.push_back(size); });

    EXPECT_TRUE(engine.start());

    constexpr std::uint64_t kBaseTimeUs = 2000000ULL;

    for (std::size_t i = 0U; i < 3U; ++i) {
        const std::uint64_t t = kBaseTimeUs + static_cast<std::uint64_t>(i) * 33333ULL;

        UasDatalinkMessage msg {};
        msg.missionId = "BROADCAST_TEST_" + std::to_string(i);
        msg.precisionTimeStampUs = t;
        EXPECT_TRUE(engine.pushTelemetry(msg));

        VideoAccessUnit au {};
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
    StanagBroadcastConfig config {};
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
    StanagBroadcastConfig config {};
    config.enableNetworkPublishing = false;
    config.maxVideoQueueSize = 2U;
    config.maxTelemetryQueueSize = 2U;
    StanagBroadcastEngine engine(config);

    // Engine is NOT started, so queues will not be drained
    VideoAccessUnit au {};
    au.codec = VideoCodec::H264;
    au.ptsUs = 1000ULL;
    au.data = { 0x00U, 0x00U, 0x00U, 0x01U, 0x41U };

    EXPECT_TRUE(engine.pushVideoAccessUnit(au));
    EXPECT_TRUE(engine.pushVideoAccessUnit(au));
    // 3rd push must overflow and drop
    EXPECT_FALSE(engine.pushVideoAccessUnit(au));

    UasDatalinkMessage msg {};
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
    StanagBroadcastConfig config {};
    config.enableNetworkPublishing = false;
    StanagBroadcastEngine engine(config);

    std::size_t packetCount = 0U;
    engine.setPacketCallback([&packetCount](const std::uint8_t* /*packet*/, std::size_t /*size*/) { packetCount++; });

    EXPECT_TRUE(engine.start());

    // Standard 16-byte MISB ST 0601 Universal Label prefix + 2-byte length + Tag 1 CRC
    const std::vector<std::uint8_t> rawKlv = { 0x06U, 0x0EU, 0x2BU, 0x34U, 0x02U, 0x0BU, 0x01U, 0x01U, 0x0EU, 0x01U,
        0x03U, 0x01U, 0x01U, 0x00U, 0x00U, 0x00U, 0x04U, 0x01U, 0x02U, 0x12U, 0x34U };

    EXPECT_TRUE(engine.pushRawKlv(rawKlv.data(), rawKlv.size(), 4000000ULL));

    VideoAccessUnit au {};
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
    StanagBroadcastConfig config {};
    config.enableNetworkPublishing = true;
    config.networkConfig.destinationIp = "127.0.0.1";
    config.networkConfig.destinationPort = 59876U;
    config.networkConfig.protocol = Transport::TsNetworkProtocol::RawUdp;

    StanagBroadcastEngine engine(config);

    EXPECT_TRUE(engine.start());
    EXPECT_TRUE(engine.stats().isNetworkOpen);

    VideoAccessUnit au {};
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
    StanagBroadcastConfig config {};
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
    VideoAccessUnit au {};
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

TEST(TestStanagBroadcastEngine, MultiPidTelemetryIngest)
{
    StanagBroadcastConfig config {};
    config.enableNetworkPublishing = false;
    MetadataStreamConfig s1;
    s1.pid = 0x01E0U;
    s1.formatIdentifier = "KLVA";
    MetadataStreamConfig s2;
    s2.pid = 0x01E1U;
    s2.formatIdentifier = "KLVA";
    config.muxerConfig.metadataStreams = { s1, s2 };

    StanagBroadcastEngine engine(config);

    std::vector<std::pair<std::uint16_t, std::vector<std::uint8_t>>> packets;
    engine.setPacketCallback([&packets](const std::uint8_t* pkt, std::size_t size) {
        const std::uint16_t pid
            = static_cast<std::uint16_t>(((static_cast<std::uint16_t>(pkt[1] & 0x1FU)) << 8U) | pkt[2]);
        packets.emplace_back(pid, std::vector<std::uint8_t>(pkt, pkt + size));
    });

    EXPECT_TRUE(engine.start());

    UasDatalinkMessage msg1;
    msg1.platformHeadingDeg = 100.0;
    msg1.precisionTimeStampUs = 1000000ULL;

    UasDatalinkMessage msg2;
    msg2.platformHeadingDeg = 200.0;
    msg2.precisionTimeStampUs = 1000010ULL;

    EXPECT_TRUE(engine.pushTelemetry(0x01E0U, msg1));
    EXPECT_TRUE(engine.pushTelemetry(0x01E1U, msg2));

    engine.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    engine.stop();

    bool foundE0 = false;
    bool foundE1 = false;
    for (const auto& p : packets) {
        if (p.first == 0x01E0U)
            foundE0 = true;
        if (p.first == 0x01E1U)
            foundE1 = true;
    }
    EXPECT_TRUE(foundE0);
    EXPECT_TRUE(foundE1);
}

TEST(TestStanagBroadcastEngine, RtspServerEgressIntegration)
{
    StanagBroadcastConfig config {};
    config.enableNetworkPublishing = false;
    config.enableRtspServer = true;
    config.rtspConfig.listenAddress = "127.0.0.1";
    config.rtspConfig.listenPort = Transport::RtspPort(19554U);
    config.rtspConfig.packetsPerDatagram = 1U;
    config.rtspConfig.streamPath = "/live";

    StanagBroadcastEngine engine(config);
    ASSERT_TRUE(engine.start());

    auto stats = engine.stats();
    EXPECT_TRUE(stats.isRtspServerActive);
    EXPECT_EQ(stats.activeRtspClients, 0U);

    Transport::Net::ensureWinsockInitialized();
    const Transport::Net::SocketHandle clientSock = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(clientSock, Transport::Net::InvalidSocket);

    sockaddr_in srvAddr {};
    srvAddr.sin_family = AF_INET;
    srvAddr.sin_port = htons(19554U);
    srvAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    ASSERT_EQ(::connect(clientSock, reinterpret_cast<struct sockaddr*>(&srvAddr), sizeof(srvAddr)), 0);

    // 1. SETUP
    const std::string setupReq = "SETUP rtsp://127.0.0.1:19554/live RTSP/1.0\r\n"
                                 "CSeq: 1\r\n"
                                 "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
                                 "\r\n";
    EXPECT_EQ(::send(clientSock, setupReq.data(), static_cast<Transport::Net::SockBufLenType>(setupReq.size()), 0),
        static_cast<decltype(::send(0, nullptr, 0, 0))>(setupReq.size()));

    std::array<char, 1024> recvBuf {};
    auto r = ::recv(clientSock, recvBuf.data(), static_cast<Transport::Net::SockBufLenType>(recvBuf.size()), 0);
    ASSERT_GT(r, 0);
    const std::string setupResp(recvBuf.data(), static_cast<std::size_t>(r));
    const std::size_t sessPos = setupResp.find("Session: ");
    ASSERT_NE(sessPos, std::string::npos);
    const std::string sessId = setupResp.substr(sessPos + 9U, 8U);

    // 2. PLAY
    const std::string playReq = "PLAY rtsp://127.0.0.1:19554/live RTSP/1.0\r\n"
                                "CSeq: 2\r\n"
                                "Session: "
        + sessId
        + "\r\n"
          "\r\n";
    EXPECT_EQ(::send(clientSock, playReq.data(), static_cast<Transport::Net::SockBufLenType>(playReq.size()), 0),
        static_cast<decltype(::send(0, nullptr, 0, 0))>(playReq.size()));

    r = ::recv(clientSock, recvBuf.data(), static_cast<Transport::Net::SockBufLenType>(recvBuf.size()), 0);
    ASSERT_GT(r, 0);

    // 3. Push video access unit and telemetry
    UasDatalinkMessage msg;
    msg.platformHeadingDeg = 145.0;
    msg.precisionTimeStampUs = 5000000ULL;
    EXPECT_TRUE(engine.pushTelemetry(msg));

    VideoAccessUnit au {};
    au.codec = VideoCodec::H264;
    au.ptsUs = 5000000ULL;
    au.isKeyframe = true;
    au.data = { 0x00U, 0x00U, 0x00U, 0x01U, 0x65U, 0x88U, 0x84U };
    EXPECT_TRUE(engine.pushVideoAccessUnit(std::move(au)));

    engine.flush();

    // 4. Client receives interleaved TS stream
    std::vector<std::uint8_t> streamed;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
    while (streamed.size() < 204U && std::chrono::steady_clock::now() < deadline) {
        Transport::Net::PollFd pfd {};
        pfd.fd = clientSock;
        pfd.events = POLLIN;
        if (Transport::Net::pollSockets(&pfd, 1U, 50) > 0 && (pfd.revents & POLLIN) != 0) {
            std::array<std::uint8_t, 1024> b {};
            const auto bytes = ::recv(clientSock, reinterpret_cast<char*>(b.data()),
                static_cast<Transport::Net::SockBufLenType>(b.size()), 0);
            if (bytes > 0) {
                streamed.insert(streamed.end(), b.begin(), b.begin() + bytes);
            }
        }
    }

    ASSERT_GE(streamed.size(), 204U);
    EXPECT_EQ(streamed[0], 0x24U); // '$'
    EXPECT_EQ(streamed[1], 0x00U); // Channel 0
    EXPECT_EQ(streamed[4], 0x80U); // RTP V=2
    EXPECT_EQ(streamed[5], 33U); // PT=33 MP2T
    EXPECT_EQ(streamed[16], 0x47U); // TS Sync

    Transport::Net::closeSocket(clientSock);
    engine.stop();
}
