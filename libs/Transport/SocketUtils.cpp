/// @file SocketUtils.cpp
/// @brief Implementation of unified cross-platform socket portability helpers.

#include "SocketUtils.h"

namespace Transport::Net {

#ifdef _WIN32

namespace {
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
} // namespace

void ensureWinsockInitialized() noexcept
{
    static WinsockInit init {};
}

void closeSocket(SocketHandle s) noexcept
{
    if (s != InvalidSocket) {
        ::closesocket(s);
    }
}

bool isWouldBlock() noexcept
{
    return ::WSAGetLastError() == WSAEWOULDBLOCK;
}

bool isConnectInProgress() noexcept
{
    return ::WSAGetLastError() == WSAEWOULDBLOCK;
}

std::string getSocketErrorString(int errCode)
{
    if (errCode == 0) {
        errCode = ::WSAGetLastError();
    }
    char* errText { nullptr };
    const DWORD len
        = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, errCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&errText), 0, nullptr);
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

bool setNonBlocking(SocketHandle s, bool nonBlocking) noexcept
{
    u_long mode = nonBlocking ? 1 : 0;
    return ::ioctlsocket(s, FIONBIO, &mode) == 0;
}

int pollSockets(PollFd* fds, unsigned long nfds, int timeoutMs) noexcept
{
    return ::WSAPoll(fds, nfds, timeoutMs);
}

#else

void ensureWinsockInitialized() noexcept
{
    // No initialization required on POSIX
}

void closeSocket(SocketHandle s) noexcept
{
    if (s != InvalidSocket) {
        ::close(s);
    }
}

bool isWouldBlock() noexcept
{
    return errno == EAGAIN || errno == EWOULDBLOCK;
}

bool isConnectInProgress() noexcept
{
    return errno == EINPROGRESS;
}

std::string getSocketErrorString(int errCode)
{
    if (errCode == 0) {
        errCode = errno;
    }
    return std::string(std::strerror(errCode));
}

bool setNonBlocking(SocketHandle s, bool nonBlocking) noexcept
{
    const int flags = ::fcntl(s, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }
    return ::fcntl(s, F_SETFL, nonBlocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK)) != -1;
}

int pollSockets(PollFd* fds, unsigned long nfds, int timeoutMs) noexcept
{
    return ::poll(fds, nfds, timeoutMs);
}

#endif

} // namespace Transport::Net
