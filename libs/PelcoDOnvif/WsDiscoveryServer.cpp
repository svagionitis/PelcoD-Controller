#include "WsDiscoveryServer.h"

#include <pugixml.hpp>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

using SocketType = SOCKET;
constexpr SocketType kInvalidSocket = INVALID_SOCKET;
#define CLOSE_SOCKET(s) ::closesocket(s)
#define POLL_SOCKET(fds, nfds, timeout) ::WSAPoll(fds, nfds, timeout)
using SockOptLenType = int;
using SockBufLenType = int;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

using SocketType = int;
constexpr SocketType kInvalidSocket = -1;
#define CLOSE_SOCKET(s) ::close(s)
#define POLL_SOCKET(fds, nfds, timeout) ::poll(fds, nfds, timeout)
using SockOptLenType = socklen_t;
using SockBufLenType = size_t;
#endif

#include <array>
#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>

namespace PelcoD::Onvif {

namespace {

    constexpr const char* kMulticastIp { "239.255.255.250" };
    constexpr uint16_t kMulticastPort { 3702 };

    std::string generateRandomUuid()
    {
        std::random_device rd {};
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);

        const uint32_t d1 = dis(gen);
        const uint16_t d2 = static_cast<uint16_t>(dis(gen) & 0xFFFF);
        const uint16_t d3 = static_cast<uint16_t>((dis(gen) & 0x0FFF) | 0x4000); // version 4
        const uint16_t d4 = static_cast<uint16_t>((dis(gen) & 0x3FFF) | 0x8000); // variant 1
        const uint32_t d5a = dis(gen);
        const uint16_t d5b = static_cast<uint16_t>(dis(gen) & 0xFFFF);

        std::ostringstream oss;
        oss << std::hex << std::setfill('0') << std::setw(8) << d1 << '-' << std::setw(4) << d2 << '-' << std::setw(4)
            << d3 << '-' << std::setw(4) << d4 << '-' << std::setw(8) << d5a << std::setw(4) << d5b;
        return oss.str();
    }

    std::string resolveLocalIp(const sockaddr_in& targetAddr)
    {
        const SocketType probeSock = socket(AF_INET, SOCK_DGRAM, 0);
        if (probeSock == kInvalidSocket) {
            return "127.0.0.1";
        }

        sockaddr_in probeTarget = targetAddr;
        if (probeTarget.sin_addr.s_addr == 0 || probeTarget.sin_addr.s_addr == INADDR_BROADCAST) {
            inet_pton(AF_INET, "8.8.8.8", &probeTarget.sin_addr);
        }
        probeTarget.sin_port = htons(53);

        if (connect(probeSock, reinterpret_cast<struct sockaddr*>(&probeTarget), sizeof(probeTarget)) == 0) {
            sockaddr_in localAddr {};
            SockOptLenType addrLen = sizeof(localAddr);
            if (getsockname(probeSock, reinterpret_cast<struct sockaddr*>(&localAddr), &addrLen) == 0) {
                char ipStr[INET_ADDRSTRLEN] { 0 };
                inet_ntop(AF_INET, &localAddr.sin_addr, ipStr, sizeof(ipStr));
                CLOSE_SOCKET(probeSock);
                return std::string(ipStr);
            }
        }

        CLOSE_SOCKET(probeSock);
        return "127.0.0.1";
    }

} // namespace

WsDiscoveryServer::WsDiscoveryServer(OnvifServerConfig config)
    : m_config(std::move(config))
{
    if (m_config.serviceUuid.empty()) {
        m_config.serviceUuid = generateRandomUuid();
    }
}

WsDiscoveryServer::~WsDiscoveryServer()
{
    stop();
}

bool WsDiscoveryServer::start()
{
    if (m_running) {
        return true;
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }
#endif

    const SocketType sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == kInvalidSocket) {
        return false;
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in bindAddr {};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(kMulticastPort);
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, reinterpret_cast<struct sockaddr*>(&bindAddr), sizeof(bindAddr)) != 0) {
        CLOSE_SOCKET(sock);
        return false;
    }

    // Join WS-Discovery multicast group
    ip_mreq mreq {};
    inet_pton(AF_INET, kMulticastIp, &mreq.imr_multiaddr.s_addr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq));

    // Enable multicast loopback
    unsigned char loop = 1;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, reinterpret_cast<const char*>(&loop), sizeof(loop));

#ifdef _WIN32
    m_sockFd = static_cast<uintptr_t>(sock);
#else
    m_sockFd = sock;
