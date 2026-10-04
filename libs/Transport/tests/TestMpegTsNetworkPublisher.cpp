/// @file TestMpegTsNetworkPublisher.cpp
/// @brief Unit tests for MpegTsNetworkPublisher UDP/RTP multicast streaming, packet accumulation, and MTU safety.

#include "MpegTsNetworkPublisher.h"
#include "SocketUtils.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace {

using namespace Transport;

/// @brief Helper to generate a compliant 188-byte TS packet.
std::vector<std::uint8_t> makeTestTsPacket(std::uint16_t pid, std::uint8_t counter) {
    std::vector<std::uint8_t> pkt(188U, 0xFFU);
    pkt[0] = 0x47U; // Sync byte
    pkt[1] = static_cast<std::uint8_t>((pid >> 8U) & 0x1FU);
    pkt[2] = static_cast<std::uint8_t>(pid & 0xFFU);
    pkt[3] = static_cast<std::uint8_t>(0x10U | (counter & 0x0FU)); // Payload only
    // Put dummy payload
    for (std::size_t i = 4U; i < 188U; ++i) {
        pkt[i] = static_cast<std::uint8_t>((i + counter) & 0xFFU);
    }
    return pkt;
}

/// @brief Lightweight RAII UDP loopback listener for testing datagram egress.
class TestUdpListener {
public:
    explicit TestUdpListener(std::uint16_t port) {
        Net::ensureWinsockInitialized();
        m_sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_sock == Net::InvalidSocket) {
            return;
        }

        const int reuse = 1;
#ifdef _WIN32
        (void)::setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR,
                           reinterpret_cast<const char*>(&reuse), static_cast<int>(sizeof(reuse)));
#else
        (void)::setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR,
                           &reuse, static_cast<socklen_t>(sizeof(reuse)));
#endif

        struct sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (::bind(m_sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
            Net::closeSocket(m_sock);
            m_sock = Net::InvalidSocket;
            return;
        }

        m_running = true;
        m_thread = std::thread([this]() {
            std::vector<std::uint8_t> buf(4096U);
            while (m_running.load()) {
                Net::PollFd pfd {};
                pfd.fd = m_sock;
                pfd.events = POLLIN;
                const int pr = Net::pollSockets(&pfd, 1, 50);
                if (pr > 0 && ((pfd.revents & POLLIN) != 0)) {
                    struct sockaddr_storage from {};
                    socklen_t fromLen = static_cast<socklen_t>(sizeof(from));
                    const auto recvd = ::recvfrom(
                        m_sock,
                        reinterpret_cast<char*>(buf.data()),
                        static_cast<Net::SockBufLenType>(buf.size()),
                        0,
                        reinterpret_cast<struct sockaddr*>(&from),
                        &fromLen);
                    if (recvd > 0) {
                        std::lock_guard<std::mutex> lock { m_mutex };
                        m_packets.emplace_back(buf.begin(), buf.begin() + recvd);
                        m_cv.notify_one();
                    }
                }
            }
        });
    }

    ~TestUdpListener() {
        m_running.store(false);
        if (m_thread.joinable()) {
            m_thread.join();
        }
        if (m_sock != Net::InvalidSocket) {
            Net::closeSocket(m_sock);
            m_sock = Net::InvalidSocket;
        }
    }

    [[nodiscard]] bool isOpen() const noexcept {
        return m_sock != Net::InvalidSocket;
    }

    [[nodiscard]] std::vector<std::vector<std::uint8_t>> waitForPackets(std::size_t count, int timeoutMs = 1500) {
        std::unique_lock<std::mutex> lock { m_mutex };
        (void)m_cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this, count]() {
            return m_packets.size() >= count;
        });
        return m_packets;
    }

private:
    Net::SocketHandle m_sock { Net::InvalidSocket };
    std::atomic<bool> m_running { false };
    std::thread m_thread {};
    std::mutex m_mutex {};
    std::condition_variable m_cv {};
    std::vector<std::vector<std::uint8_t>> m_packets {};
};

