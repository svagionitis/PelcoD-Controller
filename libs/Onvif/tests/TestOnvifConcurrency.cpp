/// @file TestOnvifConcurrency.cpp
/// @brief Concurrency and thread-safety tests for OnvifServer shared state (Finding H1).
/// @details Verifies elimination of data races (CERT CON43-C) across handler registration,
///          scope mutations, and diagnostic/backup state access.

#include "OnvifTestClient.h"

#include <Onvif/OnvifServer.h>
#include <Onvif/OnvifServerTypes.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

/// @brief Atomic port sequence generator for concurrency test ports.
std::atomic<int> g_concurrencyPort { 19550 };

/// @brief Generates an OnvifServerConfig configured for authenticated loopback testing.
/// @param[in] port TCP port number.
/// @return Populated server configuration.
[[nodiscard]] Onvif::OnvifServerConfig makeConcurrencyConfig(const int port)
{
    Onvif::OnvifServerConfig config {};
    config.bindAddress = "127.0.0.1";
    config.port = port;

    Onvif::OnvifUser adminUser {};
    adminUser.username = "admin";
    adminUser.password = "Admin_Test_1234!";
    adminUser.level = Onvif::OnvifUserLevel::Administrator;
    config.defaultUsers = { adminUser };
    config.auth.allowDefaultPassword = true;

    return config;
}

/// @class MockPtzHandler
/// @brief Mock PTZ handler recording command counts atomically.
class MockPtzHandler : public Onvif::IPtzHandler {
public:
    void handleContinuousMove(float /*panSpeed*/, float /*tiltSpeed*/, float /*zoomSpeed*/) override
    {
        moveCount.fetch_add(1, std::memory_order_relaxed);
    }

    void handleAbsoluteMove(float /*pan*/, float /*tilt*/, float /*zoom*/) override
    {
        moveCount.fetch_add(1, std::memory_order_relaxed);
    }

    void handleStop(bool /*stopPanTilt*/, bool /*stopZoom*/) override
    {
        stopCount.fetch_add(1, std::memory_order_relaxed);
    }

    [[nodiscard]] std::string handleSetPreset(const std::string& /*name*/, const std::string& /*token*/) override
    {
        return "preset_1";
    }

    bool handleGotoPreset(const std::string& /*token*/) override
    {
        return true;
    }

    bool handleRemovePreset(const std::string& /*token*/) override
    {
        return true;
    }

    [[nodiscard]] std::vector<Onvif::PtzPreset> handleGetPresets() override
    {
        return {};
    }

    [[nodiscard]] Onvif::PtzStatus handleGetStatus() override
    {
        return {};
    }

    std::atomic<std::size_t> moveCount { 0U };
    std::atomic<std::size_t> stopCount { 0U };
};

/// @class MockDeviceHandler
/// @brief Mock device management handler for concurrency testing.
class MockDeviceHandler : public Onvif::IDeviceManagementHandler {
public:
    [[nodiscard]] std::string handleGetHostname() override
    {
        return "ConcurrentHost";
    }

    bool handleSetHostname(const std::string& /*hostname*/) override
    {
        callCount.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    [[nodiscard]] std::string handleGetSystemBackup() override
    {
        return "{\"mock_backup\":true}";
    }

    [[nodiscard]] Onvif::SystemSupportInfo handleGetSystemSupportInformation() override
    {
        Onvif::SystemSupportInfo info {};
        info.rawDiagnostics = "Mock Diagnostics";
        return info;
    }

    std::atomic<std::size_t> callCount { 0U };
};

} // namespace

// ============================================================================
// Test Suite: Concurrency and Thread Safety (Finding H1)
// ============================================================================

