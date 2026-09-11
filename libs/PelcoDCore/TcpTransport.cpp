/// @file TcpTransport.cpp
/// @brief Implementation of cross-platform TCP socket transport.

#include "TcpTransport.h"

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

#define CLOSE_SOCKET(s) ::closesocket(s)
#define POLL_SOCKET(fds, nfds, timeout) ::WSAPoll(fds, nfds, timeout)
#define SEND_FLAGS 0
#define IS_WOULDBLOCK() (::WSAGetLastError() == WSAEWOULDBLOCK)
#else
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define CLOSE_SOCKET(s) ::close(s)
#define POLL_SOCKET(fds, nfds, timeout) ::poll(fds, nfds, timeout)
#define SEND_FLAGS MSG_NOSIGNAL
#define IS_WOULDBLOCK() (errno == EAGAIN || errno == EWOULDBLOCK)
#endif

#include <chrono>
#include <glog/logging.h>

namespace PelcoD {

namespace {

#ifdef _WIN32
    struct WinsockInit {
        WinsockInit()
        {
            WSADATA wsaData {};
            ::WSAStartup(MAKEWORD(2, 2), &wsaData);
        }
        ~WinsockInit()
        {
            ::WSACleanup();
        }
    };

    void ensureWinsockInitialized()
    {
        static WinsockInit init;
    }

    std::string getSocketErrorString(int errCode = 0)
    {
        if (errCode == 0) {
            errCode = ::WSAGetLastError();
        }
        char* errText = nullptr;
        const DWORD len = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
            errCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&errText), 0, nullptr);
        std::string msg
            = (len > 0 && errText != nullptr) ? std::string(errText) : "Winsock error " + std::to_string(errCode);
        if (errText != nullptr) {
            LocalFree(errText);
        }
        while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n')) {
            msg.pop_back();
        }
        return msg;
    }

    bool setNonBlocking(SOCKET s, bool nonBlocking)
    {
        u_long mode = nonBlocking ? 1 : 0;
        return ::ioctlsocket(s, FIONBIO, &mode) == 0;
    }

#else

    std::string getSocketErrorString(int errCode = 0)
    {
        if (errCode == 0) {
            errCode = errno;
        }
        return std::string(std::strerror(errCode));
    }

    bool setNonBlocking(int fd, bool nonBlocking)
    {
        const int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags == -1) {
            return false;
        }
        const int newFlags = nonBlocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
        return ::fcntl(fd, F_SETFL, newFlags) == 0;
    }
#endif

} // namespace

TcpTransport::TcpTransport(std::string host, std::uint16_t port)
    : m_host { std::move(host) }
    , m_port { port }
{
#ifdef _WIN32
    ensureWinsockInitialized();
#endif
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

bool TcpTransport::open()
{
    close();

    std::string host;
    std::uint16_t port { 4001U };
    {
        std::lock_guard<std::mutex> lock(m_writeMutex);
        host = m_host;
        port = m_port;
    }

    if (host.empty() || host.size() > 255U || port == 0U) {
        LOG(ERROR) << "Cannot open TCP transport: invalid host or port";
        notifyState(TransportState::Error, "Invalid host or port");
        return false;
    }

    LOG(INFO) << "Connecting to TCP host " << host << ":" << port;
    notifyState(TransportState::Connecting, "Connecting to " + host + ":" + std::to_string(port));

    struct addrinfo hints { };
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo* res { nullptr };
    const std::string portStr = std::to_string(port);
    const int status = ::getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res);
    if (status != 0 || res == nullptr) {
        std::string errStr = (status != 0) ? gai_strerror(status) : "Address resolution failed";
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

        if (::connect(sock, p->ai_addr, static_cast<socklen_t>(p->ai_addrlen)) == 0) {
            break; // Successfully connected
        }

        CLOSE_SOCKET(sock);
        sock = InvalidSocket;
    }

    ::freeaddrinfo(res);

    if (sock == InvalidSocket) {
        const std::string errStr = getSocketErrorString();
        LOG(ERROR) << "Failed to connect to " << host << ":" << port << " - " << errStr;
        notifyState(TransportState::Error, "Connect failed: " + errStr);
        return false;
    }

    setNonBlocking(sock, true);

    int nodelay = 1;
    ::setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));

    m_sockfd = sock;
    m_running = true;
    m_readThread = std::thread(&TcpTransport::readWorker, this);

    LOG(INFO) << "Connected to TCP host " << host << ":" << port;
    notifyState(TransportState::Connected, "Connected to " + host + ":" + std::to_string(port));
    return true;
}

