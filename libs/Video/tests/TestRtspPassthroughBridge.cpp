/// @file TestRtspPassthroughBridge.cpp
/// @brief Comprehensive unit and mock integration test suite for RTSP Direct Ingest Bridge.

#include <cstddef>
#include <cstdint>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "Klv/StanagBroadcastEngine.h"
#include "Klv/VideoNaluParser.h"
#include "Klv/VideoTypes.h"
#include "RtpDepacketizer.h"
#include "RtspPassthroughBridge.h"
#include "RtspPassthroughTypes.h"
#include "Transport/SocketUtils.h"

using namespace Video;

namespace {

[[nodiscard]] std::vector<std::uint8_t> createRtpHeader(
    std::uint16_t seq, std::uint32_t timestamp, bool marker, std::uint8_t payloadType = 96U)
{
    std::vector<std::uint8_t> pkt(12U, 0U);
    pkt[0] = 0x80U; // V=2, P=0, X=0, CC=0
    pkt[1] = static_cast<std::uint8_t>((marker ? 0x80U : 0x00U) | (payloadType & 0x7FU));
    pkt[2] = static_cast<std::uint8_t>((seq >> 8U) & 0xFFU);
    pkt[3] = static_cast<std::uint8_t>(seq & 0xFFU);
    pkt[4] = static_cast<std::uint8_t>((timestamp >> 24U) & 0xFFU);
    pkt[5] = static_cast<std::uint8_t>((timestamp >> 16U) & 0xFFU);
    pkt[6] = static_cast<std::uint8_t>((timestamp >> 8U) & 0xFFU);
    pkt[7] = static_cast<std::uint8_t>(timestamp & 0xFFU);
    pkt[8] = 0x11U; // SSRC
    pkt[9] = 0x22U;
    pkt[10] = 0x33U;
    pkt[11] = 0x44U;
    return pkt;
}

} // namespace

TEST(TestRtspPassthroughBridge, TestH264SingleNalu)
{
    RtpDepacketizer depack(Klv::VideoCodec::H264);

    std::vector<Klv::VideoAccessUnit> receivedUnits { };
    depack.setUnitCallback([&receivedUnits](const Klv::VideoAccessUnit& au) { receivedUnits.push_back(au); });

    // 1. Send SPS (NAL Type 7) - marker=false
    std::vector<std::uint8_t> spsPkt { createRtpHeader(100U, 90000U, false) };
    const std::vector<std::uint8_t> spsPayload = { 0x67U, 0x42U, 0x00U, 0x1EU, 0x96U, 0x54U };
    spsPkt.insert(spsPkt.end(), spsPayload.begin(), spsPayload.end());
    EXPECT_TRUE(depack.processPacket(spsPkt.data(), spsPkt.size()));

    // 2. Send PPS (NAL Type 8) - marker=false
    std::vector<std::uint8_t> ppsPkt { createRtpHeader(101U, 90000U, false) };
    const std::vector<std::uint8_t> ppsPayload = { 0x68U, 0xCEU, 0x38U, 0x80U };
    ppsPkt.insert(ppsPkt.end(), ppsPayload.begin(), ppsPayload.end());
    EXPECT_TRUE(depack.processPacket(ppsPkt.data(), ppsPkt.size()));

    // 3. Send IDR slice (NAL Type 5) - marker=true
    std::vector<std::uint8_t> idrPkt { createRtpHeader(102U, 90000U, true) };
    const std::vector<std::uint8_t> idrPayload = { 0x65U, 0x88U, 0x84U, 0x00U, 0x10U };
    idrPkt.insert(idrPkt.end(), idrPayload.begin(), idrPayload.end());
    EXPECT_TRUE(depack.processPacket(idrPkt.data(), idrPkt.size()));

    ASSERT_EQ(receivedUnits.size(), 1U);
    const auto& au = receivedUnits[0];
    EXPECT_TRUE(au.isKeyframe);
    EXPECT_GE(au.nalus.size(), 2U);
    EXPECT_EQ(au.codec, Klv::VideoCodec::H264);
}

