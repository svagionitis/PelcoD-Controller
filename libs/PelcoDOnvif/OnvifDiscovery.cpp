#include "OnvifDiscovery.h"

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

        std::ostringstream ss {};
        ss << std::hex << std::setfill('0') << std::setw(8) << d1 << "-" << std::setw(4) << d2 << "-" << std::setw(4)
           << d3 << "-" << std::setw(4) << d4 << "-" << std::setw(8) << d5a << std::setw(4) << d5b;
        return ss.str();
    }

    std::string urlDecode(const std::string& in)
    {
        std::string out {};
        out.reserve(in.size());
        for (size_t i = 0; i < in.size(); ++i) {
            if (in[i] == '%' && i + 2 < in.size()) {
                const int hexVal = std::stoi(in.substr(i + 1, 2), nullptr, 16);
                out.push_back(static_cast<char>(hexVal));
                i += 2;
            } else if (in[i] == '+') {
                out.push_back(' ');
            } else {
                out.push_back(in[i]);
            }
        }
        return out;
    }

    std::string extractIpFromUrl(const std::string& url)
    {
        // Search for scheme delimiter "://"
        const auto schemePos = url.find("://");
        if (schemePos == std::string::npos) {
            return {};
        }
        const size_t hostStart = schemePos + 3;
        const auto hostEnd = url.find_first_of(":/", hostStart);
        if (hostEnd == std::string::npos) {
            return url.substr(hostStart);
        }
        return url.substr(hostStart, hostEnd - hostStart);
    }

    pugi::xml_node findNodeWithSuffix(const pugi::xml_node& parent, const std::string& suffix)
    {
        for (const auto& child : parent.children()) {
            const std::string name = child.name();
            const auto colonPos = name.find(':');
            const std::string localName = (colonPos != std::string::npos) ? name.substr(colonPos + 1) : name;
            if (localName == suffix) {
                return child;
            }
        }
        return {};
    }

    void collectNodesWithSuffix(
        const pugi::xml_node& parent, const std::string& suffix, std::vector<pugi::xml_node>& result)
    {
        for (const auto& child : parent.children()) {
            const std::string name = child.name();
            const auto colonPos = name.find(':');
            const std::string localName = (colonPos != std::string::npos) ? name.substr(colonPos + 1) : name;
            if (localName == suffix) {
                result.push_back(child);
            }
            collectNodesWithSuffix(child, suffix, result);
        }
    }

} // namespace

std::string OnvifDiscovery::createProbePayload(const std::string& messageUuid)
{
    const std::string uuid = messageUuid.empty() ? generateRandomUuid() : messageUuid;

    std::ostringstream ss {};
    ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
       << "<e:Envelope xmlns:e=\"http://www.w3.org/2003/05/soap-envelope\" "
       << "xmlns:w=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
       << "xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" "
       << "xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\">\n"
       << "  <e:Header>\n"
       << "    <w:MessageID>uuid:" << uuid << "</w:MessageID>\n"
       << "    <w:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</w:To>\n"
       << "    <w:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</w:Action>\n"
       << "  </e:Header>\n"
       << "  <e:Body>\n"
       << "    <d:Probe>\n"
       << "      <d:Types>dn:NetworkVideoTransmitter</d:Types>\n"
       << "    </d:Probe>\n"
       << "  </e:Body>\n"
       << "</e:Envelope>";
    return ss.str();
}

