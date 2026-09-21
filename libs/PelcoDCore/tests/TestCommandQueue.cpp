/// @file TestCommandQueue.cpp
/// @brief Unit test verifying PacedCommandQueue priority scheduling and retry delays.

#include "PacedCommandQueue.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

namespace {

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
    bool ok = queue.popReady(notReady, [] { return false; }, std::chrono::steady_clock::time_point::max(), std::chrono::milliseconds(0));
    EXPECT_FALSE(ok);

    // Wait until backoff expires
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    PelcoD::CommandItem readyItem;
    ok = queue.popReady(readyItem, [] { return false; });
    EXPECT_TRUE(ok);
    EXPECT_EQ(readyItem.queryTag, "QueryZoom");
    EXPECT_EQ(readyItem.retryCount, 1U);
}

} // namespace
