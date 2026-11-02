/// @file TestKlvRtpPublisher.cpp
/// @brief GoogleTest unit test suite for RFC 6597 Standalone KLV over RTP publishing.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <chrono>
#include <numeric>
#include <string>
#include <vector>

#include "KlvRtpPacketizer.h"
#include "KlvRtpPublisher.h"
#include "KlvRtpTypes.h"
#include "SdpGenerator.h"
#include "SocketUtils.h"

namespace Transport {
namespace {

    TEST(TestKlvRtpPublisher, RtpTypesAndTimestampConversion)
    {
        // Test RtpSequenceNumber
        RtpSequenceNumber seq0 {};
        EXPECT_EQ(seq0.value(), 0U);
        EXPECT_EQ(seq0.next().value(), 1U);

        RtpSequenceNumber seqMax(65535U);
        EXPECT_EQ(seqMax.next().value(), 0U); // Safe 16-bit rollover

        // Test RtpTimestamp90kHz
        RtpTimestamp90kHz ts0 {};
        EXPECT_EQ(ts0.value(), 0U);

        // 1 second (1,000,000 us) at 90 kHz = 90,000 ticks
        const auto ts1s = RtpTimestamp90kHz::fromMicroseconds(1000000ULL);
        EXPECT_EQ(ts1s.value(), 90000U);

        // 2 seconds at 90 kHz = 180,000 ticks
        const auto ts2s = RtpTimestamp90kHz::fromMicroseconds(2000000ULL);
        EXPECT_EQ(ts2s.value(), 180000U);

        // Fractional timestamp: 33,333 us (~30 fps) -> floor(33333 * 9 / 100) = 2999
        const auto ts33ms = RtpTimestamp90kHz::fromMicroseconds(33333ULL);
        EXPECT_EQ(ts33ms.value(), 2999U);

        // Test SsrcId
        SsrcId defaultSsrc {};
        EXPECT_EQ(defaultSsrc.value(), 0x12345678U);
        SsrcId customSsrc(0xAABBCCDDU);
        EXPECT_EQ(customSsrc.value(), 0xAABBCCDDU);

        // Test DynamicPayloadType
        DynamicPayloadType defaultPt {};
        EXPECT_EQ(defaultPt.value(), 96U);
        DynamicPayloadType customPt(100U);
        EXPECT_EQ(customPt.value(), 100U);
        DynamicPayloadType invalidPt(33U); // Outside 96-127 range, fallbacks to 96
        EXPECT_EQ(invalidPt.value(), 96U);
    }

    TEST(TestKlvRtpPublisher, PacketizerSingleUnfragmentedPacket)
    {
        KlvRtpPacketizer packetizer(DynamicPayloadType(96U), SsrcId(0x11223344U), 1400U);
        EXPECT_EQ(packetizer.maxPayloadSize(), 1400U);
        EXPECT_EQ(packetizer.payloadType().value(), 96U);
        EXPECT_EQ(packetizer.ssrc().value(), 0x11223344U);

        // Create 128-byte synthetic KLV payload
        std::vector<std::uint8_t> klvData(128U);
        std::iota(klvData.begin(), klvData.end(), static_cast<std::uint8_t>(0x10U));

        RtpSequenceNumber seq(1000U);
        const RtpTimestamp90kHz ts(90000U);

        std::vector<std::vector<std::uint8_t>> emittedPackets;
        const std::size_t count = packetizer.packetize(
            klvData.data(), klvData.size(), ts, seq, [&emittedPackets](const std::uint8_t* data, std::size_t size) {
                emittedPackets.emplace_back(data, data + size);
            });

        EXPECT_EQ(count, 1U);
        ASSERT_EQ(emittedPackets.size(), 1U);
        EXPECT_EQ(seq.value(), 1001U); // Advanced by 1

        const auto& pkt = emittedPackets[0];
        ASSERT_EQ(pkt.size(), KlvRtpPacketizer::kRtpHeaderSize + klvData.size());

        // Verify 12-byte RFC 3550 RTP Header
        EXPECT_EQ(pkt[0], 0x80U); // V=2, P=0, X=0, CC=0
        EXPECT_EQ(pkt[1], 0xE0U); // M=1 (final/unfragmented), PT=96
        EXPECT_EQ(pkt[2], 0x03U); // Sequence 1000 (0x03E8)
        EXPECT_EQ(pkt[3], 0xE8U);
        EXPECT_EQ(pkt[4], 0x00U); // Timestamp 90000 (0x00015F90)
        EXPECT_EQ(pkt[5], 0x01U);
        EXPECT_EQ(pkt[6], 0x5FU);
        EXPECT_EQ(pkt[7], 0x90U);
        EXPECT_EQ(pkt[8], 0x11U); // SSRC 0x11223344
        EXPECT_EQ(pkt[9], 0x22U);
        EXPECT_EQ(pkt[10], 0x33U);
        EXPECT_EQ(pkt[11], 0x44U);

        // Verify payload slice
        EXPECT_TRUE(std::memcmp(pkt.data() + KlvRtpPacketizer::kRtpHeaderSize, klvData.data(), klvData.size()) == 0);
    }

