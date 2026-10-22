#include "WsDiscoveryValidator.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <pugixml.hpp>

#include "AsciiUtils.h"

namespace Onvif {

namespace {

    std::string trimWhitespace(const std::string& str)
    {
        const auto start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            return {};
        }
        const auto end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }

    /// @brief Checks if a single probe scope matches an active device scope according to RFC 3986 rules.
    [[nodiscard]] bool matchScopeRfc3986(std::string_view probeScope, std::string_view activeScope) noexcept
    {
        const auto probePathPos = probeScope.find("://");
        const auto activePathPos = activeScope.find("://");

        if (probePathPos != std::string_view::npos && activePathPos != std::string_view::npos) {
            const auto probeAuthEnd = probeScope.find('/', probePathPos + 3U);
            const auto activeAuthEnd = activeScope.find('/', activePathPos + 3U);

            const auto probeSchemeAuth
                = (probeAuthEnd == std::string_view::npos) ? probeScope : probeScope.substr(0, probeAuthEnd);
            const auto activeSchemeAuth
                = (activeAuthEnd == std::string_view::npos) ? activeScope : activeScope.substr(0, activeAuthEnd);

            if (!equalsIgnoreCaseAscii(probeSchemeAuth, activeSchemeAuth)) {
                return false;
            }

            const auto probePath
                = (probeAuthEnd == std::string_view::npos) ? std::string_view("") : probeScope.substr(probeAuthEnd);
            const auto activePath
                = (activeAuthEnd == std::string_view::npos) ? std::string_view("") : activeScope.substr(activeAuthEnd);

            if (probePath == activePath) {
                return true;
            }

            if (activePath.size() > probePath.size() && activePath.substr(0, probePath.size()) == probePath) {
                if (probePath.empty() || probePath.back() == '/' || activePath[probePath.size()] == '/') {
                    return true;
                }
            }
            return false;
        }

        if (probeScope == activeScope) {
            return true;
        }
        if (activeScope.size() > probeScope.size() && activeScope.substr(0, probeScope.size()) == probeScope) {
            if (probeScope.empty() || probeScope.back() == '/' || activeScope[probeScope.size()] == '/') {
                return true;
            }
        }
        return false;
    }

} // namespace

bool WsDiscoveryValidator::isExactProbeAction(const std::string& actionUri) noexcept
{
    const std::string trimmed = trimWhitespace(actionUri);
    return trimmed == kProbeActionUri;
}

bool WsDiscoveryValidator::isSelfMessage(const pugi::xml_document& xmlDoc, const std::string& localServiceUuid) noexcept
{
    if (localServiceUuid.empty()) {
        return false;
    }

    const pugi::xml_node addrNode = xmlDoc.select_node("//*[local-name()='Address']").node();
    if (addrNode && containsIgnoreCaseAscii(addrNode.text().as_string(), localServiceUuid)) {
        return true;
    }

    const pugi::xml_node msgIdNode = xmlDoc.select_node("//*[local-name()='MessageID']").node();
    if (msgIdNode && containsIgnoreCaseAscii(msgIdNode.text().as_string(), localServiceUuid)) {
        return true;
    }

    const pugi::xml_node relatesNode = xmlDoc.select_node("//*[local-name()='RelatesTo']").node();
    if (relatesNode && containsIgnoreCaseAscii(relatesNode.text().as_string(), localServiceUuid)) {
        return true;
    }

    return false;
}

bool WsDiscoveryValidator::matchesTypes(const pugi::xml_document& xmlDoc) noexcept
{
    const pugi::xml_node typesNode = xmlDoc.select_node("//*[local-name()='Types']").node();
    if (typesNode) {
        const std::string typesText = typesNode.text().as_string();
        if (!typesText.empty() && typesText.find("NetworkVideoTransmitter") == std::string::npos
            && typesText.find("Device") == std::string::npos) {
            return false;
        }
    }
    return true;
}

bool WsDiscoveryValidator::matchesScopes(
    const pugi::xml_document& xmlDoc, const std::vector<std::string>& activeScopes) noexcept
{
    const pugi::xml_node scopesNode = xmlDoc.select_node("//*[local-name()='Scopes']").node();
    if (!scopesNode) {
        return true;
    }

    const std::string scopesText = trimWhitespace(scopesNode.text().as_string());
    if (scopesText.empty()) {
        return true;
    }

    const std::string matchBy = scopesNode.attribute("MatchBy").as_string();
    if (matchBy == "http://schemas.xmlsoap.org/ws/2005/04/discovery/none") {
        return false;
    }

    std::istringstream iss(scopesText);
    std::string probeScope;
    while (iss >> probeScope) {
        bool matchedOne = false;
        for (const auto& activeScope : activeScopes) {
            if (matchBy == "http://schemas.xmlsoap.org/ws/2005/04/discovery/strcmp0") {
                if (probeScope == activeScope) {
                    matchedOne = true;
                    break;
                }
            } else {
                if (matchScopeRfc3986(probeScope, activeScope)) {
                    matchedOne = true;
                    break;
                }
            }
        }
        if (!matchedOne) {
            return false;
        }
    }
    return true;
}

