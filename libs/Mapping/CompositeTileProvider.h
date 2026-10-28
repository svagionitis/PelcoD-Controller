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
#include "GeoTypes.h"
#include "ITileProvider.h"
#include "TileCache.h"

namespace Mapping {

/// @class CompositeTileProvider
/// @brief Orchestrates tile queries across multiple tiers:
///        Memory LRU -> Primary Disk -> Secondary Offline Stores -> Network (optional) -> Procedural Fallback.
/// @details Uses atomic mode flags and short-lived pointer snapshots under lock so disk and network I/O
///          never block concurrent rendering threads.
class CompositeTileProvider : public ITileProvider {
public:
    /// @brief Constructs a CompositeTileProvider.
    /// @details Initializes the in-memory LRU cache and binds the optional primary disk cache tier.
    /// @param[in] diskCache Optional primary disk storage instance.
    /// @param[in] memoryCacheCapacity Number of tiles to retain in fast memory LRU cache.
    explicit CompositeTileProvider(std::shared_ptr<DiskTileCache> diskCache = nullptr,
        std::size_t memoryCacheCapacity = TileCache::kDefaultCapacity);

    /// @brief Sets the primary disk cache.
    /// @details Thread-safely updates the primary read-write DiskTileCache instance.
    /// @param[in] diskCache Shared pointer to primary DiskTileCache.
    void setDiskCache(std::shared_ptr<DiskTileCache> diskCache);

    /// @brief Adds an additional offline tile provider (e.g. secondary directory, MBTiles file).
    /// @details Appends a read-only offline tile provider to the secondary lookup chain.
    /// @param[in] provider Shared pointer to secondary offline ITileProvider.
    void addOfflineProvider(std::shared_ptr<ITileProvider> provider);

    /// @brief Configures an optional network downloader provider.
    /// @details Sets the fallback online tile provider queried only when isOfflineOnly() is false.
    /// @param[in] provider Shared pointer to network ITileProvider.
    void setNetworkProvider(std::shared_ptr<ITileProvider> provider);

    /// @brief Sets strict offline-only mode. When true, network fetches are never attempted.
    /// @details Atomically updates the offline isolation flag with release memory ordering.
    /// @param[in] offlineOnly True to disable network fetches.
    void setOfflineOnly(bool offlineOnly) noexcept;

    /// @brief Checks whether strict offline-only mode is active.
    /// @details Atomically reads the offline isolation flag with acquire memory ordering.
    /// @return True if offline-only mode is active.
    [[nodiscard]] bool isOfflineOnly() const noexcept;

    /// @brief Enables or disables synthetic procedural grid tiles when a tile is missing everywhere.
    /// @details Atomically updates the procedural fallback flag with release memory ordering.
    /// @param[in] enabled True to enable procedural fallback.
    void setProceduralFallbackEnabled(bool enabled) noexcept;

    /// @brief Checks whether procedural grid fallback is enabled.
    /// @details Atomically reads the procedural fallback flag with acquire memory ordering.
    /// @return True if procedural fallback is enabled.
    [[nodiscard]] bool isProceduralFallbackEnabled() const noexcept;

    /// @brief Queries all tiers in sequence and returns the first available tile.
    /// @details Promotes hits from disk, offline, network, or procedural tiers into the memory LRU cache.
    /// @param[in] coord Discrete tile coordinate.
    /// @return TileData if found or generated, or std::nullopt.
    [[nodiscard]] std::optional<TileData> getTile(const TileCoord& coord) override;

    /// @brief Checks if tile is available in memory or any offline store without initiating network calls.
    /// @details Queries memory LRU, primary disk, secondary offline providers, and procedural fallback status.
    /// @param[in] coord Discrete tile coordinate.
    /// @return True if tile is available locally or via procedural fallback.
    [[nodiscard]] bool hasTile(const TileCoord& coord) const override;

    /// @brief Returns provider identification.
    /// @details Identifies this instance as the multi-tier composite orchestrator.
    /// @return Provider name string.
    [[nodiscard]] std::string providerName() const override
    {
        return "CompositeTileProvider";
    }

    /// @brief Direct reference to in-memory tile cache.
    /// @details Allows callers to inspect cache size, adjust capacity, or clear cached tiles.
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
