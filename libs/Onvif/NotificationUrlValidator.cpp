/// @file NotificationUrlValidator.cpp
/// @brief Implementation of SSRF defense and URL validation for ONVIF event consumers.

#include "NotificationUrlValidator.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>

namespace Onvif {

namespace {

    /// @brief Converts an ASCII string view to lower case.
    /// @param[in] in Input string view.
    /// @return Lowercased string.
    [[nodiscard]] std::string toLower(std::string_view in)
    {
        std::string out {};
        out.reserve(in.size());
        for (const char ch : in) {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
        return out;
    }

    /// @brief Checks if a string view equals another ignoring ASCII case.
    /// @param[in] a First string.
    /// @param[in] b Second string.
    /// @return True if equal.
    [[nodiscard]] bool iequals(std::string_view a, std::string_view b)
    {
        if (a.size() != b.size()) {
            return false;
        }
        for (std::size_t i { 0U }; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
                return false;
            }
        }
        return true;
    }

} // namespace

bool NotificationUrlValidator::isLoopbackHost(std::string_view host)
{
    const std::string lower { toLower(host) };
    if (lower == "localhost" || lower == "127.0.0.1" || lower == "::1" || lower == "[::1]") {
        return true;
    }

    in_addr addr4 {};
    if (::inet_pton(AF_INET, lower.c_str(), &addr4) == 1) {
        const std::uint32_t ip { ntohl(addr4.s_addr) };
        // 127.0.0.0/8
        if ((ip & 0xFF000000U) == 0x7F000000U) {
            return true;
        }
    }

    std::string v6Host { lower };
    if (!v6Host.empty() && v6Host.front() == '[' && v6Host.back() == ']') {
        v6Host = v6Host.substr(1U, v6Host.size() - 2U);
    }
    in6_addr addr6 {};
    if (::inet_pton(AF_INET6, v6Host.c_str(), &addr6) == 1) {
        const auto* bytes { reinterpret_cast<const std::uint8_t*>(&addr6) };
        bool isV6Loopback { true };
        for (std::size_t i { 0U }; i < 15U; ++i) {
            if (bytes[i] != 0U) {
                isV6Loopback = false;
                break;
            }
        }
        if (isV6Loopback && bytes[15] == 1U) {
            return true;
        }
    }

    return false;
}

bool NotificationUrlValidator::isMetadataHost(std::string_view host)
{
    const std::string lower { toLower(host) };
    if (lower == "169.254.169.254" || lower == "instance-data" || lower == "metadata.google.internal") {
        return true;
    }

    in_addr addr4 {};
    if (::inet_pton(AF_INET, lower.c_str(), &addr4) == 1) {
        const std::uint32_t ip { ntohl(addr4.s_addr) };
        // 169.254.0.0/16 Link-local and cloud metadata
        if ((ip & 0xFFFF0000U) == 0xA9FE0000U) {
            return true;
        }
    }

    std::string v6Host { lower };
    if (!v6Host.empty() && v6Host.front() == '[' && v6Host.back() == ']') {
        v6Host = v6Host.substr(1U, v6Host.size() - 2U);
    }
    in6_addr addr6 {};
    if (::inet_pton(AF_INET6, v6Host.c_str(), &addr6) == 1) {
        const auto* bytes { reinterpret_cast<const std::uint8_t*>(&addr6) };
        // fe80::/10 link-local
        if (bytes[0] == 0xFEU && (bytes[1] & 0xC0U) == 0x80U) {
            return true;
        }
    }

    return false;
}

bool NotificationUrlValidator::isSensitivePort(std::uint16_t port)
{
    // Well-known dangerous service ports for SSRF pivot attacks
    constexpr std::array<std::uint16_t, 25> kBlockedPorts { 0U, 21U, 22U, 23U, 25U, 53U, 67U, 68U, 69U, 110U, 123U,
        135U, 137U, 138U, 139U, 143U, 161U, 389U, 445U, 636U, 1433U, 1521U, 3306U, 3389U, 6379U };
    return std::find(kBlockedPorts.begin(), kBlockedPorts.end(), port) != kBlockedPorts.end();
}

bool NotificationUrlValidator::isPrivateSubnet(std::string_view host)
{
    const std::string lower { toLower(host) };
    in_addr addr4 {};
    if (::inet_pton(AF_INET, lower.c_str(), &addr4) == 1) {
        const std::uint32_t ip { ntohl(addr4.s_addr) };
        // 10.0.0.0/8
        if ((ip & 0xFF000000U) == 0x0A000000U) {
            return true;
        }
        // 172.16.0.0/12
        if ((ip & 0xFFF00000U) == 0xAC100000U) {
            return true;
        }
        // 192.168.0.0/16
        if ((ip & 0xFFFF0000U) == 0xC0A80000U) {
            return true;
        }
    }

    std::string v6Host { lower };
    if (!v6Host.empty() && v6Host.front() == '[' && v6Host.back() == ']') {
        v6Host = v6Host.substr(1U, v6Host.size() - 2U);
    }
    in6_addr addr6 {};
    if (::inet_pton(AF_INET6, v6Host.c_str(), &addr6) == 1) {
        const auto* bytes { reinterpret_cast<const std::uint8_t*>(&addr6) };
        // fc00::/7 (Unique Local Address)
        if ((bytes[0] & 0xFEU) == 0xFCU) {
            return true;
        }
    }

    return false;
}

UrlCheckResult NotificationUrlValidator::validateUrl(
    std::string_view url, const NotificationConfig& config, bool serverLoopback)
{
    UrlCheckResult result {};
    if (url.empty()) {
        result.status = UrlCheckStatus::MalformedUrl;
        result.reason = "Empty URL";
        return result;
    }

    // Scheme extraction
    const auto schemeDelim { url.find("://") };
    if (schemeDelim == std::string_view::npos) {
        result.status = UrlCheckStatus::InvalidScheme;
        result.reason = "Missing scheme delimiter";
        return result;
    }

    const std::string scheme { toLower(url.substr(0U, schemeDelim)) };
    result.scheme = scheme;
    if (scheme != "http" && scheme != "https") {
        result.status = UrlCheckStatus::InvalidScheme;
        result.reason = "Unsupported URL scheme: only http and https permitted";
        return result;
    }

    const std::string_view afterScheme { url.substr(schemeDelim + 3U) };
    if (afterScheme.empty()) {
        result.status = UrlCheckStatus::MalformedUrl;
        result.reason = "Empty authority";
        return result;
    }

    // Split authority and path
    const auto slashPos { afterScheme.find('/') };
    const std::string_view authority { (slashPos == std::string_view::npos) ? afterScheme
                                                                            : afterScheme.substr(0U, slashPos) };
    result.path = (slashPos == std::string_view::npos) ? "/" : std::string { afterScheme.substr(slashPos) };

    // Disallow credentials in userinfo
    if (authority.find('@') != std::string_view::npos) {
        result.status = UrlCheckStatus::UserinfoDisallowed;
        result.reason = "Embedded credentials in consumer URL are forbidden";
        return result;
    }

    // Host and Port extraction
    std::string hostPart {};
    std::uint16_t parsedPort { scheme == "https" ? static_cast<std::uint16_t>(443U) : static_cast<std::uint16_t>(80U) };

    if (!authority.empty() && authority.front() == '[') {
        // IPv6 bracketed literal: [::1]:8080
        const auto closeBracket { authority.find(']') };
        if (closeBracket == std::string_view::npos) {
            result.status = UrlCheckStatus::MalformedUrl;
            result.reason = "Unclosed IPv6 bracket literal";
            return result;
        }
        hostPart = std::string { authority.substr(1U, closeBracket - 1U) };
        const std::string_view afterBracket { authority.substr(closeBracket + 1U) };
        if (!afterBracket.empty()) {
            if (afterBracket.front() != ':') {
                result.status = UrlCheckStatus::MalformedUrl;
                result.reason = "Malformed character after IPv6 bracket";
                return result;
            }
            int p { 0 };
            const auto res { std::from_chars(afterBracket.data() + 1U, afterBracket.data() + afterBracket.size(), p) };
            if (res.ec != std::errc {} || p <= 0 || p > 65535) {
                result.status = UrlCheckStatus::MalformedUrl;
                result.reason = "Invalid IPv6 port number";
                return result;
            }
            parsedPort = static_cast<std::uint16_t>(p);
        }
    } else {
        const auto colonPos { authority.rfind(':') };
        if (colonPos != std::string_view::npos) {
            hostPart = std::string { authority.substr(0U, colonPos) };
            int p { 0 };
            const std::string_view portStr { authority.substr(colonPos + 1U) };
            const auto res { std::from_chars(portStr.data(), portStr.data() + portStr.size(), p) };
            if (res.ec != std::errc {} || p <= 0 || p > 65535) {
                result.status = UrlCheckStatus::MalformedUrl;
                result.reason = "Invalid port number";
                return result;
            }
            parsedPort = static_cast<std::uint16_t>(p);
        } else {
            hostPart = std::string { authority };
        }
    }

    result.host = hostPart;
    result.port = parsedPort;

    if (hostPart.empty()) {
        result.status = UrlCheckStatus::InvalidHost;
        result.reason = "Empty host";
        return result;
    }

    // Reject 0.0.0.0 and broadcast
    if (hostPart == "0.0.0.0" || hostPart == "255.255.255.255") {
        result.status = UrlCheckStatus::InvalidHost;
        result.reason = "Unspecified or broadcast address not allowed";
        return result;
    }

    // Cloud metadata check (always blocked)
    if (isMetadataHost(hostPart)) {
        result.status = UrlCheckStatus::MetadataBlocked;
        result.reason = "Access to link-local and cloud metadata addresses is strictly forbidden";
        return result;
    }

    // Sensitive service ports check
    if (isSensitivePort(parsedPort)) {
        result.status = UrlCheckStatus::SensitivePortBlocked;
        result.reason = "Destination port is reserved for sensitive internal services";
        return result;
    }

    // Loopback check
    const bool allowLoopbackEffective { config.allowLoopback.value_or(serverLoopback) };
    if (isLoopbackHost(hostPart) && !allowLoopbackEffective) {
        result.status = UrlCheckStatus::LoopbackBlocked;
        result.reason = "Access to loopback interface is blocked by policy";
        return result;
    }

    // Private subnets check
    if (!config.allowPrivateSubnets && isPrivateSubnet(hostPart)) {
        result.status = UrlCheckStatus::PrivateBlocked;
        result.reason = "Access to private local network is blocked by policy";
        return result;
    }

    // Blocklist check
    for (const auto& blocked : config.blockedHosts) {
        if (iequals(hostPart, blocked)) {
            result.status = UrlCheckStatus::HostBlocked;
            result.reason = "Host matches administrator blacklist";
            return result;
        }
    }

    // Whitelist check (if configured)
    if (!config.allowedHosts.empty()) {
        bool allowed { false };
        for (const auto& whitelisted : config.allowedHosts) {
            if (iequals(hostPart, whitelisted)) {
                allowed = true;
                break;
            }
        }
        if (!allowed) {
            result.status = UrlCheckStatus::HostNotAllowed;
            result.reason = "Host is not in administrator whitelist";
            return result;
        }
    }

    result.status = UrlCheckStatus::Valid;
    result.reason = "URL successfully verified";
    return result;
}

} // namespace Onvif
