#pragma once

/// @file Authenticator.h
/// @brief Facade combining UsernameToken and HTTP Digest validation with failure back-off.

#include "CredentialStore.h"
#include "HttpDigest.h"
#include "NonceCache.h"
#include "OnvifAuthTypes.h"
#include "UsernameToken.h"

#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Onvif {

/// @class Authenticator
/// @brief Routes a request's credentials to the enabled mechanism and tracks failed attempts.
/// @details Precedence: a WS-Security UsernameToken in the SOAP header is evaluated first (if
///          enabled); otherwise an HTTP Digest Authorization header is evaluated (if enabled).
///          Both mechanisms share one NonceCache sized by OnvifAuthConfig::nonceCacheSize with a
///          lifetime of max(2 x maxClockSkew, digestNonceTtl).
///
///          Failed attempts are counted per peer address in a 60 s window. Up to five failures
///          carry no penalty; afterwards the penalty doubles from 100 ms up to 2000 ms. The table
///          is bounded to 1024 peers (oldest windows are evicted first).
/// @note authenticate() and challenges() are thread-safe; recordFailure()/penalty() are
///       internally synchronised.
class Authenticator {
public:
    /// @brief Constructs the facade.
    /// @param[in] store Authoritative user store (must not be null).
    /// @param[in] config Authentication policy.
    Authenticator(std::shared_ptr<CredentialStore> store, OnvifAuthConfig config);

    /// @brief Authenticates a request.
    /// @param[in] input Credential-bearing request fields.
    /// @return Outcome and principal; NoCredentials if no enabled mechanism found credentials.
    [[nodiscard]] AuthResult authenticate(const AuthInput& input) const;

    /// @brief Builds WWW-Authenticate values for a 401 response.
    /// @param[in] stale True to signal an expired Digest nonce.
    /// @return Two Digest challenges (SHA-256, MD5).
    [[nodiscard]] std::vector<std::string> challenges(bool stale) const;

    /// @brief Records a failed authentication attempt from a peer.
    /// @param[in] remoteAddr Peer address.
    /// @param[in] now Current monotonic time.
    void recordFailure(std::string_view remoteAddr, std::chrono::steady_clock::time_point now);

    /// @brief Returns the delay to impose on a peer before answering a failed attempt.
    /// @param[in] remoteAddr Peer address.
    /// @param[in] now Current monotonic time.
    /// @return 0 ms for five or fewer recent failures, otherwise 100..2000 ms.
    [[nodiscard]] std::chrono::milliseconds penalty(
        std::string_view remoteAddr, std::chrono::steady_clock::time_point now) const;

private:
    struct FailureWindow {
        std::size_t count { 0U };
        std::chrono::steady_clock::time_point start {};
    };

    void evictOldest(std::chrono::steady_clock::time_point now);

    OnvifAuthConfig m_config;
    std::shared_ptr<CredentialStore> m_store;
    std::shared_ptr<NonceCache> m_cache;
    UsernameTokenValidator m_usernameToken;
    DigestValidator m_digest;

    mutable std::mutex m_failMutex {};
    std::unordered_map<std::string, FailureWindow> m_failures {};
};

} // namespace Onvif
