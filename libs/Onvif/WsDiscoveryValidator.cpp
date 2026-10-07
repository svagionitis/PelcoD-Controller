#include "WsDiscoveryValidator.h"
#include <pugixml.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>

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

    bool containsUuid(const std::string& text, const std::string& uuid)
    {
        if (uuid.empty() || text.empty()) {
            return false;
        }

        std::string textLower = text;
        std::string uuidLower = uuid;
        std::transform(textLower.begin(), textLower.end(), textLower.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        std::transform(uuidLower.begin(), uuidLower.end(), uuidLower.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

        return textLower.find(uuidLower) != std::string::npos;
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
    if (addrNode && containsUuid(addrNode.text().as_string(), localServiceUuid)) {
        return true;
    }

    const pugi::xml_node msgIdNode = xmlDoc.select_node("//*[local-name()='MessageID']").node();
    if (msgIdNode && containsUuid(msgIdNode.text().as_string(), localServiceUuid)) {
        return true;
    }

    const pugi::xml_node relatesNode = xmlDoc.select_node("//*[local-name()='RelatesTo']").node();
    if (relatesNode && containsUuid(relatesNode.text().as_string(), localServiceUuid)) {
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

DiscoveryValidationResult WsDiscoveryValidator::validateProbe(const pugi::xml_document& xmlDoc,
    std::uint16_t senderPort, const std::string& localServiceUuid, bool dropReflectionPort3702) noexcept
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

    // 6. Extract MessageID for RelatesTo header in ProbeMatches
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
