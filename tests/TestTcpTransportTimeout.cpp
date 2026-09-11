/// @file TestTcpTransportTimeout.cpp
/// @brief TDD regression test: TcpTransport::open() must not block the caller
///        indefinitely when the remote host is unreachable or refuses connection.

#include "TcpTransport.h"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>

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
    // ECONNREFUSED is immediate; allow 500 ms margin.
    assert(elapsed.count() < 500 && "open() must return fast on ECONNREFUSED");

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

int main()
{
    std::cout << "[TestTcpTransportTimeout] Running...\n";
    testConnectRefusedFast();
    testConnectTimesOutFast();
    testDnsFailureFast();
    std::cout << "[TestTcpTransportTimeout] All tests passed.\n";
    return 0;
}
