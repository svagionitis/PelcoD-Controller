/// @file TestRtspServerPublisher.cpp
/// @brief Comprehensive unit tests for RtspServerPublisher embedded streaming server.

#include "RtspServerPublisher.h"
#include "SocketUtils.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using namespace Transport;

[[nodiscard]] std::uint16_t getFreePort()
{
    Net::ensureWinsockInitialized();
    const Net::SocketHandle s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s == Net::InvalidSocket) {
        return 18554U;
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::bind(s, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        Net::closeSocket(s);
        return 18554U;
    }

    Net::SockOptLenType len = sizeof(addr);
    if (::getsockname(s, reinterpret_cast<struct sockaddr*>(&addr), &len) != 0) {
        Net::closeSocket(s);
        return 18554U;
    }

    const std::uint16_t port = ntohs(addr.sin_port);
    Net::closeSocket(s);
    return port;
}

[[nodiscard]] std::vector<std::uint8_t> makeTsPacket(std::uint16_t pid, std::uint8_t cc)
{
    std::vector<std::uint8_t> pkt(188U, 0xFFU);
    pkt[0] = 0x47U; // Sync byte
    pkt[1] = static_cast<std::uint8_t>((pid >> 8U) & 0x1FU);
    pkt[2] = static_cast<std::uint8_t>(pid & 0xFFU);
    pkt[3] = static_cast<std::uint8_t>(0x10U | (cc & 0x0FU));
    for (std::size_t i { 4U }; i < 188U; ++i) {
        pkt[i] = static_cast<std::uint8_t>((i + cc) & 0xFFU);
    }
    return pkt;
}

class TestTcpClient {
public:
    explicit TestTcpClient(std::uint16_t port)
    {
        Net::ensureWinsockInitialized();
        m_sock = ::socket(AF_INET, SOCK_STREAM, 0);
        if (m_sock == Net::InvalidSocket) {
            return;
        }

        sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (::connect(m_sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
            Net::closeSocket(m_sock);
            m_sock = Net::InvalidSocket;
        }
    }

    ~TestTcpClient()
    {
        if (m_sock != Net::InvalidSocket) {
            Net::closeSocket(m_sock);
        }
    }

    [[nodiscard]] bool isConnected() const noexcept
    {
        return m_sock != Net::InvalidSocket;
    }

    bool send(std::string_view msg)
    {
        if (m_sock == Net::InvalidSocket) {
            return false;
        }
        const auto sent = ::send(m_sock, msg.data(), static_cast<Net::SockBufLenType>(msg.size()), 0);
        return sent == static_cast<decltype(sent)>(msg.size());
    }

    [[nodiscard]] std::string receiveResponse(int timeoutMs = 2000)
    {
        std::string response;
        if (m_sock == Net::InvalidSocket) {
            return response;
        }

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

        while (std::chrono::steady_clock::now() < deadline) {
            Net::PollFd pfd {};
            pfd.fd = m_sock;
            pfd.events = POLLIN;

            const int ready = Net::pollSockets(&pfd, 1U, 50);
            if (ready > 0 && (pfd.revents & POLLIN) != 0) {
                std::array<char, 2048> buf {};
                const auto r = ::recv(m_sock, buf.data(), static_cast<Net::SockBufLenType>(buf.size()), 0);
                if (r > 0) {
                    response.append(buf.data(), static_cast<std::size_t>(r));
                    if (response.find("\r\n\r\n") != std::string::npos) {
                        break;
                    }
                } else if (r == 0) {
                    break;
                }
            }
        }
        return response;
    }

    [[nodiscard]] std::vector<std::uint8_t> receiveRawBytes(std::size_t expectedCount, int timeoutMs = 2000)
    {
        std::vector<std::uint8_t> out;
        if (m_sock == Net::InvalidSocket) {
            return out;
        }

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

        while (out.size() < expectedCount && std::chrono::steady_clock::now() < deadline) {
            Net::PollFd pfd {};
            pfd.fd = m_sock;
            pfd.events = POLLIN;

            const int ready = Net::pollSockets(&pfd, 1U, 50);
            if (ready > 0 && (pfd.revents & POLLIN) != 0) {
                std::array<std::uint8_t, 2048> buf {};
                const auto r = ::recv(
                    m_sock, reinterpret_cast<char*>(buf.data()), static_cast<Net::SockBufLenType>(buf.size()), 0);
                if (r > 0) {
                    out.insert(out.end(), buf.begin(), buf.begin() + r);
                } else if (r == 0) {
                    break;
                }
            }
        }
        return out;
    }

private:
    Net::SocketHandle m_sock { Net::InvalidSocket };
};

class TestUdpClient {
public:
    explicit TestUdpClient(std::uint16_t port)
    {
        Net::ensureWinsockInitialized();
        m_sock = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (m_sock == Net::InvalidSocket) {
            return;
        }

        const int reuse = 1;
        static_cast<void>(
            ::setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse)));

        sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (::bind(m_sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
            Net::closeSocket(m_sock);
            m_sock = Net::InvalidSocket;
        }
    }

    ~TestUdpClient()
    {
        if (m_sock != Net::InvalidSocket) {
            Net::closeSocket(m_sock);
        }
    }

    [[nodiscard]] bool isValid() const noexcept
    {
        return m_sock != Net::InvalidSocket;
    }

    [[nodiscard]] std::vector<std::uint8_t> receiveDatagram(int timeoutMs = 2000)
    {
        std::vector<std::uint8_t> out;
        if (m_sock == Net::InvalidSocket) {
            return out;
        }

        Net::PollFd pfd {};
        pfd.fd = m_sock;
        pfd.events = POLLIN;

        const int ready = Net::pollSockets(&pfd, 1U, timeoutMs);
        if (ready > 0 && (pfd.revents & POLLIN) != 0) {
            std::array<std::uint8_t, 2048> buf {};
            const auto r
                = ::recv(m_sock, reinterpret_cast<char*>(buf.data()), static_cast<Net::SockBufLenType>(buf.size()), 0);
            if (r > 0) {
                out.assign(buf.begin(), buf.begin() + r);
            }
        }
        return out;
    }

private:
    Net::SocketHandle m_sock { Net::InvalidSocket };
};

} // namespace

TEST(TestRtspServerPublisher, LifecycleStartStop)
{
    const std::uint16_t port = getFreePort();
    RtspServerConfig config {};
    config.listenAddress = "127.0.0.1";
    config.listenPort = RtspPort(port);
    config.streamPath = "/live";

    RtspServerPublisher server(config);
    EXPECT_FALSE(server.isRunning());

    EXPECT_TRUE(server.start());
    EXPECT_TRUE(server.isRunning());

    // Idempotent start
    EXPECT_TRUE(server.start());

    server.stop();
    EXPECT_FALSE(server.isRunning());

    // Idempotent stop
    server.stop();
    EXPECT_FALSE(server.isRunning());
}

TEST(TestRtspServerPublisher, OptionsHandshake)
{
    const std::uint16_t port = getFreePort();
    RtspServerConfig config {};
    config.listenAddress = "127.0.0.1";
    config.listenPort = RtspPort(port);

    RtspServerPublisher server(config);
    ASSERT_TRUE(server.start());

    TestTcpClient client(port);
    ASSERT_TRUE(client.isConnected());

    const std::string optReq = "OPTIONS rtsp://127.0.0.1:" + std::to_string(port)
        + "/live RTSP/1.0\r\n"
          "CSeq: 1\r\n"
          "\r\n";
    EXPECT_TRUE(client.send(optReq));

    const std::string resp = client.receiveResponse();
    EXPECT_NE(resp.find("RTSP/1.0 200 OK"), std::string::npos);
    EXPECT_NE(resp.find("CSeq: 1"), std::string::npos);
    EXPECT_NE(resp.find("Public:"), std::string::npos);
    EXPECT_NE(resp.find("DESCRIBE"), std::string::npos);
    EXPECT_NE(resp.find("SETUP"), std::string::npos);
    EXPECT_NE(resp.find("PLAY"), std::string::npos);

    server.stop();
}

