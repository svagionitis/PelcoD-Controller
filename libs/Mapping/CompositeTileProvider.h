#pragma once

/// @file CompositeTileProvider.h
/// @brief Multi-tier chained tile provider with memory LRU, offline disk stores, and offline-only enforcement.

#include "DiskTileCache.h"
#include "ITileProvider.h"
#include "TileCache.h"

#include <memory>
#include <mutex>
#include <vector>

namespace Mapping {

/// @class CompositeTileProvider
/// @brief Orchestrates tile queries across multiple tiers:
///        Memory LRU -> Primary Disk -> Secondary Offline Stores -> Network (optional) -> Procedural Fallback.
class CompositeTileProvider : public ITileProvider {
public:
    /// @brief Constructs a CompositeTileProvider.
    /// @param[in] diskCache Optional primary disk storage instance.
    /// @param[in] memoryCacheCapacity Number of tiles to retain in fast memory LRU cache.
    explicit CompositeTileProvider(std::shared_ptr<DiskTileCache> diskCache = nullptr,
                                   std::size_t memoryCacheCapacity = TileCache::kDefaultCapacity);

    /// @brief Sets the primary disk cache.
    void setDiskCache(std::shared_ptr<DiskTileCache> diskCache);

    /// @brief Adds an additional offline tile provider (e.g. secondary directory, MBTiles file).
    void addOfflineProvider(std::shared_ptr<ITileProvider> provider);

    /// @brief Configures an optional network downloader provider.
    void setNetworkProvider(std::shared_ptr<ITileProvider> provider);

    /// @brief Sets strict offline-only mode. When true, network fetches are never attempted.
    void setOfflineOnly(bool offlineOnly) noexcept;

    /// @brief Checks whether strict offline-only mode is active.
    [[nodiscard]] bool isOfflineOnly() const noexcept;

    /// @brief Enables or disables synthetic procedural grid tiles when a tile is missing everywhere.
    void setProceduralFallbackEnabled(bool enabled) noexcept;

    /// @brief Checks whether procedural grid fallback is enabled.
    [[nodiscard]] bool isProceduralFallbackEnabled() const noexcept;

    /// @brief Queries all tiers in sequence and returns the first available tile.
    [[nodiscard]] std::optional<TileData> getTile(const TileCoord& coord) override;

    /// @brief Checks if tile is available in memory or any offline store without initiating network calls.
    [[nodiscard]] bool hasTile(const TileCoord& coord) const noexcept override;

    /// @brief Returns provider identification.
    [[nodiscard]] std::string providerName() const override { return "CompositeTileProvider"; }

    /// @brief Direct reference to in-memory tile cache.
    [[nodiscard]] TileCache& memoryCache() noexcept { return m_memoryCache; }

private:
    TileCache m_memoryCache;
    std::shared_ptr<DiskTileCache> m_diskCache;
    std::vector<std::shared_ptr<ITileProvider>> m_offlineProviders;
    std::shared_ptr<ITileProvider> m_networkProvider;

    bool m_offlineOnly { true };
    bool m_enableProceduralFallback { true };
    mutable std::mutex m_mutex;
};

} // namespace Mapping
