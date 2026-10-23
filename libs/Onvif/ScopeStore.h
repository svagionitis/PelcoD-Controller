#pragma once

/// @file ScopeStore.h
/// @brief Canonical, thread-safe storage and observer synchronization for ONVIF device scopes.

#include <cstddef>
#include <cstdint>

#include <functional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Onvif {

/// @class ScopeStore
/// @brief Thread-safe single source of truth for ONVIF device scopes and WS-Discovery synchronization.
/// @details Implements a Reader-Writer synchronization model using std::shared_mutex. Allows concurrent
///          reads by WS-Discovery probe matchers and SOAP GetScopes queries, while serializing updates.
///          Notifies registered observers outside the lock upon scope mutations to prevent deadlocks.
class ScopeStore {
public:
    /// @brief Observer callback invoked when scopes change and MetadataVersion increments.
    /// @param[in] newMetadataVersion Updated 32-bit WS-Discovery metadata version counter.
    using ScopeObserver = std::function<void(std::uint32_t newMetadataVersion)>;

    /// @brief Default constructor initializing empty configurable scopes and MetadataVersion 1.
    ScopeStore() = default;

    /// @brief Parameterized constructor initializing default scopes and optional initial version.
    /// @param[in] initialScopes Vector of initial configurable scope URIs.
    /// @param[in] initialVersion Initial metadata version (defaults to 1).
    explicit ScopeStore(const std::vector<std::string>& initialScopes, std::uint32_t initialVersion = 1U);

    ~ScopeStore() = default;

    ScopeStore(const ScopeStore&) = delete;
    ScopeStore& operator=(const ScopeStore&) = delete;
    ScopeStore(ScopeStore&&) = delete;
    ScopeStore& operator=(ScopeStore&&) = delete;

    /// @brief Retrieves the current configurable scopes snapshot under a shared read lock.
    /// @return Snapshot vector of configurable scope URIs.
    [[nodiscard]] std::vector<std::string> getScopes() const;

    /// @brief Retrieves all active scopes combining ONVIF fixed scopes and configurable scopes.
    /// @param[in] deviceName Advertised human-readable device name.
    /// @param[in] model Advertised hardware/model string.
    /// @return Complete vector of active scope URIs.
    [[nodiscard]] std::vector<std::string> getActiveScopes(
        const std::string& deviceName, const std::string& model) const;

    /// @brief Checks whether a specific scope URI exists in the store.
    /// @param[in] scopeUri Target URI to inspect.
    /// @return True if scope is present, false otherwise.
    [[nodiscard]] bool hasScope(std::string_view scopeUri) const;

    /// @brief Returns the total number of configurable scopes.
    /// @return Count of configured scopes.
    [[nodiscard]] std::size_t size() const;

    /// @brief Checks whether there are no configurable scopes.
    /// @return True if configurable scopes are empty, false otherwise.
    [[nodiscard]] bool empty() const;

    /// @brief Overwrites all configurable scopes, increments version, and notifies observers.
    /// @param[in] newScopes Replacement vector of configurable scope URIs.
    void setScopes(const std::vector<std::string>& newScopes);

    /// @brief Adds new scopes with duplicate prevention, increments version, and notifies observers.
    /// @param[in] addedScopes Vector of scope URIs to append.
    void addScopes(const std::vector<std::string>& addedScopes);

    /// @brief Removes matching scopes by URI, increments version, and notifies observers.
    /// @param[in] removedScopes Vector of scope URIs to delete.
    void removeScopes(const std::vector<std::string>& removedScopes);

    /// @brief Returns the current metadata version.
    /// @return Incremental 32-bit metadata version counter.
    [[nodiscard]] std::uint32_t getMetadataVersion() const noexcept;

    /// @brief Registers an observer callback for scope mutations.
    /// @param[in] observer Callback functor to register.
    void registerObserver(ScopeObserver observer);

    /// @brief Removes all registered observers.
    void clearObservers();

private:
    static void notifyObservers(const std::vector<ScopeObserver>& observers, std::uint32_t version);

    mutable std::shared_mutex m_mutex { };
    std::vector<std::string> m_scopes { };
    std::uint32_t m_metadataVersion { 1U };
    std::vector<ScopeObserver> m_observers { };
};

} // namespace Onvif

namespace PelcoD::Onvif {
using ScopeStore = ::Onvif::ScopeStore;
} // namespace PelcoD::Onvif
