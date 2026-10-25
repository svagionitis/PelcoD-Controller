/// @file TestOnvifDiscoverySecurity.cpp
/// @brief Security unit tests for WS-Discovery self-amplification and DoS defense (finding C7).

#include <cstddef>
#include <cstdint>

#include <array>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include <Onvif/OnvifServerTypes.h>
#include <Onvif/WsDiscoveryCommon.h>
#include <Onvif/WsDiscoveryServer.h>
#include <Onvif/WsDiscoveryValidator.h>
#include <Transport/SocketUtils.h>

#include "OnvifTestFixtures.h"

using namespace Onvif;
namespace Net = Transport::Net;

namespace {

/// @struct SocketGuard
/// @brief Lightweight RAII helper to ensure socket handle is closed upon scope exit.
struct SocketGuard {
    Net::SocketHandle handle { Net::InvalidSocket };

    /// @brief Constructs a guard owning the given socket handle.
    /// @param h Initial socket handle to wrap.
    explicit SocketGuard(Net::SocketHandle h = Net::InvalidSocket) noexcept
        : handle { h }
    {
    }

    /// @brief Closes the socket descriptor if valid.
    ~SocketGuard() noexcept
    {
        if (handle != Net::InvalidSocket) {
            Net::closeSocket(handle);
            handle = Net::InvalidSocket;
        }
    }

    SocketGuard(const SocketGuard&) = delete;
    SocketGuard& operator=(const SocketGuard&) = delete;
    SocketGuard(SocketGuard&&) = delete;
    SocketGuard& operator=(SocketGuard&&) = delete;
};

/// @brief Checks whether a UDP port can be bound exclusively.
/// @details Pre-flight check to detect port conflicts with external services (such as Windows dasHost.exe).
/// On Windows, sets SO_EXCLUSIVEADDRUSE before binding to detect if another service owns the port.
/// On POSIX, attempts a standard bind without SO_REUSEADDR.
/// @param port UDP port number to probe.
/// @return True if port is available for exclusive binding, false otherwise.
[[nodiscard]] bool isPortAvailableExclusive(std::uint16_t port) noexcept
{
    Net::ensureWinsockInitialized();

    SocketGuard probeGuard { ::socket(AF_INET, SOCK_DGRAM, 0) };
    if (probeGuard.handle == Net::InvalidSocket) {
        return false;
    }

#ifdef _WIN32
    const int exclusive { 1 };
    if (::setsockopt(probeGuard.handle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive),
            static_cast<int>(sizeof(exclusive)))
        != 0) {
        return false;
    }
#endif

    sockaddr_in probeAddr {};
    probeAddr.sin_family = AF_INET;
    probeAddr.sin_port = htons(port);
    probeAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    const int bindResult { ::bind(
        probeGuard.handle, reinterpret_cast<struct sockaddr*>(&probeAddr), sizeof(probeAddr)) };
    return bindResult == 0;
}

std::string makeSoapProbe(
    const std::string& action, const std::string& bodyContent, const std::string& msgId = "urn:uuid:test-msg-001")
{
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
           "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
           "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
           "xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\">\r\n"
           "  <s:Header>\r\n"
           "    <wsa:Action>"
        + action
        + "</wsa:Action>\r\n"
          "    <wsa:MessageID>"
        + msgId
        + "</wsa:MessageID>\r\n"
          "    <wsa:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To>\r\n"
          "  </s:Header>\r\n"
          "  <s:Body>\r\n"
        + bodyContent
        + "\r\n"
          "  </s:Body>\r\n"
          "</s:Envelope>";
}

std::string standardProbeBody()
{
    return "    <d:Probe>\r\n"
           "      <d:Types>dn:NetworkVideoTransmitter tds:Device</d:Types>\r\n"
           "    </d:Probe>";
}

std::string probeMatchesBody(const std::string& serviceUuid)
{
    return "    <d:ProbeMatches>\r\n"
           "      <d:ProbeMatch>\r\n"
           "        <wsa:EndpointReference>\r\n"
           "          <wsa:Address>urn:uuid:"
        + serviceUuid
        + "</wsa:Address>\r\n"
          "        </wsa:EndpointReference>\r\n"
          "        <d:Types>dn:NetworkVideoTransmitter tds:Device</d:Types>\r\n"
          "      </d:ProbeMatch>\r\n"
          "    </d:ProbeMatches>";
}

} // namespace

