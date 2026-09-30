/// @file SightlineUdpTransport.cpp
/// @brief Implementation of dual-port UDP socket transport for Sightline SLA protocols.

#include "SightlineUdpTransport.h"

#include <glog/logging.h>

#ifndef _WIN32
#include <sys/ioctl.h>
#endif

namespace Transport {

SightlineUdpTransport::SightlineUdpTransport(std::string host, std::uint16_t commandPort, std::uint16_t replyPort)
    : m_host(std::move(host))
    , m_commandPort(commandPort)
    , m_replyPort(replyPort)
{
}

SightlineUdpTransport::~SightlineUdpTransport()
{
    close();
}

void SightlineUdpTransport::setHost(const std::string& host)
{
    if (isOpen()) {
        LOG(WARNING) << "SightlineUdpTransport: Cannot change host while open.";
        return;
    }
    m_host = host;
}

std::string SightlineUdpTransport::getHost() const
{
    return m_host;
}

void SightlineUdpTransport::setCommandPort(std::uint16_t port)
{
    if (isOpen()) {
        LOG(WARNING) << "SightlineUdpTransport: Cannot change command port while open.";
        return;
    }
    m_commandPort = port;
}

std::uint16_t SightlineUdpTransport::getCommandPort() const noexcept
{
    return m_commandPort;
}

void SightlineUdpTransport::setReplyPort(std::uint16_t port)
{
    if (isOpen()) {
        LOG(WARNING) << "SightlineUdpTransport: Cannot change reply port while open.";
        return;
    }
    m_replyPort = port;
}

std::uint16_t SightlineUdpTransport::getReplyPort() const noexcept
{
    return m_replyPort;
}

bool SightlineUdpTransport::isOpen() const noexcept
{
    return m_running.load() && (m_txSock.load() != InvalidSocket) && (m_rxSock.load() != InvalidSocket);
}

bool SightlineUdpTransport::open()
{
    if (isOpen()) {
        close();
    }

    if (m_host.empty()) {
        const std::string err = "Destination host cannot be empty";
        LOG(ERROR) << "SightlineUdpTransport: " << err;
        notifyState(TransportState::Error, err);
        return false;
    }

    if (m_commandPort == 0U || m_replyPort == 0U) {
        const std::string err = "Command and reply ports cannot be 0";
        LOG(ERROR) << "SightlineUdpTransport: " << err;
        notifyState(TransportState::Error, err);
        return false;
    }

    Net::ensureWinsockInitialized();
    notifyState(TransportState::Connecting, "");

    // Resolve remote destination address
    struct addrinfo hints { };
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    const std::string cmdPortStr = std::to_string(m_commandPort);
    struct addrinfo* result = nullptr;
    const int gaiErr = ::getaddrinfo(m_host.c_str(), cmdPortStr.c_str(), &hints, &result);
    if (gaiErr != 0 || result == nullptr) {
        const std::string err = "Failed to resolve host " + m_host + ": " + gai_strerror(gaiErr);
        LOG(ERROR) << "SightlineUdpTransport: " << err;
        notifyState(TransportState::Error, err);
        return false;
    }

    // Save destination address for sendto
    std::memcpy(&m_destAddr, result->ai_addr, result->ai_addrlen);
    m_destAddrLen = static_cast<socklen_t>(result->ai_addrlen);
    const int family = result->ai_family;
    ::freeaddrinfo(result);

    // 1. Create TX socket
    const auto tx = static_cast<SocketHandle>(::socket(family, SOCK_DGRAM, IPPROTO_UDP));
    if (tx == InvalidSocket) {
        const std::string err = "Failed to create TX UDP socket: " + Net::getSocketErrorString();
        LOG(ERROR) << "SightlineUdpTransport: " << err;
        notifyState(TransportState::Error, err);
        return false;
    }

    // 2. Create RX listener socket
    const auto rx = static_cast<SocketHandle>(::socket(family, SOCK_DGRAM, IPPROTO_UDP));
    if (rx == InvalidSocket) {
        Net::closeSocket(tx);
        const std::string err = "Failed to create RX UDP socket: " + Net::getSocketErrorString();
        LOG(ERROR) << "SightlineUdpTransport: " << err;
        notifyState(TransportState::Error, err);
        return false;
    }

    // Allow local address/port reuse
    const int reuse = 1;
    ::setsockopt(rx, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    // Bind RX socket to local port
    bool bindOk = false;
    if (family == AF_INET) {
        struct sockaddr_in addr4 { };
        addr4.sin_family = AF_INET;
        addr4.sin_addr.s_addr = htonl(INADDR_ANY);
        addr4.sin_port = htons(m_replyPort);
        bindOk = (::bind(rx, reinterpret_cast<struct sockaddr*>(&addr4), sizeof(addr4)) == 0);
    } else if (family == AF_INET6) {
        struct sockaddr_in6 addr6 { };
        addr6.sin6_family = AF_INET6;
        addr6.sin6_addr = in6addr_any;
        addr6.sin6_port = htons(m_replyPort);
        bindOk = (::bind(rx, reinterpret_cast<struct sockaddr*>(&addr6), sizeof(addr6)) == 0);
    }

    if (!bindOk) {
        const std::string err = "Failed to bind RX UDP socket to port " + std::to_string(m_replyPort) + ": "
            + Net::getSocketErrorString();
        LOG(ERROR) << "SightlineUdpTransport: " << err;
        Net::closeSocket(tx);
        Net::closeSocket(rx);
        notifyState(TransportState::Error, err);
        return false;
    }

    if (!Net::setNonBlocking(rx, true)) {
        LOG(WARNING) << "SightlineUdpTransport: Failed to set non-blocking RX mode: " << Net::getSocketErrorString();
    }

    m_txSock.store(tx);
    m_rxSock.store(rx);
    m_running.store(true);

    m_readThread = std::thread(&SightlineUdpTransport::readWorker, this);

    LOG(INFO) << "SightlineUdpTransport: Connected to " << m_host << ":" << m_commandPort
              << " (listening on local port " << m_replyPort << ")";
    notifyState(TransportState::Connected, "");
    return true;
}

void SightlineUdpTransport::close()
{
    if (!m_running.exchange(false)) {
        return;
    }

    const auto tx = m_txSock.exchange(InvalidSocket);
    if (tx != InvalidSocket) {
        Net::closeSocket(tx);
    }

    const auto rx = m_rxSock.exchange(InvalidSocket);
    if (rx != InvalidSocket) {
        Net::closeSocket(rx);
    }

    stopReadThread();
    notifyState(TransportState::Disconnected, "");
}

bool SightlineUdpTransport::sendData(const std::vector<std::uint8_t>& data)
{
    if (!isOpen() || data.empty()) {
        return false;
    }

    std::scoped_lock lock(m_writeMutex);
    const auto sock = m_txSock.load();
    if (sock == InvalidSocket) {
        return false;
    }

    const auto bytesSent
        = ::sendto(sock, reinterpret_cast<const char*>(data.data()), static_cast<Net::SockBufLenType>(data.size()),
            Net::SendFlags, reinterpret_cast<const sockaddr*>(&m_destAddr), m_destAddrLen);

    if (bytesSent < 0 || static_cast<std::size_t>(bytesSent) != data.size()) {
        recordTxError();
        LOG(WARNING) << "SightlineUdpTransport: sendto error: " << Net::getSocketErrorString();
        return false;
    }

    recordBytesSent(data.size());
    return true;
}

void SightlineUdpTransport::readWorker()
{
    std::vector<std::uint8_t> rxBuffer(4096U);

    while (m_running.load()) {
        const auto sock = m_rxSock.load();
        if (sock == InvalidSocket) {
            break;
        }

        Net::PollFd pfd {};
        pfd.fd = static_cast<decltype(pfd.fd)>(sock);
        pfd.events = POLLIN;

        const int pollResult = Net::pollSockets(&pfd, 1, 100);

        if (!m_running.load()) {
            break;
        }

        if (pollResult < 0) {
            if (!m_running.load()) {
                break;
            }
            LOG(WARNING) << "SightlineUdpTransport: poll error: " << Net::getSocketErrorString();
            continue;
        }

        if (pollResult == 0) {
            continue;
        }

        if (pfd.revents & POLLIN) {
            struct sockaddr_storage srcAddr { };
            socklen_t srcLen = sizeof(srcAddr);

            const auto bytesRead = ::recvfrom(sock, reinterpret_cast<char*>(rxBuffer.data()),
                static_cast<Net::SockBufLenType>(rxBuffer.size()), 0, reinterpret_cast<sockaddr*>(&srcAddr), &srcLen);

            if (bytesRead > 0) {
                const auto count = static_cast<std::size_t>(bytesRead);
                recordBytesReceived(count);
                const std::vector<std::uint8_t> received(
                    rxBuffer.begin(), rxBuffer.begin() + static_cast<std::ptrdiff_t>(count));
                invokeDataCallback(received);
            } else if (bytesRead < 0) {
                if (!Net::isWouldBlock()) {
                    recordRxError();
                    LOG(WARNING) << "SightlineUdpTransport: recvfrom error: " << Net::getSocketErrorString();
                }
            }
        }
    }
}

TransportStatsSnapshot SightlineUdpTransport::getStats() const
{
    TransportStatsSnapshot snap = BaseTransport::getStats();
    snap.udp = queryKernelStats();
    return snap;
}

UdpKernelStats SightlineUdpTransport::queryKernelStats() const noexcept
{
    UdpKernelStats stats {};
    const auto sock = m_rxSock.load();
    if (sock == InvalidSocket) {
        return stats;
    }

#ifdef _WIN32
    u_long pendingBytes = 0;
    if (::ioctlsocket(sock, FIONREAD, &pendingBytes) == 0) {
        stats.queuedRxBytes = static_cast<std::uint32_t>(pendingBytes);
        stats.supported = true;
    }

    int rcvBuf = 0;
    int optLen = sizeof(rcvBuf);
    if (::getsockopt(sock, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&rcvBuf), &optLen) == 0 && rcvBuf >= 0) {
        stats.socketRxBufferSize = static_cast<std::uint32_t>(rcvBuf);
        stats.supported = true;
    }

    int sndBuf = 0;
    optLen = sizeof(sndBuf);
    if (::getsockopt(sock, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&sndBuf), &optLen) == 0 && sndBuf >= 0) {
        stats.socketTxBufferSize = static_cast<std::uint32_t>(sndBuf);
        stats.supported = true;
    }
#else
#ifdef SIOCINQ
    int pending = 0;
    if (::ioctl(sock, SIOCINQ, &pending) == 0 && pending >= 0) {
        stats.queuedRxBytes = static_cast<std::uint32_t>(pending);
        stats.supported = true;
    }
#endif

    int rcvBuf = 0;
    socklen_t optLen = sizeof(rcvBuf);
    if (::getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvBuf, &optLen) == 0 && rcvBuf >= 0) {
        stats.socketRxBufferSize = static_cast<std::uint32_t>(rcvBuf);
        stats.supported = true;
    }

    int sndBuf = 0;
    optLen = sizeof(sndBuf);
    if (::getsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sndBuf, &optLen) == 0 && sndBuf >= 0) {
        stats.socketTxBufferSize = static_cast<std::uint32_t>(sndBuf);
        stats.supported = true;
    }

#ifdef SO_RXQ_OVFL
    unsigned int drops = 0;
    optLen = sizeof(drops);
    if (::getsockopt(sock, SOL_SOCKET, SO_RXQ_OVFL, &drops, &optLen) == 0) {
        stats.rxDroppedPackets = static_cast<std::uint64_t>(drops);
        stats.supported = true;
    }
#endif
#endif

    return stats;
}

} // namespace Transport