void TcpTransport::close()
{
    m_running = false;

    if (m_readThread.joinable()) {
        m_readThread.join();
    }

    if (m_sockfd != InvalidSocket) {
        LOG(INFO) << "Closing TCP socket";
        CLOSE_SOCKET(m_sockfd);
        m_sockfd = InvalidSocket;
        notifyState(TransportState::Disconnected, "Socket closed");
    }
}

bool TcpTransport::isOpen() const noexcept
{
    return (m_sockfd != InvalidSocket) && m_running.load();
}

bool TcpTransport::sendData(const std::vector<std::uint8_t>& data)
{
    if (!isOpen() || data.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_writeMutex);

    std::size_t totalSent { 0U };
    const std::size_t toSend { data.size() };

    while (totalSent < toSend && m_running.load()) {
#ifdef _WIN32
        const int sent = ::send(m_sockfd, reinterpret_cast<const char*>(data.data() + totalSent),
            static_cast<int>(toSend - totalSent), SEND_FLAGS);
#else
        const ssize_t sent
            = ::send(m_sockfd, reinterpret_cast<const char*>(data.data() + totalSent), toSend - totalSent, SEND_FLAGS);
#endif

        if (sent > 0) {
            totalSent += static_cast<std::size_t>(sent);
        } else if (sent < 0) {
            if (IS_WOULDBLOCK()) {
#ifdef _WIN32
                WSAPOLLFD pfd {};
                pfd.fd = m_sockfd;
                pfd.events = POLLOUT;
                POLL_SOCKET(&pfd, 1, 50);
#else
                struct pollfd pfd { };
                pfd.fd = m_sockfd;
                pfd.events = POLLOUT;
                POLL_SOCKET(&pfd, 1, 50);
#endif
                continue;
            }
            return false;
        }
    }

    return (totalSent == toSend);
}

void TcpTransport::setDataCallback(DataReceivedCallback callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_dataCallback = std::move(callback);
}

void TcpTransport::setStateCallback(StateChangedCallback callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_stateCallback = std::move(callback);
}

void TcpTransport::readWorker()
{
    std::vector<std::uint8_t> buffer(2048U, 0x00U);

    while (m_running.load()) {
#ifdef _WIN32
        WSAPOLLFD pfd {};
        pfd.fd = m_sockfd;
        pfd.events = POLLIN;
        const int ret = POLL_SOCKET(&pfd, 1, 50);
#else
        struct pollfd pfd { };
        pfd.fd = m_sockfd;
        pfd.events = POLLIN;
        const int ret = POLL_SOCKET(&pfd, 1, 50);
#endif

        if (ret > 0 && (pfd.revents & POLLIN)) {
#ifdef _WIN32
            const int bytesRead
                = ::recv(m_sockfd, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
#else
            const ssize_t bytesRead = ::recv(m_sockfd, reinterpret_cast<char*>(buffer.data()), buffer.size(), 0);
#endif

            if (bytesRead > 0) {
                std::vector<std::uint8_t> chunk(buffer.begin(), buffer.begin() + bytesRead);

                DataReceivedCallback cb;
                {
                    std::lock_guard<std::mutex> lock(m_callbackMutex);
                    cb = m_dataCallback;
                }
                if (cb) {
                    cb(chunk);
                }
            } else if (bytesRead == 0) {
                if (m_running.load()) {
                    notifyState(TransportState::Disconnected, "Remote host closed connection");
                }
                break;
            } else if (!IS_WOULDBLOCK()) {
                if (m_running.load()) {
                    notifyState(TransportState::Error, "Socket read error: " + getSocketErrorString());
                }
                break;
            }
        }
    }
}

void TcpTransport::notifyState(TransportState state, const std::string& errorMsg)
{
    StateChangedCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        cb = m_stateCallback;
    }
    if (cb) {
        cb(state, errorMsg);
    }
}

} // namespace PelcoD
