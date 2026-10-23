/// @file TestOnvifScopeStore.cpp
/// @brief Comprehensive unit and concurrency tests for ScopeStore thread-safe scope synchronization.

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "ScopeStore.h"

namespace {

TEST(TestOnvifScopeStore, DefaultInitialization)
{
    const Onvif::ScopeStore store;
    EXPECT_TRUE(store.getScopes().empty());
    EXPECT_EQ(store.size(), 0U);
    EXPECT_EQ(store.getMetadataVersion(), 1U);
}

TEST(TestOnvifScopeStore, ParameterizedInitialization)
{
    const std::vector<std::string> initScopes { "onvif://www.onvif.org/Profile/S",
        "onvif://www.onvif.org/location/room1" };
    const Onvif::ScopeStore store(initScopes, 42U);

    EXPECT_EQ(store.size(), 2U);
    EXPECT_EQ(store.getMetadataVersion(), 42U);
    const auto scopes = store.getScopes();
    ASSERT_EQ(scopes.size(), 2U);
    EXPECT_EQ(scopes[0], "onvif://www.onvif.org/Profile/S");
    EXPECT_EQ(scopes[1], "onvif://www.onvif.org/location/room1");
}

TEST(TestOnvifScopeStore, HasScope)
{
    const std::vector<std::string> initScopes { "onvif://www.onvif.org/Profile/S",
        "onvif://www.onvif.org/location/building_a" };
    const Onvif::ScopeStore store(initScopes);

    EXPECT_TRUE(store.hasScope("onvif://www.onvif.org/Profile/S"));
    EXPECT_TRUE(store.hasScope("onvif://www.onvif.org/location/building_a"));
    EXPECT_FALSE(store.hasScope("onvif://www.onvif.org/Profile/T"));
    EXPECT_FALSE(store.hasScope(""));
}

TEST(TestOnvifScopeStore, SetScopesOverwritesAndDeduplicates)
{
    Onvif::ScopeStore store({ "onvif://initial" }, 1U);

    const std::vector<std::string> newScopes {
        "onvif://scope1", "onvif://scope2",
        "onvif://scope1" // duplicate
    };
    store.setScopes(newScopes);

    EXPECT_EQ(store.size(), 2U);
    EXPECT_EQ(store.getMetadataVersion(), 2U);

    const auto scopes = store.getScopes();
    ASSERT_EQ(scopes.size(), 2U);
    EXPECT_EQ(scopes[0], "onvif://scope1");
    EXPECT_EQ(scopes[1], "onvif://scope2");
}

TEST(TestOnvifScopeStore, AddScopesDeduplication)
{
    Onvif::ScopeStore store({ "onvif://scopeA" }, 1U);

    // Add unique scope
    store.addScopes({ "onvif://scopeB" });
    EXPECT_EQ(store.size(), 2U);
    EXPECT_EQ(store.getMetadataVersion(), 2U);
    EXPECT_TRUE(store.hasScope("onvif://scopeB"));

    // Add duplicate scope - should be no-op, version unchanged
    store.addScopes({ "onvif://scopeA", "onvif://scopeB" });
    EXPECT_EQ(store.size(), 2U);
    EXPECT_EQ(store.getMetadataVersion(), 2U);
}

TEST(TestOnvifScopeStore, RemoveScopes)
{
    Onvif::ScopeStore store({ "onvif://scopeA", "onvif://scopeB", "onvif://scopeC" }, 10U);

    // Remove existing scope
    store.removeScopes({ "onvif://scopeB" });
    EXPECT_EQ(store.size(), 2U);
    EXPECT_EQ(store.getMetadataVersion(), 11U);
    EXPECT_FALSE(store.hasScope("onvif://scopeB"));
    EXPECT_TRUE(store.hasScope("onvif://scopeA"));
    EXPECT_TRUE(store.hasScope("onvif://scopeC"));

    // Remove non-existing scope - version should remain unchanged
    store.removeScopes({ "onvif://non_existing" });
    EXPECT_EQ(store.size(), 2U);
    EXPECT_EQ(store.getMetadataVersion(), 11U);
}

TEST(TestOnvifScopeStore, GetActiveScopesComposition)
{
    const Onvif::ScopeStore store({ "onvif://www.onvif.org/location/gate" });
    const auto active = store.getActiveScopes("Camera01", "ModelX");

    ASSERT_GE(active.size(), 5U);
    EXPECT_EQ(active[0], "onvif://www.onvif.org/type/video_encoder");
    EXPECT_EQ(active[1], "onvif://www.onvif.org/type/ptz");
    EXPECT_EQ(active[2], "onvif://www.onvif.org/name/Camera01");
    EXPECT_EQ(active[3], "onvif://www.onvif.org/hardware/ModelX");
    EXPECT_EQ(active[4], "onvif://www.onvif.org/location/gate");
}

TEST(TestOnvifScopeStore, ObserverNotification)
{
    Onvif::ScopeStore store({ "onvif://scope1" }, 1U);

    std::uint32_t notifiedVersion { 0U };
    std::size_t notificationCount { 0U };

    store.registerObserver([&notifiedVersion, &notificationCount](std::uint32_t newVersion) {
        notifiedVersion = newVersion;
        ++notificationCount;
    });

    store.addScopes({ "onvif://scope2" });
    EXPECT_EQ(notificationCount, 1U);
    EXPECT_EQ(notifiedVersion, 2U);

    store.removeScopes({ "onvif://scope1" });
    EXPECT_EQ(notificationCount, 2U);
    EXPECT_EQ(notifiedVersion, 3U);

    store.clearObservers();
    store.addScopes({ "onvif://scope3" });
    EXPECT_EQ(notificationCount, 2U); // Not notified after clear
}

TEST(TestOnvifScopeStore, ObserverReentrancySafety)
{
    Onvif::ScopeStore store({ "onvif://scope1" }, 1U);

    std::size_t scopesCountInCallback { 0U };
    std::uint32_t versionInCallback { 0U };

    // Register an observer that re-enters ScopeStore to query getScopes() and getMetadataVersion()
    store.registerObserver([&store, &scopesCountInCallback, &versionInCallback](std::uint32_t /*v*/) {
        scopesCountInCallback = store.getScopes().size();
        versionInCallback = store.getMetadataVersion();
    });

    // Mutation should not deadlock on re-entry
    store.addScopes({ "onvif://scope2" });
    EXPECT_EQ(scopesCountInCallback, 2U);
    EXPECT_EQ(versionInCallback, 2U);
}

TEST(TestOnvifScopeStore, ConcurrentReadWriteStress)
{
    Onvif::ScopeStore store({ "onvif://base/scope" }, 1U);

    std::atomic<bool> keepRunning { true };
    std::atomic<std::size_t> readOps { 0U };
    std::atomic<std::size_t> writeOps { 0U };

    constexpr std::size_t kReaderThreads { 4U };
    constexpr std::size_t kWriterThreads { 2U };

    std::vector<std::thread> threads;
    threads.reserve(kReaderThreads + kWriterThreads);

    // Reader threads
    for (std::size_t i { 0U }; i < kReaderThreads; ++i) {
        threads.emplace_back([&store, &keepRunning, &readOps]() {
            while (keepRunning.load(std::memory_order_relaxed)) {
                const auto scopes = store.getScopes();
                const bool has = store.hasScope("onvif://base/scope");
                const auto ver = store.getMetadataVersion();
                (void)scopes;
                (void)has;
                (void)ver;
                readOps.fetch_add(1U, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        });
    }

    // Writer threads
    for (std::size_t i { 0U }; i < kWriterThreads; ++i) {
        threads.emplace_back([&store, &keepRunning, &writeOps, i]() {
            std::size_t count { 0U };
            while (keepRunning.load(std::memory_order_relaxed)) {
                const std::string s = "onvif://thread_" + std::to_string(i) + "/scope_" + std::to_string(count % 10U);
                if ((count % 2U) == 0U) {
                    store.addScopes({ s });
                } else {
                    store.removeScopes({ s });
                }
                writeOps.fetch_add(1U, std::memory_order_relaxed);
                ++count;
                std::this_thread::yield();
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    keepRunning.store(false, std::memory_order_relaxed);

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    EXPECT_GT(readOps.load(), 100U);
    EXPECT_GT(writeOps.load(), 50U);
    EXPECT_TRUE(store.hasScope("onvif://base/scope"));
}

} // namespace
