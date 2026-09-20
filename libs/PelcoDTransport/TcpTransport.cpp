/// @file TcpTransport.cpp
/// @brief Implementation of cross-platform TCP socket transport.

#include "TcpTransport.h"
#include "SocketUtils.h"

#include <chrono>
#include <glog/logging.h>

namespace PelcoD::Transport {

TcpTransport::TcpTransport(std::string host, std::uint16_t port)
    : m_host { std::move(host) }
    , m_port { port }
{
    Net::ensureWinsockInitialized();
}

TcpTransport::~TcpTransport()
{
    close();
}

void TcpTransport::setHost(const std::string& host)
{
    std::lock_guard<std::mutex> lock(m_writeMutex);
    m_host = host;
}

std::string TcpTransport::getHost() const
{
    std::lock_guard<std::mutex> lock(m_writeMutex);
    return m_host;
}

void TcpTransport::setPort(std::uint16_t port)
{
    std::lock_guard<std::mutex> lock(m_writeMutex);
    m_port = port;
}

std::uint16_t TcpTransport::getPort() const noexcept
{
    return m_port;
}

void TcpTransport::setConnectTimeout(int ms) noexcept
{
    if (ms > 0) {
        m_connectTimeoutMs = ms;
    }
}

int TcpTransport::getConnectTimeout() const noexcept
{
    return m_connectTimeoutMs;
}

bool TcpTransport::open()
{
    close();

    std::string host;
    std::uint16_t port { 4001U };
    int timeoutMs { 5000 };
    {
        std::lock_guard<std::mutex> lock(m_writeMutex);
        host = m_host;
        port = m_port;
        timeoutMs = m_connectTimeoutMs;
    }

    if (host.empty() || host.size() > 255U || port == 0U) {
        LOG(ERROR) << "Cannot open TCP transport: invalid host or port";
        notifyState(TransportState::Error, "Invalid host or port");
        return false;
    }

    LOG(INFO) << "Connecting to TCP host " << host << ":" << port << " (timeout=" << timeoutMs << "ms)";
    notifyState(TransportState::Connecting, "Connecting to " + host + ":" + std::to_string(port));

    // --- DNS resolution (synchronous; entire open() must run off the UI thread) ---
    struct addrinfo hints { };
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo* res { nullptr };
    const std::string portStr = std::to_string(port);
    const int status = ::getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res);
    if (status != 0 || res == nullptr) {
        const std::string errStr = (status != 0) ? gai_strerror(status) : "Address resolution failed";
        LOG(ERROR) << "Failed to resolve host " << host << ": " << errStr;
        notifyState(TransportState::Error, "Resolve error: " + errStr);
        return false;
    }

    SocketHandle sock { InvalidSocket };
    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        sock = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock == InvalidSocket) {
            continue;
        }

        // Set non-blocking BEFORE connect() — the OS must not block the caller.
        if (!Net::setNonBlocking(sock, true)) {
            LOG(WARNING) << "setNonBlocking failed; connect may block";
        }

        const int connRet = ::connect(sock, p->ai_addr, static_cast<socklen_t>(p->ai_addrlen));

        bool connected { false };
        if (connRet == 0) {
            // Immediate success (e.g. loopback).
            connected = true;
        } else if (connRet < 0) {
            if (Net::isConnectInProgress()) {
                // Await writability within the configured timeout.
                Net::PollFd pfd {};
                pfd.fd = sock;
                pfd.events = POLLOUT;
                const int pollRet = Net::pollSockets(&pfd, 1, timeoutMs);
                if (pollRet > 0 && (pfd.revents & POLLOUT)) {
                    // Confirm via SO_ERROR.
                    int soErr { 0 };
                    socklen_t soErrLen = sizeof(soErr);
                    ::getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soErr), &soErrLen);
                    connected = (soErr == 0);
                    if (!connected) {
                        LOG(WARNING) << "connect SO_ERROR: " << Net::getSocketErrorString(soErr);
                    }
                } else if (pollRet == 0) {
                    LOG(WARNING) << "connect timed out after " << timeoutMs << "ms";
                    notifyState(TransportState::Error, "Connect timed out after " + std::to_string(timeoutMs) + "ms");
                }
            }
        }

        if (!connected) {
            Net::closeSocket(sock);
            sock = InvalidSocket;
            continue;
        }
        break; // Connected.
    }

    ::freeaddrinfo(res);

    if (sock == InvalidSocket) {
        const std::string errStr = Net::getSocketErrorString();
        LOG(ERROR) << "Failed to connect to " << host << ":" << port << " - " << errStr;
        notifyState(TransportState::Error, "Connect failed: " + errStr);
        return false;
    }

    // Socket is already non-blocking from above.
    int nodelay = 1;
    ::setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));

    m_sockfd.store(sock);
    m_running = true;
    m_readThread = std::thread(&TcpTransport::readWorker, this);

    LOG(INFO) << "Connected to TCP host " << host << ":" << port;
    notifyState(TransportState::Connected, "Connected to " + host + ":" + std::to_string(port));
    return true;
}

