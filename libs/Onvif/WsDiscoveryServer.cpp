#include "WsDiscoveryServer.h"

#include <cstdint>
#include <cstring>

#include <array>
#include <chrono>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <Transport/SocketUtils.h>
#include <pugixml.hpp>

#include "WsDiscoveryCommon.h"
#include "XmlUtils.h"

namespace Onvif {

namespace Net = Transport::Net;

namespace {

    std::string resolveLocalIp(const sockaddr_in& targetAddr)
    {
        const Net::SocketHandle probeSock = socket(AF_INET, SOCK_DGRAM, 0);
        if (probeSock == Net::InvalidSocket) {
            return "127.0.0.1";
        }

        sockaddr_in probeTarget = targetAddr;
        if (probeTarget.sin_addr.s_addr == 0 || probeTarget.sin_addr.s_addr == INADDR_BROADCAST) {
            inet_pton(AF_INET, "8.8.8.8", &probeTarget.sin_addr);
        }
        probeTarget.sin_port = htons(53);

        if (connect(probeSock, reinterpret_cast<struct sockaddr*>(&probeTarget), sizeof(probeTarget)) == 0) {
            sockaddr_in localAddr {};
            Net::SockOptLenType addrLen = sizeof(localAddr);
            if (getsockname(probeSock, reinterpret_cast<struct sockaddr*>(&localAddr), &addrLen) == 0) {
                char ipStr[INET_ADDRSTRLEN] { 0 };
                inet_ntop(AF_INET, &localAddr.sin_addr, ipStr, sizeof(ipStr));
                Net::closeSocket(probeSock);
                return std::string(ipStr);
            }
        }

        Net::closeSocket(probeSock);
        return "127.0.0.1";
    }

} // namespace

WsDiscoveryServer::WsDiscoveryServer(OnvifServerConfig config)
    : m_config(std::move(config))
    , m_discoveryMode(m_config.discoveryMode)
    , m_rateLimiter(m_config.discovery.maxResponsesPerSecPerIp, m_config.discovery.maxGlobalResponsesPerSec)
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

    Net::ensureWinsockInitialized();

    const Net::SocketHandle sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == Net::InvalidSocket) {
        return false;
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in bindAddr {};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(kMulticastPort);
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, reinterpret_cast<struct sockaddr*>(&bindAddr), sizeof(bindAddr)) != 0) {
        Net::closeSocket(sock);
        return false;
    }

    // Join WS-Discovery multicast group
    ip_mreq mreq {};
    inet_pton(AF_INET, kMulticastIp, &mreq.imr_multiaddr.s_addr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq));

    // Multicast loopback control (disabled by default to prevent self-amplification, C7)
    const unsigned char loop = m_config.discovery.enableMulticastLoopback ? 1 : 0;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, reinterpret_cast<const char*>(&loop), sizeof(loop));

    m_sockFd = sock;
    m_running = true;
    m_thread = std::thread(&WsDiscoveryServer::runListener, this);

    // Broadcast Hello announcement if Discoverable
    if (m_discoveryMode.load() == DiscoveryMode::Discoverable) {
        broadcastHello();
    }

    return true;
}

void WsDiscoveryServer::broadcastHello()
{
    const Net::SocketHandle sock = m_sockFd;
    if (sock == Net::InvalidSocket) {
        return;
    }

    sockaddr_in mcastDest {};
    mcastDest.sin_family = AF_INET;
    mcastDest.sin_port = htons(kMulticastPort);
    inet_pton(AF_INET, kMulticastIp, &mcastDest.sin_addr);

    const std::string localIp = (m_config.bindAddress != "0.0.0.0" && !m_config.bindAddress.empty())
        ? m_config.bindAddress
        : resolveLocalIp(mcastDest);

    const std::string hello = createHelloPayload(localIp);
    sendto(sock, hello.data(), static_cast<Net::SockBufLenType>(hello.size()), 0,
        reinterpret_cast<struct sockaddr*>(&mcastDest), sizeof(mcastDest));
}

void WsDiscoveryServer::setDiscoveryMode(DiscoveryMode mode) noexcept
{
    m_discoveryMode.store(mode);
}

DiscoveryMode WsDiscoveryServer::getDiscoveryMode() const noexcept
{
    return m_discoveryMode.load();
}

std::uint32_t WsDiscoveryServer::getMetadataVersion() const noexcept
{
    std::scoped_lock lock(m_scopeMutex);
    return m_metadataVersion;
}