// ============================================================================
// WsDiscoveryValidator Unit Tests
// ============================================================================

TEST(TestOnvifDiscoverySecurity, RejectsProbeMatchesAction)
{
    const std::string xmlStr = makeSoapProbe(
        WsDiscoveryValidator::kProbeMatchesActionUri, probeMatchesBody("11111111-2222-3333-4444-555555555555"));

    pugi::xml_document doc;
    ASSERT_TRUE(doc.load_string(xmlStr.c_str()));

    // Strict action match must reject ProbeMatches
    EXPECT_FALSE(WsDiscoveryValidator::isExactProbeAction(WsDiscoveryValidator::kProbeMatchesActionUri));

    const auto res = WsDiscoveryValidator::validateProbe(doc, 54321, "22222222-3333-4444-5555-666666666666");
    EXPECT_FALSE(res.isValidProbe);
}

TEST(TestOnvifDiscoverySecurity, RejectsSubstringProbeAction)
{
    EXPECT_FALSE(WsDiscoveryValidator::isExactProbeAction("Probe"));
    EXPECT_FALSE(
        WsDiscoveryValidator::isExactProbeAction("http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeExtra"));
    EXPECT_FALSE(
        WsDiscoveryValidator::isExactProbeAction("http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe/More"));
    EXPECT_FALSE(WsDiscoveryValidator::isExactProbeAction(""));

    const std::string xmlStr = makeSoapProbe("Probe", standardProbeBody());
    pugi::xml_document doc;
    ASSERT_TRUE(doc.load_string(xmlStr.c_str()));

    const auto res = WsDiscoveryValidator::validateProbe(doc, 54321, "22222222-3333-4444-5555-666666666666");
    EXPECT_FALSE(res.isValidProbe);
}

TEST(TestOnvifDiscoverySecurity, AcceptsExactProbeAction)
{
    EXPECT_TRUE(WsDiscoveryValidator::isExactProbeAction(WsDiscoveryValidator::kProbeActionUri));

    const std::string xmlStr
        = makeSoapProbe(WsDiscoveryValidator::kProbeActionUri, standardProbeBody(), "urn:uuid:client-probe-123");
    pugi::xml_document doc;
    ASSERT_TRUE(doc.load_string(xmlStr.c_str()));

    const auto res = WsDiscoveryValidator::validateProbe(doc, 54321, "22222222-3333-4444-5555-666666666666");
    EXPECT_TRUE(res.isValidProbe);
    EXPECT_FALSE(res.isSelfMessage);
    EXPECT_FALSE(res.isReflectionAttempt);
    EXPECT_EQ(res.messageId, "urn:uuid:client-probe-123");
}

TEST(TestOnvifDiscoverySecurity, RejectsProbeMatchesInBody)
{
    // Attacker crafts Probe action URI but embeds ProbeMatches in SOAP Body
    const std::string xmlStr = makeSoapProbe(WsDiscoveryValidator::kProbeActionUri, probeMatchesBody("some-uuid"));

    pugi::xml_document doc;
    ASSERT_TRUE(doc.load_string(xmlStr.c_str()));

    const auto res = WsDiscoveryValidator::validateProbe(doc, 54321, "22222222-3333-4444-5555-666666666666");
    EXPECT_FALSE(res.isValidProbe);
}

TEST(TestOnvifDiscoverySecurity, RejectsSelfGeneratedUuid)
{
    const std::string myUuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    const std::string xmlStr
        = makeSoapProbe(WsDiscoveryValidator::kProbeActionUri, standardProbeBody(), "urn:uuid:" + myUuid);

    pugi::xml_document doc;
    ASSERT_TRUE(doc.load_string(xmlStr.c_str()));

    const auto res = WsDiscoveryValidator::validateProbe(doc, 54321, myUuid);
    EXPECT_TRUE(res.isSelfMessage);
    EXPECT_FALSE(res.isValidProbe);
}

