#include "TileCache.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace Mapping;

TEST(TestTileCache, PutAndGetHit) {
    TileCache cache(10);
    const TileCoord coord { 10, 20, 5 };

    TileData sample;
    sample.bytes = { 0xDE, 0xAD, 0xBE, 0xEF };
    sample.mimeType = "image/png";
    sample.valid = true;

    cache.put(coord, sample);

    EXPECT_TRUE(cache.contains(coord));
    EXPECT_EQ(cache.size(), 1U);

    const auto retrieved = cache.get(coord);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->bytes, sample.bytes);
    EXPECT_EQ(retrieved->mimeType, "image/png");
    EXPECT_TRUE(retrieved->valid);
}

TEST(TestTileCache, MissReturnsNullopt) {
    TileCache cache(10);
    const TileCoord coord { 99, 99, 5 };

    EXPECT_FALSE(cache.contains(coord));
    EXPECT_FALSE(cache.get(coord).has_value());
}

TEST(TestTileCache, LruEvictionPolicy) {
    TileCache cache(3); // Capacity of 3 items

    const TileCoord c1 { 1, 0, 1 };
    const TileCoord c2 { 2, 0, 1 };
    const TileCoord c3 { 3, 0, 1 };
    const TileCoord c4 { 4, 0, 1 };

    TileData dummy;
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

TEST(TestTileCache, ClearAndRemove) {
    TileCache cache(5);
    const TileCoord c1 { 1, 1, 1 };
    const TileCoord c2 { 2, 2, 2 };

    cache.put(c1, TileData {});
    cache.put(c2, TileData {});
    EXPECT_EQ(cache.size(), 2U);

    EXPECT_TRUE(cache.remove(c1));
    EXPECT_FALSE(cache.contains(c1));
    EXPECT_EQ(cache.size(), 1U);

    cache.clear();
    EXPECT_EQ(cache.size(), 0U);
}

TEST(TestTileCache, ThreadSafeConcurrentAccess) {
    TileCache cache(50);
    constexpr int kNumThreads = 4;
    constexpr int kOpsPerThread = 200;

    std::vector<std::thread> threads;
    threads.reserve(kNumThreads);

    for (int t = 0; t < kNumThreads; ++t) {
        threads.emplace_back([&cache, t]() {
            for (int i = 0; i < kOpsPerThread; ++i) {
                const TileCoord coord { (t * 50) + (i % 25), i % 10, 5 };
                TileData data;
                data.bytes = { static_cast<std::uint8_t>(i & 0xFF) };
                cache.put(coord, data);
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