TEST(TestRtspPassthroughBridge, TestH264StapA)
{
    RtpDepacketizer depack(Klv::VideoCodec::H264);

    std::vector<Klv::VideoAccessUnit> receivedUnits { };
    depack.setUnitCallback([&receivedUnits](const Klv::VideoAccessUnit& au) { receivedUnits.push_back(au); });

    // STAP-A Packet: NAL Type 24
    // Contains SPS (6 bytes) + PPS (4 bytes)
    std::vector<std::uint8_t> stapPkt { createRtpHeader(200U, 180000U, false) };
    stapPkt.push_back(0x38U); // STAP-A indicator: NRI=1, Type=24 (0x20 | 0x18 = 0x38)

    // SPS: size=6
    stapPkt.push_back(0x00U);
    stapPkt.push_back(0x06U);
    const std::vector<std::uint8_t> sps = { 0x67U, 0x42U, 0x00U, 0x1EU, 0x96U, 0x54U };
    stapPkt.insert(stapPkt.end(), sps.begin(), sps.end());

    // PPS: size=4
    stapPkt.push_back(0x00U);
    stapPkt.push_back(0x04U);
    const std::vector<std::uint8_t> pps = { 0x68U, 0xCEU, 0x38U, 0x80U };
    stapPkt.insert(stapPkt.end(), pps.begin(), pps.end());

    EXPECT_TRUE(depack.processPacket(stapPkt.data(), stapPkt.size()));

    // IDR slice with marker=true
    std::vector<std::uint8_t> idrPkt { createRtpHeader(201U, 180000U, true) };
    const std::vector<std::uint8_t> idr = { 0x65U, 0x88U, 0x84U, 0x00U, 0x20U };
    idrPkt.insert(idrPkt.end(), idr.begin(), idr.end());
    EXPECT_TRUE(depack.processPacket(idrPkt.data(), idrPkt.size()));

    ASSERT_EQ(receivedUnits.size(), 1U);
    const auto& au = receivedUnits[0];
    EXPECT_TRUE(au.isKeyframe);
    EXPECT_GE(au.nalus.size(), 3U);
}

TEST(TestRtspPassthroughBridge, TestH264FuAFragment)
{
    RtpDepacketizer depack(Klv::VideoCodec::H264);

    std::vector<Klv::VideoAccessUnit> receivedUnits { };
    depack.setUnitCallback([&receivedUnits](const Klv::VideoAccessUnit& au) { receivedUnits.push_back(au); });

    // Fragmented IDR slice: FU-A Type 28, Original NAL Type 5
    // Frag 1: S=1, E=0
    std::vector<std::uint8_t> pkt1 { createRtpHeader(300U, 270000U, false) };
    pkt1.push_back(0x7CU); // Indicator: F=0, NRI=3, Type=28
    pkt1.push_back(0x85U); // Header: S=1, E=0, R=0, Type=5
    pkt1.push_back(0x88U); // Payload part 1
    pkt1.push_back(0x84U);
    EXPECT_TRUE(depack.processPacket(pkt1.data(), pkt1.size()));

    // Frag 2: S=0, E=0
    std::vector<std::uint8_t> pkt2 { createRtpHeader(301U, 270000U, false) };
    pkt2.push_back(0x7CU); // Indicator
    pkt2.push_back(0x05U); // Header: S=0, E=0, Type=5
    pkt2.push_back(0xAAU); // Payload part 2
    pkt2.push_back(0xBBU);
    EXPECT_TRUE(depack.processPacket(pkt2.data(), pkt2.size()));

    // Frag 3: S=0, E=1, marker=true
    std::vector<std::uint8_t> pkt3 { createRtpHeader(302U, 270000U, true) };
    pkt3.push_back(0x7CU); // Indicator
    pkt3.push_back(0x45U); // Header: S=0, E=1, Type=5
    pkt3.push_back(0xCCU); // Payload part 3
    pkt3.push_back(0xDDU);
    EXPECT_TRUE(depack.processPacket(pkt3.data(), pkt3.size()));

    ASSERT_EQ(receivedUnits.size(), 1U);
    const auto& au = receivedUnits[0];
    EXPECT_TRUE(au.isKeyframe);
    ASSERT_EQ(au.nalus.size(), 1U);
    EXPECT_EQ(au.nalus[0].naluType, 5U);
    // 1 header byte + 6 payload bytes = 7 bytes
    EXPECT_EQ(au.nalus[0].size, 7U);
}

