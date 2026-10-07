#pragma once

/// @file WsDiscoveryValidator.h
/// @brief Strict validator and rate limiter for WS-Discovery datagrams (review finding C7).

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace pugi {
class xml_document;
}

namespace Onvif {

/// @struct DiscoveryValidationResult
/// @brief Result of WS-Discovery incoming datagram validation.
struct DiscoveryValidationResult {
    /// @brief True if the message is a strictly valid Probe targeting this device.
    bool isValidProbe { false };

    /// @brief True if the message originated from this device itself (self-loop).
    bool isSelfMessage { false };

    /// @brief True if the message appears to be a reflection amplification attempt.
    bool isReflectionAttempt { false };

    /// @brief RelatesTo MessageID to echo back in response header, if available.
    std::string messageId {};
};

/// @class WsDiscoveryValidator
/// @brief Strict parser and security validator for WS-Discovery datagrams.
/// @details Remediates review finding C7 (self-amplification, DoS loop, and reflection attacks).
class WsDiscoveryValidator {
public:
    /// @brief Standard WS-Discovery 2005/04 Probe action URI.
    static constexpr const char* kProbeActionUri { "http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe" };

    /// @brief Standard WS-Discovery 2005/04 ProbeMatches action URI.
    static constexpr const char* kProbeMatchesActionUri {
        "http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatches"
    };

    /// @brief Validates incoming XML and address against WS-Discovery §5 rules.
    /// @param[in] xmlDoc Parsed XML document of the incoming message.
    /// @param[in] senderPort Port number of the sender (host byte order).
    /// @param[in] localServiceUuid Server's configured service UUID.
    /// @param[in] dropReflectionPort3702 If true, rejects messages from source port 3702.
    /// @return Validation result structure indicating validity and drop reasons.
    [[nodiscard]] static DiscoveryValidationResult validateProbe(const pugi::xml_document& xmlDoc,
        std::uint16_t senderPort, const std::string& localServiceUuid, bool dropReflectionPort3702 = true) noexcept;

    /// @brief Checks if Action header strictly equals WS-Discovery Probe URI.
    /// @param[in] actionUri Trimmed Action URI string.
    /// @return True only if exact match for WS-Discovery 2005/04 Probe.
    [[nodiscard]] static bool isExactProbeAction(const std::string& actionUri) noexcept;

    /// @brief Checks whether the document contains our own UUID (self-loop).
    /// @param[in] xmlDoc Parsed XML document.
    /// @param[in] localServiceUuid Server's configured service UUID.
    /// @return True if document originated from this server.
    [[nodiscard]] static bool isSelfMessage(
        const pugi::xml_document& xmlDoc, const std::string& localServiceUuid) noexcept;

    /// @brief Verifies that Types filter in Probe matches supported device types.
    /// @param[in] xmlDoc Parsed XML document.
    /// @return True if Types filter matches or is absent (wildcard).
    [[nodiscard]] static bool matchesTypes(const pugi::xml_document& xmlDoc) noexcept;
};

/// @class DiscoveryRateLimiter
/// @brief Token-bucket rate limiter mitigating UDP amplification floods (CWE-406).
class DiscoveryRateLimiter {
public:
    /// @brief Constructs rate limiter with per-IP and global caps.
    /// @param[in] maxPerIpPerSec Max allowed responses per second for single IP.
    /// @param[in] maxGlobalPerSec Max allowed responses per second globally.
    DiscoveryRateLimiter(std::size_t maxPerIpPerSec = 20, std::size_t maxGlobalPerSec = 60) noexcept;

    /// @brief Checks if a response to the specified sender IP is permitted.
    /// @param[in] senderIp Sender IPv4 string address.
    /// @return True if within rate limit, false if throttled.
    [[nodiscard]] bool checkRateLimit(const std::string& senderIp);

    /// @brief Resets all rate limit counters and tracking history.
    void resetRateLimits();

private:
    struct Bucket {
        std::size_t tokens { 0 };
        std::uint64_t lastRefillSec { 0 };
    };

    std::size_t m_maxPerIpPerSec { 20 };
    std::size_t m_maxGlobalPerSec { 60 };
    std::mutex m_mutex {};

    std::unordered_map<std::string, Bucket> m_ipBuckets {};
    Bucket m_globalBucket {};
};

} // namespace Onvif