std::vector<std::string> WsDiscoveryServer::getActiveScopes() const
{
    std::vector<std::string> scopes;
    scopes.push_back("onvif://www.onvif.org/type/video_encoder");
    scopes.push_back("onvif://www.onvif.org/type/ptz");
    scopes.push_back("onvif://www.onvif.org/name/" + m_config.deviceName);
    scopes.push_back("onvif://www.onvif.org/hardware/" + m_config.model);
    {
        std::scoped_lock lock(m_scopeMutex);
        for (const auto& s : m_config.scopes) {
            scopes.push_back(s);
        }
    }
    return scopes;
}

void WsDiscoveryServer::stop()
{
    if (!m_running) {
        return;
    }

    const Net::SocketHandle sock = m_sockFd;
    if (sock != Net::InvalidSocket) {
        // Broadcast Bye departure
        sockaddr_in mcastDest {};
        mcastDest.sin_family = AF_INET;
        mcastDest.sin_port = htons(kMulticastPort);
        inet_pton(AF_INET, kMulticastIp, &mcastDest.sin_addr);

        const std::string bye = createByePayload();
        sendto(sock, bye.data(), static_cast<Net::SockBufLenType>(bye.size()), 0,
            reinterpret_cast<struct sockaddr*>(&mcastDest), sizeof(mcastDest));
    }

    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }

    if (sock != Net::InvalidSocket) {
        Net::closeSocket(sock);
        m_sockFd = Net::InvalidSocket;
    }
}

bool WsDiscoveryServer::isRunning() const noexcept
{
    return m_running;
}