DiscoveryValidationResult WsDiscoveryValidator::validateProbe(const pugi::xml_document& xmlDoc,
    std::uint16_t senderPort, const std::string& localServiceUuid, bool dropReflectionPort3702) noexcept
{
    return validateProbe(xmlDoc, senderPort, localServiceUuid, dropReflectionPort3702, {});
}

DiscoveryValidationResult WsDiscoveryValidator::validateProbe(const pugi::xml_document& xmlDoc,
    std::uint16_t senderPort, const std::string& localServiceUuid, bool dropReflectionPort3702,
    const std::vector<std::string>& activeScopes) noexcept
{
    DiscoveryValidationResult result {};

    // 1. Reflection defense: probes arrive from client ephemeral ports (> 1024), never 3702
    if (dropReflectionPort3702 && senderPort == 3702) {
        result.isReflectionAttempt = true;
        return result;
    }

    // 2. Self-message loop suppression
    if (isSelfMessage(xmlDoc, localServiceUuid)) {
        result.isSelfMessage = true;
        return result;
    }

    // 3. Strict Action matching (exact WS-Discovery 2005/04 Probe URI)
    const pugi::xml_node actionNode = xmlDoc.select_node("//*[local-name()='Action']").node();
    const std::string actionText = actionNode ? actionNode.text().as_string() : "";
    if (!isExactProbeAction(actionText)) {
        return result;
    }

    // 4. SOAP Body structure validation
    const pugi::xml_node bodyNode = xmlDoc.select_node("//*[local-name()='Body']").node();
    if (!bodyNode) {
        return result;
    }

    // Explicitly reject if Body contains ProbeMatches, Hello, Bye, Resolve
    if (bodyNode.child("ProbeMatches") || bodyNode.select_node(".//*[local-name()='ProbeMatches']")) {
        return result;
    }
    if (bodyNode.child("Hello") || bodyNode.select_node(".//*[local-name()='Hello']")) {
        return result;
    }
    if (bodyNode.child("Bye") || bodyNode.select_node(".//*[local-name()='Bye']")) {
        return result;
    }

    // Must contain Probe element in Body
    const pugi::xml_node probeNode = bodyNode.select_node(".//*[local-name()='Probe']").node();
    if (!probeNode) {
        return result;
    }

    // 5. Types filter check
    if (!matchesTypes(xmlDoc)) {
        return result;
    }

    // 6. Scopes filter check (WS-Discovery §5.2)
    if (!activeScopes.empty() && !matchesScopes(xmlDoc, activeScopes)) {
        return result;
    }

    // 7. Extract MessageID for RelatesTo header in ProbeMatches
    const pugi::xml_node msgIdNode = xmlDoc.select_node("//*[local-name()='MessageID']").node();
    result.messageId = msgIdNode ? trimWhitespace(msgIdNode.text().as_string()) : "";
    result.isValidProbe = true;
    return result;
}

DiscoveryRateLimiter::DiscoveryRateLimiter(std::size_t maxPerIpPerSec, std::size_t maxGlobalPerSec) noexcept
    : m_maxPerIpPerSec(maxPerIpPerSec)
    , m_maxGlobalPerSec(maxGlobalPerSec)
{
    m_globalBucket.tokens = maxGlobalPerSec;
}

bool DiscoveryRateLimiter::checkRateLimit(const std::string& senderIp)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto nowSec = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count());

    // Refill global bucket
    if (nowSec > m_globalBucket.lastRefillSec) {
        m_globalBucket.tokens = m_maxGlobalPerSec;
        m_globalBucket.lastRefillSec = nowSec;
    }

    if (m_globalBucket.tokens == 0) {
        return false;
    }

    // Refill per-IP bucket
    auto& ipBucket = m_ipBuckets[senderIp];
    if (nowSec > ipBucket.lastRefillSec) {
        ipBucket.tokens = m_maxPerIpPerSec;
        ipBucket.lastRefillSec = nowSec;
    }

    if (ipBucket.tokens == 0) {
        return false;
    }

    --m_globalBucket.tokens;
    --ipBucket.tokens;
    return true;
}

void DiscoveryRateLimiter::resetRateLimits()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ipBuckets.clear();
    m_globalBucket.tokens = m_maxGlobalPerSec;
    m_globalBucket.lastRefillSec = 0;
}

} // namespace Onvif
