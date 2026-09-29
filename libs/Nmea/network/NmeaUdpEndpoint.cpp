/// @file NmeaUdpEndpoint.cpp
/// @brief Implementation of NMEA 0183 UDP unicast and broadcast transceiver.

#include "NmeaUdpEndpoint.h"

#include <array>
#include <chrono>
#include <cstring>

namespace Nmea::Network {

NmeaUdpEndpoint::NmeaUdpEndpoint(std::uint16_t rxPort, std::string broadcastIp, std::uint16_t txPort)
    : m_rxPort { rxPort }
    , m_broadcastIp { std::move(broadcastIp) }
    , m_txPort { txPort }
{
}

NmeaUdpEndpoint::~NmeaUdpEndpoint()
{
    close();
}

bool NmeaUdpEndpoint::open()
{
    if (m_running.load()) {
        return true;
    }

    Transport::Net::ensureWinsockInitialized();

    const auto s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == Transport::Net::InvalidSocket) {
        return false;
    }

    int opt { 1 };
    ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
    ::setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in bindAddr {};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(m_rxPort);
    bindAddr.sin_addr.s_addr = INADDR_ANY;

    if (::bind(s, reinterpret_cast<const sockaddr*>(&bindAddr), sizeof(bindAddr)) != 0) {
        Transport::Net::closeSocket(s);
        return false;
    }

    Transport::Net::setNonBlocking(s, true);
    m_sockfd.store(s);
    m_running.store(true);

    m_rxThread = std::thread(&NmeaUdpEndpoint::rxWorkerLoop, this);
    return true;
}

void NmeaUdpEndpoint::close()
{
    if (!m_running.exchange(false)) {
        return;
    }

    const auto s = m_sockfd.exchange(Transport::Net::InvalidSocket);
    if (s != Transport::Net::InvalidSocket) {
        Transport::Net::closeSocket(s);
    }

    if (m_rxThread.joinable()) {
        m_rxThread.join();
    }
}

bool NmeaUdpEndpoint::isOpen() const noexcept
{
    return m_running.load() && m_sockfd.load() != Transport::Net::InvalidSocket;
}

bool NmeaUdpEndpoint::sendData(const std::vector<std::uint8_t>& data)
{
    const auto s = m_sockfd.load();
    if (s == Transport::Net::InvalidSocket || data.empty()) {
        return false;
    }

    sockaddr_in targetAddr {};
    targetAddr.sin_family = AF_INET;
    targetAddr.sin_port = htons(m_txPort);
    ::inet_pton(AF_INET, m_broadcastIp.c_str(), &targetAddr.sin_addr);

    const auto sent = ::sendto(s, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()),
                               Transport::Net::SendFlags, reinterpret_cast<const sockaddr*>(&targetAddr),
                               sizeof(targetAddr));
    return sent > 0;
}

bool NmeaUdpEndpoint::sendSentence(std::string_view sentence)
{
    std::string formatted { sentence };
    if (formatted.empty() || (formatted.back() != '\n' && formatted.back() != '\r')) {
        formatted += "\r\n";
    }

    const auto* pData = reinterpret_cast<const std::uint8_t*>(formatted.data());
    return sendData(std::vector<std::uint8_t>(pData, pData + formatted.size()));
}

std::uint16_t NmeaUdpEndpoint::getRxPort() const noexcept
{
    return m_rxPort;
}

std::uint16_t NmeaUdpEndpoint::getTxPort() const noexcept
{
    return m_txPort;
}

std::string NmeaUdpEndpoint::getBroadcastIp() const
{
    return m_broadcastIp;
}

void NmeaUdpEndpoint::rxWorkerLoop()
{
    std::array<char, 2048> buf {};
    while (m_running.load()) {
        const auto s = m_sockfd.load();
        if (s == Transport::Net::InvalidSocket) {
            break;
        }

        sockaddr_in fromAddr {};
        auto fromLen = static_cast<Transport::Net::SockOptLenType>(sizeof(fromAddr));
        const auto n = ::recvfrom(s, buf.data(), static_cast<int>(buf.size()), 0,
                                  reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);

        if (n > 0) {
            const auto* pBytes = reinterpret_cast<const std::uint8_t*>(buf.data());
            invokeDataCallback(std::vector<std::uint8_t>(pBytes, pBytes + n));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
}

} // namespace Nmea::Network