void WsDiscoveryServer::runListener()
{
    const Net::SocketHandle sock = m_sockFd;
    std::array<char, 8192> buffer {};

    while (m_running) {
        Net::PollFd pfd {};
        pfd.fd = sock;
        pfd.events = POLLIN;

        const int pollRet = Net::pollSockets(&pfd, 1, 200);
        if (pollRet <= 0 || !(pfd.revents & POLLIN)) {
            continue;
        }

        sockaddr_in senderAddr {};
        Net::SockOptLenType senderLen = sizeof(senderAddr);
        const auto recvd = recvfrom(sock, buffer.data(), static_cast<Net::SockBufLenType>(buffer.size() - 1), 0,
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

        // If discovery mode is NonDiscoverable, suppress responding to probes
        if (m_discoveryMode.load() == DiscoveryMode::NonDiscoverable) {
            continue;
        }

        // Strict validation against WS-Discovery §5 and self-amplification defense (C7)
        const auto activeScopes = getActiveScopes();
        const auto senderPort = ntohs(senderAddr.sin_port);
        const auto valResult = WsDiscoveryValidator::validateProbe(
            doc, senderPort, m_config.serviceUuid, m_config.discovery.dropReflectionPort3702, activeScopes);

        if (!valResult.isValidProbe) {
            continue;
        }

        // Token-bucket rate limiting mitigation against UDP amplification (CWE-406)
        char senderIpStr[INET_ADDRSTRLEN] { 0 };
        inet_ntop(AF_INET, &senderAddr.sin_addr, senderIpStr, sizeof(senderIpStr));
        if (m_config.discovery.enableRateLimiting && !m_rateLimiter.checkRateLimit(senderIpStr)) {
            continue;
        }

        // Random jitter delay APP_MAX_DELAY (0 to maxAppDelayMs) per WS-Discovery §7.2 / Profile S §6.1
        if (m_config.discovery.maxAppDelayMs > 0U && m_running) {
            std::random_device rd {};
            std::mt19937 gen(rd());
            std::uniform_int_distribution<std::uint32_t> dis(0U, m_config.discovery.maxAppDelayMs);
            const auto delayMs = dis(gen);
            const auto startWait = std::chrono::steady_clock::now();
            while (m_running && (std::chrono::steady_clock::now() - startWait < std::chrono::milliseconds(delayMs))) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (!m_running) {
                break;
            }
        }

        const std::string localIp = (m_config.bindAddress != "0.0.0.0" && !m_config.bindAddress.empty())
            ? m_config.bindAddress
            : resolveLocalIp(senderAddr);

        const std::string probeMatches = createProbeMatchesPayload(valResult.messageId, localIp);
        sendto(sock, probeMatches.data(), static_cast<Net::SockBufLenType>(probeMatches.size()), 0,
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
        oss << "    <wsa:RelatesTo>" << Xml::escapeXml(relatesToMessageId) << "</wsa:RelatesTo>\r\n";
    }

    oss << "    <wsa:To>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:To>\r\n"
        << "    <wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatches</wsa:Action>\r\n"
        << m_appSequence.formatHeader() << "  </SOAP-ENV:Header>\r\n"
        << "  <SOAP-ENV:Body>\r\n"
        << "    <d:ProbeMatches>\r\n"
        << "      <d:ProbeMatch>\r\n"
        << "        <wsa:EndpointReference>\r\n"
        << "          <wsa:Address>urn:uuid:" << Xml::escapeXml(m_config.serviceUuid) << "</wsa:Address>\r\n"
        << "        </wsa:EndpointReference>\r\n"
        << "        <d:Types>dn:NetworkVideoTransmitter tds:Device</d:Types>\r\n"
        << "        <d:Scopes>\r\n"
        << "          onvif://www.onvif.org/type/video_encoder\r\n"
        << "          onvif://www.onvif.org/type/ptz\r\n"
        << "          onvif://www.onvif.org/name/" << Xml::escapeXml(m_config.deviceName) << "\r\n"
        << "          onvif://www.onvif.org/hardware/" << Xml::escapeXml(m_config.model) << "\r\n";

    {
        std::scoped_lock lock(m_scopeMutex);
        for (const auto& scope : m_config.scopes) {
            oss << "          " << Xml::escapeXml(scope) << "\r\n";
        }
    }

    oss << "        </d:Scopes>\r\n"
        << "        <d:XAddrs>http://" << Xml::escapeXml(localIp) << ":" << m_config.port
        << "/onvif/device_service</d:XAddrs>\r\n"
        << "        <d:MetadataVersion>" << getMetadataVersion() << "</d:MetadataVersion>\r\n"
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
        << m_appSequence.formatHeader() << "  </SOAP-ENV:Header>\r\n"
        << "  <SOAP-ENV:Body>\r\n"
        << "    <d:Hello>\r\n"
        << "      <wsa:EndpointReference>\r\n"
        << "        <wsa:Address>urn:uuid:" << Xml::escapeXml(m_config.serviceUuid) << "</wsa:Address>\r\n"
        << "      </wsa:EndpointReference>\r\n"
        << "      <d:Types>dn:NetworkVideoTransmitter tds:Device</d:Types>\r\n"
        << "      <d:Scopes>\r\n"
        << "        onvif://www.onvif.org/type/video_encoder\r\n"
        << "        onvif://www.onvif.org/type/ptz\r\n"
        << "        onvif://www.onvif.org/name/" << Xml::escapeXml(m_config.deviceName) << "\r\n"
        << "        onvif://www.onvif.org/hardware/" << Xml::escapeXml(m_config.model) << "\r\n";

    {
        std::scoped_lock lock(m_scopeMutex);
        for (const auto& scope : m_config.scopes) {
            oss << "        " << Xml::escapeXml(scope) << "\r\n";
        }
    }

    oss << "      </d:Scopes>\r\n"
        << "      <d:XAddrs>http://" << Xml::escapeXml(localIp) << ":" << m_config.port
        << "/onvif/device_service</d:XAddrs>\r\n"
        << "      <d:MetadataVersion>" << getMetadataVersion() << "</d:MetadataVersion>\r\n"
        << "    </d:Hello>\r\n"
        << "  </SOAP-ENV:Body>\r\n"
        << "</SOAP-ENV:Envelope>\r\n";

    return oss.str();
}

void WsDiscoveryServer::updateScopes(const std::vector<std::string>& scopes)
{
    {
        std::scoped_lock lock(m_scopeMutex);
        m_config.scopes = scopes;
        ++m_metadataVersion;
    }
    if (m_running && (m_discoveryMode.load() == DiscoveryMode::Discoverable)) {
        broadcastHello();
    }
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
        << m_appSequence.formatHeader() << "  </SOAP-ENV:Header>\r\n"
        << "  <SOAP-ENV:Body>\r\n"
        << "    <d:Bye>\r\n"
        << "      <wsa:EndpointReference>\r\n"
        << "        <wsa:Address>urn:uuid:" << Xml::escapeXml(m_config.serviceUuid) << "</wsa:Address>\r\n"
        << "      </wsa:EndpointReference>\r\n"
        << "    </d:Bye>\r\n"
        << "  </SOAP-ENV:Body>\r\n"
        << "</SOAP-ENV:Envelope>\r\n";

    return oss.str();
}

} // namespace Onvif
