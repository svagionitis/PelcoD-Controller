/// @file ScopeStore.cpp
/// @brief Implementation of thread-safe ScopeStore for ONVIF device scopes and discovery synchronization.

#include "ScopeStore.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Onvif {

namespace {

    /// @brief Deduplicates a collection of scope URIs while preserving insertion order.
    /// @param[in] scopes Raw vector of scope strings.
    /// @return Cleaned vector with empty strings and duplicate URIs removed.
    [[nodiscard]] std::vector<std::string> deduplicateScopes(const std::vector<std::string>& scopes)
    {
        std::vector<std::string> result;
        result.reserve(scopes.size());
        for (const auto& uri : scopes) {
            if (!uri.empty() && (std::find(result.begin(), result.end(), uri) == result.end())) {
                result.push_back(uri);
            }
        }
        return result;
    }

} // namespace

ScopeStore::ScopeStore(const std::vector<std::string>& initialScopes, std::uint32_t initialVersion)
    : m_scopes(deduplicateScopes(initialScopes))
    , m_metadataVersion(initialVersion)
{
}

std::vector<std::string> ScopeStore::getScopes() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_scopes;
}

std::vector<std::string> ScopeStore::getActiveScopes(const std::string& deviceName, const std::string& model) const
{
    std::vector<std::string> active;
    active.reserve(4U + m_scopes.size());

    // Canonical ONVIF Fixed Scopes
    active.push_back("onvif://www.onvif.org/type/video_encoder");
    active.push_back("onvif://www.onvif.org/type/ptz");
    active.push_back("onvif://www.onvif.org/name/" + deviceName);
    active.push_back("onvif://www.onvif.org/hardware/" + model);

    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        for (const auto& scope : m_scopes) {
            active.push_back(scope);
        }
    }

    return active;
}

bool ScopeStore::hasScope(std::string_view scopeUri) const
{
    if (scopeUri.empty()) {
        return false;
    }
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return std::find(m_scopes.begin(), m_scopes.end(), scopeUri) != m_scopes.end();
}

std::size_t ScopeStore::size() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_scopes.size();
}

bool ScopeStore::empty() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_scopes.empty();
}

void ScopeStore::setScopes(const std::vector<std::string>& newScopes)
{
    auto cleaned = deduplicateScopes(newScopes);
    std::vector<ScopeObserver> observersToNotify;
    std::uint32_t versionToNotify { 0U };

    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_scopes = std::move(cleaned);
        ++m_metadataVersion;
        versionToNotify = m_metadataVersion;
        observersToNotify = m_observers;
    }

    notifyObservers(observersToNotify, versionToNotify);
}

void ScopeStore::addScopes(const std::vector<std::string>& addedScopes)
{
    if (addedScopes.empty()) {
        return;
    }

    std::vector<ScopeObserver> observersToNotify;
    std::uint32_t versionToNotify { 0U };
    bool changed { false };

    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        for (const auto& uri : addedScopes) {
            if (!uri.empty() && (std::find(m_scopes.begin(), m_scopes.end(), uri) == m_scopes.end())) {
                m_scopes.push_back(uri);
                changed = true;
            }
        }
        if (changed) {
            ++m_metadataVersion;
            versionToNotify = m_metadataVersion;
            observersToNotify = m_observers;
        }
    }

    if (changed) {
        notifyObservers(observersToNotify, versionToNotify);
    }
}

void ScopeStore::removeScopes(const std::vector<std::string>& removedScopes)
{
    if (removedScopes.empty()) {
        return;
    }

    std::vector<ScopeObserver> observersToNotify;
    std::uint32_t versionToNotify { 0U };
    bool changed { false };

    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        for (const auto& uri : removedScopes) {
            const auto it = std::find(m_scopes.begin(), m_scopes.end(), uri);
            if (it != m_scopes.end()) {
                m_scopes.erase(it);
                changed = true;
            }
        }
        if (changed) {
            ++m_metadataVersion;
            versionToNotify = m_metadataVersion;
            observersToNotify = m_observers;
        }
    }

    if (changed) {
        notifyObservers(observersToNotify, versionToNotify);
    }
}

std::uint32_t ScopeStore::getMetadataVersion() const noexcept
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_metadataVersion;
}

void ScopeStore::registerObserver(ScopeObserver observer)
{
    if (!observer) {
        return;
    }
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_observers.push_back(std::move(observer));
}

void ScopeStore::clearObservers()
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_observers.clear();
}

void ScopeStore::notifyObservers(const std::vector<ScopeObserver>& observers, std::uint32_t version)
{
    for (const auto& obs : observers) {
        if (obs) {
            obs(version);
        }
    }
}

} // namespace Onvif
