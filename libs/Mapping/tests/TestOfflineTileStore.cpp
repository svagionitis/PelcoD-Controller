/// @file TestOfflineTileStore.cpp
/// @brief Unit tests for DiskTileCache, ProceduralGridTileProvider, and CompositeTileProvider.

#include "CompositeTileProvider.h"

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

#include "DiskTileCache.h"
#include "GeoTypes.h"
#include "ITileProvider.h"
#include "ProceduralGridTileProvider.h"

using namespace Mapping;

namespace {
class MockNetworkTileProvider : public ITileProvider {
public:
    std::atomic<int> fetchCount { 0 };

        const TileCoord& coord) override
        {
            (void)coord;
            ++fetchCount;
            TileData d { };
            d.bytes = { 0x01U, 0x02U };
            d.valid = true;
            return d;
        }

        bool hasTile(const TileCoord& coord) const noexcept override
        {
            (void)coord;
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
    const std::filesystem::path tempDir { std::filesystem::temp_directory_path() / "pelcod_test_mapping_cache" };
    std::filesystem::remove_all(tempDir);

    DiskTileCache diskCache(tempDir);
    const TileCoord coord { 42, 88, 7 };

    TileData original { };
    original.bytes = { 0x89U, 'P', 'N', 'G', '\r', '\n', 0x1AU, '\n', 0x00U, 0x01U, 0x02U };
    original.mimeType = "image/png";
    original.valid = true;

    // Store tile
    EXPECT_TRUE(diskCache.storeTile(coord, original));
    EXPECT_TRUE(diskCache.hasTile(coord));

    // Retrieve tile
    const auto retrieved { diskCache.getTile(coord) };
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->bytes, original.bytes);
    EXPECT_EQ(retrieved->mimeType, "image/png");

    std::filesystem::remove_all(tempDir);
}

TEST(TestOfflineTileStore, DiskTileCacheSecondarySearchDirectory)
{
    const std::filesystem::path primaryDir { std::filesystem::temp_directory_path() / "pelcod_primary_cache" };
    const std::filesystem::path offlineUsbDir { std::filesystem::temp_directory_path() / "pelcod_offline_usb" };
    std::filesystem::remove_all(primaryDir);
    std::filesystem::remove_all(offlineUsbDir);

    // Write a tile directly into the offline "USB" structure: {root}/{z}/{x}/{y}.png
    const TileCoord coord { 12, 34, 6 };
    const std::filesystem::path tilePath { offlineUsbDir / "6" / "12" / "34.png" };
    std::filesystem::create_directories(tilePath.parent_path());
    {
        std::ofstream f(tilePath, std::ios::binary);
        f.write("OFFLINE_MAP_DATA", 16);
    }

    DiskTileCache diskCache(primaryDir);
    diskCache.addSearchDirectory(offlineUsbDir);

    // Primary shouldn't have it, but hasTile and getTile should find it in secondary
    EXPECT_TRUE(diskCache.hasTile(coord));
    const auto retrieved { diskCache.getTile(coord) };
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
    const TileData grid { ProceduralGridTileProvider::generateGridTile(coord) };

    EXPECT_TRUE(grid.valid);
    EXPECT_EQ(grid.mimeType, "image/x-portable-pixmap");
    // PPM header + 256 * 256 * 3 bytes
    constexpr std::size_t kExpectedPixelBytes { 256U * 256U * 3U };
    EXPECT_GT(grid.bytes.size(), kExpectedPixelBytes);
}