#endif

    m_running = true;
    m_thread = std::thread(&WsDiscoveryServer::runListener, this);

    // Broadcast Hello announcement
    sockaddr_in mcastDest {};
    mcastDest.sin_family = AF_INET;
    mcastDest.sin_port = htons(kMulticastPort);
    inet_pton(AF_INET, kMulticastIp, &mcastDest.sin_addr);

    const std::string localIp = (m_config.bindAddress != "0.0.0.0" && !m_config.bindAddress.empty())
        ? m_config.bindAddress
        : resolveLocalIp(mcastDest);

    const std::string hello = createHelloPayload(localIp);
    sendto(sock, hello.data(), static_cast<SockBufLenType>(hello.size()), 0,
        reinterpret_cast<struct sockaddr*>(&mcastDest), sizeof(mcastDest));

    return true;
}

void WsDiscoveryServer::stop()
{
    if (!m_running) {
        return;
    }

    const SocketType sock = static_cast<SocketType>(m_sockFd);
    if (sock != kInvalidSocket) {
        // Broadcast Bye departure
        sockaddr_in mcastDest {};
        mcastDest.sin_family = AF_INET;
        mcastDest.sin_port = htons(kMulticastPort);
        inet_pton(AF_INET, kMulticastIp, &mcastDest.sin_addr);

        const std::string bye = createByePayload();
        sendto(sock, bye.data(), static_cast<SockBufLenType>(bye.size()), 0,
            reinterpret_cast<struct sockaddr*>(&mcastDest), sizeof(mcastDest));
    }

    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }

    if (sock != kInvalidSocket) {
        CLOSE_SOCKET(sock);
#ifdef _WIN32
        m_sockFd = ~static_cast<uintptr_t>(0);
#else
        m_sockFd = -1;
#endif
    }
}

bool WsDiscoveryServer::isRunning() const noexcept
{
    return m_running;
}

void WsDiscoveryServer::runListener()
{
    const SocketType sock = static_cast<SocketType>(m_sockFd);
    std::array<char, 8192> buffer {};

    while (m_running) {
#ifdef _WIN32
        WSAPOLLFD pfd {};
#else
        pollfd pfd {};
#endif
        pfd.fd = sock;
        pfd.events = POLLIN;

        const int pollRet = POLL_SOCKET(&pfd, 1, 200);
        if (pollRet <= 0 || !(pfd.revents & POLLIN)) {
            continue;
        }

        sockaddr_in senderAddr {};
        SockOptLenType senderLen = sizeof(senderAddr);
        const auto recvd = recvfrom(sock, buffer.data(), static_cast<SockBufLenType>(buffer.size() - 1), 0,
            reinterpret_cast<struct sockaddr*>(&senderAddr), &senderLen);

        if (recvd <= 0) {
            continue;
        }

        buffer[static_cast<size_t>(recvd)] = '\0';
        const std::string requestStr(buffer.data(), static_cast<size_t>(recvd));

        pugi::xml_document doc;
        const auto parseResult = doc.load_string(requestStr.c_str());
        if (!parseResult) {
            continue;
        }

        // Check if message is a Probe
        const pugi::xml_node actionNode = doc.select_node("//*[local-name()='Action']").node();
        const std::string actionText = actionNode ? actionNode.text().as_string() : "";

        if (actionText.find("http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe") == std::string::npos
            && actionText.find("Probe") == std::string::npos) {
            continue;
        }

        // Check Types filter if specified
        const pugi::xml_node typesNode = doc.select_node("//*[local-name()='Types']").node();
        if (typesNode) {
            const std::string typesText = typesNode.text().as_string();
            if (!typesText.empty() && typesText.find("NetworkVideoTransmitter") == std::string::npos
                && typesText.find("Device") == std::string::npos) {
                continue;
            }
        }

        // Extract MessageID to relate to
        const pugi::xml_node msgIdNode = doc.select_node("//*[local-name()='MessageID']").node();
        const std::string msgId = msgIdNode ? msgIdNode.text().as_string() : "";

        const std::string localIp = (m_config.bindAddress != "0.0.0.0" && !m_config.bindAddress.empty())
            ? m_config.bindAddress
            : resolveLocalIp(senderAddr);

        const std::string probeMatches = createProbeMatchesPayload(msgId, localIp);
        sendto(sock, probeMatches.data(), static_cast<SockBufLenType>(probeMatches.size()), 0,
            reinterpret_cast<struct sockaddr*>(&senderAddr), senderLen);
    }
}

