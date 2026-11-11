/// @file TestStabilizerArena.cpp
/// @brief Unit tests for StabilizerArena monotonic scratchpad memory allocator.
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <utility>

#include "StabilizerArena.h"

using namespace Stabilizer;

TEST(TestStabilizerArena, BasicAllocationAndAlignment)
{
    constexpr std::size_t Capacity = 1024U * 1024U; // 1 MB
    StabilizerArena arena { Capacity };

    EXPECT_EQ(arena.capacity(), Capacity);
    EXPECT_EQ(arena.usedBytes(), 0U);
    EXPECT_EQ(arena.availableBytes(), Capacity);

    // 64-byte aligned allocation
    void* ptr1 = arena.allocate(100U, 64U);
    ASSERT_NE(ptr1, nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(ptr1) % 64U, 0U);
    EXPECT_GE(arena.usedBytes(), 100U);

    // Second allocation
    void* ptr2 = arena.allocate(250U, 64U);
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(ptr2) % 64U, 0U);
    EXPECT_GT(ptr2, ptr1);

    // Reset
    arena.reset();
    EXPECT_EQ(arena.usedBytes(), 0U);
    EXPECT_EQ(arena.availableBytes(), Capacity);

    // Re-allocate after reset
    void* ptr3 = arena.allocate(500U, 64U);
    ASSERT_NE(ptr3, nullptr);
    EXPECT_EQ(ptr3, ptr1);
}

TEST(TestStabilizerArena, CapacityExhaustion)
{
    constexpr std::size_t SmallCapacity = 512U;
    StabilizerArena arena { SmallCapacity };

    void* ptr1 = arena.allocate(400U, 64U);
    ASSERT_NE(ptr1, nullptr);

    // Requesting more than remaining capacity must safely return nullptr
    void* ptr2 = arena.allocate(300U, 64U);
    EXPECT_EQ(ptr2, nullptr);
}

TEST(TestStabilizerArena, MoveSemantics)
{
    StabilizerArena arena1 { 4096U };
    void* ptr = arena1.allocate(128U, 64U);
    EXPECT_NE(ptr, nullptr);

    StabilizerArena arena2 { std::move(arena1) };
    EXPECT_EQ(arena2.capacity(), 4096U);
    EXPECT_GE(arena2.usedBytes(), 128U);
    EXPECT_EQ(arena1.capacity(), 0U);
}