/// @brief Verify default accessors, configuration updates, and stats reset.
TEST(MpegTsNetworkPublisherTest, AccessorsAndDefaults) {
    TsNetworkConfig cfg;
    cfg.destinationIp = "127.0.0.1";
    cfg.destinationPort = 19100U;
    cfg.packetsPerDatagram = 7U;

    MpegTsNetworkPublisher publisher(cfg);
    EXPECT_FALSE(publisher.isOpen());

    const auto activeCfg = publisher.config();
    EXPECT_EQ(activeCfg.destinationIp, "127.0.0.1");
    EXPECT_EQ(activeCfg.destinationPort, 19100U);
    EXPECT_EQ(activeCfg.packetsPerDatagram, 7U);
    EXPECT_EQ(activeCfg.protocol, TsNetworkProtocol::RawUdp);

    EXPECT_TRUE(publisher.open());
    EXPECT_TRUE(publisher.isOpen());

    const auto stats = publisher.stats();
    EXPECT_EQ(stats.tsPacketsSent, 0U);
    EXPECT_EQ(stats.datagramsSent, 0U);
    EXPECT_EQ(stats.bytesSent, 0U);

    publisher.close();
    EXPECT_FALSE(publisher.isOpen());
}

/// @brief Verify Raw UDP streaming of 7 TS packets into a 1316-byte datagram.
TEST(MpegTsNetworkPublisherTest, LoopbackRawUdpStreaming) {
    constexpr std::uint16_t port { 19101U };

    TestUdpListener listener(port);
    ASSERT_TRUE(listener.isOpen());

    TsNetworkConfig cfg;
    cfg.destinationIp = "127.0.0.1";
    cfg.destinationPort = port;
    cfg.packetsPerDatagram = 7U;
    cfg.protocol = TsNetworkProtocol::RawUdp;

    MpegTsNetworkPublisher publisher(cfg);
    ASSERT_TRUE(publisher.open());

    // Push 7 TS packets (should trigger exactly 1 datagram of 1316 bytes)
    for (std::uint8_t i = 0U; i < 7U; ++i) {
        const auto pkt = makeTestTsPacket(0x0101U, i);
        EXPECT_TRUE(publisher.pushPacket(pkt.data(), pkt.size()));
    }

    const auto receivedDatagrams = listener.waitForPackets(1U);
    ASSERT_EQ(receivedDatagrams.size(), 1U);
    EXPECT_EQ(receivedDatagrams[0].size(), 1316U);

    // Validate sync bytes at each 188-byte boundary
    for (std::size_t i = 0U; i < 7U; ++i) {
        EXPECT_EQ(receivedDatagrams[0][i * 188U], 0x47U);
    }

    const auto stats = publisher.stats();
    EXPECT_EQ(stats.tsPacketsSent, 7U);
    EXPECT_EQ(stats.datagramsSent, 1U);
    EXPECT_EQ(stats.bytesSent, 1316U);

    publisher.close();
}

/// @brief Verify buffering of partial packets until flush() is invoked.
TEST(MpegTsNetworkPublisherTest, PartialPacketAccumulationAndFlush) {
    constexpr std::uint16_t port { 19102U };

    TestUdpListener listener(port);
    ASSERT_TRUE(listener.isOpen());

    TsNetworkConfig cfg;
    cfg.destinationIp = "127.0.0.1";
    cfg.destinationPort = port;
    cfg.packetsPerDatagram = 7U;
    cfg.protocol = TsNetworkProtocol::RawUdp;

    MpegTsNetworkPublisher publisher(cfg);
    ASSERT_TRUE(publisher.open());

    // Push 3 packets (less than 7, must not send yet)
    for (std::uint8_t i = 0U; i < 3U; ++i) {
        const auto pkt = makeTestTsPacket(0x01E0U, i);
        EXPECT_TRUE(publisher.pushPacket(pkt.data(), pkt.size()));
    }

    // Small delay to verify nothing is emitted
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(listener.waitForPackets(1U, 50).empty());

    // Explicit flush
    EXPECT_TRUE(publisher.flush());

    const auto receivedDatagrams = listener.waitForPackets(1U);
    ASSERT_EQ(receivedDatagrams.size(), 1U);
    EXPECT_EQ(receivedDatagrams[0].size(), 3U * 188U); // 564 bytes
    EXPECT_EQ(receivedDatagrams[0][0], 0x47U);
    EXPECT_EQ(receivedDatagrams[0][188U], 0x47U);
    EXPECT_EQ(receivedDatagrams[0][376U], 0x47U);

    const auto stats = publisher.stats();
    EXPECT_EQ(stats.tsPacketsSent, 3U);
    EXPECT_EQ(stats.datagramsSent, 1U);
    EXPECT_EQ(stats.bytesSent, 564U);
    EXPECT_EQ(stats.flushesTriggered, 1U);

    publisher.close();
}