std::string WsDiscoveryServer::createProbeMatchesPayload(
    const std::string& relatesToMessageId, const std::string& localIp) const
{
    const std::string responseUuid = generateRandomUuid();
    std::ostringstream oss;

    oss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
        << "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
        << "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
        << "xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" "
        << "xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\" "
        << "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
        << "  <SOAP-ENV:Header>\r\n"
        << "    <wsa:MessageID>urn:uuid:" << responseUuid << "</wsa:MessageID>\r\n";

    if (!relatesToMessageId.empty()) {
        oss << "    <wsa:RelatesTo>" << relatesToMessageId << "</wsa:RelatesTo>\r\n";
    }

    oss << "    <wsa:To>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:To>\r\n"
        << "    <wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatches</wsa:Action>\r\n"
        << "  </SOAP-ENV:Header>\r\n"
        << "  <SOAP-ENV:Body>\r\n"
        << "    <d:ProbeMatches>\r\n"
        << "      <d:ProbeMatch>\r\n"
        << "        <wsa:EndpointReference>\r\n"
        << "          <wsa:Address>urn:uuid:" << m_config.serviceUuid << "</wsa:Address>\r\n"
        << "        </wsa:EndpointReference>\r\n"
        << "        <d:Types>dn:NetworkVideoTransmitter tds:Device</d:Types>\r\n"
        << "        <d:Scopes>\r\n"
        << "          onvif://www.onvif.org/type/video_encoder\r\n"
        << "          onvif://www.onvif.org/type/ptz\r\n"
        << "          onvif://www.onvif.org/name/" << m_config.deviceName << "\r\n"
        << "          onvif://www.onvif.org/hardware/" << m_config.model << "\r\n";

    for (const auto& scope : m_config.scopes) {
        oss << "          " << scope << "\r\n";
    }

    oss << "        </d:Scopes>\r\n"
        << "        <d:XAddrs>http://" << localIp << ":" << m_config.port << "/onvif/device_service</d:XAddrs>\r\n"
        << "        <d:MetadataVersion>1</d:MetadataVersion>\r\n"
        << "      </d:ProbeMatch>\r\n"
        << "    </d:ProbeMatches>\r\n"
        << "  </SOAP-ENV:Body>\r\n"
        << "</SOAP-ENV:Envelope>\r\n";

    return oss.str();
}

std::string WsDiscoveryServer::createHelloPayload(const std::string& localIp) const
{
    const std::string msgUuid = generateRandomUuid();
    std::ostringstream oss;

    oss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
        << "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
        << "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
        << "xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" "
        << "xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\" "
        << "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
        << "  <SOAP-ENV:Header>\r\n"
        << "    <wsa:MessageID>urn:uuid:" << msgUuid << "</wsa:MessageID>\r\n"
        << "    <wsa:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To>\r\n"
        << "    <wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Hello</wsa:Action>\r\n"
        << "  </SOAP-ENV:Header>\r\n"
        << "  <SOAP-ENV:Body>\r\n"
        << "    <d:Hello>\r\n"
        << "      <wsa:EndpointReference>\r\n"
        << "        <wsa:Address>urn:uuid:" << m_config.serviceUuid << "</wsa:Address>\r\n"
        << "      </wsa:EndpointReference>\r\n"
        << "      <d:Types>dn:NetworkVideoTransmitter tds:Device</d:Types>\r\n"
        << "      <d:Scopes>\r\n"
        << "        onvif://www.onvif.org/type/video_encoder\r\n"
        << "        onvif://www.onvif.org/type/ptz\r\n"
        << "        onvif://www.onvif.org/name/" << m_config.deviceName << "\r\n"
        << "        onvif://www.onvif.org/hardware/" << m_config.model << "\r\n";

    for (const auto& scope : m_config.scopes) {
        oss << "        " << scope << "\r\n";
    }

    oss << "      </d:Scopes>\r\n"
        << "      <d:XAddrs>http://" << localIp << ":" << m_config.port << "/onvif/device_service</d:XAddrs>\r\n"
        << "      <d:MetadataVersion>1</d:MetadataVersion>\r\n"
        << "    </d:Hello>\r\n"
        << "  </SOAP-ENV:Body>\r\n"
        << "</SOAP-ENV:Envelope>\r\n";

    return oss.str();
}

std::string WsDiscoveryServer::createByePayload() const
{
    const std::string msgUuid = generateRandomUuid();
    std::ostringstream oss;

    oss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
        << "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
        << "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
        << "xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\">\r\n"
        << "  <SOAP-ENV:Header>\r\n"
        << "    <wsa:MessageID>urn:uuid:" << msgUuid << "</wsa:MessageID>\r\n"
        << "    <wsa:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To>\r\n"
        << "    <wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Bye</wsa:Action>\r\n"
        << "  </SOAP-ENV:Header>\r\n"
        << "  <SOAP-ENV:Body>\r\n"
        << "    <d:Bye>\r\n"
        << "      <wsa:EndpointReference>\r\n"
        << "        <wsa:Address>urn:uuid:" << m_config.serviceUuid << "</wsa:Address>\r\n"
        << "      </wsa:EndpointReference>\r\n"
        << "    </d:Bye>\r\n"
        << "  </SOAP-ENV:Body>\r\n"
        << "</SOAP-ENV:Envelope>\r\n";

    return oss.str();
}

} // namespace PelcoD::Onvif