TEST(TestRtspPassthroughBridge, TestH265SingleNalu)
{
    RtpDepacketizer depack(Klv::VideoCodec::H265);

    std::vector<Klv::VideoAccessUnit> receivedUnits { };
    depack.setUnitCallback([&receivedUnits](const Klv::VideoAccessUnit& au) { receivedUnits.push_back(au); });

    // H.265 IDR (Type 19 - IDR_W_RADL): (19 << 1) = 38 = 0x26
    std::vector<std::uint8_t> pkt { createRtpHeader(400U, 360000U, true) };
    pkt.push_back(0x26U); // NAL Type 19
    pkt.push_back(0x01U); // LayerID=0, TID=1
    pkt.push_back(0xAFU); // Payload
    pkt.push_back(0xBFU);

    EXPECT_TRUE(depack.processPacket(pkt.data(), pkt.size()));
    ASSERT_EQ(receivedUnits.size(), 1U);
    const auto& au = receivedUnits[0];
    EXPECT_TRUE(au.isKeyframe);
    ASSERT_EQ(au.nalus.size(), 1U);
    EXPECT_EQ(au.nalus[0].naluType, 19U);
}

TEST(TestRtspPassthroughBridge, TestH265ApAggregation)
{
    RtpDepacketizer depack(Klv::VideoCodec::H265);

    std::vector<Klv::VideoAccessUnit> receivedUnits { };
    depack.setUnitCallback([&receivedUnits](const Klv::VideoAccessUnit& au) { receivedUnits.push_back(au); });

    // AP Packet: Type 48 -> (48 << 1) = 96 = 0x60
    std::vector<std::uint8_t> apPkt { createRtpHeader(500U, 450000U, true) };
    apPkt.push_back(0x60U); // Type 48
    apPkt.push_back(0x01U); // TID=1

    // NALU 1: VPS (Type 32 -> 0x40)
    apPkt.push_back(0x00U);
    apPkt.push_back(0x03U); // len = 3
    apPkt.push_back(0x40U);
    apPkt.push_back(0x01U);
    apPkt.push_back(0x0CU);

    // NALU 2: SPS (Type 33 -> 0x42)
    apPkt.push_back(0x00U);
    apPkt.push_back(0x03U); // len = 3
    apPkt.push_back(0x42U);
    apPkt.push_back(0x01U);
    apPkt.push_back(0x0DU);

    EXPECT_TRUE(depack.processPacket(apPkt.data(), apPkt.size()));
    ASSERT_EQ(receivedUnits.size(), 1U);
    const auto& au = receivedUnits[0];
    EXPECT_EQ(au.nalus.size(), 2U);
    EXPECT_EQ(au.nalus[0].naluType, 32U);
    EXPECT_EQ(au.nalus[1].naluType, 33U);
}