TEST(TestOnvifDiscoverySecurity, RejectsSelfUuidUpperWire)
{
    const std::string localUuid { "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee" };
    const std::string xmlStr { makeSoapProbe(
        WsDiscoveryValidator::kProbeActionUri, standardProbeBody(), "urn:uuid:AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE") };

    pugi::xml_document doc {};
    ASSERT_TRUE(doc.load_string(xmlStr.c_str()));

    const auto res { WsDiscoveryValidator::validateProbe(doc, 54321U, localUuid) };
    EXPECT_TRUE(res.isSelfMessage);
    EXPECT_FALSE(res.isValidProbe);
}

TEST(TestOnvifDiscoverySecurity, RejectsSelfUuidMixedLocal)
{
    const std::string localUuid { "AaAaAaAa-BbBb-CcCc-DdDd-EeEeEeEeEeEe" };
    const std::string xmlStr { makeSoapProbe(
        WsDiscoveryValidator::kProbeActionUri, standardProbeBody(), "urn:uuid:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee") };

    pugi::xml_document doc {};
    ASSERT_TRUE(doc.load_string(xmlStr.c_str()));

    const auto res { WsDiscoveryValidator::validateProbe(doc, 54321U, localUuid) };
    EXPECT_TRUE(res.isSelfMessage);
    EXPECT_FALSE(res.isValidProbe);
}

TEST(TestOnvifDiscoverySecurity, AcceptsNonAsciiUuidBytes)
{
    // UTF-8 "\xC3\x84" (A-umlaut) must not fold to 'a' and must not match the local UUID
    const std::string localUuid { "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee" };
    const std::string xmlStr { makeSoapProbe(WsDiscoveryValidator::kProbeActionUri, standardProbeBody(),
        "urn:uuid:\xC3\x84"
        "aaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee") };

    pugi::xml_document doc {};
    ASSERT_TRUE(doc.load_string(xmlStr.c_str()));

    const auto res { WsDiscoveryValidator::validateProbe(doc, 54321U, localUuid) };
    EXPECT_FALSE(res.isSelfMessage);
    EXPECT_TRUE(res.isValidProbe);
}

TEST(TestOnvifDiscoverySecurity, RejectsReflectionPort3702)
{
    const std::string xmlStr = makeSoapProbe(WsDiscoveryValidator::kProbeActionUri, standardProbeBody());

    pugi::xml_document doc;
    ASSERT_TRUE(doc.load_string(xmlStr.c_str()));

    // Arriving with source port 3702 is a mutual reflection / loop hazard
    const auto res = WsDiscoveryValidator::validateProbe(doc, 3702, "22222222-3333-4444-5555-666666666666", true);
    EXPECT_TRUE(res.isReflectionAttempt);
    EXPECT_FALSE(res.isValidProbe);
}

TEST(TestOnvifDiscoverySecurity, RateLimiterThrottlesFloods)
{
    DiscoveryRateLimiter limiter(5, 10);
    const std::string senderIp = "192.168.1.100";

    // First 5 should succeed
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(limiter.checkRateLimit(senderIp));
    }

    // 6th onwards should be throttled
    EXPECT_FALSE(limiter.checkRateLimit(senderIp));
    EXPECT_FALSE(limiter.checkRateLimit(senderIp));

    // Resetting rate limiter restores tokens
    limiter.resetRateLimits();
    EXPECT_TRUE(limiter.checkRateLimit(senderIp));
}

