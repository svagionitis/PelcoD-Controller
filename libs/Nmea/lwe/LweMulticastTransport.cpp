#include "LweMulticastTransport.h"
#include "NmeaTagBlockBuilder.h"
#include "NmeaTypes.h"

#include <glog/logging.h>

namespace Nmea::Lwe {

LweMulticastTransport::LweMulticastTransport(TransmissionGroup tg, std::string interfaceIp, std::string systemId)
    : LweMulticastTransport(
        getTransmissionGroupIp(tg), getTransmissionGroupPort(tg), std::move(interfaceIp), std::move(systemId))
{
}

LweMulticastTransport::LweMulticastTransport(
    std::string groupAddress, std::uint16_t port, std::string interfaceIp, std::string systemId)
    : m_groupAddress(std::move(groupAddress))
    , m_port(port)
    , m_interfaceIp(std::move(interfaceIp))
    , m_systemId(std::move(systemId))
{
    Transport::Net::ensureWinsockInitialized();
}

LweMulticastTransport::~LweMulticastTransport()
{
    close();
}

std::string LweMulticastTransport::getGroupAddress() const
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_groupAddress;
}

void LweMulticastTransport::setGroupAddress(const std::string& groupAddress)
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_groupAddress = groupAddress;
}

std::uint16_t LweMulticastTransport::getPort() const noexcept
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_port;
}

void LweMulticastTransport::setPort(std::uint16_t port) noexcept
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_port = port;
}

std::string LweMulticastTransport::getInterfaceIp() const
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_interfaceIp;
}

void LweMulticastTransport::setInterfaceIp(const std::string& interfaceIp)
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_interfaceIp = interfaceIp;
}

std::string LweMulticastTransport::getSystemId() const
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_systemId;
}

void LweMulticastTransport::setSystemId(const std::string& systemId)
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_systemId = systemId;
}

std::uint8_t LweMulticastTransport::getMulticastTtl() const noexcept
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_multicastTtl;
}

void LweMulticastTransport::setMulticastTtl(std::uint8_t ttl) noexcept
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_multicastTtl = ttl;
}

bool LweMulticastTransport::isLoopbackEnabled() const noexcept
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_loopbackEnabled;
}

void LweMulticastTransport::setLoopbackEnabled(bool enabled) noexcept
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_loopbackEnabled = enabled;
}

bool LweMulticastTransport::open()
{
    if (isOpen()) {
        return true;
    }

    std::string groupAddress;
    std::string interfaceIp;
    std::uint16_t port = 0;
    std::uint8_t ttl = 1;
    bool loopback = true;

    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        groupAddress = m_groupAddress;
        interfaceIp = m_interfaceIp;
        port = m_port;
        ttl = m_multicastTtl;
        loopback = m_loopbackEnabled;
    }

    if (groupAddress.empty() || port == 0U) {
        notifyState(Transport::TransportState::Error, "Invalid multicast group address or port");
        return false;
    }

    notifyState(Transport::TransportState::Connecting, "");

    const SocketHandle s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == InvalidSocket) {
        notifyState(
            Transport::TransportState::Error, "Failed to create UDP socket: " + Transport::Net::getSocketErrorString());
        return false;
    }

    // 1. Configure SO_REUSEADDR and SO_REUSEPORT to allow multiple processes/listeners on same group
    int reuse = 1;
    (void)setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#ifdef SO_REUSEPORT
    (void)setsockopt(s, SOL_SOCKET, SO_REUSEPORT, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#endif

    // 2. Bind socket to local port
    sockaddr_in bindAddr {};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(port);
#ifdef _WIN32
    bindAddr.sin_addr.s_addr
        = (interfaceIp == "0.0.0.0" || interfaceIp.empty()) ? htonl(INADDR_ANY) : inet_addr(interfaceIp.c_str());
#else
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
#endif

    if (bind(s, reinterpret_cast<const sockaddr*>(&bindAddr), sizeof(bindAddr)) < 0) {
        const std::string err = "Failed to bind UDP multicast socket to port " + std::to_string(port) + ": "
            + Transport::Net::getSocketErrorString();
        Transport::Net::closeSocket(s);
        notifyState(Transport::TransportState::Error, err);
        return false;
    }

    // 3. Join Multicast Group (IP_ADD_MEMBERSHIP)
    struct ip_mreq mreq { };
    mreq.imr_multiaddr.s_addr = inet_addr(groupAddress.c_str());
    if (interfaceIp.empty() || interfaceIp == "0.0.0.0") {
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    } else {
        mreq.imr_interface.s_addr = inet_addr(interfaceIp.c_str());
    }

    if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq)) < 0) {
        const std::string err
            = "Failed to join multicast group " + groupAddress + ": " + Transport::Net::getSocketErrorString();
        Transport::Net::closeSocket(s);
        notifyState(Transport::TransportState::Error, err);
        return false;
    }

    // 4. Multicast Loopback & TTL
    const unsigned char loopVal = loopback ? 1U : 0U;
    (void)setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, reinterpret_cast<const char*>(&loopVal), sizeof(loopVal));

    const unsigned char ttlVal = ttl;
    (void)setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char*>(&ttlVal), sizeof(ttlVal));

    // 5. Outgoing Multicast Interface
    if (!interfaceIp.empty() && interfaceIp != "0.0.0.0") {
        struct in_addr ifAddr { };
        ifAddr.s_addr = inet_addr(interfaceIp.c_str());
        (void)setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<const char*>(&ifAddr), sizeof(ifAddr));
    }

    // 6. Set non-blocking I/O
    if (!Transport::Net::setNonBlocking(s, true)) {
        LOG(WARNING) << "Failed to set non-blocking mode on LWE multicast socket";
    }

    m_sockfd.store(s);
    m_running.store(true);
    notifyState(Transport::TransportState::Connected, "");

    m_rxThread = std::thread(&LweMulticastTransport::rxWorkerLoop, this);
    return true;
}

