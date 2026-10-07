/// @file TestOnvifServerLifecycle.cpp
/// @brief Conformance tests for ONVIF server startup, failure handling, and lifecycle (Finding H2).

#include <Onvif/OnvifServer.h>
#include <Onvif/OnvifServerTypes.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

namespace {

/// @brief Atomic port sequence generator for isolated lifecycle test ports.
std::atomic<int> g_lifecyclePort { 19450 };

/// @brief Creates an OnvifServerConfig pre-provisioned with valid credentials.
/// @param[in] port TCP port number to configure.
/// @param[in] host IP address string to bind.
/// @return Configured OnvifServerConfig.
[[nodiscard]] Onvif::OnvifServerConfig makeTestConfig(const int port, const std::string& host = "127.0.0.1")
{
    Onvif::OnvifServerConfig config {};
    config.bindAddress = host;
    config.port = port;

    Onvif::OnvifUser adminUser {};
    adminUser.username = "admin";
    adminUser.password = "Admin_Test_1234!";
    adminUser.level = Onvif::OnvifUserLevel::Administrator;
    config.defaultUsers = { adminUser };
    config.auth.allowDefaultPassword = true;

    return config;
}

} // namespace

// ============================================================================
// Lifecycle & Failure Unit Tests (Finding H2)
// ============================================================================

/// @brief Verifies that attempting to bind to an already-bound port causes start() to return false.
TEST(TestOnvifServerLifecycle, PortConflictFails)
{
    const int port { g_lifecyclePort.fetch_add(1) };
    const auto config1 { makeTestConfig(port) };
    const auto config2 { makeTestConfig(port) };

    Onvif::OnvifServer server1 { config1 };
    EXPECT_TRUE(server1.start());
    EXPECT_TRUE(server1.isRunning());

    // Second server attempts to bind to the same host:port
    Onvif::OnvifServer server2 { config2 };
    EXPECT_FALSE(server2.start());
    EXPECT_FALSE(server2.isRunning());

    server1.stop();
    EXPECT_FALSE(server1.isRunning());
}

/// @brief Verifies that calling start() on an already-running server is idempotent and returns true.
TEST(TestOnvifServerLifecycle, IdempotentStart)
{
    const int port { g_lifecyclePort.fetch_add(1) };
    const auto config { makeTestConfig(port) };

    Onvif::OnvifServer server { config };
    EXPECT_TRUE(server.start());
    EXPECT_TRUE(server.isRunning());

    // Second start() call should succeed immediately without spawning another thread
    EXPECT_TRUE(server.start());
    EXPECT_TRUE(server.isRunning());

    server.stop();
    EXPECT_FALSE(server.isRunning());
}

/// @brief Verifies that a server can be cleanly stopped and restarted on the same port.
TEST(TestOnvifServerLifecycle, StartStopRestart)
{
    const int port { g_lifecyclePort.fetch_add(1) };
    const auto config { makeTestConfig(port) };

    Onvif::OnvifServer server { config };
    EXPECT_TRUE(server.start());
    EXPECT_TRUE(server.isRunning());

    server.stop();
    EXPECT_FALSE(server.isRunning());

    // Give socket a brief pause to release
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    EXPECT_TRUE(server.start());
    EXPECT_TRUE(server.isRunning());

    server.stop();
    EXPECT_FALSE(server.isRunning());
}

/// @brief Verifies that an OnvifServer instance destructs cleanly after a failed start() without crashing.
TEST(TestOnvifServerLifecycle, DestructionOnFail)
{
    const int port { g_lifecyclePort.fetch_add(1) };
    const auto primaryConfig { makeTestConfig(port) };
    Onvif::OnvifServer primaryServer { primaryConfig };
    EXPECT_TRUE(primaryServer.start());
    EXPECT_TRUE(primaryServer.isRunning());

    {
        const auto collidingConfig { makeTestConfig(port) };
        Onvif::OnvifServer collidingServer { collidingConfig };
        EXPECT_FALSE(collidingServer.start());
        EXPECT_FALSE(collidingServer.isRunning());
        // collidingServer destructs here upon leaving scope
    }

    primaryServer.stop();
    EXPECT_FALSE(primaryServer.isRunning());
}

/// @brief Verifies that start() refuses unauthenticated operation on non-loopback bindings.
TEST(TestOnvifServerLifecycle, RefuseNoAuthNonLoop)
{
    const int port { g_lifecyclePort.fetch_add(1) };
    auto config { makeTestConfig(port, "192.168.1.100") };
    config.auth.enabled = false;

    Onvif::OnvifServer server { config };
    EXPECT_FALSE(server.start());
    EXPECT_FALSE(server.isRunning());
}

/// @brief Verifies that start() refuses default passwords on non-loopback bindings.
TEST(TestOnvifServerLifecycle, RefuseDefPwdNonLoop)
{
    const int port { g_lifecyclePort.fetch_add(1) };
    auto config { makeTestConfig(port, "192.168.1.100") };
    config.auth.enabled = true;
    config.auth.allowDefaultPassword = false;

    Onvif::OnvifUser defaultUser {};
    defaultUser.username = "admin";
    defaultUser.password = "admin"; // Known default credential
    defaultUser.level = Onvif::OnvifUserLevel::Administrator;
    config.defaultUsers = { defaultUser };

    Onvif::OnvifServer server { config };
    EXPECT_FALSE(server.start());
    EXPECT_FALSE(server.isRunning());
}