TEST(TestRtspServerPublisher, DescribeSdpGeneration)
{
    const std::uint16_t port = getFreePort();
    RtspServerConfig config {};
    config.listenAddress = "127.0.0.1";
    config.listenPort = RtspPort(port);
    config.streamPath = "/stanag4609";

    RtspServerPublisher server(config);
    ASSERT_TRUE(server.start());

    TestTcpClient client(port);
    ASSERT_TRUE(client.isConnected());

    const std::string descReq = "DESCRIBE rtsp://127.0.0.1:" + std::to_string(port)
        + "/stanag4609 RTSP/1.0\r\n"
          "CSeq: 2\r\n"
          "Accept: application/sdp\r\n"
          "\r\n";
    EXPECT_TRUE(client.send(descReq));

    const std::string resp = client.receiveResponse();
    EXPECT_NE(resp.find("RTSP/1.0 200 OK"), std::string::npos);
    EXPECT_NE(resp.find("CSeq: 2"), std::string::npos);
    EXPECT_NE(resp.find("Content-Type: application/sdp"), std::string::npos);
    EXPECT_NE(resp.find("m=video "), std::string::npos);
    EXPECT_NE(resp.find("RTP/AVP 33"), std::string::npos);
    EXPECT_NE(resp.find("a=rtpmap:33 MP2T/90000"), std::string::npos);

    server.stop();
}

TEST(TestRtspServerPublisher, SetupTcpInterleaved)
{
    const std::uint16_t port = getFreePort();
    RtspServerConfig config {};
    config.listenAddress = "127.0.0.1";
    config.listenPort = RtspPort(port);

    RtspServerPublisher server(config);
    ASSERT_TRUE(server.start());

    TestTcpClient client(port);
    ASSERT_TRUE(client.isConnected());

    const std::string setupReq = "SETUP rtsp://127.0.0.1:" + std::to_string(port)
        + "/live/track0 RTSP/1.0\r\n"
          "CSeq: 3\r\n"
          "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
          "\r\n";
    EXPECT_TRUE(client.send(setupReq));

    const std::string resp = client.receiveResponse();
    EXPECT_NE(resp.find("RTSP/1.0 200 OK"), std::string::npos);
    EXPECT_NE(resp.find("CSeq: 3"), std::string::npos);
    EXPECT_NE(resp.find("Transport: RTP/AVP/TCP;unicast;interleaved=0-1"), std::string::npos);
    EXPECT_NE(resp.find("Session:"), std::string::npos);

    server.stop();
}

TEST(TestRtspServerPublisher, SetupUdpUnicast)
{
    const std::uint16_t port = getFreePort();
    RtspServerConfig config {};
    config.listenAddress = "127.0.0.1";
    config.listenPort = RtspPort(port);

    RtspServerPublisher server(config);
    ASSERT_TRUE(server.start());

    TestTcpClient client(port);
    ASSERT_TRUE(client.isConnected());

    const std::string setupReq = "SETUP rtsp://127.0.0.1:" + std::to_string(port)
        + "/live/track0 RTSP/1.0\r\n"
          "CSeq: 3\r\n"
          "Transport: RTP/AVP;unicast;client_port=32000-32001\r\n"
          "\r\n";
    EXPECT_TRUE(client.send(setupReq));

    const std::string resp = client.receiveResponse();
    EXPECT_NE(resp.find("RTSP/1.0 200 OK"), std::string::npos);
    EXPECT_NE(resp.find("CSeq: 3"), std::string::npos);
    EXPECT_NE(resp.find("client_port=32000-32001"), std::string::npos);
    EXPECT_NE(resp.find("server_port="), std::string::npos);
    EXPECT_NE(resp.find("Session:"), std::string::npos);

    server.stop();
}