void LweMulticastTransport::close()
{
    if (!isOpen() && m_sockfd.load() == InvalidSocket) {
        return;
    }

    m_running.store(false);

    if (m_rxThread.joinable()) {
        m_rxThread.join();
    }

    const SocketHandle s = m_sockfd.exchange(InvalidSocket);
    if (s != InvalidSocket) {
        std::string groupAddress;
        std::string interfaceIp;
        {
            std::lock_guard<std::mutex> lock(m_configMutex);
            groupAddress = m_groupAddress;
            interfaceIp = m_interfaceIp;
        }

        if (!groupAddress.empty()) {
            struct ip_mreq mreq { };
            mreq.imr_multiaddr.s_addr = inet_addr(groupAddress.c_str());
            if (interfaceIp.empty() || interfaceIp == "0.0.0.0") {
                mreq.imr_interface.s_addr = htonl(INADDR_ANY);
            } else {
                mreq.imr_interface.s_addr = inet_addr(interfaceIp.c_str());
            }
            (void)setsockopt(s, IPPROTO_IP, IP_DROP_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq));
        }

        Transport::Net::closeSocket(s);
    }

    notifyState(Transport::TransportState::Disconnected, "");
}

bool LweMulticastTransport::isOpen() const noexcept
{
    return m_running.load() && m_sockfd.load() != InvalidSocket;
}

bool LweMulticastTransport::sendData(const std::vector<std::uint8_t>& data)
{
    const SocketHandle s = m_sockfd.load();
    if (s == InvalidSocket || !m_running.load()) {
        return false;
    }

    std::string groupAddress;
    std::uint16_t port = 0;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        groupAddress = m_groupAddress;
        port = m_port;
    }

    sockaddr_in destAddr {};
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(port);
    destAddr.sin_addr.s_addr = inet_addr(groupAddress.c_str());

    const auto ret = sendto(s, reinterpret_cast<const char*>(data.data()),
        static_cast<Transport::Net::SockBufLenType>(data.size()), Transport::Net::SendFlags,
        reinterpret_cast<const sockaddr*>(&destAddr), sizeof(destAddr));

    return ret >= 0;
}

bool LweMulticastTransport::sendSentence(std::string_view sentence, bool attachTagBlock)
{
    std::string finalStr;
    if (attachTagBlock) {
        std::string systemId;
        {
            std::lock_guard<std::mutex> lock(m_configMutex);
            systemId = m_systemId;
        }

        NmeaTagBlock block {};
        block.sourceId = systemId;
        block.lineCount = m_nextSeqNumber.fetch_add(1U);

        finalStr = NmeaTagBlockBuilder::wrapSentence(block, sentence);
    } else {
        finalStr = std::string(sentence);
    }

    if (finalStr.size() < 2 || finalStr.substr(finalStr.size() - 2) != "\r\n") {
        finalStr += "\r\n";
    }

    const std::vector<std::uint8_t> payload(finalStr.begin(), finalStr.end());
    return sendData(payload);
}

void LweMulticastTransport::rxWorkerLoop()
{
    std::vector<std::uint8_t> buffer(kLweMaxDatagramSize + 256U);

    while (m_running.load()) {
        const SocketHandle s = m_sockfd.load();
        if (s == InvalidSocket) {
            break;
        }

        Transport::Net::PollFd pfd {};
        pfd.fd = s;
        pfd.events = POLLIN;
        pfd.revents = 0;

#ifdef _WIN32
        const int pollRet = WSAPoll(&pfd, 1, 50);
#else
        const int pollRet = poll(&pfd, 1, 50);
#endif

        if (pollRet < 0) {
            if (!m_running.load()) {
                break;
            }
            continue;
        }

        if (pollRet == 0) {
            // Timeout, loop to re-check m_running
            continue;
        }

        if ((pfd.revents & POLLIN) != 0) {
            sockaddr_in senderAddr {};
            Transport::Net::SockOptLenType senderLen = sizeof(senderAddr);

            const auto bytesRead = recvfrom(s, reinterpret_cast<char*>(buffer.data()),
                static_cast<Transport::Net::SockBufLenType>(buffer.size()), 0, reinterpret_cast<sockaddr*>(&senderAddr),
                &senderLen);

            if (bytesRead > 0) {
                const std::vector<std::uint8_t> datagram(
                    buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(bytesRead));
                invokeDataCallback(datagram);
            }
        }
    }
}

} // namespace Nmea::Lwe