std::vector<DiscoveredDevice> OnvifDiscovery::parseProbeMatches(
    const std::string& xmlResponse, const std::string& senderIp)
{
    std::vector<DiscoveredDevice> devices {};

    pugi::xml_document doc {};
    const pugi::xml_parse_result result = doc.load_string(xmlResponse.c_str());
    if (!result) {
        return devices;
    }

    std::vector<pugi::xml_node> probeMatchNodes {};
    collectNodesWithSuffix(doc, "ProbeMatch", probeMatchNodes);

    for (const auto& matchNode : probeMatchNodes) {
        DiscoveredDevice dev {};

        const auto xAddrsNode = findNodeWithSuffix(matchNode, "XAddrs");
        if (xAddrsNode) {
            const std::string xAddrsStr = xAddrsNode.text().as_string();
            // XAddrs can contain multiple space-separated URIs; pick first valid HTTP/HTTPS URI
            std::istringstream iss(xAddrsStr);
            std::string token {};
            while (iss >> token) {
                if (token.rfind("http://", 0) == 0 || token.rfind("https://", 0) == 0) {
                    if (dev.endpoint.empty()) {
                        dev.endpoint = token;
                    }
                }
            }
        }

        if (dev.endpoint.empty()) {
            continue;
        }

        const std::string extractedIp = extractIpFromUrl(dev.endpoint);
        dev.ip = !extractedIp.empty() ? extractedIp : senderIp;

        const auto scopesNode = findNodeWithSuffix(matchNode, "Scopes");
        if (scopesNode) {
            const std::string scopesStr = scopesNode.text().as_string();
            std::istringstream iss(scopesStr);
            std::string scopeUri {};
            while (iss >> scopeUri) {
                dev.scopes.push_back(scopeUri);

                constexpr const char* kNamePrefix = "onvif://www.onvif.org/name/";
                constexpr const char* kHwPrefix = "onvif://www.onvif.org/hardware/";
                constexpr const char* kLocPrefix = "onvif://www.onvif.org/location/";

                if (scopeUri.rfind(kNamePrefix, 0) == 0) {
                    dev.name = urlDecode(scopeUri.substr(std::strlen(kNamePrefix)));
                } else if (scopeUri.rfind(kHwPrefix, 0) == 0) {
                    dev.hardware = urlDecode(scopeUri.substr(std::strlen(kHwPrefix)));
                } else if (scopeUri.rfind(kLocPrefix, 0) == 0) {
                    dev.location = urlDecode(scopeUri.substr(std::strlen(kLocPrefix)));
                }
            }
        }

        if (dev.name.empty()) {
            dev.name = dev.hardware.empty() ? dev.ip : dev.hardware;
        }

        devices.push_back(std::move(dev));
    }

    return devices;
}

std::vector<DiscoveredDevice> OnvifDiscovery::discoverDevices(std::chrono::milliseconds timeout)
{
    std::vector<DiscoveredDevice> discovered {};

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return discovered;
    }
#endif

    const SocketType sockFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockFd == kInvalidSocket) {
#ifdef _WIN32
        WSACleanup();
#endif
        return discovered;
    }

    // Set SO_REUSEADDR
    int reuse = 1;
    setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    // Set multicast TTL
#ifdef _WIN32
    DWORD ttl = 4;
    setsockopt(sockFd, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char*>(&ttl), sizeof(ttl));
#else
    unsigned char ttl = 4;
    setsockopt(sockFd, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char*>(&ttl), sizeof(ttl));
#endif

    sockaddr_in destAddr {};
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(kMulticastPort);
    inet_pton(AF_INET, kMulticastIp, &destAddr.sin_addr);

    const std::string probePayload = createProbePayload();
    const auto sent = sendto(sockFd, probePayload.data(), static_cast<SockBufLenType>(probePayload.size()), 0,
        reinterpret_cast<struct sockaddr*>(&destAddr), sizeof(destAddr));

    if (sent < 0) {
        CLOSE_SOCKET(sockFd);
#ifdef _WIN32
        WSACleanup();
#endif
        return discovered;
    }

    const auto startTime = std::chrono::steady_clock::now();
    std::array<char, 16384> buffer {};

    while (true) {
        const auto elapsed
            = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
        if (elapsed >= timeout) {
            break;
        }

        const int remainingMs = static_cast<int>((timeout - elapsed).count());
#ifdef _WIN32
        WSAPOLLFD pfd {};
#else
        pollfd pfd {};
#endif
        pfd.fd = sockFd;
        pfd.events = POLLIN;

        const int pollRet = POLL_SOCKET(&pfd, 1, remainingMs);
        if (pollRet <= 0) {
            break;
        }

        if (pfd.revents & POLLIN) {
            sockaddr_in senderAddr {};
            SockOptLenType senderLen = sizeof(senderAddr);
            const auto recvd = recvfrom(sockFd, buffer.data(), static_cast<SockBufLenType>(buffer.size() - 1), 0,
                reinterpret_cast<struct sockaddr*>(&senderAddr), &senderLen);

            if (recvd > 0) {
                buffer[static_cast<size_t>(recvd)] = '\0';
                char senderIpStr[INET_ADDRSTRLEN] { 0 };
                inet_ntop(AF_INET, &senderAddr.sin_addr, senderIpStr, sizeof(senderIpStr));

                const std::string responseStr(buffer.data(), static_cast<size_t>(recvd));
                const auto matches = parseProbeMatches(responseStr, senderIpStr);

                for (const auto& dev : matches) {
                    bool alreadyFound = false;
                    for (const auto& existing : discovered) {
                        if (existing.endpoint == dev.endpoint) {
                            alreadyFound = true;
                            break;
                        }
                    }
                    if (!alreadyFound) {
                        discovered.push_back(dev);
                    }
                }
            }
        }
    }

    CLOSE_SOCKET(sockFd);
#ifdef _WIN32
    WSACleanup();
#endif
    return discovered;
}

} // namespace PelcoD::Onvif
