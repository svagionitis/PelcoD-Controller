/// @file TestTcpTransportTimeout.cpp
/// @brief TDD regression test: TcpTransport::open() must not block the caller
///        indefinitely when the remote host is unreachable or refuses connection.

#include "TcpTransport.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>

#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define TEST_CLOSE_SOCKET(s) ::closesocket(s)
using TestSocket = SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define TEST_CLOSE_SOCKET(s) ::close(s)
using TestSocket = int;
constexpr TestSocket INVALID_SOCKET = -1;
#endif

/// @brief open() to an unreachable address must complete within timeout + margin.
static void testConnectTimesOutFast()
{
    // 198.51.100.0/24 is TEST-NET-2 (RFC 5737) — guaranteed not routable.
    PelcoD::TcpTransport transport("198.51.100.1", 9999U);
    transport.setConnectTimeout(1000); // 1 s for a fast test

    const auto before = std::chrono::steady_clock::now();
    const bool ok = transport.open();
    const auto elapsed
        = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - before);

    assert(!ok && "open() must fail for unreachable host");
    // Must complete within timeout + 2 s margin.
    assert(elapsed.count() < 3000 && "open() must not block beyond timeout");

    std::cout << "  testConnectTimesOutFast: elapsed=" << elapsed.count() << "ms — PASSED\n";
}

/// @brief open() to localhost on a port with no listener must fail quickly (ECONNREFUSED).
static void testConnectRefusedFast()
{
    // Port 1 is almost never in use; kernel replies ECONNREFUSED immediately.
    PelcoD::TcpTransport transport("127.0.0.1", 1U);
    transport.setConnectTimeout(5000);

    const auto before = std::chrono::steady_clock::now();
    const bool ok = transport.open();
    const auto elapsed
        = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - before);

    assert(!ok && "open() must fail when connection is refused");
#ifdef _WIN32
    // Windows TCP/IP stack performs SYN retry backoff on closed loopback ports (~2 s)
    assert(elapsed.count() < 3500 && "open() must return fast on ECONNREFUSED");
#else
    // ECONNREFUSED is immediate; allow 500 ms margin.
    assert(elapsed.count() < 500 && "open() must return fast on ECONNREFUSED");
#endif

    std::cout << "  testConnectRefusedFast: elapsed=" << elapsed.count() << "ms — PASSED\n";
}

/// @brief open() to an invalid hostname must fail without hanging.
static void testDnsFailureFast()
{
    PelcoD::TcpTransport transport("this.hostname.does.not.exist.invalid", 4001U);
    transport.setConnectTimeout(5000);

    const auto before = std::chrono::steady_clock::now();
    const bool ok = transport.open();
    const auto elapsed
        = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - before);

    assert(!ok && "open() must fail for unresolvable host");
    // DNS failure on most systems is fast; give 8 s worst-case.
    assert(elapsed.count() < 8000 && "open() must not hang on DNS failure");

    std::cout << "  testDnsFailureFast: elapsed=" << elapsed.count() << "ms — PASSED\n";
}

