/// @file Authenticator.cpp
/// @brief Implementation of the authentication facade and failure back-off.

#include "Authenticator.h"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace Onvif {

namespace {

    constexpr std::size_t kFreeFailures { 5U };
    constexpr std::size_t kMaxPeers { 1024U };
    constexpr std::chrono::seconds kWindow { 60 };
    constexpr std::chrono::milliseconds kBasePenalty { 100 };
    constexpr std::chrono::milliseconds kMaxPenalty { 2000 };

    [[nodiscard]] std::chrono::seconds cacheLifetime(const OnvifAuthConfig& cfg) noexcept
    {
        return std::max(cfg.maxClockSkew * 2, cfg.digestNonceTtl);
    }

} // namespace

Authenticator::Authenticator(std::shared_ptr<CredentialStore> store, OnvifAuthConfig config)
    : m_config { std::move(config) }
    , m_store { std::move(store) }
    , m_cache { std::make_shared<NonceCache>(m_config.nonceCacheSize, cacheLifetime(m_config)) }
    , m_usernameToken { m_store, m_cache, m_config }
    , m_digest { m_store, m_cache, m_config }
{
}

AuthResult Authenticator::authenticate(const AuthInput& input) const
{
    const auto now { std::chrono::system_clock::now() };
    if (m_config.allowUsernameToken && input.securityHeader) {
        const AuthResult result { m_usernameToken.validate(input.securityHeader, now) };
        if (result.outcome != AuthOutcome::NoCredentials) {
            return result;
        }
    }
    if (m_config.allowHttpDigest && !input.authorization.empty()) {
        return m_digest.validate(input.authorization, input.method, input.uri, now);
    }
    return AuthResult { AuthOutcome::NoCredentials, Principal {} };
}

std::vector<std::string> Authenticator::challenges(bool stale) const
{
    return m_digest.makeChallenges(stale, std::chrono::system_clock::now());
}

void Authenticator::evictOldest(std::chrono::steady_clock::time_point now)
{
    for (auto it { m_failures.begin() }; it != m_failures.end();) {
        if ((now - it->second.start) > kWindow) {
            it = m_failures.erase(it);
        } else {
            ++it;
        }
    }
    if (m_failures.size() >= kMaxPeers) {
        const auto oldest { std::min_element(m_failures.begin(), m_failures.end(),
            [](const auto& a, const auto& b) { return a.second.start < b.second.start; }) };
        if (oldest != m_failures.end()) {
            static_cast<void>(m_failures.erase(oldest));
        }
    }
}

void Authenticator::recordFailure(std::string_view remoteAddr, std::chrono::steady_clock::time_point now)
{
    const std::scoped_lock lock { m_failMutex };
    const std::string key { remoteAddr };
    auto it { m_failures.find(key) };
    if (it == m_failures.end()) {
        if (m_failures.size() >= kMaxPeers) {
            evictOldest(now);
        }
        it = m_failures.emplace(key, FailureWindow { 0U, now }).first;
    } else if ((now - it->second.start) > kWindow) {
        it->second = FailureWindow { 0U, now };
    } else {
        // Within the current window: keep counting.
    }
    ++it->second.count;
}

std::chrono::milliseconds Authenticator::penalty(
    std::string_view remoteAddr, std::chrono::steady_clock::time_point now) const
{
    const std::scoped_lock lock { m_failMutex };
    const auto it { m_failures.find(std::string { remoteAddr }) };
    if ((it == m_failures.end()) || ((now - it->second.start) > kWindow) || (it->second.count <= kFreeFailures)) {
        return std::chrono::milliseconds { 0 };
    }
    const std::size_t excess { std::min<std::size_t>(it->second.count - kFreeFailures - 1U, 5U) };
    const std::chrono::milliseconds scaled { kBasePenalty * (std::int64_t { 1 } << excess) };
    return std::min(scaled, kMaxPenalty);
}

} // namespace Onvif
