#include "CompositeTileProvider.h"
#include "ProceduralGridTileProvider.h"

namespace Mapping {

CompositeTileProvider::CompositeTileProvider(std::shared_ptr<DiskTileCache> diskCache,
                                             std::size_t memoryCacheCapacity)
    : m_memoryCache(memoryCacheCapacity)
    , m_diskCache(std::move(diskCache)) {}

void CompositeTileProvider::setDiskCache(std::shared_ptr<DiskTileCache> diskCache) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_diskCache = std::move(diskCache);
}

void CompositeTileProvider::addOfflineProvider(std::shared_ptr<ITileProvider> provider) {
    if (!provider) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_offlineProviders.push_back(std::move(provider));
}

void CompositeTileProvider::setNetworkProvider(std::shared_ptr<ITileProvider> provider) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_networkProvider = std::move(provider);
}

void CompositeTileProvider::setOfflineOnly(bool offlineOnly) noexcept {
    m_offlineOnly = offlineOnly;
}

bool CompositeTileProvider::isOfflineOnly() const noexcept {
    return m_offlineOnly;
}

void CompositeTileProvider::setProceduralFallbackEnabled(bool enabled) noexcept {
    m_enableProceduralFallback = enabled;
}

bool CompositeTileProvider::isProceduralFallbackEnabled() const noexcept {
    return m_enableProceduralFallback;
}

bool CompositeTileProvider::hasTile(const TileCoord& coord) const noexcept {
    // 1. Check memory cache
    if (m_memoryCache.contains(coord)) {
        return true;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // 2. Check disk cache
    if (m_diskCache && m_diskCache->hasTile(coord)) {
        return true;
    }

    // 3. Check offline providers
    for (const auto& prov : m_offlineProviders) {
        if (prov && prov->hasTile(coord)) {
            return true;
        }
    }

    // Procedural fallback can provide any tile
    return m_enableProceduralFallback;
}

std::optional<TileData> CompositeTileProvider::getTile(const TileCoord& coord) {
    // 1. Check memory LRU cache
    if (auto cached = m_memoryCache.get(coord); cached.has_value()) {
        return cached;
    }

    std::unique_lock<std::mutex> lock(m_mutex);

    // 2. Check primary disk store
    if (m_diskCache && m_diskCache->hasTile(coord)) {
        auto diskData = m_diskCache->getTile(coord);
        if (diskData.has_value()) {
            m_memoryCache.put(coord, *diskData);
            return diskData;
        }
    }

    // 3. Check secondary offline providers
    for (const auto& prov : m_offlineProviders) {
        if (prov && prov->hasTile(coord)) {
            auto data = prov->getTile(coord);
            if (data.has_value()) {
                m_memoryCache.put(coord, *data);
                return data;
            }
        }
    }

    // 4. Check network provider if not in strict offline mode
    if (!m_offlineOnly && m_networkProvider) {
        auto netData = m_networkProvider->getTile(coord);
        if (netData.has_value()) {
            if (m_diskCache) {
                m_diskCache->storeTile(coord, *netData);
            }
            m_memoryCache.put(coord, *netData);
            return netData;
        }
    }

    // 5. Procedural fallback
    if (m_enableProceduralFallback) {
        auto gridTile = ProceduralGridTileProvider::generateGridTile(coord);
        m_memoryCache.put(coord, gridTile);
        return gridTile;
    }

    return std::nullopt;
}

} // namespace Mapping