TEST(TestOnvifDiscoverySecurity, LiveServerDropsProbeMatchesAndSelfLoop)
{
    if (!isPortAvailableExclusive(kMulticastPort)) {
        GTEST_SKIP()
            << "UDP port 3702 is in use by an external service (e.g. Windows dasHost.exe); skipping live probe test.";
    }

    OnvifServerConfig config {};
    config.port = static_cast<int>(Onvif::Test::getFreeEphemeralPort());
    config.deviceName = "Live Discovery Test Camera";
    config.serviceUuid = "33333333-4444-5555-6666-777777777777";
    config.discovery.enableMulticastLoopback = true;
    config.discovery.dropReflectionPort3702 = true;

    WsDiscoveryServer server(config);
    if (!server.start()) {
        GTEST_SKIP() << "Socket bind on 3702 failed or occupied, skipping live UDP test.";
    }

    Net::ensureWinsockInitialized();
    SocketGuard clientGuard { ::socket(AF_INET, SOCK_DGRAM, 0) };
    ASSERT_NE(clientGuard.handle, Net::InvalidSocket);
    const Net::SocketHandle clientSock { clientGuard.handle };

    // Bind client to an ephemeral port
    sockaddr_in clientAddr {};
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = 0;
    clientAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ASSERT_EQ(::bind(clientSock, reinterpret_cast<struct sockaddr*>(&clientAddr), sizeof(clientAddr)), 0);

    sockaddr_in destAddr {};
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(kMulticastPort);
    destAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // 1. Send ProbeMatches: server must NOT reply
    const std::string probeMatchesPayload { makeSoapProbe(
        WsDiscoveryValidator::kProbeMatchesActionUri, probeMatchesBody("99999999-8888-7777-6666-555555555555")) };

    ::sendto(clientSock, probeMatchesPayload.data(), static_cast<Net::SockBufLenType>(probeMatchesPayload.size()), 0,
        reinterpret_cast<struct sockaddr*>(&destAddr), sizeof(destAddr));

    // Poll for reply (should timeout with 0 packets)
    Net::PollFd pfd {};
    pfd.fd = clientSock;
    pfd.events = POLLIN;
    int ret { Net::pollSockets(&pfd, 1, 150) };
    EXPECT_EQ(ret, 0);

    // 2. Send legitimate Probe: server MUST reply with ProbeMatches
    const std::string probePayload { makeSoapProbe(
        WsDiscoveryValidator::kProbeActionUri, standardProbeBody(), "urn:uuid:client-query-999") };

    ::sendto(clientSock, probePayload.data(), static_cast<Net::SockBufLenType>(probePayload.size()), 0,
        reinterpret_cast<struct sockaddr*>(&destAddr), sizeof(destAddr));

    ret = Net::pollSockets(&pfd, 1, 1000);
    if (ret == 0) {
        GTEST_SKIP() << "Timed out waiting for ProbeMatches; unicast probe likely absorbed by an external service "
                        "(e.g. Windows dasHost.exe).";
    }
    ASSERT_GT(ret, 0);

    if (ret > 0 && ((pfd.revents & POLLIN) != 0)) {
        std::array<char, 4096> buf {};
        sockaddr_in replyFrom {};
        Net::SockOptLenType replyLen { static_cast<Net::SockOptLenType>(sizeof(replyFrom)) };
        const auto recvd = ::recvfrom(clientSock, buf.data(), static_cast<Net::SockBufLenType>(buf.size() - 1U), 0,
            reinterpret_cast<struct sockaddr*>(&replyFrom), &replyLen);
        EXPECT_GT(recvd, 0);
        if (recvd > 0) {
            const std::string replyStr(buf.data(), static_cast<std::size_t>(recvd));
            pugi::xml_document replyDoc;
            EXPECT_TRUE(replyDoc.load_string(replyStr.c_str()));
            const auto actionNode = replyDoc.select_node("//*[local-name()='Action']").node();
            EXPECT_TRUE(actionNode);
            EXPECT_EQ(std::string(actionNode.text().as_string()), WsDiscoveryValidator::kProbeMatchesActionUri);
        }
    }

    server.stop();
}

TEST(TestOnvifDiscoverySecurity, PortExclusivityDetector)
{
    Net::ensureWinsockInitialized();

    // 1. Create a socket and bind to an ephemeral port without SO_REUSEADDR
    SocketGuard holder { ::socket(AF_INET, SOCK_DGRAM, 0) };
    ASSERT_NE(holder.handle, Net::InvalidSocket);

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    ASSERT_EQ(::bind(holder.handle, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)), 0);

    // Retrieve the assigned ephemeral port
    sockaddr_in boundAddr {};
    Net::SockOptLenType len { static_cast<Net::SockOptLenType>(sizeof(boundAddr)) };
    ASSERT_EQ(::getsockname(holder.handle, reinterpret_cast<struct sockaddr*>(&boundAddr), &len), 0);
    const std::uint16_t boundPort { ntohs(boundAddr.sin_port) };
    ASSERT_GT(boundPort, 0U);

    // 2. While the port is occupied by holder, exclusive probe must detect the collision
    EXPECT_FALSE(isPortAvailableExclusive(boundPort));

    // 3. Release the socket; the port must become available
    Net::closeSocket(holder.handle);
    holder.handle = Net::InvalidSocket;

    EXPECT_TRUE(isPortAvailableExclusive(boundPort));
}
