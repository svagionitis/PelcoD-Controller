#pragma once

/// @file UsernameToken.h
/// @brief Server-side validation of WS-Security UsernameToken Profile 1.0 credentials.

#include "CredentialStore.h"
#include "NonceCache.h"
#include "OnvifAuthTypes.h"

#include <pugixml.hpp>

#include <chrono>
#include <memory>

namespace Onvif {

/// @class UsernameTokenValidator
/// @brief Verifies `wsse:UsernameToken` elements against a CredentialStore.
/// @details Implements the server half of OASIS UsernameToken Profile 1.0 as profiled by
///          ONVIF Core §5.12.2:
///          - PasswordDigest = Base64(SHA-1(nonce ‖ created ‖ password)), compared in constant time;
///          - `wsu:Created` must lie within ±maxClockSkew of the server clock;
///          - each nonce is accepted only once (NonceCache);
///          - PasswordText is rejected unless explicitly enabled.
///          Unknown users incur the same digest computation as known users to avoid a timing oracle.
/// @note validate() is thread-safe provided the injected store and cache are (they are).
/// @note When PasswordText is enabled a Nonce and Created are still required, so replay and
///       freshness checks apply to both password types.
class UsernameTokenValidator {
public:
    /// @brief Constructs a validator.
    /// @param[in] store Authoritative user store (must not be null).
    /// @param[in] cache Replay cache shared with other validators (must not be null).
    /// @param[in] config Authentication policy (skew window, PasswordText switch).
    UsernameTokenValidator(
        std::shared_ptr<CredentialStore> store, std::shared_ptr<NonceCache> cache, OnvifAuthConfig config);

    /// @brief Validates a `wsse:Security` element.
    /// @param[in] security The Security header element, or a null node if absent.
    /// @param[in] now Current wall-clock time used for the freshness check.
    /// @return Authentication result.
    /// @retval AuthOutcome::NoCredentials No Security element or no UsernameToken in it.
    /// @retval AuthOutcome::Unsupported PasswordText or unknown password type.
    /// @retval AuthOutcome::InvalidCredentials Malformed token, unknown user or digest mismatch.
    /// @retval AuthOutcome::Stale `wsu:Created` outside the accepted clock-skew window.
    /// @retval AuthOutcome::Replay The nonce was already accepted within its lifetime.
    [[nodiscard]] AuthResult validate(pugi::xml_node security, std::chrono::system_clock::time_point now) const;

private:
    std::shared_ptr<CredentialStore> m_store;
    std::shared_ptr<NonceCache> m_cache;
    OnvifAuthConfig m_config;
};

/// @brief Locates the `wsse:Security` header block of a SOAP envelope.
/// @details Searches only direct children of `Envelope/Header` (namespace-prefix agnostic), so a
///          token smuggled into the Body is ignored.
/// @param[in] doc Parsed SOAP document.
/// @return The Security element, or a null node if absent.
[[nodiscard]] pugi::xml_node findSecurityHeader(const pugi::xml_document& doc);

} // namespace Onvif
