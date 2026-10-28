#pragma once

/// @file TileCache.h
/// @brief Thread-safe in-memory Least Recently Used (LRU) tile cache.

#include <cstddef>

#include <list>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>

#include "GeoTypes.h"

namespace Mapping {

/// @class TileCache
/// @brief Thread-safe LRU cache storing decoded/encoded tile data in system memory.
/// @details Synchronizes all cache lookups, insertions, and evictions with an internal mutex.
class TileCache {
public:
    /// @brief Default cache capacity (256 tiles, approx 16-32 MB).
    static constexpr std::size_t kDefaultCapacity { 256U };

    /// @brief Constructs a TileCache with the specified maximum tile capacity.
    /// @details Falls back to kDefaultCapacity if capacity is zero.
    /// @param[in] capacity Maximum number of tiles to retain in memory.
    explicit TileCache(std::size_t capacity = kDefaultCapacity) noexcept;

    /// @brief Default destructor.
    /// @details Releases all cached tile buffers.
    ~TileCache() = default;

    TileCache(const TileCache&) = delete;
    TileCache& operator=(const TileCache&) = delete;
    TileCache(TileCache&&) = delete;
    TileCache& operator=(TileCache&&) = delete;

    /// @brief Retrieves a tile from cache and promotes it to the most recently used position.
    /// @details Acquires the internal lock and splices the matching node to the front of the LRU list.
    /// @param[in] coord Discrete tile coordinate.
    /// @return TileData if present, or std::nullopt.
    [[nodiscard]] std::optional<TileData> get(const TileCoord& coord);

    /// @brief Inserts or updates a tile in the cache, evicting the least recently used item if full.
    /// @details Moves existing keys to the front or inserts a new entry at the head of the LRU list.
    /// @param[in] coord Discrete tile coordinate.
    /// @param[in] data Tile data payload.
    void put(const TileCoord& coord, TileData data);

    /// @brief Checks whether the cache contains a tile without modifying LRU order.
    /// @details Performs an O(1) hash table lookup under lock without splicing the LRU list.
    /// @param[in] coord Discrete tile coordinate.
    /// @return True if cached, false otherwise.
    [[nodiscard]] bool contains(const TileCoord& coord) const;

    /// @brief Removes a specific tile from the cache.
    /// @details Erases the tile from both the hash map and the doubly-linked LRU list.
    /// @param[in] coord Discrete tile coordinate.
    /// @return True if removed, false if not found.
    [[nodiscard]] bool remove(const TileCoord& coord);

    /// @brief Clears all entries from the cache.
    /// @details Empties both the lookup map and the LRU ordering list under lock.
    void clear();

    /// @brief Returns the number of tiles currently stored.
    /// @details Queries the current element count under lock.
    /// @return Number of cached tiles.
    [[nodiscard]] std::size_t size() const;

    /// @brief Returns the maximum number of tiles the cache can hold.
    /// @details Queries the configured maximum capacity under lock.
    /// @return Maximum tile capacity.
    [[nodiscard]] std::size_t capacity() const;

    /// @brief Adjusts the maximum capacity, evicting items if current size exceeds new capacity.
    /// @details Clamps newCapacity to at least 1 and evicts least-recently-used entries from the tail.
    /// @param[in] newCapacity New maximum number of items.
    void setCapacity(std::size_t newCapacity);

private:
    std::size_t m_capacity { kDefaultCapacity };
    mutable std::mutex m_mutex { };

    // List of (TileCoord, TileData), front is most recently used, back is least recently used
    std::list<std::pair<TileCoord, TileData>> m_lruList { };

    // Map from TileCoord to iterator in m_lruList for O(1) lookup
    std::unordered_map<TileCoord, std::list<std::pair<TileCoord, TileData>>::iterator> m_cacheMap { };
};

} // namespace Mapping
