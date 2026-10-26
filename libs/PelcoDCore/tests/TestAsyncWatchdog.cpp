/// @file TestAsyncWatchdog.cpp
/// @brief Unit tests for PelcoD::AsyncWatchdog deadline scheduler.

#include "AsyncWatchdog.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <vector>

namespace {

using namespace std::chrono_literals;

TEST(AsyncWatchdogTest, ScheduleAndFire)
{
    PelcoD::AsyncWatchdog watchdog;

    std::promise<bool> promise;
    auto future = promise.get_future();

    const auto timerId = watchdog.schedule(20ms, [&] { promise.set_value(true); });
    EXPECT_GT(timerId, 0U);

    const auto res = future.wait_for(300ms);
    ASSERT_EQ(res, std::future_status::ready);
    EXPECT_TRUE(future.get());
}

TEST(AsyncWatchdogTest, CancelBeforeExpiry)
{
    PelcoD::AsyncWatchdog watchdog;

    std::atomic<bool> fired { false };
    const auto timerId = watchdog.schedule(50ms, [&] { fired.store(true); });
    EXPECT_GT(timerId, 0U);

    // Cancel immediately
    EXPECT_TRUE(watchdog.cancel(timerId));

    std::this_thread::sleep_for(100ms);
    EXPECT_FALSE(fired.load());
}

TEST(AsyncWatchdogTest, MultipleTimersInOrder)
{
    PelcoD::AsyncWatchdog watchdog;

    std::vector<int> executionOrder;
    std::mutex orderMutex;
    std::promise<void> donePromise;
    auto doneFuture = donePromise.get_future();
    std::atomic<int> completedCount { 0 };

    auto record = [&](int id) {
        std::scoped_lock lock(orderMutex);
        executionOrder.push_back(id);
        if (++completedCount == 3) {
            donePromise.set_value();
        }
    };

    // Schedule 3 timers with different deadlines in arbitrary order
    static_cast<void>(watchdog.schedule(60ms, [&] { record(3); }));
    static_cast<void>(watchdog.schedule(20ms, [&] { record(1); }));
    static_cast<void>(watchdog.schedule(40ms, [&] { record(2); }));

    const auto res = doneFuture.wait_for(500ms);
    ASSERT_EQ(res, std::future_status::ready);

    std::scoped_lock lock(orderMutex);
    ASSERT_EQ(executionOrder.size(), 3U);
    EXPECT_EQ(executionOrder[0], 1);
    EXPECT_EQ(executionOrder[1], 2);
    EXPECT_EQ(executionOrder[2], 3);
}

TEST(AsyncWatchdogTest, ShutdownCleansUpImmediately)
{
    PelcoD::AsyncWatchdog watchdog;

    std::atomic<bool> fired { false };
    static_cast<void>(watchdog.schedule(500ms, [&] { fired.store(true); }));

    // Immediate shutdown
    watchdog.shutdown();

    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(fired.load());
}

} // namespace
