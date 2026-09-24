#pragma once

/// @file TileCache.h
/// @brief Thread-safe in-memory Least Recently Used (LRU) tile cache.

#include "GeoTypes.h"

#include <cstddef>
#include <list>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace Mapping {

/// @class TileCache
/// @brief Thread-safe LRU cache storing decoded/encoded tile data in system memory.
class TileCache {
public:
    /// @brief Default cache capacity (256 tiles, approx 16-32 MB).
    static constexpr std::size_t kDefaultCapacity { 256 };

    explicit TileCache(std::size_t capacity = kDefaultCapacity) noexcept;
    ~TileCache() = default;

    TileCache(const TileCache&) = delete;
    TileCache& operator=(const TileCache&) = delete;
    TileCache(TileCache&&) = delete;
    TileCache& operator=(TileCache&&) = delete;

    /// @brief Retrieves a tile from cache and promotes it to the most recently used position.
    /// @param[in] coord Discrete tile coordinate.
    /// @return TileData if present, or std::nullopt.
    [[nodiscard]] std::optional<TileData> get(const TileCoord& coord);

    /// @brief Inserts or updates a tile in the cache, evicting the least recently used item if full.
    /// @param[in] coord Discrete tile coordinate.
    /// @param[in] data Tile data payload.
    void put(const TileCoord& coord, TileData data);

    /// @brief Checks whether the cache contains a tile without modifying LRU order.
    /// @param[in] coord Discrete tile coordinate.
    /// @return True if cached, false otherwise.
    [[nodiscard]] bool contains(const TileCoord& coord) const;

    /// @brief Removes a specific tile from the cache.
    /// @param[in] coord Discrete tile coordinate.
    /// @return True if removed, false if not found.
    bool remove(const TileCoord& coord);

    /// @brief Clears all entries from the cache.
    void clear();

    /// @brief Returns the number of tiles currently stored.
    [[nodiscard]] std::size_t size() const;

    /// @brief Returns the maximum number of tiles the cache can hold.
    [[nodiscard]] std::size_t capacity() const;

    /// @brief Adjusts the maximum capacity, evicting items if current size exceeds new capacity.
    /// @param[in] newCapacity New maximum number of items.
    void setCapacity(std::size_t newCapacity);

private:
    std::size_t m_capacity;
    mutable std::mutex m_mutex;

    // List of (TileCoord, TileData), front is most recently used, back is least recently used
    std::list<std::pair<TileCoord, TileData>> m_lruList;

    // Map from TileCoord to iterator in m_lruList for O(1) lookup
    std::unordered_map<TileCoord, std::list<std::pair<TileCoord, TileData>>::iterator> m_cacheMap;
};

} // namespace Mapping