TEST(TestOfflineTileStore, CompositeProviderOfflineOnlyEnforcement)
{
    const std::filesystem::path tempDir { std::filesystem::temp_directory_path() / "pelcod_composite_cache" };
    std::filesystem::remove_all(tempDir);

    auto diskCache { std::make_shared<DiskTileCache>(tempDir) };
    CompositeTileProvider composite(diskCache);

    auto mockNet { std::make_shared<MockNetworkTileProvider>() };
    composite.setNetworkProvider(mockNet);

    // 1. With offlineOnly = true, network provider is NEVER called
    composite.setOfflineOnly(true);
    composite.setProceduralFallbackEnabled(false);

    const TileCoord missingCoord { 999, 999, 10 };
    const auto result { composite.getTile(missingCoord) };
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(mockNet->fetchCount.load(), 0); // Strictly isolated from network!

    // 2. Enable procedural fallback in offline mode
    composite.setProceduralFallbackEnabled(true);
    const auto fallbackResult { composite.getTile(missingCoord) };
    ASSERT_TRUE(fallbackResult.has_value());
    EXPECT_TRUE(fallbackResult->valid);
    EXPECT_EQ(mockNet->fetchCount.load(), 0); // Still 0 network fetches

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

TEST(TestOfflineTileStore, DiskTileCacheAtomicOverwriteAndSizeCap)
{
    const std::filesystem::path tempDir { std::filesystem::temp_directory_path() / "pelcod_atomic_size_cap_cache" };
    std::filesystem::remove_all(tempDir);

    DiskTileCache diskCache(tempDir);
    const TileCoord coord { 1, 2, 3 };

    TileData initial { };
    initial.bytes = { 0x89U, 0x50U, 0x4EU, 0x47U };
    initial.mimeType = "image/png";
    initial.valid = true;
    EXPECT_TRUE(diskCache.storeTile(coord, initial));

    // Overwrite existing tile atomically and verify no leftover .tmp file remains
    TileData updated { };
    updated.bytes = { 0x89U, 0x50U, 0x4EU, 0x47U, 0x0DU, 0x0AU };
    updated.mimeType = "image/png";
    updated.valid = true;
    EXPECT_TRUE(diskCache.storeTile(coord, updated));

    const auto loaded { diskCache.getTile(coord) };
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->bytes, updated.bytes);

    const std::filesystem::path tmpFile { tempDir / "3" / "1" / "2.png.tmp" };
    EXPECT_FALSE(std::filesystem::exists(tmpFile));

    // Outbound storeTile must reject payloads exceeding 16 MB (kMaxTileFileBytes)
    TileData oversized { };
    oversized.bytes.resize((16U * 1024U * 1024U) + 1U, 0xAAU);
    oversized.mimeType = "image/png";
    oversized.valid = true;
    EXPECT_FALSE(diskCache.storeTile(TileCoord { 2, 2, 3 }, oversized));
    EXPECT_FALSE(diskCache.hasTile(TileCoord { 2, 2, 3 }));

    std::filesystem::remove_all(tempDir);
}

TEST(TestOfflineTileStore, DiskTileCacheJpegMimeAndEmptyFileRejection)
{
    const std::filesystem::path tempDir { std::filesystem::temp_directory_path() / "pelcod_jpeg_and_empty_cache" };
    std::filesystem::remove_all(tempDir);

    DiskTileCache diskCache(tempDir);
    const TileCoord jpgCoord { 3, 5, 4 };

    TileData jpgTile { };
    jpgTile.bytes = { 0xFFU, 0xD8U, 0xFFU, 0xE0U };
    jpgTile.mimeType = "image/jpeg";
    jpgTile.valid = true;

    EXPECT_TRUE(diskCache.storeTile(jpgCoord, jpgTile));
    const auto loadedJpg { diskCache.getTile(jpgCoord) };
    ASSERT_TRUE(loadedJpg.has_value());
    EXPECT_EQ(loadedJpg->mimeType, "image/jpeg");
    EXPECT_EQ(loadedJpg->bytes, jpgTile.bytes);

    // Create a 0-byte empty file on disk and verify getTile rejects it
    const TileCoord emptyCoord { 4, 5, 4 };
    const std::filesystem::path emptyFilePath { tempDir / "4" / "4" / "5.png" };
    std::filesystem::create_directories(emptyFilePath.parent_path());
    {
        std::ofstream emptyFile(emptyFilePath, std::ios::binary | std::ios::trunc);
    }
    EXPECT_TRUE(diskCache.hasTile(emptyCoord));
    EXPECT_FALSE(diskCache.getTile(emptyCoord).has_value());

    std::filesystem::remove_all(tempDir);
}