/// @brief Verifies concurrent handler swapping and getter snapshotting without data races.
TEST(TestOnvifConcurrency, ConcurrentHandlerAccess)
{
    const int port { g_concurrencyPort.fetch_add(1) };
    const auto config { makeConcurrencyConfig(port) };

    Onvif::OnvifServer server { config };
    EXPECT_TRUE(server.start());

    std::atomic<bool> keepRunning { true };
    std::atomic<std::size_t> setOperations { 0U };
    std::atomic<std::size_t> getOperations { 0U };

    constexpr std::size_t kSetterThreads { 4U };
    constexpr std::size_t kGetterThreads { 4U };
    std::vector<std::thread> threads {};
    threads.reserve(kSetterThreads + kGetterThreads);

    // Threads continuously swapping handlers
    for (std::size_t i { 0U }; i < kSetterThreads; ++i) {
        threads.emplace_back([&server, &keepRunning, &setOperations, i]() {
            while (keepRunning.load(std::memory_order_relaxed)) {
                if ((i % 2U) == 0U) {
                    server.setPtzHandler(std::make_shared<MockPtzHandler>());
                    server.setDeviceManagementHandler(std::make_shared<MockDeviceHandler>());
                } else {
                    server.setPtzHandler(nullptr);
                    server.setDeviceManagementHandler(nullptr);
                }
                setOperations.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        });
    }

    // Threads continuously snapshotting handlers and querying status
    for (std::size_t i { 0U }; i < kGetterThreads; ++i) {
        threads.emplace_back([&server, &keepRunning, &getOperations]() {
            while (keepRunning.load(std::memory_order_relaxed)) {
                const auto ptz { server.getPtzHandler() };
                if (ptz) {
                    ptz->handleContinuousMove(0.1f, 0.2f, 0.0f);
                }
                const auto dev { server.getDeviceManagementHandler() };
                if (dev) {
                    static_cast<void>(dev->handleGetHostname());
                }
                getOperations.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    keepRunning.store(false, std::memory_order_relaxed);

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    EXPECT_GT(setOperations.load(), 100U);
    EXPECT_GT(getOperations.load(), 100U);
    server.stop();
}

/// @brief Verifies concurrent scope modifications and config reads are thread-safe.
TEST(TestOnvifConcurrency, ConcurrentScopeModifications)
{
    const int port { g_concurrencyPort.fetch_add(1) };
    const auto config { makeConcurrencyConfig(port) };

    Onvif::OnvifServer server { config };
    EXPECT_TRUE(server.start());

    std::atomic<bool> keepRunning { true };
    std::atomic<std::size_t> completedRequests { 0U };

    constexpr std::size_t kClientThreads { 4U };
    std::vector<std::thread> threads {};
    threads.reserve(kClientThreads);

    // Threads issuing AddScopes, RemoveScopes, and GetScopes concurrently
    for (std::size_t i { 0U }; i < kClientThreads; ++i) {
        threads.emplace_back([port, &keepRunning, &completedRequests, i]() {
            OnvifTest::OnvifTestClient client("127.0.0.1", port);
            const std::string scopeUri = "onvif://www.onvif.org/location/room_" + std::to_string(i);
            const std::string addSoap = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                        "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                        "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                        "  <SOAP-ENV:Body>\r\n"
                                        "    <tds:AddScopes>\r\n"
                                        "      <tds:ScopeItem>"
                + scopeUri
                + "</tds:ScopeItem>\r\n"
                  "    </tds:AddScopes>\r\n"
                  "  </SOAP-ENV:Body>\r\n"
                  "</SOAP-ENV:Envelope>";

            const std::string getSoap = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                        "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                        "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                        "  <SOAP-ENV:Body>\r\n"
                                        "    <tds:GetScopes/>\r\n"
                                        "  </SOAP-ENV:Body>\r\n"
                                        "</SOAP-ENV:Envelope>";

            while (keepRunning.load(std::memory_order_relaxed)) {
                auto resAdd = client.Post("/onvif/device_service", addSoap, "application/soap+xml");
                EXPECT_TRUE(resAdd);
                auto resGet = client.Post("/onvif/device_service", getSoap, "application/soap+xml");
                EXPECT_TRUE(resGet);
                completedRequests.fetch_add(2, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        });
    }

    // Thread concurrently calling getConfig()
    std::thread configReader([&server, &keepRunning]() {
        while (keepRunning.load(std::memory_order_relaxed)) {
            const auto cfg = server.getConfig();
            EXPECT_FALSE(cfg.deviceName.empty());
            std::this_thread::yield();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    keepRunning.store(false, std::memory_order_relaxed);

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    if (configReader.joinable()) {
        configReader.join();
    }

    EXPECT_GT(completedRequests.load(), 10U);
    server.stop();
}

/// @brief Verifies concurrent system diagnostics reads and network updates execute without data race.
TEST(TestOnvifConcurrency, ConcurrentDiagnosticReads)
{
    const int port { g_concurrencyPort.fetch_add(1) };
    const auto config { makeConcurrencyConfig(port) };

    Onvif::OnvifServer server { config };
    EXPECT_TRUE(server.start());

    std::atomic<bool> keepRunning { true };
    std::atomic<std::size_t> completedOps { 0U };

    const std::string backupSoap = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <tds:GetSystemBackup/>\r\n"
                                   "  </SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";

    const std::string supportSoap = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body>\r\n"
                                    "    <tds:GetSystemSupportInformation>\r\n"
                                    "      <tds:TokenType>System</tds:TokenType>\r\n"
                                    "    </tds:GetSystemSupportInformation>\r\n"
                                    "  </SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";

    const std::string setHostSoap = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body>\r\n"
                                    "    <tds:SetHostname>\r\n"
                                    "      <tds:Name>Dynamic-Pelco-Host</tds:Name>\r\n"
                                    "    </tds:SetHostname>\r\n"
                                    "  </SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";

    std::vector<std::thread> workers {};
    workers.reserve(3U);

    // Reader 1: GetSystemBackup
    workers.emplace_back([port, &keepRunning, &completedOps, &backupSoap]() {
        OnvifTest::OnvifTestClient client("127.0.0.1", port);
        while (keepRunning.load(std::memory_order_relaxed)) {
            auto res = client.Post("/onvif/device_service", backupSoap, "application/soap+xml");
            EXPECT_TRUE(res);
            completedOps.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::yield();
        }
    });

    // Reader 2: GetSystemSupportInformation
    workers.emplace_back([port, &keepRunning, &completedOps, &supportSoap]() {
        OnvifTest::OnvifTestClient client("127.0.0.1", port);
        while (keepRunning.load(std::memory_order_relaxed)) {
            auto res = client.Post("/onvif/device_service", supportSoap, "application/soap+xml");
            EXPECT_TRUE(res);
            completedOps.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::yield();
        }
    });

    // Writer: SetHostname
    workers.emplace_back([port, &keepRunning, &completedOps, &setHostSoap]() {
        OnvifTest::OnvifTestClient client("127.0.0.1", port);
        while (keepRunning.load(std::memory_order_relaxed)) {
            auto res = client.Post("/onvif/device_service", setHostSoap, "application/soap+xml");
            EXPECT_TRUE(res);
            completedOps.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::yield();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    keepRunning.store(false, std::memory_order_relaxed);

    for (auto& w : workers) {
        if (w.joinable()) {
            w.join();
        }
    }

    EXPECT_GT(completedOps.load(), 10U);
    server.stop();
}