    TEST(TestKlvRtpPublisher, PacketizerFragmentationMultiPacket)
    {
        // Max payload size 1000 bytes
        KlvRtpPacketizer packetizer(DynamicPayloadType(96U), SsrcId(0xDEADBEEFU), 1000U);

        // Create 2500-byte synthetic KLV payload (will produce 3 fragments: 1000, 1000, 500)
        std::vector<std::uint8_t> klvData(2500U);
        for (std::size_t i = 0U; i < klvData.size(); ++i) {
            klvData[i] = static_cast<std::uint8_t>(i & 0xFFU);
        }

        RtpSequenceNumber seq(50U);
        const RtpTimestamp90kHz ts(180000U);

        std::vector<std::vector<std::uint8_t>> emittedPackets;
        const std::size_t count = packetizer.packetize(
            klvData.data(), klvData.size(), ts, seq, [&emittedPackets](const std::uint8_t* data, std::size_t size) {
                emittedPackets.emplace_back(data, data + size);
            });

        EXPECT_EQ(count, 3U);
        ASSERT_EQ(emittedPackets.size(), 3U);
        EXPECT_EQ(seq.value(), 53U); // Advanced by 3

        // Fragment 0: 1000 bytes payload, M=0, Seq=50
        EXPECT_EQ(emittedPackets[0].size(), 12U + 1000U);
        EXPECT_EQ(emittedPackets[0][1], 0x60U); // M=0, PT=96
        EXPECT_EQ(emittedPackets[0][3], 50U);

        // Fragment 1: 1000 bytes payload, M=0, Seq=51
        EXPECT_EQ(emittedPackets[1].size(), 12U + 1000U);
        EXPECT_EQ(emittedPackets[1][1], 0x60U); // M=0, PT=96
        EXPECT_EQ(emittedPackets[1][3], 51U);

        // Fragment 2: 500 bytes payload, M=1, Seq=52
        EXPECT_EQ(emittedPackets[2].size(), 12U + 500U);
        EXPECT_EQ(emittedPackets[2][1], 0xE0U); // M=1, PT=96
        EXPECT_EQ(emittedPackets[2][3], 52U);

        // Reconstruct full payload and verify bit-for-bit identity
        std::vector<std::uint8_t> reconstructed;
        for (const auto& pkt : emittedPackets) {
            reconstructed.insert(reconstructed.end(),
                pkt.begin() + static_cast<std::ptrdiff_t>(KlvRtpPacketizer::kRtpHeaderSize), pkt.end());
        }

        ASSERT_EQ(reconstructed.size(), klvData.size());
        EXPECT_EQ(reconstructed, klvData);
    }

    TEST(TestKlvRtpPublisher, PacketizerEdgeCases)
    {
        KlvRtpPacketizer packetizer(DynamicPayloadType(96U), SsrcId(0x12345678U), 1400U);
        RtpSequenceNumber seq(0U);

        // Null or zero size
        EXPECT_EQ(packetizer.packetize(nullptr, 100U, RtpTimestamp90kHz(0U), seq, nullptr), 0U);
        const std::uint8_t dummy = 0xFFU;
        EXPECT_EQ(packetizer.packetize(&dummy, 0U, RtpTimestamp90kHz(0U), seq, nullptr), 0U);

        // Exact multiple of max payload size (e.g. 2000 bytes with max 1000)
        KlvRtpPacketizer packetizer1000(DynamicPayloadType(96U), SsrcId(0x12345678U), 1000U);
        std::vector<std::uint8_t> data2000(2000U, 0xABU);
        std::vector<std::vector<std::uint8_t>> pkts;

        const std::size_t count = packetizer1000.packetize(data2000.data(), data2000.size(), RtpTimestamp90kHz(100U),
            seq, [&pkts](const std::uint8_t* d, std::size_t s) { pkts.emplace_back(d, d + s); });

        EXPECT_EQ(count, 2U);
        ASSERT_EQ(pkts.size(), 2U);
        EXPECT_EQ(pkts[0][1], 0x60U); // M=0
        EXPECT_EQ(pkts[1][1], 0xE0U); // M=1
    }

