#include "CancellationToken.h"
#include "TaskScheduler.h"
#include "ThreadPoolExecutor.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

// ============================================================================
// CancellationToken Tests
// ============================================================================

TEST(CancellationTokenTest, InitialStateNotCancelled)
{
    PelcoD::CancellationTokenSource source;
    auto token = source.getToken();

    EXPECT_FALSE(token.isCancellationRequested());
}

TEST(CancellationTokenTest, CancelNotifiesToken)
{
    PelcoD::CancellationTokenSource source;
    auto token = source.getToken();

    source.cancel();
    EXPECT_TRUE(token.isCancellationRequested());
}

TEST(CancellationTokenTest, RegisterCallbackFiredOnCancel)
{
    PelcoD::CancellationTokenSource source;
    auto token = source.getToken();
    std::atomic<bool> callbackFired { false };

    token.registerCallback([&callbackFired] { callbackFired = true; });

    EXPECT_FALSE(callbackFired.load());
    source.cancel();
    EXPECT_TRUE(callbackFired.load());
}

TEST(CancellationTokenTest, SleepForInterruptsEarly)
{
    PelcoD::CancellationTokenSource source;
    auto token = source.getToken();

    const auto startTime = std::chrono::steady_clock::now();

    std::thread worker([token] {
        const bool finished = token.sleepFor(2000ms);
        EXPECT_FALSE(finished); // Interrupted
    });

    std::this_thread::sleep_for(30ms);
    source.cancel();

    if (worker.joinable()) {
        worker.join();
    }

    const auto elapsed
        = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);

    EXPECT_LT(elapsed.count(), 500); // Woke up early, did not sleep 2000ms
}

// ============================================================================
// TaskScheduler Tests
// ============================================================================

TEST(TaskSchedulerTest, ScheduleOneShotExecutes)
{
    PelcoD::TaskScheduler scheduler;
    scheduler.start();

    std::atomic<bool> executed { false };
    const auto taskId = scheduler.schedule(30ms, [&executed] { executed = true; });
    EXPECT_NE(taskId, 0U);

    EXPECT_FALSE(executed.load());
    std::this_thread::sleep_for(80ms);
    EXPECT_TRUE(executed.load());

    scheduler.stop();
}

TEST(TaskSchedulerTest, CancelPreventsExecution)
{
    PelcoD::TaskScheduler scheduler;
    scheduler.start();

    std::atomic<bool> executed { false };
    const auto taskId = scheduler.schedule(60ms, [&executed] { executed = true; });

    const bool cancelled = scheduler.cancel(taskId);
    EXPECT_TRUE(cancelled);

    std::this_thread::sleep_for(100ms);
    EXPECT_FALSE(executed.load());

    scheduler.stop();
}

TEST(TaskSchedulerTest, PeriodicTaskRepeats)
{
    PelcoD::TaskScheduler scheduler;
    scheduler.start();

    std::atomic<int> counter { 0 };
    const auto taskId = scheduler.schedulePeriodic(20ms, [&counter] { ++counter; });

    std::this_thread::sleep_for(90ms);
    EXPECT_TRUE(scheduler.cancel(taskId));
    const int countAfterCancel = counter.load();
    EXPECT_GE(countAfterCancel, 3);

    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(counter.load(), countAfterCancel); // No further executions after cancel

    scheduler.stop();
}

TEST(TaskSchedulerTest, SchedulerShutdownCleansUpSafely)
{
    {
        PelcoD::TaskScheduler scheduler;
        scheduler.start();
        const auto id1 = scheduler.schedule(500ms, [] {});
        const auto id2 = scheduler.schedulePeriodic(100ms, [] {});
        EXPECT_NE(id1, 0U);
        EXPECT_NE(id2, 0U);
        // Destruction stops scheduler cleanly
    }
    SUCCEED();
}

// ============================================================================
// ThreadPoolExecutor Tests
// ============================================================================

TEST(ThreadPoolExecutorTest, SubmitAndExecuteTasks)
{
    PelcoD::ThreadPoolExecutor pool(2U);
    pool.start();

    std::atomic<int> completedTasks { 0 };
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(pool.submit([&completedTasks] {
            std::this_thread::sleep_for(10ms);
            ++completedTasks;
        }));
    }

    std::this_thread::sleep_for(150ms);
    EXPECT_EQ(completedTasks.load(), 10);

    pool.stop();
}

TEST(ThreadPoolExecutorTest, ShutdownDrainsOrStopsSafely)
{
    {
        PelcoD::ThreadPoolExecutor pool(2U);
        pool.start();
        for (int i = 0; i < 5; ++i) {
            EXPECT_TRUE(pool.submit([] { std::this_thread::sleep_for(50ms); }));
        }
        pool.stop();
    }
    SUCCEED();
}

} // namespace