/// @brief Remote socket closure must cause isOpen() to become false and fail subsequent writes.
static void testTcpRemoteClosureReportsClosed()
{
#ifdef _WIN32
    WSADATA wsaData {};
    ::WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    const TestSocket listenSock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(listenSock != INVALID_SOCKET);

    sockaddr_in serverAddr {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serverAddr.sin_port = htons(0); // Ephemeral port

    const int bindRet = ::bind(listenSock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
    assert(bindRet == 0);

    const int listenRet = ::listen(listenSock, 1);
    assert(listenRet == 0);

    sockaddr_in boundAddr {};
    socklen_t addrLen = sizeof(boundAddr);
    ::getsockname(listenSock, reinterpret_cast<sockaddr*>(&boundAddr), &addrLen);
    const std::uint16_t port = ntohs(boundAddr.sin_port);

    std::atomic<TestSocket> acceptedClient { INVALID_SOCKET };
    std::thread serverThread([listenSock, &acceptedClient]() {
        sockaddr_in clientAddr {};
        socklen_t clientLen = sizeof(clientAddr);
        const TestSocket client = ::accept(listenSock, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        acceptedClient.store(client);
    });

    PelcoD::TcpTransport transport("127.0.0.1", port);
    transport.setConnectTimeout(3000);

    std::atomic<bool> disconnectedNotified { false };
    transport.setStateCallback([&](PelcoD::TransportState state, const std::string&) {
        if (state == PelcoD::TransportState::Disconnected) {
            disconnectedNotified.store(true);
        }
    });

    const bool connected = transport.open();
    assert(connected);
    assert(transport.isOpen());

    if (serverThread.joinable()) {
        serverThread.join();
    }
    const TestSocket clientSock = acceptedClient.load();
    assert(clientSock != INVALID_SOCKET);

    // Abruptly close client and listener on server side to induce EOF on client transport
    ::shutdown(clientSock, 2);
    TEST_CLOSE_SOCKET(clientSock);
    TEST_CLOSE_SOCKET(listenSock);

    // Give time for transport readWorker to observe EOF
    for (int attempt = 0; attempt < 100 && !disconnectedNotified.load(); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    assert(disconnectedNotified.load() && "Transport must notify Disconnected upon remote closure");
    assert(!transport.isOpen() && "isOpen() must be false after remote closure");
    assert(!transport.sendData({ 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01 }) && "sendData() must fail when socket is dead");

    transport.close();
    assert(!transport.isOpen());

#ifdef _WIN32
    ::WSACleanup();
#endif

    std::cout << "  testTcpRemoteClosureReportsClosed: PASSED\n";
}

/// @brief Verify concurrent sendData() and close() do not race or crash.
static void testConcurrentSendAndClose()
{
#ifdef _WIN32
    WSADATA wsaData {};
    ::WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    const TestSocket listenSock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(listenSock != INVALID_SOCKET);

    sockaddr_in serverAddr {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serverAddr.sin_port = htons(0);

    const int bindRet = ::bind(listenSock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
    assert(bindRet == 0);
    assert(::listen(listenSock, 1) == 0);

    sockaddr_in boundAddr {};
    socklen_t addrLen = sizeof(boundAddr);
    ::getsockname(listenSock, reinterpret_cast<sockaddr*>(&boundAddr), &addrLen);
    const std::uint16_t port = ntohs(boundAddr.sin_port);

    std::atomic<TestSocket> acceptedClient { INVALID_SOCKET };
    std::thread serverThread([listenSock, &acceptedClient]() {
        sockaddr_in clientAddr {};
        socklen_t clientLen = sizeof(clientAddr);
        const TestSocket client = ::accept(listenSock, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        acceptedClient.store(client);
    });

    PelcoD::TcpTransport transport("127.0.0.1", port);
    transport.setConnectTimeout(3000);
    assert(transport.open());
    assert(transport.isOpen());

    if (serverThread.joinable()) {
        serverThread.join();
    }
    const TestSocket clientSock = acceptedClient.load();
    assert(clientSock != INVALID_SOCKET);

    // Launch concurrent sendData workers
    std::atomic<bool> stopSending { false };
    constexpr int workerCount = 4;
    std::vector<std::thread> workers;
    workers.reserve(workerCount);

    const std::vector<std::uint8_t> frame { 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01 };
    for (int i = 0; i < workerCount; ++i) {
        workers.emplace_back([&transport, &stopSending, &frame]() {
            while (!stopSending.load() && transport.isOpen()) {
                static_cast<void>(transport.sendData(frame));
            }
        });
    }

    // Let them send briefly, then concurrently close the transport
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    transport.close();
    stopSending.store(true);

    for (auto& w : workers) {
        if (w.joinable()) {
            w.join();
        }
    }

    assert(!transport.isOpen());
    assert(!transport.sendData(frame));

    TEST_CLOSE_SOCKET(clientSock);
    TEST_CLOSE_SOCKET(listenSock);

#ifdef _WIN32
    ::WSACleanup();
#endif

    std::cout << "  testConcurrentSendAndClose: PASSED\n";
}

int main()
{
#if defined(_MSC_VER)
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif

    std::cout << "[TestTcpTransportTimeout] Running...\n";
    testConnectRefusedFast();
    testConnectTimesOutFast();
    testDnsFailureFast();
    testTcpRemoteClosureReportsClosed();
    testConcurrentSendAndClose();
    std::cout << "[TestTcpTransportTimeout] All tests passed.\n";
    return 0;
}
