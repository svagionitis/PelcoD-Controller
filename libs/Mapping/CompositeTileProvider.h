#pragma once

/// @file CompositeTileProvider.h
/// @brief Multi-tier chained tile provider with memory LRU, offline disk stores, and offline-only enforcement.

#include <cstddef>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "DiskTileCache.h"
#include "ITileProvider.h"
#include "TileCache.h"

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
    /// @param[in] diskCache Shared pointer to primary DiskTileCache.
    void setDiskCache(std::shared_ptr<DiskTileCache> diskCache);

    /// @brief Adds an additional offline tile provider (e.g. secondary directory, MBTiles file).
    /// @param[in] provider Shared pointer to secondary offline ITileProvider.
    void addOfflineProvider(std::shared_ptr<ITileProvider> provider);

    /// @brief Configures an optional network downloader provider.
    /// @param[in] provider Shared pointer to network ITileProvider.
    void setNetworkProvider(std::shared_ptr<ITileProvider> provider);

    /// @brief Sets strict offline-only mode. When true, network fetches are never attempted.
    /// @param[in] offlineOnly True to disable network fetches.
    void setOfflineOnly(bool offlineOnly) noexcept;

    /// @brief Checks whether strict offline-only mode is active.
    /// @return True if offline-only mode is active.
    [[nodiscard]] bool isOfflineOnly() const noexcept;

    /// @brief Enables or disables synthetic procedural grid tiles when a tile is missing everywhere.
    /// @param[in] enabled True to enable procedural fallback.
    void setProceduralFallbackEnabled(bool enabled) noexcept;

    /// @brief Checks whether procedural grid fallback is enabled.
    /// @return True if procedural fallback is enabled.
    [[nodiscard]] bool isProceduralFallbackEnabled() const noexcept;

    /// @brief Queries all tiers in sequence and returns the first available tile.
    /// @param[in] coord Discrete tile coordinate.
    /// @return TileData if found or generated, or std::nullopt.
    [[nodiscard]] std::optional<TileData> getTile(const TileCoord& coord) override;

    /// @brief Checks if tile is available in memory or any offline store without initiating network calls.
    /// @param[in] coord Discrete tile coordinate.
    /// @return True if tile is available locally or via procedural fallback.
    [[nodiscard]] bool hasTile(const TileCoord& coord) const override;

    /// @brief Returns provider identification.
    /// @return Provider name string.
    [[nodiscard]] std::string providerName() const override
    {
        return "CompositeTileProvider";
    }

    /// @brief Direct reference to in-memory tile cache.
    /// @return Reference to internal TileCache.
    [[nodiscard]] TileCache& memoryCache() noexcept
    {
        return m_memoryCache;
    }

private:
    TileCache m_memoryCache { };
    std::shared_ptr<DiskTileCache> m_diskCache { };
    std::vector<std::shared_ptr<ITileProvider>> m_offlineProviders { };
    std::shared_ptr<ITileProvider> m_networkProvider { };

    std::atomic<bool> m_offlineOnly { true };
    std::atomic<bool> m_enableProceduralFallback { true };
    mutable std::mutex m_mutex { };
};

} // namespace Mapping
