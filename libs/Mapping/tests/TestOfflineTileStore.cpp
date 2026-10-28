/// @file TestOfflineTileStore.cpp
/// @brief Unit tests for DiskTileCache, ProceduralGridTileProvider, and CompositeTileProvider.

#include "CompositeTileProvider.h"
#include "DiskTileCache.h"
#include "ProceduralGridTileProvider.h"

#include <cstddef>
#include <cstdint>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace Mapping;

namespace {
class MockNetworkTileProvider : public ITileProvider {
public:
    std::atomic<int> fetchCount { 0 };

    std::optional<TileData> getTile(const TileCoord& /*coord*/) override
    {
        ++fetchCount;
        TileData d { };
        d.bytes = { 0x01U, 0x02U };
        d.valid = true;
        return d;
    }

    bool hasTile(const TileCoord& /*coord*/) const noexcept override
    {
        return true;
    }

    std::string providerName() const override
    {
        return "MockNetworkTileProvider";
    }
};
} // namespace

TEST(TestOfflineTileStore, DiskTileCacheStoreAndRetrieve)
{
    const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "pelcod_test_mapping_cache";
    std::filesystem::remove_all(tempDir);

    DiskTileCache diskCache(tempDir);
    const TileCoord coord { 42, 88, 7 };

    TileData original;
    original.bytes = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n', 0x00, 0x01, 0x02 };
    original.mimeType = "image/png";
    original.valid = true;

    // Store tile
    EXPECT_TRUE(diskCache.storeTile(coord, original));
    EXPECT_TRUE(diskCache.hasTile(coord));

    // Retrieve tile
    const auto retrieved = diskCache.getTile(coord);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->bytes, original.bytes);
    EXPECT_EQ(retrieved->mimeType, "image/png");

    std::filesystem::remove_all(tempDir);
}

TEST(TestOfflineTileStore, DiskTileCacheSecondarySearchDirectory)
{
    const std::filesystem::path primaryDir = std::filesystem::temp_directory_path() / "pelcod_primary_cache";
    const std::filesystem::path offlineUsbDir = std::filesystem::temp_directory_path() / "pelcod_offline_usb";
    std::filesystem::remove_all(primaryDir);
    std::filesystem::remove_all(offlineUsbDir);

    // Write a tile directly into the offline "USB" structure: {root}/{z}/{x}/{y}.png
    const TileCoord coord { 12, 34, 6 };
    const std::filesystem::path tilePath = offlineUsbDir / "6" / "12" / "34.png";
    std::filesystem::create_directories(tilePath.parent_path());
    {
        std::ofstream f(tilePath, std::ios::binary);
        f.write("OFFLINE_MAP_DATA", 16);
    }

    DiskTileCache diskCache(primaryDir);
    diskCache.addSearchDirectory(offlineUsbDir);

    // Primary shouldn't have it, but hasTile and getTile should find it in secondary
    EXPECT_TRUE(diskCache.hasTile(coord));
    const auto retrieved = diskCache.getTile(coord);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->bytes.size(), 16U);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(retrieved->bytes.data()), retrieved->bytes.size()),
        "OFFLINE_MAP_DATA");

    std::filesystem::remove_all(primaryDir);
    std::filesystem::remove_all(offlineUsbDir);
}

TEST(TestOfflineTileStore, ProceduralGridTileGeneration)
{
    const TileCoord coord { 100, 200, 9 };
    const TileData grid = ProceduralGridTileProvider::generateGridTile(coord);

    EXPECT_TRUE(grid.valid);
    EXPECT_EQ(grid.mimeType, "image/x-portable-pixmap");
    // PPM header + 256 * 256 * 3 bytes
    constexpr std::size_t kExpectedPixelBytes = 256 * 256 * 3;
    EXPECT_GT(grid.bytes.size(), kExpectedPixelBytes);
}

