#pragma once

/// @file NonceCache.h
/// @brief Bounded, time-limited set of recently seen nonces used for replay detection.

#include <chrono>
#include <cstddef>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Onvif {

/// @class NonceCache
/// @brief Thread-safe replay cache with TTL expiry and a hard capacity bound.
/// @details Each key is remembered for @c ttl after insertion. When the cache is full the
///          oldest entry is evicted first (FIFO, which equals LRU because entries are never
///          refreshed). Memory is therefore O(capacity) regardless of request rate.
/// @note Evicting an unexpired entry under flood re-opens its replay window; size the cache
///       to exceed the expected authenticated request rate multiplied by the TTL.
class NonceCache {
public:
    /// @brief Constructs an empty cache.
    /// @param[in] capacity Maximum number of remembered keys (values of 0 are raised to 1).
    /// @param[in] ttl Time each key is remembered after insertion.
    NonceCache(std::size_t capacity, std::chrono::seconds ttl);

    /// @brief Records a key if it has not been seen within the TTL.
    /// @details Expired entries are purged first. A key that is still live is rejected.
    /// @param[in] key Opaque nonce identifier.
    /// @param[in] now Current monotonic time.
    /// @return True if the key was new (accepted); false if it is a replay.
    /// @note Thread-safe.
    [[nodiscard]] bool insert(const std::string& key, std::chrono::steady_clock::time_point now);

    /// @brief Returns the number of currently remembered keys.
    /// @return Entry count (always <= capacity).
    /// @note Thread-safe.
    [[nodiscard]] std::size_t size() const;

private:
    struct Entry {
        std::string key {};
        std::chrono::steady_clock::time_point inserted {};
    };

    void purgeExpired(std::chrono::steady_clock::time_point now);

    std::size_t m_capacity { 1U };
    std::chrono::seconds m_ttl { 0 };
    mutable std::mutex m_mutex {};
    std::list<Entry> m_order {};
    std::unordered_map<std::string, std::list<Entry>::iterator> m_index {};
};

} // namespace Onvif