TEST(TestRtspServerPublisher, PlayAndTeardown)
{
    const std::uint16_t port = getFreePort();
    RtspServerConfig config {};
    config.listenAddress = "127.0.0.1";
    config.listenPort = RtspPort(port);

    RtspServerPublisher server(config);
    ASSERT_TRUE(server.start());

    TestTcpClient client(port);
    ASSERT_TRUE(client.isConnected());

    // 1. SETUP
    const std::string setupReq = "SETUP rtsp://127.0.0.1:" + std::to_string(port)
        + "/live RTSP/1.0\r\n"
          "CSeq: 3\r\n"
          "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
          "\r\n";
    EXPECT_TRUE(client.send(setupReq));
    const std::string setupResp = client.receiveResponse();
    const std::size_t sessPos = setupResp.find("Session: ");
    ASSERT_NE(sessPos, std::string::npos);
    const std::string sessId = setupResp.substr(sessPos + 9U, 8U);

    // 2. PLAY
    const std::string playReq = "PLAY rtsp://127.0.0.1:" + std::to_string(port)
        + "/live RTSP/1.0\r\n"
          "CSeq: 4\r\n"
          "Session: "
        + sessId
        + "\r\n"
          "Range: npt=0.000-\r\n"
          "\r\n";
    EXPECT_TRUE(client.send(playReq));
    const std::string playResp = client.receiveResponse();
    EXPECT_NE(playResp.find("RTSP/1.0 200 OK"), std::string::npos);
    EXPECT_NE(playResp.find("CSeq: 4"), std::string::npos);
    EXPECT_NE(playResp.find("Range: npt=0.000-"), std::string::npos);

    auto stats = server.stats();
    EXPECT_EQ(stats.activeClients, 1U);
    EXPECT_EQ(stats.streamingClients, 1U);

    // 3. TEARDOWN
    const std::string tearReq = "TEARDOWN rtsp://127.0.0.1:" + std::to_string(port)
        + "/live RTSP/1.0\r\n"
          "CSeq: 5\r\n"
          "Session: "
        + sessId
        + "\r\n"
          "\r\n";
    EXPECT_TRUE(client.send(tearReq));
    const std::string tearResp = client.receiveResponse();
    EXPECT_NE(tearResp.find("RTSP/1.0 200 OK"), std::string::npos);

    // Allow worker loop cycle to purge teardown session
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stats = server.stats();
    EXPECT_EQ(stats.streamingClients, 0U);

    server.stop();
}

TEST(TestRtspServerPublisher, TcpInterleavedStreaming)
{
    const std::uint16_t port = getFreePort();
    RtspServerConfig config {};
    config.listenAddress = "127.0.0.1";
    config.listenPort = RtspPort(port);
    config.packetsPerDatagram = 1U; // Flush per 1 TS packet for immediate test reception

    RtspServerPublisher server(config);
    ASSERT_TRUE(server.start());

    TestTcpClient client(port);
    ASSERT_TRUE(client.isConnected());

    // Setup & Play
    EXPECT_TRUE(client.send(
        "SETUP rtsp://127.0.0.1/live RTSP/1.0\r\nCSeq: 1\r\nTransport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n\r\n"));
    const std::string setupResp = client.receiveResponse();
    const std::size_t sessPos = setupResp.find("Session: ");
    ASSERT_NE(sessPos, std::string::npos);
    const std::string sessId = setupResp.substr(sessPos + 9U, 8U);

    EXPECT_TRUE(client.send("PLAY rtsp://127.0.0.1/live RTSP/1.0\r\nCSeq: 2\r\nSession: " + sessId + "\r\n\r\n"));
    static_cast<void>(client.receiveResponse());

    // Push 1 TS packet
    const auto ts = makeTsPacket(0x0101U, 1U);
    EXPECT_TRUE(server.pushPacket(ts.data(), ts.size(), 1000000ULL));

    // Client expects: 4-byte '$' header + 12-byte RTP header + 188-byte TS packet = 204 bytes
    const auto bytes = client.receiveRawBytes(204U);
    ASSERT_GE(bytes.size(), 204U);

    EXPECT_EQ(bytes[0], 0x24U); // '$'
    EXPECT_EQ(bytes[1], 0x00U); // Channel 0
    const std::size_t payloadLen = (static_cast<std::size_t>(bytes[2]) << 8U) | bytes[3];
    EXPECT_EQ(payloadLen, 12U + 188U);

    // RTP Header verification
    EXPECT_EQ(bytes[4], 0x80U); // V=2
    EXPECT_EQ(bytes[5], 33U); // PT=33 MP2T

    // TS Packet sync byte
    EXPECT_EQ(bytes[16], 0x47U);

    server.stop();
}

