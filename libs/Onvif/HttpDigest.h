#pragma once

/// @file HttpDigest.h
/// @brief Server-side RFC 7616 HTTP Digest authentication (SHA-256 and MD5, qop=auth).

#include "CredentialStore.h"
#include "NonceCache.h"
#include "OnvifAuthTypes.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Onvif {

/// @class DigestValidator
/// @brief Issues Digest challenges and verifies Digest Authorization headers.
/// @details Nonces are stateless: `Base64(ts ‖ HMAC-SHA256(key, ts ‖ realm))` where @c ts is the
///          issue time in big-endian seconds and @c key is 32 random bytes generated per instance.
///          A nonce older than digestNonceTtl yields AuthOutcome::Stale so that clients can retry
///          transparently (RFC 7616 §3.3 `stale=true`). Replay is prevented by remembering each
///          `nonce ‖ nc ‖ cnonce` tuple in the shared NonceCache. The request URI is bound to the
///          Digest `uri` parameter.
/// @note ONVIF Core §5.9.2 requires HTTP Digest support; SHA-256 is offered first and MD5 only for
///       legacy client interoperability.
/// @note Thread-safe after construction.
class DigestValidator {
public:
    /// @brief Constructs a validator with a fresh random nonce key.
    /// @param[in] store Authoritative user store (must not be null).
    /// @param[in] cache Replay cache (must not be null).
    /// @param[in] config Authentication policy (realm, nonce lifetime).
    DigestValidator(std::shared_ptr<CredentialStore> store, std::shared_ptr<NonceCache> cache, OnvifAuthConfig config);

    /// @brief Builds the WWW-Authenticate header values for a 401 response.
    /// @param[in] stale True to signal that the previous nonce expired (`stale=true`).
    /// @param[in] now Current wall-clock time embedded in the nonce.
    /// @return Exactly two values: the SHA-256 challenge followed by the MD5 challenge.
    [[nodiscard]] std::vector<std::string> makeChallenges(bool stale, std::chrono::system_clock::time_point now) const;

    /// @brief Verifies an Authorization header.
    /// @param[in] header Raw Authorization header value.
    /// @param[in] method HTTP request method.
    /// @param[in] uri HTTP request-target; must equal the Digest `uri` parameter.
    /// @param[in] now Current wall-clock time for the nonce age check.
    /// @return Authentication result.
    /// @retval AuthOutcome::NoCredentials Header empty or not a Digest scheme (e.g. Basic).
    /// @retval AuthOutcome::InvalidCredentials Malformed header, forged nonce, URI mismatch, unknown
    ///         user or wrong response.
    /// @retval AuthOutcome::Stale Genuine nonce older than digestNonceTtl.
    /// @retval AuthOutcome::Replay `nonce ‖ nc ‖ cnonce` already accepted.
    [[nodiscard]] AuthResult validate(std::string_view header, std::string_view method, std::string_view uri,
        std::chrono::system_clock::time_point now) const;

private:
    enum class NonceState : std::uint8_t { Valid, Forged, Expired };

    [[nodiscard]] std::string makeNonce(std::chrono::system_clock::time_point now) const;
    [[nodiscard]] NonceState checkNonce(std::string_view nonce, std::chrono::system_clock::time_point now) const;
    [[nodiscard]] std::vector<std::uint8_t> nonceMac(const std::vector<std::uint8_t>& stamp) const;

    std::shared_ptr<CredentialStore> m_store;
    std::shared_ptr<NonceCache> m_cache;
    OnvifAuthConfig m_config;
    std::vector<std::uint8_t> m_key {};
};

} // namespace Onvif