TEST(TestRtspPassthroughBridge, TestH265FuFragment)
{
    RtpDepacketizer depack(Klv::VideoCodec::H265);

    std::vector<Klv::VideoAccessUnit> receivedUnits { };
    depack.setUnitCallback([&receivedUnits](const Klv::VideoAccessUnit& au) { receivedUnits.push_back(au); });

    // H.265 FU: Type 49 -> (49 << 1) = 98 = 0x62
    // Original NAL Type 19 (IDR)
    // Frag 1: S=1, E=0
    std::vector<std::uint8_t> pkt1 { createRtpHeader(600U, 540000U, false) };
    pkt1.push_back(0x62U); // FU header
    pkt1.push_back(0x01U); // TID=1
    pkt1.push_back(0x93U); // FU header: S=1, E=0, FuType=19 (0x80 | 19 = 0x93)
    pkt1.push_back(0x10U);
    pkt1.push_back(0x20U);
    EXPECT_TRUE(depack.processPacket(pkt1.data(), pkt1.size()));

    // Frag 2: S=0, E=1, marker=true
    std::vector<std::uint8_t> pkt2 { createRtpHeader(601U, 540000U, true) };
    pkt2.push_back(0x62U);
    pkt2.push_back(0x01U);
    pkt2.push_back(0x53U); // S=0, E=1, FuType=19 (0x40 | 19 = 0x53)
    pkt2.push_back(0x30U);
    pkt2.push_back(0x40U);
    EXPECT_TRUE(depack.processPacket(pkt2.data(), pkt2.size()));

    ASSERT_EQ(receivedUnits.size(), 1U);
    const auto& au = receivedUnits[0];
    EXPECT_TRUE(au.isKeyframe);
    ASSERT_EQ(au.nalus.size(), 1U);
    EXPECT_EQ(au.nalus[0].naluType, 19U);
}

TEST(TestRtspPassthroughBridge, TestSequenceLossDetection)
{
    RtpDepacketizer depack(Klv::VideoCodec::H264);

    std::vector<std::uint8_t> pkt1 { createRtpHeader(100U, 90000U, false) };
    pkt1.push_back(0x67U); // SPS
    pkt1.push_back(0x42U);
    EXPECT_TRUE(depack.processPacket(pkt1.data(), pkt1.size()));
    EXPECT_EQ(depack.packetLossCount(), 0U);

    // Sequence jump: 100 -> 103 (missing 101, 102)
    std::vector<std::uint8_t> pkt3 { createRtpHeader(103U, 90000U, true) };
    pkt3.push_back(0x68U); // PPS
    pkt3.push_back(0xCEU);
    EXPECT_TRUE(depack.processPacket(pkt3.data(), pkt3.size()));
    EXPECT_EQ(depack.packetLossCount(), 2U);
}

TEST(TestRtspPassthroughBridge, TestTimestampRollover)
{
    RtpDepacketizer depack(Klv::VideoCodec::H264);

    std::vector<std::uint64_t> timestamps { };
    depack.setUnitCallback([&timestamps](const Klv::VideoAccessUnit& au) { timestamps.push_back(au.ptsUs); });

    // Near 32-bit rollover: 0xFFFFFF00
    std::vector<std::uint8_t> pkt1 { createRtpHeader(1U, 0xFFFFFF00U, true) };
    pkt1.push_back(0x65U); // IDR
    pkt1.push_back(0x88U);
    EXPECT_TRUE(depack.processPacket(pkt1.data(), pkt1.size()));

    // Wrap around to 0x00000F00 (advanced by 0x1000 = 4096 ticks)
    std::vector<std::uint8_t> pkt2 { createRtpHeader(2U, 0x00000F00U, true) };
    pkt2.push_back(0x65U); // IDR
    pkt2.push_back(0x88U);
    EXPECT_TRUE(depack.processPacket(pkt2.data(), pkt2.size()));

    ASSERT_EQ(timestamps.size(), 2U);
    EXPECT_GT(timestamps[1], timestamps[0]);
    // 4096 ticks * 1000000 / 90000 = ~45511 us
    const auto diff = timestamps[1] - timestamps[0];
    EXPECT_GE(diff, 45000U);
    EXPECT_LE(diff, 46000U);
}