TEST(TestRtspServerPublisher, UdpUnicastStreaming)
{
    const std::uint16_t serverPort = getFreePort();
    const std::uint16_t clientUdpPort = getFreePort();

    RtspServerConfig config {};
    config.listenAddress = "127.0.0.1";
    config.listenPort = RtspPort(serverPort);
    config.packetsPerDatagram = 1U;

    RtspServerPublisher server(config);
    ASSERT_TRUE(server.start());

    TestUdpClient udpListener(clientUdpPort);
    ASSERT_TRUE(udpListener.isValid());

    TestTcpClient client(serverPort);
    ASSERT_TRUE(client.isConnected());

    // Setup requesting UDP
    const std::string setupReq = "SETUP rtsp://127.0.0.1/live RTSP/1.0\r\n"
                                 "CSeq: 1\r\n"
                                 "Transport: RTP/AVP;unicast;client_port="
        + std::to_string(clientUdpPort) + "-" + std::to_string(clientUdpPort + 1U) + "\r\n\r\n";
    EXPECT_TRUE(client.send(setupReq));
    const std::string setupResp = client.receiveResponse();
    const std::size_t sessPos = setupResp.find("Session: ");
    ASSERT_NE(sessPos, std::string::npos);
    const std::string sessId = setupResp.substr(sessPos + 9U, 8U);

    // Play
    EXPECT_TRUE(client.send("PLAY rtsp://127.0.0.1/live RTSP/1.0\r\nCSeq: 2\r\nSession: " + sessId + "\r\n\r\n"));
    static_cast<void>(client.receiveResponse());

    // Push TS packet
    const auto ts = makeTsPacket(0x01E0U, 5U);
    EXPECT_TRUE(server.pushPacket(ts.data(), ts.size(), 2000000ULL));

    // Verify datagram received by UDP listener
    const auto dgram = udpListener.receiveDatagram();
    ASSERT_EQ(dgram.size(), 12U + 188U);
    EXPECT_EQ(dgram[0], 0x80U); // V=2
    EXPECT_EQ(dgram[1], 33U); // PT=33 MP2T
    EXPECT_EQ(dgram[12], 0x47U); // TS Sync

    server.stop();
}

TEST(TestRtspServerPublisher, MultiClientConcurrency)
{
    const std::uint16_t port = getFreePort();
    RtspServerConfig config {};
    config.listenAddress = "127.0.0.1";
    config.listenPort = RtspPort(port);
    config.packetsPerDatagram = 1U;

    RtspServerPublisher server(config);
    ASSERT_TRUE(server.start());

    // Connect Client 1
    TestTcpClient client1(port);
    ASSERT_TRUE(client1.isConnected());
    EXPECT_TRUE(client1.send(
        "SETUP rtsp://127.0.0.1/live RTSP/1.0\r\nCSeq: 1\r\nTransport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n\r\n"));
    const std::string resp1 = client1.receiveResponse();
    const std::string sess1 = resp1.substr(resp1.find("Session: ") + 9U, 8U);
    EXPECT_TRUE(client1.send("PLAY rtsp://127.0.0.1/live RTSP/1.0\r\nCSeq: 2\r\nSession: " + sess1 + "\r\n\r\n"));
    static_cast<void>(client1.receiveResponse());

    // Connect Client 2
    TestTcpClient client2(port);
    ASSERT_TRUE(client2.isConnected());
    EXPECT_TRUE(client2.send(
        "SETUP rtsp://127.0.0.1/live RTSP/1.0\r\nCSeq: 1\r\nTransport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n\r\n"));
    const std::string resp2 = client2.receiveResponse();
    const std::string sess2 = resp2.substr(resp2.find("Session: ") + 9U, 8U);
    EXPECT_TRUE(client2.send("PLAY rtsp://127.0.0.1/live RTSP/1.0\r\nCSeq: 2\r\nSession: " + sess2 + "\r\n\r\n"));
    static_cast<void>(client2.receiveResponse());

    EXPECT_EQ(server.stats().streamingClients, 2U);

    // Push TS Packet
    const auto ts = makeTsPacket(0x0101U, 9U);
    EXPECT_TRUE(server.pushPacket(ts.data(), ts.size(), 3000000ULL));

    // Both clients receive identical packet
    const auto b1 = client1.receiveRawBytes(204U);
    const auto b2 = client2.receiveRawBytes(204U);

    ASSERT_EQ(b1.size(), 204U);
    ASSERT_EQ(b2.size(), 204U);
    EXPECT_EQ(b1, b2);

    server.stop();
}

