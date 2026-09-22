#pragma once

/// @file SocketUtils.h
/// @brief Unified cross-platform socket primitives, type definitions, and portability helpers.

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
#endif

#include <cstdint>
#include <string>

namespace Transport::Net {

#ifdef _WIN32
using SocketHandle = SOCKET;
inline constexpr SocketHandle InvalidSocket { INVALID_SOCKET };
using SockOptLenType = int;
using SockBufLenType = int;
inline constexpr int SendFlags { 0 };
using PollFd = WSAPOLLFD;
#else
using SocketHandle = int;
inline constexpr SocketHandle InvalidSocket { -1 };
using SockOptLenType = socklen_t;
using SockBufLenType = size_t;
inline constexpr int SendFlags { MSG_NOSIGNAL };
using PollFd = pollfd;
#endif

/// @brief Ensures Winsock is initialized once on Windows systems. No-op on POSIX.
void ensureWinsockInitialized() noexcept;

/// @brief Closes an open socket descriptor safely across platforms.
/// @param[in] s Socket descriptor to close.
void closeSocket(SocketHandle s) noexcept;

/// @brief Tests if the most recent socket error indicates a non-blocking would-block condition.
/// @return True if errno/WSAGetLastError equals EWOULDBLOCK or EAGAIN.
[[nodiscard]] bool isWouldBlock() noexcept;

/// @brief Tests if the most recent socket error indicates an in-progress non-blocking connect.
/// @return True if errno equals EINPROGRESS (POSIX) or WSAGetLastError equals WSAEWOULDBLOCK (Windows).
[[nodiscard]] bool isConnectInProgress() noexcept;

/// @brief Translates a platform socket error code into a human-readable string.
/// @param[in] errCode Error code to translate (0 fetches current errno or WSAGetLastError).
/// @return Detailed textual error message.
[[nodiscard]] std::string getSocketErrorString(int errCode = 0);

/// @brief Toggles non-blocking I/O mode on the specified socket descriptor.
/// @param[in] s Target socket descriptor.
/// @param[in] nonBlocking True to set non-blocking, false to set blocking.
/// @return True if ioctl or fcntl succeeded.
bool setNonBlocking(SocketHandle s, bool nonBlocking) noexcept;

/// @brief Polls a set of socket descriptors with a timeout in milliseconds.
/// @param[in,out] fds Pointer to PollFd structures.
/// @param[in] nfds Number of file descriptors in array.
/// @param[in] timeoutMs Maximum duration in milliseconds to block (-1 for indefinite).
/// @return Number of ready descriptors, 0 on timeout, or negative on error.
int pollSockets(PollFd* fds, unsigned long nfds, int timeoutMs) noexcept;

} // namespace Transport::Net