TEST(TestOfflineTileStore, CompositeProviderOfflineOnlyEnforcement)
{
    const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "pelcod_composite_cache";
    std::filesystem::remove_all(tempDir);

    auto diskCache = std::make_shared<DiskTileCache>(tempDir);
    CompositeTileProvider composite(diskCache);

    auto mockNet = std::make_shared<MockNetworkTileProvider>();
    composite.setNetworkProvider(mockNet);

    // 1. With offlineOnly = true, network provider is NEVER called
    composite.setOfflineOnly(true);
    composite.setProceduralFallbackEnabled(false);

    const TileCoord missingCoord { 999, 999, 10 };
    auto result = composite.getTile(missingCoord);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(mockNet->fetchCount, 0); // Strictly isolated from network!

    // 2. Enable procedural fallback in offline mode
    composite.setProceduralFallbackEnabled(true);
    auto fallbackResult = composite.getTile(missingCoord);
    ASSERT_TRUE(fallbackResult.has_value());
    EXPECT_TRUE(fallbackResult->valid);
    EXPECT_EQ(mockNet->fetchCount, 0); // Still 0 network fetches

    // 3. Tile is now cached in memory
    EXPECT_TRUE(composite.memoryCache().contains(missingCoord));

    std::filesystem::remove_all(tempDir);
}

TEST(TestOfflineTileStore, DiskTileCacheRejectsInvalidCoords)
{
    const std::filesystem::path tempDir { std::filesystem::temp_directory_path() / "pelcod_invalid_coords_cache" };
    std::filesystem::remove_all(tempDir);

    DiskTileCache diskCache(tempDir);
    TileData sample { };
    sample.bytes = { 0x89U, 0x50U };
    sample.valid = true;

    // Negative zoom or negative x/y or zoom > 22 must be rejected
    EXPECT_FALSE(diskCache.storeTile(TileCoord { -1, 0, 5 }, sample));
    EXPECT_FALSE(diskCache.storeTile(TileCoord { 0, -1, 5 }, sample));
    EXPECT_FALSE(diskCache.storeTile(TileCoord { 0, 0, -1 }, sample));
    EXPECT_FALSE(diskCache.storeTile(TileCoord { 0, 0, 25 }, sample));
    EXPECT_FALSE(diskCache.storeTile(TileCoord { 8, 0, 3 }, sample)); // x >= (1 << 3)

    EXPECT_FALSE(diskCache.hasTile(TileCoord { -1, 0, 5 }));
    EXPECT_FALSE(diskCache.getTile(TileCoord { -1, 0, 5 }).has_value());

    std::filesystem::remove_all(tempDir);
}

TEST(TestOfflineTileStore, CompositeProviderConcurrentAccessAndOnlineWriteThrough)
{
    const std::filesystem::path tempDir { std::filesystem::temp_directory_path() / "pelcod_concurrent_composite" };
    std::filesystem::remove_all(tempDir);

    auto diskCache { std::make_shared<DiskTileCache>(tempDir) };
    CompositeTileProvider composite(diskCache);
    auto mockNet { std::make_shared<MockNetworkTileProvider>() };
    composite.setNetworkProvider(mockNet);

    // Verify online write-through to disk and memory cache
    composite.setOfflineOnly(false);
    composite.setProceduralFallbackEnabled(false);
    const TileCoord netCoord { 3, 4, 5 };
    const auto fetched { composite.getTile(netCoord) };
    ASSERT_TRUE(fetched.has_value());
    EXPECT_EQ(mockNet->fetchCount.load(), 1);
    EXPECT_TRUE(diskCache->hasTile(netCoord));
    EXPECT_TRUE(composite.memoryCache().contains(netCoord));

    // Concurrent stress test toggling offlineOnly and proceduralFallback while querying tiles
    std::vector<std::thread> threads { };
    threads.reserve(4U);
    for (int t { 0 }; t < 4; ++t) {
        threads.emplace_back([&composite, t]() {
            for (int i { 0 }; i < 50; ++i) {
                composite.setOfflineOnly((i % 2) == 0);
                composite.setProceduralFallbackEnabled((i % 3) != 0);
                (void)composite.isOfflineOnly();
                (void)composite.isProceduralFallbackEnabled();
                const TileCoord c { (t * 4) + (i % 4), i % 4, 4 };
                (void)composite.hasTile(c);
                (void)composite.getTile(c);
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }

    std::filesystem::remove_all(tempDir);
}