TEST(TestRtspServerPublisher, KeepAliveGetParameter)
{
    const std::uint16_t port = getFreePort();
    RtspServerConfig config {};
    config.listenAddress = "127.0.0.1";
    config.listenPort = RtspPort(port);

    RtspServerPublisher server(config);
    ASSERT_TRUE(server.start());

    TestTcpClient client(port);
    ASSERT_TRUE(client.isConnected());

    EXPECT_TRUE(client.send(
        "SETUP rtsp://127.0.0.1/live RTSP/1.0\r\nCSeq: 1\r\nTransport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n\r\n"));
    const std::string setupResp = client.receiveResponse();
    const std::string sessId = setupResp.substr(setupResp.find("Session: ") + 9U, 8U);

    EXPECT_TRUE(
        client.send("GET_PARAMETER rtsp://127.0.0.1/live RTSP/1.0\r\nCSeq: 2\r\nSession: " + sessId + "\r\n\r\n"));
    const std::string getResp = client.receiveResponse();
    EXPECT_NE(getResp.find("RTSP/1.0 200 OK"), std::string::npos);
    EXPECT_NE(getResp.find("CSeq: 2"), std::string::npos);

    server.stop();
}

TEST(TestRtspServerPublisher, SlowClientBackpressure)
{
    const std::uint16_t port = getFreePort();
    RtspServerConfig config {};
    config.listenAddress = "127.0.0.1";
    config.listenPort = RtspPort(port);
    config.packetsPerDatagram = 1U;
    config.maxClientQueuePackets = 4U; // Small queue to trigger drop quickly

    RtspServerPublisher server(config);
    ASSERT_TRUE(server.start());

    TestTcpClient client(port);
    ASSERT_TRUE(client.isConnected());

    EXPECT_TRUE(client.send(
        "SETUP rtsp://127.0.0.1/live RTSP/1.0\r\nCSeq: 1\r\nTransport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n\r\n"));
    const std::string setupResp = client.receiveResponse();
    const std::string sessId = setupResp.substr(setupResp.find("Session: ") + 9U, 8U);

    EXPECT_TRUE(client.send("PLAY rtsp://127.0.0.1/live RTSP/1.0\r\nCSeq: 2\r\nSession: " + sessId + "\r\n\r\n"));
    static_cast<void>(client.receiveResponse());

    // Send many TS packets without reading from client socket
    for (std::size_t i { 0U }; i < 200U; ++i) {
        const auto ts = makeTsPacket(0x0101U, static_cast<std::uint8_t>(i & 0x0FU));
        server.pushPacket(ts.data(), ts.size(), 1000ULL * static_cast<std::uint64_t>(i));
    }

    // Drops should have occurred or server remains healthy
    const auto stats = server.stats();
    EXPECT_EQ(stats.streamingClients, 1U);
    EXPECT_GT(stats.rtpPacketsDispatched, 0U);

    server.stop();
}

TEST(TestRtspServerPublisher, StreamUrlAndConfig)
{
    const std::uint16_t port = getFreePort();
    RtspServerConfig config {};
    config.listenAddress = "192.168.1.50";
    config.listenPort = RtspPort(port);
    config.streamPath = "/stanag";

    RtspServerPublisher server(config);
    EXPECT_EQ(server.streamUrl(), "rtsp://192.168.1.50:" + std::to_string(port) + "/stanag");
    EXPECT_EQ(server.streamUrl("myhost.local"), "rtsp://myhost.local:" + std::to_string(port) + "/stanag");
    EXPECT_EQ(server.config().listenPort.value(), port);
}
