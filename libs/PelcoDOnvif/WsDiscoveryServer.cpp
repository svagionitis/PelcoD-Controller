#include "WsDiscoveryServer.h"
#include "WsDiscoveryCommon.h"

#include <PelcoDCore/SocketUtils.h>
#include <pugixml.hpp>

#include <array>
#include <cstring>
#include <sstream>

namespace PelcoD::Onvif {

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

    // Enable multicast loopback
    unsigned char loop = 1;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, reinterpret_cast<const char*>(&loop), sizeof(loop));

    m_sockFd = sock;
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
    sendto(sock, hello.data(), static_cast<Net::SockBufLenType>(hello.size()), 0,
        reinterpret_cast<struct sockaddr*>(&mcastDest), sizeof(mcastDest));

    return true;
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