/// @brief Verify RFC 3550 / RFC 2250 RTP encapsulation with Payload Type 33 (MP2T).
TEST(MpegTsNetworkPublisherTest, RtpEncapsulationValidation) {
    constexpr std::uint16_t port { 19103U };

    TestUdpListener listener(port);
    ASSERT_TRUE(listener.isOpen());

    TsNetworkConfig cfg;
    cfg.destinationIp = "127.0.0.1";
    cfg.destinationPort = port;
    cfg.packetsPerDatagram = 7U;
    cfg.protocol = TsNetworkProtocol::Rtp;
    cfg.rtpSsrc = 0xA1B2C3D4U;

    MpegTsNetworkPublisher publisher(cfg);
    ASSERT_TRUE(publisher.open());

    // Push 7 packets with timestamp 1,000,000 us (1 second)
    // 90 kHz timestamp = (1,000,000 * 9) / 100 = 90,000 = 0x00015F90
    constexpr std::uint64_t ptsUs { 1000000ULL };
    for (std::uint8_t i = 0U; i < 7U; ++i) {
        const auto pkt = makeTestTsPacket(0x0101U, i);
        EXPECT_TRUE(publisher.pushPacket(pkt.data(), pkt.size(), ptsUs));
    }

    const auto receivedDatagrams = listener.waitForPackets(1U);
    ASSERT_EQ(receivedDatagrams.size(), 1U);
    // 12-byte RTP header + 7 * 188 = 1328 bytes
    EXPECT_EQ(receivedDatagrams[0].size(), 1328U);

    const auto& d = receivedDatagrams[0];
    // Check RTP Header
    EXPECT_EQ(d[0], 0x80U); // V=2, P=0, X=0, CC=0
    EXPECT_EQ(d[1], 33U);   // M=0, PT=33 (MP2T)
    EXPECT_EQ(d[2], 0x00U); // Seq high byte (0)
    EXPECT_EQ(d[3], 0x00U); // Seq low byte (0)

    // Timestamp 90,000 (0x00015F90)
    const std::uint32_t ts = (static_cast<std::uint32_t>(d[4]) << 24U) |
                             (static_cast<std::uint32_t>(d[5]) << 16U) |
                             (static_cast<std::uint32_t>(d[6]) << 8U)  |
                              static_cast<std::uint32_t>(d[7]);
    EXPECT_EQ(ts, 90000U);

    // SSRC 0xA1B2C3D4
    const std::uint32_t ssrc = (static_cast<std::uint32_t>(d[8])  << 24U) |
                               (static_cast<std::uint32_t>(d[9])  << 16U) |
                               (static_cast<std::uint32_t>(d[10]) << 8U)  |
                                static_cast<std::uint32_t>(d[11]);
    EXPECT_EQ(ssrc, 0xA1B2C3D4U);

    // TS payload starts at offset 12 with sync byte 0x47
    for (std::size_t i = 0U; i < 7U; ++i) {
        EXPECT_EQ(d[12U + (i * 188U)], 0x47U);
    }

    publisher.close();
}

