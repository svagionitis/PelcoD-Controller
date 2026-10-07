#pragma once

/// @file NotificationUrlValidator.h
/// @brief URL parsing and Server-Side Request Forgery (SSRF) validation for ONVIF event consumers.
/// @details Validates wsnt:ConsumerReference URIs against unauthorized local or intranet targets,
///          addressing review finding C5 (CWE-918, CWE-400).

#include "OnvifServerTypes.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace Onvif {

/// @enum UrlCheckStatus
/// @brief Status codes returned from ConsumerReference URL validation.
enum class UrlCheckStatus : std::uint8_t {
    Valid = 0,
    InvalidScheme,
    UserinfoDisallowed,
    InvalidHost,
    LoopbackBlocked,
    LinkLocalBlocked,
    MetadataBlocked,
    MulticastBlocked,
    PrivateBlocked,
    SensitivePortBlocked,
    HostNotAllowed,
    HostBlocked,
    MalformedUrl
};

/// @struct UrlCheckResult
/// @brief Detailed result of URL validation including parsed components and error rationale.
struct UrlCheckResult {
    UrlCheckStatus status { UrlCheckStatus::MalformedUrl };
    std::string reason {};
    std::string scheme {};
    std::string host {};
    std::uint16_t port { 0U };
    std::string path {};

    /// @brief Checks whether the URL successfully passed all security validations.
    /// @return True if status is Valid.
    [[nodiscard]] bool isValid() const noexcept
    {
        return status == UrlCheckStatus::Valid;
    }
};

/// @class NotificationUrlValidator
/// @brief Security validator protecting push notification endpoints from SSRF exploits.
/// @details Enforces strict scheme checking, prevents loopback/cloud-metadata probing, blocks
///          sensitive service ports, and honors administrator whitelists and blacklists.
class NotificationUrlValidator {
public:
    /// @brief Validates a notification ConsumerReference URL according to policy.
    /// @param[in] url Candidate URL string.
    /// @param[in] config Active notification security settings.
    /// @param[in] serverLoopback True if the hosting ONVIF server is bound to loopback.
    /// @return Populated UrlCheckResult structure.
    [[nodiscard]] static UrlCheckResult validateUrl(
        std::string_view url, const NotificationConfig& config, bool serverLoopback = false);

    /// @brief Tests if a hostname or IP represents a loopback address.
    /// @param[in] host Hostname or IP string.
    /// @return True if loopback (e.g. 127.0.0.1, localhost, ::1).
    [[nodiscard]] static bool isLoopbackHost(std::string_view host);

    /// @brief Tests if a hostname or IP represents cloud metadata or link-local address.
    /// @param[in] host Hostname or IP string.
    /// @return True if link-local or metadata (e.g. 169.254.169.254).
    [[nodiscard]] static bool isMetadataHost(std::string_view host);

    /// @brief Tests if a port is considered a dangerous or sensitive service port.
    /// @param[in] port Destination port number.
    /// @return True if port is reserved for sensitive internal services (SSH, SMTP, etc.).
    [[nodiscard]] static bool isSensitivePort(std::uint16_t port);

    /// @brief Tests if an IP address belongs to RFC 1918 / RFC 4193 private ranges.
    /// @param[in] host Hostname or IP string.
    /// @return True if private LAN address.
    [[nodiscard]] static bool isPrivateSubnet(std::string_view host);
};

} // namespace Onvif
