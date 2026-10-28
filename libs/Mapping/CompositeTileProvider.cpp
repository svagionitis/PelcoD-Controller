/// @file CompositeTileProvider.cpp
/// @brief Implementation of multi-tier chained tile provider with atomic state and lock-free I/O dispatch.

#include "CompositeTileProvider.h"

#include <cstddef>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include "ProceduralGridTileProvider.h"

namespace Mapping {

CompositeTileProvider::CompositeTileProvider(std::shared_ptr<DiskTileCache> diskCache, std::size_t memoryCacheCapacity)
    : m_memoryCache(memoryCacheCapacity)
    , m_diskCache(std::move(diskCache))
{
}

void CompositeTileProvider::setDiskCache(std::shared_ptr<DiskTileCache> diskCache)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_diskCache = std::move(diskCache);
}

void CompositeTileProvider::addOfflineProvider(std::shared_ptr<ITileProvider> provider)
{
    if (!provider) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_offlineProviders.push_back(std::move(provider));
}

void CompositeTileProvider::setNetworkProvider(std::shared_ptr<ITileProvider> provider)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_networkProvider = std::move(provider);
}

void CompositeTileProvider::setOfflineOnly(bool offlineOnly) noexcept
{
    m_offlineOnly.store(offlineOnly, std::memory_order_release);
}

bool CompositeTileProvider::isOfflineOnly() const noexcept
{
    return m_offlineOnly.load(std::memory_order_acquire);
}

void CompositeTileProvider::setProceduralFallbackEnabled(bool enabled) noexcept
{
    m_enableProceduralFallback.store(enabled, std::memory_order_release);
}

bool CompositeTileProvider::isProceduralFallbackEnabled() const noexcept
{
    return m_enableProceduralFallback.load(std::memory_order_acquire);
}

bool CompositeTileProvider::hasTile(const TileCoord& coord) const
{
    // 1. Check memory cache
    if (m_memoryCache.contains(coord)) {
        return true;
    }

    std::shared_ptr<DiskTileCache> diskCache { };
    std::vector<std::shared_ptr<ITileProvider>> offlineProviders { };
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        diskCache = m_diskCache;
        offlineProviders = m_offlineProviders;
    }

    // 2. Check disk cache outside m_mutex
    if (diskCache && diskCache->hasTile(coord)) {
        return true;
    }

    // 3. Check offline providers outside m_mutex
    for (const auto& prov : offlineProviders) {
        if (prov && prov->hasTile(coord)) {
            return true;
        }
    }

    // Procedural fallback can provide any tile
    return isProceduralFallbackEnabled();
}

std::optional<TileData> CompositeTileProvider::getTile(const TileCoord& coord)
{
    // 1. Check memory LRU cache
    if (auto cached = m_memoryCache.get(coord); cached.has_value()) {
        return cached;
    }

    // Snapshot provider pointers under brief lock so disk/network I/O does not block concurrent callers
    std::shared_ptr<DiskTileCache> diskCache { };
    std::vector<std::shared_ptr<ITileProvider>> offlineProviders { };
    std::shared_ptr<ITileProvider> networkProvider { };
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        diskCache = m_diskCache;
        offlineProviders = m_offlineProviders;
        networkProvider = m_networkProvider;
    }

    // 2. Check primary disk store (single lookup)
    if (diskCache) {
        if (auto diskData = diskCache->getTile(coord); diskData.has_value()) {
            m_memoryCache.put(coord, *diskData);
            return diskData;
        }
    }

    // 3. Check secondary offline providers (single lookup per provider)
    for (const auto& prov : offlineProviders) {
        if (prov) {
            if (auto data = prov->getTile(coord); data.has_value()) {
                m_memoryCache.put(coord, *data);
                return data;
            }
        }
    }

    // 4. Check network provider if not in strict offline mode
    if (!isOfflineOnly() && networkProvider) {
        if (auto netData = networkProvider->getTile(coord); netData.has_value()) {
            if (diskCache) {
                (void)diskCache->storeTile(coord, *netData);
            }
            m_memoryCache.put(coord, *netData);
            return netData;
        }
    }

    // 5. Procedural fallback
    if (isProceduralFallbackEnabled()) {
        TileData gridTile { ProceduralGridTileProvider::generateGridTile(coord) };
        m_memoryCache.put(coord, gridTile);
        return gridTile;
    }

    return std::nullopt;
}

} // namespace Mapping
