#pragma once

/// @file OnvifAuthTypes.h
/// @brief Shared vocabulary types for ONVIF server-side authentication and authorization.
/// @details Defines the ONVIF Core access classes, authentication outcomes, the authenticated
///          principal and the authentication configuration embedded in OnvifServerConfig.

#include "OnvifTypes.h"

#include <pugixml.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace Onvif {

/// @enum AccessClass
/// @brief ONVIF Core Specification §5.9.4 service access classes.
/// @details Every SOAP operation is mapped to exactly one class; unknown operations are
///          classified as Unrecoverable so that only administrators may invoke them (fail closed).
enum class AccessClass : std::uint8_t {
    PreAuth, ///< May be invoked without authentication (e.g. GetSystemDateAndTime).
    ReadSystem, ///< Reads non-sensitive system configuration.
    ReadSystemSensitive, ///< Reads sensitive system configuration (e.g. user list).
    ReadSystemSecret, ///< Reads secret system data (backups, logs).
    WriteSystem, ///< Modifies system configuration (network, users, certificates).
    Unrecoverable, ///< Operations that cannot be undone (reboot, factory default).
    ReadMedia, ///< Reads media configuration or streams.
    Actuate ///< Physically or logically actuates the device (PTZ, imaging, relays, NUC).
};

/// @enum AuthOutcome
/// @brief Result category of an authentication attempt.
enum class AuthOutcome : std::uint8_t {
    Success, ///< Credentials verified.
    NoCredentials, ///< No (supported) credentials were presented.
    InvalidCredentials, ///< Credentials malformed, unknown user or wrong password.
    Replay, ///< Nonce (or Digest nonce/nc/cnonce tuple) was already used.
    Stale, ///< Timestamp or Digest nonce outside the accepted window.
    Unsupported ///< A credential form that is disabled by policy (e.g. PasswordText).
};

/// @struct Principal
/// @brief Identity of an authenticated caller.
struct Principal {
    std::string username {}; ///< Authenticated user name (empty if anonymous).
    OnvifUserLevel level { OnvifUserLevel::Anonymous }; ///< Privilege level of the user.
};

/// @struct AuthResult
/// @brief Outcome of an authentication attempt together with the resolved principal.
struct AuthResult {
    AuthOutcome outcome { AuthOutcome::NoCredentials }; ///< Result category.
    Principal principal {}; ///< Valid only when outcome == AuthOutcome::Success.
};

/// @struct AuthInput
/// @brief Transport-independent view of the credential-bearing parts of a request.
/// @note All views must outlive the authenticate() call that consumes them.
struct AuthInput {
    std::string_view method {}; ///< HTTP method (e.g. "POST").
    std::string_view uri {}; ///< HTTP request-target used for Digest uri binding.
    std::string_view authorization {}; ///< Raw HTTP Authorization header value (may be empty).
    pugi::xml_node securityHeader {}; ///< wsse:Security element from the SOAP Header (may be null).
    std::string_view remoteAddr {}; ///< Peer address, used for logging and back-off only.
};

/// @struct OnvifAuthConfig
/// @brief Authentication policy for the embedded ONVIF server.
struct OnvifAuthConfig {
    /// @brief Master switch. When false the server only starts on a loopback bind address.
    bool enabled { true };

    /// @brief Accept WS-Security UsernameToken (PasswordDigest) credentials.
    bool allowUsernameToken { true };

    /// @brief Accept RFC 7616 HTTP Digest credentials.
    bool allowHttpDigest { true };

    /// @brief Accept UsernameToken PasswordText (plaintext). Disabled by default (CWE-319).
    bool allowPasswordText { false };

    /// @brief Permit a non-loopback start while a factory-default password is still configured.
    bool allowDefaultPassword { false };

    /// @brief Maximum accepted difference between wsu:Created and the server clock.
    std::chrono::seconds maxClockSkew { 300 };

    /// @brief Lifetime of a server-issued HTTP Digest nonce.
    std::chrono::seconds digestNonceTtl { 300 };

    /// @brief Maximum number of remembered nonces for replay detection.
    std::size_t nonceCacheSize { 4096U };

    /// @brief HTTP Digest protection realm.
    std::string realm { "ONVIF" };
};

} // namespace Onvif
