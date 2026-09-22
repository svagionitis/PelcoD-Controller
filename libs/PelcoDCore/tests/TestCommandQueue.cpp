/// @file TestCommandQueue.cpp
/// @brief Unit test verifying PacedCommandQueue priority scheduling, capacity bounds, and retry delays.

#include "PacedCommandQueue.h"
#include "RetryPolicy.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <future>
#include <thread>
#include <vector>

namespace {

/// @brief Verify prioritized ordering between Normal and Urgent command items.
/// @details Ensures that enqueuing an Urgent item after a Normal item results in the Urgent
///          item being popped first via popReady.
TEST(CommandQueueTest, PriorityOrdering)
{
    PelcoD::PacedCommandQueue queue;
    EXPECT_TRUE(queue.empty());

    const std::vector<std::uint8_t> normalFrame { 0xFF, 0x01, 0x00, 0x02, 0x20, 0x00, 0x23 };
    const std::vector<std::uint8_t> urgentFrame { 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01 };

    queue.enqueue(normalFrame, "Normal1", PelcoD::CommandPriority::Normal);
    queue.enqueue(urgentFrame, "Urgent1", PelcoD::CommandPriority::Urgent);

    PelcoD::CommandItem item1;
    bool ok = queue.popReady(item1, [] { return false; });
    EXPECT_TRUE(ok);
    EXPECT_EQ(item1.queryTag, "Urgent1");
    EXPECT_EQ(item1.priority, PelcoD::CommandPriority::Urgent);

    PelcoD::CommandItem item2;
    ok = queue.popReady(item2, [] { return false; });
    EXPECT_TRUE(ok);
    EXPECT_EQ(item2.queryTag, "Normal1");
    EXPECT_EQ(item2.priority, PelcoD::CommandPriority::Normal);

    EXPECT_TRUE(queue.empty());
}

/// @brief Verify that enqueuing an Urgent command purges all pending Low-priority commands.
/// @details Tests that background telemetry queries (Low priority) are discarded when a high-priority
///          command (e.g. Stop or PTZ command) is queued.
TEST(CommandQueueTest, UrgentPurgesLowPriority)
{
    PelcoD::PacedCommandQueue queue;

    const std::vector<std::uint8_t> lowFrame { 0xFF, 0x01, 0x00, 0x51, 0x00, 0x00, 0x52 };
    const std::vector<std::uint8_t> urgentFrame { 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01 };

    queue.enqueue(lowFrame, "QueryPan", PelcoD::CommandPriority::Low);
    EXPECT_TRUE(queue.hasLowPriorityPending());
    EXPECT_EQ(queue.size(), 1U);

    queue.enqueue(urgentFrame, "Stop", PelcoD::CommandPriority::Urgent);
    EXPECT_FALSE(queue.hasLowPriorityPending());
    EXPECT_EQ(queue.size(), 1U);

    PelcoD::CommandItem popped;
    const bool ok = queue.popReady(popped, [] { return false; });
    EXPECT_TRUE(ok);
    EXPECT_EQ(popped.queryTag, "Stop");
    EXPECT_TRUE(queue.empty());
}

/// @brief Verify scheduled retry delay and exponential backoff timing in command queue.
/// @details Tests scheduleRetry setting earliestDispatchTime in the future and popReady
///          deferring dispatch until the backoff interval expires.
TEST(CommandQueueTest, RetryBackoff)
{
    PelcoD::PacedCommandQueue queue;

    PelcoD::RetryConfig cfg;
    cfg.maxRetries = 3U;
    cfg.initialBackoff = std::chrono::milliseconds(40);
    cfg.strategy = PelcoD::BackoffStrategy::Exponential;
    cfg.backoffMultiplier = 2.0;

    PelcoD::CommandItem item;
    item.frame = { 0xFF, 0x01, 0x00, 0x55, 0x00, 0x00, 0x56 };
    item.queryTag = "QueryZoom";
    item.priority = PelcoD::CommandPriority::Normal;
    item.retryCount = 0U;

    const auto delay1 = queue.scheduleRetry(item, cfg, "Attempt 1");
    EXPECT_EQ(delay1.count(), 40);
    EXPECT_EQ(queue.size(), 1U);

    // Immediate pop should not return item because earliestDispatchTime is in the future
    PelcoD::CommandItem notReady;
    bool ok = queue.popReady(
        notReady, [] { return false; }, std::chrono::steady_clock::time_point::max(), std::chrono::milliseconds(0));
    EXPECT_FALSE(ok);

    // Wait until backoff expires
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    PelcoD::CommandItem readyItem;
    ok = queue.popReady(readyItem, [] { return false; });
    EXPECT_TRUE(ok);
    EXPECT_EQ(readyItem.queryTag, "QueryZoom");
    EXPECT_EQ(readyItem.retryCount, 1U);
}

/// @brief Verify capacity bounds and eviction of oldest non-urgent commands upon queue overflow.
/// @details Instantiates a queue with small capacity (e.g. 3), fills it, and verifies that adding
///          new items evicts non-urgent items while maintaining queue size at capacity.
TEST(CommandQueueTest, QueueCapacityAndEviction)
{
    // Queue with small capacity of 3 items
    PelcoD::PacedCommandQueue smallQueue(3U);
    EXPECT_EQ(smallQueue.maxCapacity(), 3U);

    smallQueue.enqueue({ 0xFF, 0x01, 0x00, 0x01 }, "Normal1", PelcoD::CommandPriority::Normal);
    smallQueue.enqueue({ 0xFF, 0x01, 0x00, 0x02 }, "Normal2", PelcoD::CommandPriority::Normal);
    smallQueue.enqueue({ 0xFF, 0x01, 0x00, 0x03 }, "Normal3", PelcoD::CommandPriority::Normal);
    EXPECT_EQ(smallQueue.size(), 3U);

    // 4th item enqueued should cause oldest non-urgent item ("Normal1") to be dropped
    smallQueue.enqueue({ 0xFF, 0x01, 0x00, 0x04 }, "Normal4", PelcoD::CommandPriority::Normal);
    EXPECT_EQ(smallQueue.size(), 3U);

    PelcoD::CommandItem popped;
    ASSERT_TRUE(smallQueue.popReady(popped, [] { return false; }));
    EXPECT_EQ(popped.queryTag, "Normal2");

    ASSERT_TRUE(smallQueue.popReady(popped, [] { return false; }));
    EXPECT_EQ(popped.queryTag, "Normal3");

    ASSERT_TRUE(smallQueue.popReady(popped, [] { return false; }));
    EXPECT_EQ(popped.queryTag, "Normal4");

    EXPECT_TRUE(smallQueue.empty());
}

/// @brief Verify queue clearing and empty state inquiry methods.
/// @details Enqueues several commands across priority tiers, invokes clear(), and verifies
///          that empty() returns true, size() is zero, and popReady returns false immediately.
TEST(CommandQueueTest, ClearAndEmptyState)
{
    PelcoD::PacedCommandQueue queue;

    queue.enqueue({ 0xFF, 0x01, 0x00, 0x00 }, "Norm0", PelcoD::CommandPriority::Normal);
    queue.enqueue({ 0xFF, 0x01, 0x00, 0x01 }, "Norm1", PelcoD::CommandPriority::Normal);
    queue.enqueue({ 0xFF, 0x01, 0x00, 0x02 }, "Urg1", PelcoD::CommandPriority::Urgent);
    EXPECT_EQ(queue.size(), 3U);
    EXPECT_FALSE(queue.empty());

    queue.clear();
    EXPECT_EQ(queue.size(), 0U);
    EXPECT_TRUE(queue.empty());
    EXPECT_FALSE(queue.hasLowPriorityPending());

    PelcoD::CommandItem popped;
    const bool ok = queue.popReady(popped, [] { return true; });
    EXPECT_FALSE(ok);
}

/// @brief Verify thread unblocking via stop predicate and wakeAll().
/// @details Spawns a worker waiting on popReady on an empty queue, triggers wakeAll with stopPredicate
///          returning true, and verifies prompt return without thread deadlock.
TEST(CommandQueueTest, WakeAllAndCancellation)
{
    PelcoD::PacedCommandQueue queue;
    std::atomic<bool> shouldStop { false };

    auto future = std::async(std::launch::async, [&]() {
        PelcoD::CommandItem item;
        return queue.popReady(
            item, [&]() { return shouldStop.load(); }, std::chrono::steady_clock::time_point::max(),
            std::chrono::milliseconds(5000));
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    shouldStop.store(true);
    queue.wakeAll();

    ASSERT_EQ(future.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    EXPECT_FALSE(future.get());
}

/// @brief Verify three-tier priority interleaving (Low, Normal, Urgent).
/// @details Enqueues items in order [Low1, Normal1, Low2, Urgent1, Normal2], verifying that
///          Urgent purges Lows and pops before Normal items.
TEST(CommandQueueTest, ThreeTierPriorityInterleaving)
{
    PelcoD::PacedCommandQueue queue;

    queue.enqueue({ 0xFF, 0x01 }, "Low1", PelcoD::CommandPriority::Low);
    queue.enqueue({ 0xFF, 0x02 }, "Norm1", PelcoD::CommandPriority::Normal);
    queue.enqueue({ 0xFF, 0x03 }, "Urg1", PelcoD::CommandPriority::Urgent);
    queue.enqueue({ 0xFF, 0x04 }, "Norm2", PelcoD::CommandPriority::Normal);

    // Urgent1 must have purged Low1. Normal1 and Normal2 should remain.
    EXPECT_EQ(queue.size(), 3U);
    EXPECT_FALSE(queue.hasLowPriorityPending());

    PelcoD::CommandItem item;
    ASSERT_TRUE(queue.popReady(item, [] { return false; }));
    EXPECT_EQ(item.queryTag, "Urg1");

    ASSERT_TRUE(queue.popReady(item, [] { return false; }));
    EXPECT_EQ(item.queryTag, "Norm1");

    ASSERT_TRUE(queue.popReady(item, [] { return false; }));
    EXPECT_EQ(item.queryTag, "Norm2");

    EXPECT_TRUE(queue.empty());
}

/// @brief Verify pure calculateBackoffDelay algorithms and boundaries in RetryPolicy.h.
/// @details Validates Fixed, Linear, and Exponential backoff delay computations, verifying
///          multiplier scaling, zero-attempt returns, and maxBackoff ceiling clamping.
TEST(CommandQueueTest, BackoffStrategyCalculations)
{
    PelcoD::RetryConfig cfg;
    cfg.initialBackoff = std::chrono::milliseconds(100);
    cfg.maxBackoff = std::chrono::milliseconds(500);

    // Attempt 0 returns 0
    EXPECT_EQ(PelcoD::calculateBackoffDelay(cfg, 0U).count(), 0);

    // Fixed strategy
    cfg.strategy = PelcoD::BackoffStrategy::Fixed;
    EXPECT_EQ(PelcoD::calculateBackoffDelay(cfg, 1U).count(), 100);
    EXPECT_EQ(PelcoD::calculateBackoffDelay(cfg, 5U).count(), 100);

    // Linear strategy (100 * attempt)
    cfg.strategy = PelcoD::BackoffStrategy::Linear;
    EXPECT_EQ(PelcoD::calculateBackoffDelay(cfg, 1U).count(), 100);
    EXPECT_EQ(PelcoD::calculateBackoffDelay(cfg, 2U).count(), 200);
    EXPECT_EQ(PelcoD::calculateBackoffDelay(cfg, 3U).count(), 300);
    EXPECT_EQ(PelcoD::calculateBackoffDelay(cfg, 10U).count(), 500); // Clamped to maxBackoff 500

    // Exponential strategy (initial * 2^(attempt-1))
    cfg.strategy = PelcoD::BackoffStrategy::Exponential;
    cfg.backoffMultiplier = 2.0;
    EXPECT_EQ(PelcoD::calculateBackoffDelay(cfg, 1U).count(), 100);
    EXPECT_EQ(PelcoD::calculateBackoffDelay(cfg, 2U).count(), 200);
    EXPECT_EQ(PelcoD::calculateBackoffDelay(cfg, 3U).count(), 400);
    EXPECT_EQ(PelcoD::calculateBackoffDelay(cfg, 4U).count(), 500); // Clamped to 500
}

} // namespace