TEST(TestRtspPassthroughBridge, TestSdpSpropExtraction)
{
    // Valid Base64 SPS and PPS
    // Z0IAHqaAoAt0IA== -> SPS
    // aM44gA== -> PPS
    const std::string sprop { "Z0IAHqaAoAt0IA==,aM44gA==" };
    const auto paramSets = RtpDepacketizer::parseSprop(sprop);
    ASSERT_EQ(paramSets.size(), 2U);

    RtpDepacketizer depack(Klv::VideoCodec::H264);
    depack.setParameterSets(paramSets);

    std::vector<Klv::VideoAccessUnit> receivedUnits { };
    depack.setUnitCallback([&receivedUnits](const Klv::VideoAccessUnit& au) { receivedUnits.push_back(au); });

    // IDR slice without SPS/PPS in-band
    std::vector<std::uint8_t> idrPkt { createRtpHeader(10U, 1000U, true) };
    idrPkt.push_back(0x65U);
    idrPkt.push_back(0x88U);
    EXPECT_TRUE(depack.processPacket(idrPkt.data(), idrPkt.size()));

    ASSERT_EQ(receivedUnits.size(), 1U);
    const auto& au = receivedUnits[0];
    // Must contain injected SPS and PPS before the IDR slice
    EXPECT_TRUE(au.isKeyframe);
    EXPECT_GE(au.nalus.size(), 3U);
    EXPECT_EQ(au.nalus[0].naluType, 7U); // SPS
    EXPECT_EQ(au.nalus[1].naluType, 8U); // PPS
    EXPECT_EQ(au.nalus[2].naluType, 5U); // IDR
}