    TEST(TestKlvRtpPublisher, SdpGeneration)
    {
        KlvRtpConfig config {};
        config.destinationIp = "239.255.0.1";
        config.destinationPort = 5004U;
        config.payloadType = DynamicPayloadType(96U);
        config.timeToLive = 32U;
        config.sessionName = "Test Tactical KLV";

        const std::string sdp = SdpGenerator::generateKlvRtpSdp(config);

        EXPECT_NE(sdp.find("v=0"), std::string::npos);
        EXPECT_NE(sdp.find("s=Test Tactical KLV"), std::string::npos);
        EXPECT_NE(sdp.find("c=IN IP4 239.255.0.1/32"), std::string::npos);
        EXPECT_NE(sdp.find("m=video 5004 RTP/AVP 96"), std::string::npos);
        EXPECT_NE(sdp.find("a=rtpmap:96 smpte336m/90000"), std::string::npos);
        EXPECT_NE(sdp.find("a=recvonly"), std::string::npos);

        // Test publisher delegation
        KlvRtpPublisher publisher(config);
        EXPECT_EQ(publisher.generateSdp(), sdp);
    }

    TEST(TestKlvRtpPublisher, PublisherLifecycleAndReconfiguration)
    {
        KlvRtpConfig config {};
        config.destinationIp = "127.0.0.1";
        config.destinationPort = 15998U;

        KlvRtpPublisher publisher(config);
        EXPECT_FALSE(publisher.isRunning());

        EXPECT_TRUE(publisher.start());
        EXPECT_TRUE(publisher.isRunning());

        // Idempotent start
        EXPECT_TRUE(publisher.start());
        EXPECT_TRUE(publisher.isRunning());

        // Dynamic reconfiguration
        config.destinationPort = 15999U;
        EXPECT_TRUE(publisher.setConfig(config));
        EXPECT_EQ(publisher.config().destinationPort, 15999U);
        EXPECT_TRUE(publisher.isRunning());

        publisher.stop();
        EXPECT_FALSE(publisher.isRunning());
    }

    TEST(TestKlvRtpPublisher, LocalLoopbackDatagramTransmission)
    {
        Net::ensureWinsockInitialized();

        // Create a local UDP listener socket to capture emitted KLV RTP datagrams
        const Net::SocketHandle recvSock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        ASSERT_NE(recvSock, Net::InvalidSocket);

        struct sockaddr_in bindAddr { };
        bindAddr.sin_family = AF_INET;
        bindAddr.sin_port = htons(15984U);
        bindAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        const int bindRes = ::bind(recvSock, reinterpret_cast<const struct sockaddr*>(&bindAddr), sizeof(bindAddr));
        ASSERT_EQ(bindRes, 0);

        // Set receive timeout of 1000 ms
#ifdef _WIN32
        const DWORD timeoutMs = 1000UL;
        (void)::setsockopt(
            recvSock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
#else
        struct timeval tv { };
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        (void)::setsockopt(recvSock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

        // Setup publisher targeting listener
        KlvRtpConfig pubConfig {};
        pubConfig.destinationIp = "127.0.0.1";
        pubConfig.destinationPort = 15984U;
        pubConfig.maxPayloadSize = 500U; // Small MTU to test fragmentation over socket

        KlvRtpPublisher publisher(pubConfig);
        EXPECT_TRUE(publisher.start());

        // Send a 1200-byte synthetic KLV payload -> should fragment into 3 datagrams (500, 500, 200)
        std::vector<std::uint8_t> payload(1200U);
        std::iota(payload.begin(), payload.end(), static_cast<std::uint8_t>(0x01U));

        const bool sent = publisher.sendKlvPacket(payload.data(), payload.size(), 1000000ULL);
        EXPECT_TRUE(sent);

        // Receive the 3 datagrams
        std::vector<std::uint8_t> rxBuffer(2048U);
        std::vector<std::uint8_t> reassembled;

        for (std::size_t i = 0U; i < 3U; ++i) {
#ifdef _WIN32
            const int bytesRead
                = ::recv(recvSock, reinterpret_cast<char*>(rxBuffer.data()), static_cast<int>(rxBuffer.size()), 0);
#else
            const ssize_t bytesRead = ::recv(recvSock, rxBuffer.data(), rxBuffer.size(), 0);
#endif
            ASSERT_GT(bytesRead, 12);

            // Check RTP Header
            EXPECT_EQ(rxBuffer[0], 0x80U);
            const bool isLast = (i == 2U);
            EXPECT_EQ(rxBuffer[1], isLast ? 0xE0U : 0x60U); // M bit

            reassembled.insert(reassembled.end(),
                rxBuffer.begin() + static_cast<std::ptrdiff_t>(KlvRtpPacketizer::kRtpHeaderSize),
                rxBuffer.begin() + bytesRead);
        }

        ASSERT_EQ(reassembled.size(), payload.size());
        EXPECT_EQ(reassembled, payload);

        // Verify stats
        const KlvRtpStats stats = publisher.stats();
        EXPECT_EQ(stats.messagesSent, 1U);
        EXPECT_EQ(stats.packetsSent, 3U);
        EXPECT_EQ(stats.fragmentsSent, 2U);
        EXPECT_EQ(stats.bytesSent, (3U * 12U) + 1200U);

        publisher.stop();
        Net::closeSocket(recvSock);
    }

} // namespace
} // namespace Transport
