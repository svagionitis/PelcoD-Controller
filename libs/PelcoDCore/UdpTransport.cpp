/// @file UdpTransport.cpp
/// @brief Implementation of cross-platform UDP socket transport.

#include "UdpTransport.h"

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
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }
    flags = nonBlocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return ::fcntl(fd, F_SETFL, flags) == 0;
}

#endif

} // namespace

UdpTransport::UdpTransport(std::string host, std::uint16_t port, std::uint16_t localPort)
    : m_host { std::move(host) }
    , m_port { port }
    , m_localPort { localPort }
{
#ifdef _WIN32
    ensureWinsockInitialized();
#endif
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

#ifdef _WIN32
    ensureWinsockInitialized();
#endif

    notifyState(TransportState::Connecting, "");

    struct addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    const std::string portStr = std::to_string(m_port);
    struct addrinfo* result = nullptr;
    const int gaiErr = ::getaddrinfo(m_host.c_str(), portStr.c_str(), &hints, &result);
    if (gaiErr != 0 || result == nullptr) {
        const std::string err = "Failed to resolve host " + m_host + ": "
#ifdef _WIN32
            + getSocketErrorString(gaiErr);
#else
            + std::string(gai_strerror(gaiErr));
#endif
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
                struct sockaddr_in addr4 {};
                addr4.sin_family = AF_INET;
                addr4.sin_addr.s_addr = htonl(INADDR_ANY);
                addr4.sin_port = htons(m_localPort);
                bindOk = (::bind(static_cast<SOCKET>(s), reinterpret_cast<struct sockaddr*>(&addr4), sizeof(addr4)) == 0);
            } else if (ptr->ai_family == AF_INET6) {
                struct sockaddr_in6 addr6 {};
                addr6.sin6_family = AF_INET6;
                addr6.sin6_addr = in6addr_any;
                addr6.sin6_port = htons(m_localPort);
                bindOk = (::bind(static_cast<SOCKET>(s), reinterpret_cast<struct sockaddr*>(&addr6), sizeof(addr6)) == 0);
            }

            if (!bindOk) {
                LOG(WARNING) << "UdpTransport: Failed to bind to local port " << m_localPort << ": " << getSocketErrorString();
                CLOSE_SOCKET(static_cast<SOCKET>(s));
                continue;
            }
        }

        // Connect the UDP socket to destination address to establish default peer and filter incoming datagrams
        if (::connect(static_cast<SOCKET>(s), ptr->ai_addr, static_cast<socklen_t>(ptr->ai_addrlen)) != 0) {
            LOG(WARNING) << "UdpTransport: Failed to connect UDP socket to " << m_host << ":" << m_port << ": " << getSocketErrorString();
            CLOSE_SOCKET(static_cast<SOCKET>(s));
            continue;
        }

        if (!setNonBlocking(static_cast<SOCKET>(s), true)) {
            LOG(WARNING) << "UdpTransport: Failed to set non-blocking mode: " << getSocketErrorString();
            CLOSE_SOCKET(static_cast<SOCKET>(s));
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
        CLOSE_SOCKET(static_cast<SOCKET>(sock));
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

    const auto bytesSent = ::send(static_cast<SOCKET>(sock), reinterpret_cast<const char*>(data.data()),
        static_cast<int>(data.size()), SEND_FLAGS);

    if (bytesSent != static_cast<int>(data.size())) {
        LOG(WARNING) << "UdpTransport: send error: " << getSocketErrorString();
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

        pollfd pfd {};
        pfd.fd = static_cast<decltype(pfd.fd)>(sock);
        pfd.events = POLLIN;

        const int pollResult = POLL_SOCKET(&pfd, 1, 100);

        if (!m_running.load()) {
            break;
        }

        if (pollResult < 0) {
            if (!m_running.load()) {
                break;
            }
            LOG(WARNING) << "UdpTransport: poll error: " << getSocketErrorString();
            continue;
        }

        if (pollResult == 0) {
            continue;
        }

        if (pfd.revents & POLLIN) {
            const int bytesRead = ::recv(static_cast<SOCKET>(sock), reinterpret_cast<char*>(rxBuffer.data()),
                static_cast<int>(rxBuffer.size()), 0);

            if (bytesRead > 0) {
                const std::vector<std::uint8_t> packet(rxBuffer.begin(), rxBuffer.begin() + bytesRead);
                invokeDataCallback(packet);
            } else if (bytesRead < 0) {
                if (IS_WOULDBLOCK()) {
                    continue;
                }
                if (!m_running.load()) {
                    break;
                }
                LOG(WARNING) << "UdpTransport: recv error: " << getSocketErrorString();
            }
        }
    }
}

} // namespace PelcoD
