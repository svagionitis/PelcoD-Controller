#include "TileCache.h"

namespace Mapping {

TileCache::TileCache(std::size_t capacity) noexcept
    : m_capacity(capacity > 0 ? capacity : kDefaultCapacity) {}

std::optional<TileData> TileCache::get(const TileCoord& coord) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cacheMap.find(coord);
    if (it == m_cacheMap.end()) {
        return std::nullopt;
    }

    // Splice item to front of LRU list
    m_lruList.splice(m_lruList.begin(), m_lruList, it->second);
    return it->second->second;
}

void TileCache::put(const TileCoord& coord, TileData data) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_cacheMap.find(coord);
    if (it != m_cacheMap.end()) {
        // Update existing entry and move to front
        it->second->second = std::move(data);
        m_lruList.splice(m_lruList.begin(), m_lruList, it->second);
        return;
    }

    // Evict if at capacity
    if (m_lruList.size() >= m_capacity && !m_lruList.empty()) {
        const TileCoord& oldest = m_lruList.back().first;
        m_cacheMap.erase(oldest);
        m_lruList.pop_back();
    }

    // Insert new item at front
    m_lruList.emplace_front(coord, std::move(data));
    m_cacheMap[coord] = m_lruList.begin();
}

bool TileCache::contains(const TileCoord& coord) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cacheMap.find(coord) != m_cacheMap.end();
}

bool TileCache::remove(const TileCoord& coord) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cacheMap.find(coord);
    if (it == m_cacheMap.end()) {
        return false;
    }

    m_lruList.erase(it->second);
    m_cacheMap.erase(it);
    return true;
}

void TileCache::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cacheMap.clear();
    m_lruList.clear();
}

std::size_t TileCache::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cacheMap.size();
}

std::size_t TileCache::capacity() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_capacity;
}

void TileCache::setCapacity(std::size_t newCapacity) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_capacity = (newCapacity > 0) ? newCapacity : 1;
    while (m_lruList.size() > m_capacity) {
        const TileCoord& oldest = m_lruList.back().first;
        m_cacheMap.erase(oldest);
        m_lruList.pop_back();
    }
}

} // namespace Mapping
