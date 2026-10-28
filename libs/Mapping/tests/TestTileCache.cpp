/// @file TestTileCache.cpp
/// @brief Unit tests for TileCache LRU eviction, capacity resizing, and thread safety.

#include "TileCache.h"

#include <cstdint>

#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "GeoTypes.h"

using namespace Mapping;

TEST(TestTileCache, PutAndGetHit)
{
    TileCache cache(10U);
    const TileCoord coord { 10, 20, 5 };

    TileData sample { };
    sample.bytes = { 0xDEU, 0xADU, 0xBEU, 0xEFU };
    sample.mimeType = "image/png";
    sample.valid = true;

    cache.put(coord, sample);

    EXPECT_TRUE(cache.contains(coord));
    EXPECT_EQ(cache.size(), 1U);

    const auto retrieved { cache.get(coord) };
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->bytes, sample.bytes);
    EXPECT_EQ(retrieved->mimeType, "image/png");
    EXPECT_TRUE(retrieved->valid);
}

TEST(TestTileCache, MissReturnsNullopt)
{
    TileCache cache(10U);
    const TileCoord coord { 99, 99, 5 };

    EXPECT_FALSE(cache.contains(coord));
    EXPECT_FALSE(cache.get(coord).has_value());
}

TEST(TestTileCache, LruEvictionPolicy)
{
    TileCache cache(3U); // Capacity of 3 items

    const TileCoord c1 { 1, 0, 1 };
    const TileCoord c2 { 2, 0, 1 };
    const TileCoord c3 { 3, 0, 1 };
    const TileCoord c4 { 4, 0, 1 };

    TileData dummy { };
    dummy.valid = true;

    cache.put(c1, dummy); // Cache: [c1]
    cache.put(c2, dummy); // Cache: [c2, c1]
    cache.put(c3, dummy); // Cache: [c3, c2, c1]

    EXPECT_EQ(cache.size(), 3U);

    // Access c1 so it becomes MRU: Cache order [c1, c3, c2]
    EXPECT_TRUE(cache.get(c1).has_value());

    // Insert c4 -> least recently used (c2) must be evicted!
    cache.put(c4, dummy);

    EXPECT_EQ(cache.size(), 3U);
    EXPECT_TRUE(cache.contains(c1));
    EXPECT_TRUE(cache.contains(c3));
    EXPECT_TRUE(cache.contains(c4));
    EXPECT_FALSE(cache.contains(c2)); // c2 was evicted
}

TEST(TestTileCache, ClearAndRemove)
{
    TileCache cache(5U);
    const TileCoord c1 { 1, 1, 1 };
    const TileCoord c2 { 2, 2, 2 };

    cache.put(c1, TileData { });
    cache.put(c2, TileData { });
    EXPECT_EQ(cache.size(), 2U);

    EXPECT_TRUE(cache.remove(c1));
    EXPECT_FALSE(cache.remove(c1));
    EXPECT_FALSE(cache.contains(c1));
    EXPECT_EQ(cache.size(), 1U);

    cache.clear();
    EXPECT_EQ(cache.size(), 0U);
}

TEST(TestTileCache, SetCapacityEvictsOldestEntries)
{
    TileCache cache(5U);
    TileData dummy { };
    dummy.valid = true;

    for (int i { 1 }; i <= 5; ++i) {
        cache.put(TileCoord { i, 0, 2 }, dummy);
    }
    EXPECT_EQ(cache.size(), 5U);

    // Shrink capacity to 2 -> oldest 3 items (x=1, x=2, x=3) must be evicted, retaining x=4 and x=5
    cache.setCapacity(2U);
    EXPECT_EQ(cache.capacity(), 2U);
    EXPECT_EQ(cache.size(), 2U);
    EXPECT_FALSE(cache.contains(TileCoord { 1, 0, 2 }));
    EXPECT_FALSE(cache.contains(TileCoord { 2, 0, 2 }));
    EXPECT_FALSE(cache.contains(TileCoord { 3, 0, 2 }));
    EXPECT_TRUE(cache.contains(TileCoord { 4, 0, 2 }));
    EXPECT_TRUE(cache.contains(TileCoord { 5, 0, 2 }));

    // Setting capacity to 0 must clamp to 1 and retain only the most recently inserted item (x=5)
    cache.setCapacity(0U);
    EXPECT_EQ(cache.capacity(), 1U);
    EXPECT_EQ(cache.size(), 1U);
    EXPECT_TRUE(cache.contains(TileCoord { 5, 0, 2 }));
}

TEST(TestTileCache, ThreadSafeConcurrentAccess)
{
    TileCache cache(50U);
    constexpr int kNumThreads { 4 };
    constexpr int kOpsPerThread { 200 };

    std::vector<std::thread> threads { };
    threads.reserve(static_cast<std::size_t>(kNumThreads));

    for (int t { 0 }; t < kNumThreads; ++t) {
        threads.emplace_back([&cache, t]() {
            for (int i { 0 }; i < kOpsPerThread; ++i) {
                const TileCoord coord { (t * 50) + (i % 25), i % 10, 5 };
                TileData data { };
                data.bytes.push_back(static_cast<std::uint8_t>(i & 0xFF));
                cache.put(coord, std::move(data));
                (void)cache.get(coord);
                (void)cache.contains(coord);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_LE(cache.size(), cache.capacity());
}