void TcpTransport::close()
{
    stopReadThread();

    bool wasClosed { false };
    {
        std::lock_guard<std::mutex> lock(m_writeMutex);
        const SocketHandle sock = m_sockfd.exchange(InvalidSocket);
        if (sock != InvalidSocket) {
            LOG(INFO) << "Closing TCP socket";
            Net::closeSocket(sock);
            wasClosed = true;
        }
    }

    if (wasClosed) {
        notifyState(TransportState::Disconnected, "Socket closed");
    }
}

bool TcpTransport::isOpen() const noexcept
{
    return m_running.load() && (m_sockfd.load() != InvalidSocket);
}

bool TcpTransport::sendData(const std::vector<std::uint8_t>& data)
{
    if (!isOpen() || data.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_writeMutex);
    const SocketHandle sock = m_sockfd.load();
    if (sock == InvalidSocket || !m_running.load()) {
        return false;
    }

    std::size_t totalSent { 0U };
    const std::size_t toSend { data.size() };

    while (totalSent < toSend && m_running.load()) {
        const auto sent = ::send(sock, reinterpret_cast<const char*>(data.data() + totalSent),
            static_cast<Net::SockBufLenType>(toSend - totalSent), Net::SendFlags);

        if (sent > 0) {
            totalSent += static_cast<std::size_t>(sent);
        } else if (sent < 0) {
            if (Net::isWouldBlock()) {
                Net::PollFd pfd {};
                pfd.fd = sock;
                pfd.events = POLLOUT;
                Net::pollSockets(&pfd, 1, 50);
                continue;
            }
            return false;
        }
    }

    return (totalSent == toSend);
}

void TcpTransport::readWorker()
{
    std::vector<std::uint8_t> buffer(2048U, 0x00U);
    bool unrecoverableError { false };

    while (m_running.load()) {
        Net::PollFd pfd {};
        pfd.fd = m_sockfd.load();
        pfd.events = POLLIN;
        const int ret = Net::pollSockets(&pfd, 1, 50);

        if (ret > 0 && (pfd.revents & (POLLIN | POLLHUP | POLLERR))) {
            const auto bytesRead = ::recv(m_sockfd.load(), reinterpret_cast<char*>(buffer.data()),
                static_cast<Net::SockBufLenType>(buffer.size()), 0);

            if (bytesRead > 0) {
                std::vector<std::uint8_t> chunk(buffer.begin(), buffer.begin() + bytesRead);
                invokeDataCallback(chunk);
            } else if (bytesRead == 0 || (pfd.revents & POLLHUP)) {
                if (m_running.exchange(false)) {
                    unrecoverableError = true;
                    notifyState(TransportState::Disconnected, "Remote host closed connection");
                }
                break;
            } else if (!Net::isWouldBlock()) {
                if (m_running.exchange(false)) {
                    unrecoverableError = true;
                    notifyState(TransportState::Error, "Socket read error: " + Net::getSocketErrorString());
                }
                break;
            }
        }
    }

    if (unrecoverableError) {
        std::lock_guard<std::mutex> lock(m_writeMutex);
        const SocketHandle sock = m_sockfd.exchange(InvalidSocket);
        if (sock != InvalidSocket) {
            Net::closeSocket(sock);
        }
    }
}

} // namespace PelcoD::Transport