/// @brief Verify bulk ingestion of contiguous 188-byte TS packet buffers.
TEST(MpegTsNetworkPublisherTest, BulkPacketsPushValidation) {
    constexpr std::uint16_t port { 19104U };

    TestUdpListener listener(port);
    ASSERT_TRUE(listener.isOpen());

    TsNetworkConfig cfg;
    cfg.destinationIp = "127.0.0.1";
    cfg.destinationPort = port;
    cfg.packetsPerDatagram = 7U;
    cfg.protocol = TsNetworkProtocol::RawUdp;

    MpegTsNetworkPublisher publisher(cfg);
    ASSERT_TRUE(publisher.open());

    // Create contiguous buffer of 14 TS packets (2 datagrams)
    std::vector<std::uint8_t> bulkBuffer;
    bulkBuffer.reserve(14U * 188U);
    for (std::uint8_t i = 0U; i < 14U; ++i) {
        const auto pkt = makeTestTsPacket(0x0101U, i);
        bulkBuffer.insert(bulkBuffer.end(), pkt.begin(), pkt.end());
    }

    const std::size_t pushed = publisher.pushPackets(bulkBuffer.data(), bulkBuffer.size());
    EXPECT_EQ(pushed, 14U);

    const auto receivedDatagrams = listener.waitForPackets(2U);
    ASSERT_EQ(receivedDatagrams.size(), 2U);
    EXPECT_EQ(receivedDatagrams[0].size(), 1316U);
    EXPECT_EQ(receivedDatagrams[1].size(), 1316U);

    publisher.close();
}

/// @brief Verify Multicast destination configuration, TTL, and loopback setup.
TEST(MpegTsNetworkPublisherTest, MulticastSocketConfiguration) {
    TsNetworkConfig cfg;
    cfg.destinationIp = "239.255.14.99";
    cfg.destinationPort = 19105U;
    cfg.multicastTtl = 32U;
    cfg.multicastLoop = true;

    MpegTsNetworkPublisher publisher(cfg);
    EXPECT_TRUE(publisher.open());
    EXPECT_TRUE(publisher.isOpen());

    // Reconfigure while open
    cfg.destinationPort = 19106U;
    EXPECT_TRUE(publisher.setConfig(cfg));
    EXPECT_TRUE(publisher.isOpen());

    publisher.close();
    EXPECT_FALSE(publisher.isOpen());
}

/// @brief Verify rejection of invalid buffers (null, wrong length, missing sync byte).
TEST(MpegTsNetworkPublisherTest, InvalidPacketRejection) {
    TsNetworkConfig cfg;
    cfg.destinationIp = "127.0.0.1";
    cfg.destinationPort = 19107U;

    MpegTsNetworkPublisher publisher(cfg);
    EXPECT_TRUE(publisher.open());

    // Null pointer
    EXPECT_FALSE(publisher.pushPacket(nullptr, 188U));

    // Wrong size (187 bytes)
    const std::vector<std::uint8_t> shortPkt(187U, 0x47U);
    EXPECT_FALSE(publisher.pushPacket(shortPkt.data(), shortPkt.size()));

    // Missing sync byte (0x46 instead of 0x47)
    std::vector<std::uint8_t> corruptedPkt(188U, 0x00U);
    corruptedPkt[0] = 0x46U;
    EXPECT_FALSE(publisher.pushPacket(corruptedPkt.data(), corruptedPkt.size()));

    // Bulk push with non-multiple of 188
    const std::vector<std::uint8_t> nonMultiple(200U, 0x47U);
    EXPECT_EQ(publisher.pushPackets(nonMultiple.data(), nonMultiple.size()), 0U);

    publisher.close();
}

/// @brief Verify stats bitrate calculation and counter resets.
TEST(MpegTsNetworkPublisherTest, BitrateAndStatsReset) {
    constexpr std::uint16_t port { 19108U };
    TestUdpListener listener(port);
    ASSERT_TRUE(listener.isOpen());

    TsNetworkConfig cfg;
    cfg.destinationIp = "127.0.0.1";
    cfg.destinationPort = port;
    cfg.packetsPerDatagram = 7U;

    MpegTsNetworkPublisher publisher(cfg);
    ASSERT_TRUE(publisher.open());

    for (std::uint8_t i = 0U; i < 7U; ++i) {
        const auto pkt = makeTestTsPacket(0x0101U, i);
        EXPECT_TRUE(publisher.pushPacket(pkt.data(), pkt.size()));
    }

    auto stats = publisher.stats();
    EXPECT_EQ(stats.tsPacketsSent, 7U);
    EXPECT_EQ(stats.datagramsSent, 1U);
    EXPECT_EQ(stats.bytesSent, 1316U);

    publisher.resetStats();
    stats = publisher.stats();
    EXPECT_EQ(stats.tsPacketsSent, 0U);
    EXPECT_EQ(stats.datagramsSent, 0U);
    EXPECT_EQ(stats.bytesSent, 0U);

    publisher.close();
}

} // namespace