TEST(TestRtspPassthroughBridge, TestRtspHandshakeMock)
{
    Transport::Net::ensureWinsockInitialized();

    // Setup Mock RTSP Server on local loopback
    Transport::Net::SocketHandle listenSock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_NE(listenSock, Transport::Net::InvalidSocket);

    struct sockaddr_in addr { };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; // Bind to ephemeral port

    ASSERT_EQ(::bind(listenSock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)), 0);
    ASSERT_EQ(::listen(listenSock, 1), 0);

    socklen_t addrLen = sizeof(addr);
    ASSERT_EQ(::getsockname(listenSock, reinterpret_cast<struct sockaddr*>(&addr), &addrLen), 0);
    const auto port = ntohs(addr.sin_port);

    std::atomic<bool> serverStop { false };
    std::thread serverThread([listenSock, &serverStop]() {
        Transport::Net::PollFd pfd { };
        pfd.fd = listenSock;
        pfd.events = POLLIN;

        if (Transport::Net::pollSockets(&pfd, 1U, 2000) <= 0) {
            Transport::Net::closeSocket(listenSock);
            return;
        }

        struct sockaddr_in clientAddr { };
        socklen_t clientLen = sizeof(clientAddr);
        Transport::Net::SocketHandle clientSock
            = ::accept(listenSock, reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);
        if (clientSock == Transport::Net::InvalidSocket) {
            Transport::Net::closeSocket(listenSock);
            return;
        }

        char buf[2048] { };
        while (!serverStop.load()) {
            pfd.fd = clientSock;
            pfd.events = POLLIN;
            if (Transport::Net::pollSockets(&pfd, 1U, 200) <= 0) {
                continue;
            }

            const auto ret = ::recv(clientSock, buf, sizeof(buf) - 1, 0);
            if (ret <= 0) {
                break;
            }
            buf[ret] = '\0';
            std::string req(buf);

            if (req.find("OPTIONS") != std::string::npos) {
                const std::string resp
                    = "RTSP/1.0 200 OK\r\nCSeq: 1\r\nPublic: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN\r\n\r\n";
                ::send(clientSock, resp.c_str(), static_cast<Transport::Net::SockBufLenType>(resp.size()), 0);
            } else if (req.find("DESCRIBE") != std::string::npos) {
                const std::string sdp
                    = "v=0\r\nm=video 0 RTP/AVP 96\r\na=rtpmap:96 H264/90000\r\na=control:trackID=1\r\n";
                const std::string resp
                    = "RTSP/1.0 200 OK\r\nCSeq: 2\r\nContent-Type: application/sdp\r\nContent-Length: "
                    + std::to_string(sdp.size()) + "\r\n\r\n" + sdp;
                ::send(clientSock, resp.c_str(), static_cast<Transport::Net::SockBufLenType>(resp.size()), 0);
            } else if (req.find("SETUP") != std::string::npos) {
                const std::string resp = "RTSP/1.0 200 OK\r\nCSeq: 3\r\nSession: 12345678;timeout=60\r\nTransport: "
                                         "RTP/AVP/TCP;unicast;interleaved=0-1\r\n\r\n";
                ::send(clientSock, resp.c_str(), static_cast<Transport::Net::SockBufLenType>(resp.size()), 0);
            } else if (req.find("PLAY") != std::string::npos) {
                const std::string resp = "RTSP/1.0 200 OK\r\nCSeq: 4\r\nSession: 12345678\r\n\r\n";
                ::send(clientSock, resp.c_str(), static_cast<Transport::Net::SockBufLenType>(resp.size()), 0);

                // Send 1 interleaved RTP packet ($ 00 <len> <rtp>)
                std::vector<std::uint8_t> rtp { createRtpHeader(1U, 90000U, true) };
                rtp.push_back(0x65U); // IDR
                rtp.push_back(0x88U);

                std::vector<std::uint8_t> frame { };
                frame.push_back(0x24U); // '$'
                frame.push_back(0x00U); // channel 0
                frame.push_back(static_cast<std::uint8_t>((rtp.size() >> 8U) & 0xFFU));
                frame.push_back(static_cast<std::uint8_t>(rtp.size() & 0xFFU));
                frame.insert(frame.end(), rtp.begin(), rtp.end());

                ::send(clientSock, reinterpret_cast<const char*>(frame.data()),
                    static_cast<Transport::Net::SockBufLenType>(frame.size()), 0);
            } else if (req.find("TEARDOWN") != std::string::npos) {
                const std::string resp = "RTSP/1.0 200 OK\r\nCSeq: 5\r\n\r\n";
                ::send(clientSock, resp.c_str(), static_cast<Transport::Net::SockBufLenType>(resp.size()), 0);
                break;
            }
        }

        Transport::Net::closeSocket(clientSock);
        Transport::Net::closeSocket(listenSock);
    });

    RtspPassthroughConfig config { };
    config.rtspUrl = "rtsp://127.0.0.1:" + std::to_string(port) + "/live";
    config.autoReconnect = false;
    config.connectTimeoutMs = 1500U;

    RtspPassthroughBridge bridge(config);
    std::atomic<bool> auReceived { false };
    bridge.setUnitCallback([&auReceived](const Klv::VideoAccessUnit& au) {
        if (au.isKeyframe) {
            auReceived.store(true);
        }
    });

    EXPECT_TRUE(bridge.start());

    // Wait for handshake and frame delivery
    for (int i { 0 }; i < 30; ++i) {
        if (auReceived.load()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    EXPECT_TRUE(auReceived.load());
    const auto stats = bridge.stats();
    EXPECT_GE(stats.rtpPacketsReceived, 1U);

    bridge.stop();
    serverStop.store(true);
    if (serverThread.joinable()) {
        serverThread.join();
    }
}

TEST(TestRtspPassthroughBridge, TestEngineIntegration)
{
    // Verify emitted VideoAccessUnit can be passed directly to StanagBroadcastEngine
    Klv::StanagBroadcastConfig config { };
    config.enableNetworkPublishing = false;
    Klv::StanagBroadcastEngine engine(config);
    EXPECT_TRUE(engine.start());

    RtpDepacketizer depack(Klv::VideoCodec::H264);
    depack.setUnitCallback([&engine](const Klv::VideoAccessUnit& au) { EXPECT_TRUE(engine.pushVideoAccessUnit(au)); });

    std::vector<std::uint8_t> pkt { createRtpHeader(1U, 90000U, true) };
    pkt.push_back(0x65U); // IDR
    pkt.push_back(0x88U);
    pkt.push_back(0x84U);
    EXPECT_TRUE(depack.processPacket(pkt.data(), pkt.size()));

    engine.flush();
    const auto stats = engine.stats();
    EXPECT_EQ(stats.videoFramesIngested, 1U);

    engine.stop();
}
