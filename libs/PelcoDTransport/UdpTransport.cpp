/// @file UdpTransport.cpp
/// @brief Implementation of cross-platform UDP socket transport.

#include "UdpTransport.h"
#include "SocketUtils.h"

#include <chrono>
#include <glog/logging.h>

namespace PelcoD::Transport {

UdpTransport::UdpTransport(std::string host, std::uint16_t port, std::uint16_t localPort)
    : m_host { std::move(host) }
    , m_port { port }
    , m_localPort { localPort }
{
    Net::ensureWinsockInitialized();
}

UdpTransport::~UdpTransport()
{
    close();
}

void UdpTransport::setHost(const std::string& host)
{
    if (isOpen()) {
        LOG(WARNING) << "UdpTransport: Cannot change host while transport is open.";
        return;
    }
    m_host = host;
}

std::string UdpTransport::getHost() const
{
    return m_host;
}

void UdpTransport::setPort(std::uint16_t port)
{
    if (isOpen()) {
        LOG(WARNING) << "UdpTransport: Cannot change port while transport is open.";
        return;
    }
    m_port = port;
}

std::uint16_t UdpTransport::getPort() const noexcept
{
    return m_port;
}

void UdpTransport::setLocalPort(std::uint16_t localPort)
{
    if (isOpen()) {
        LOG(WARNING) << "UdpTransport: Cannot change local port while transport is open.";
        return;
    }
    m_localPort = localPort;
}

std::uint16_t UdpTransport::getLocalPort() const noexcept
{
    return m_localPort;
}

bool UdpTransport::isOpen() const noexcept
{
    return m_running.load() && (m_sockfd.load() != InvalidSocket);
}

bool UdpTransport::open()
{
    if (isOpen()) {
        close();
    }

    if (m_host.empty()) {
        const std::string err = "Destination host cannot be empty";
        LOG(ERROR) << "UdpTransport: " << err;
        notifyState(TransportState::Error, err);
        return false;
    }

    if (m_port == 0U) {
        const std::string err = "Destination port cannot be 0";
        LOG(ERROR) << "UdpTransport: " << err;
        notifyState(TransportState::Error, err);
        return false;
    }

    Net::ensureWinsockInitialized();

    notifyState(TransportState::Connecting, "");

    struct addrinfo hints { };
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    const std::string portStr = std::to_string(m_port);
    struct addrinfo* result = nullptr;
    const int gaiErr = ::getaddrinfo(m_host.c_str(), portStr.c_str(), &hints, &result);
    if (gaiErr != 0 || result == nullptr) {
        const std::string err = "Failed to resolve host " + m_host + ": " + gai_strerror(gaiErr);
        LOG(ERROR) << "UdpTransport: " << err;
        notifyState(TransportState::Error, err);
        return false;
    }

    SocketHandle chosenSock { InvalidSocket };

    for (struct addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        const auto s = static_cast<SocketHandle>(::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol));
        if (s == InvalidSocket) {
            continue;
        }

        // Bind to local port if specified
        if (m_localPort > 0U) {
            bool bindOk = false;
            if (ptr->ai_family == AF_INET) {
                struct sockaddr_in addr4 { };
                addr4.sin_family = AF_INET;
                addr4.sin_addr.s_addr = htonl(INADDR_ANY);
                addr4.sin_port = htons(m_localPort);
                bindOk = (::bind(s, reinterpret_cast<struct sockaddr*>(&addr4), sizeof(addr4)) == 0);
            } else if (ptr->ai_family == AF_INET6) {
                struct sockaddr_in6 addr6 { };
                addr6.sin6_family = AF_INET6;
                addr6.sin6_addr = in6addr_any;
                addr6.sin6_port = htons(m_localPort);
                bindOk = (::bind(s, reinterpret_cast<struct sockaddr*>(&addr6), sizeof(addr6)) == 0);
            }

            if (!bindOk) {
                LOG(WARNING) << "UdpTransport: Failed to bind to local port " << m_localPort << ": "
                             << Net::getSocketErrorString();
                Net::closeSocket(s);
                continue;
            }
        }

        // Connect the UDP socket to destination address to establish default peer and filter incoming datagrams
        if (::connect(s, ptr->ai_addr, static_cast<socklen_t>(ptr->ai_addrlen)) != 0) {
            LOG(WARNING) << "UdpTransport: Failed to connect UDP socket to " << m_host << ":" << m_port << ": "
                         << Net::getSocketErrorString();
            Net::closeSocket(s);
            continue;
        }

        if (!Net::setNonBlocking(s, true)) {
            LOG(WARNING) << "UdpTransport: Failed to set non-blocking mode: " << Net::getSocketErrorString();
            Net::closeSocket(s);
            continue;
        }

        chosenSock = s;
        break;
    }

    ::freeaddrinfo(result);

    if (chosenSock == InvalidSocket) {
        const std::string err = "Failed to establish UDP socket to " + m_host + ":" + std::to_string(m_port);
        LOG(ERROR) << "UdpTransport: " << err;
        notifyState(TransportState::Error, err);
        return false;
    }

    m_sockfd.store(chosenSock);
    m_running.store(true);

    m_readThread = std::thread(&UdpTransport::readWorker, this);

    LOG(INFO) << "UdpTransport: Connected to " << m_host << ":" << m_port
              << (m_localPort > 0 ? " (local port " + std::to_string(m_localPort) + ")" : "");
    notifyState(TransportState::Connected, "");
    return true;
}

void UdpTransport::close()
{
    if (!m_running.exchange(false)) {
        return;
    }

    const auto sock = m_sockfd.exchange(InvalidSocket);
    if (sock != InvalidSocket) {
        Net::closeSocket(sock);
    }

    stopReadThread();
    notifyState(TransportState::Disconnected, "");
}

bool UdpTransport::sendData(const std::vector<std::uint8_t>& data)
{
    if (!isOpen() || data.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_writeMutex);
    const auto sock = m_sockfd.load();
    if (sock == InvalidSocket) {
        return false;
    }

    const auto bytesSent = ::send(sock, reinterpret_cast<const char*>(data.data()),
        static_cast<Net::SockBufLenType>(data.size()), Net::SendFlags);

    if (bytesSent < 0 || static_cast<std::size_t>(bytesSent) != data.size()) {
        LOG(WARNING) << "UdpTransport: send error: " << Net::getSocketErrorString();
        return false;
    }

    return true;
}

void UdpTransport::readWorker()
{
    std::vector<std::uint8_t> rxBuffer(2048U);

    while (m_running.load()) {
        const auto sock = m_sockfd.load();
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
            LOG(WARNING) << "UdpTransport: poll error: " << Net::getSocketErrorString();
            continue;
        }

        if (pollResult == 0) {
            continue;
        }

        if (pfd.revents & POLLIN) {
            const auto bytesRead = ::recv(
                sock, reinterpret_cast<char*>(rxBuffer.data()), static_cast<Net::SockBufLenType>(rxBuffer.size()), 0);

            if (bytesRead > 0) {
                const std::vector<std::uint8_t> packet(rxBuffer.begin(), rxBuffer.begin() + bytesRead);
                invokeDataCallback(packet);
            } else if (bytesRead < 0) {
                if (Net::isWouldBlock()) {
                    continue;
                }
                if (!m_running.load()) {
                    break;
                }
                LOG(WARNING) << "UdpTransport: recv error: " << Net::getSocketErrorString();
            }
        }
    }
}

} // namespace PelcoD::Transport
