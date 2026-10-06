/// @file NonceCache.cpp
/// @brief Implementation of the bounded nonce replay cache.

#include "NonceCache.h"

#include <algorithm>

namespace Onvif {

NonceCache::NonceCache(std::size_t capacity, std::chrono::seconds ttl)
    : m_capacity { std::max<std::size_t>(capacity, 1U) }
    , m_ttl { ttl }
{
}

void NonceCache::purgeExpired(std::chrono::steady_clock::time_point now)
{
    while (!m_order.empty()) {
        const Entry& front { m_order.front() };
        if ((now - front.inserted) < m_ttl) {
            break;
        }
        static_cast<void>(m_index.erase(front.key));
        m_order.pop_front();
    }
}

bool NonceCache::insert(const std::string& key, std::chrono::steady_clock::time_point now)
{
    const std::scoped_lock lock { m_mutex };
    purgeExpired(now);

    const auto found { m_index.find(key) };
    if (found != m_index.end()) {
        if ((now - found->second->inserted) < m_ttl) {
            return false;
        }
        static_cast<void>(m_order.erase(found->second));
        static_cast<void>(m_index.erase(found));
    }

    while (m_order.size() >= m_capacity) {
        static_cast<void>(m_index.erase(m_order.front().key));
        m_order.pop_front();
    }

    m_order.push_back(Entry { key, now });
    auto last { m_order.end() };
    --last;
    static_cast<void>(m_index.emplace(key, last));
    return true;
}

std::size_t NonceCache::size() const
{
    const std::scoped_lock lock { m_mutex };
    return m_order.size();
}

} // namespace Onvif
