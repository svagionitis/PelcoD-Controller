/// @file Iec61162_460Firewall.cpp
/// @brief Implementation of IEC 61162-460 Secure Marine Gateway Firewall.

#include "Iec61162_460Firewall.h"

#include <algorithm>
#include <sstream>

namespace Nmea::Network {

Iec61162_460Firewall::Iec61162_460Firewall() = default;

void Iec61162_460Firewall::addRule(const FirewallRule& rule)
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_rules.push_back(rule);
}

void Iec61162_460Firewall::clearRules()
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_rules.clear();
}

void Iec61162_460Firewall::bindMacAddress(std::string_view ip, std::string_view mac)
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_macBindings[std::string { ip }] = std::string { mac };
}

void Iec61162_460Firewall::attachAlertManager(std::shared_ptr<Bam::BridgeAlertManager> bam)
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_bam = std::move(bam);
}

void Iec61162_460Firewall::setAlertCallback(SecurityAlertCallback cb)
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_alertCallback = std::move(cb);
}

bool Iec61162_460Firewall::inspectInbound(SecurityZone zone, std::string_view sourceIp,
                                          std::string_view sourceMac, std::string_view payload)
{
    std::lock_guard<std::mutex> lock { m_mutex };

    // 1. Check Anti-Spoofing IP-to-MAC Binding
    const auto itMac = m_macBindings.find(std::string { sourceIp });
    if (itMac != m_macBindings.end() && !sourceMac.empty() && itMac->second != sourceMac) {
        recordViolation(SecurityViolationType::MacIpMismatch, zone, sourceIp, sourceMac,
                        "MAC address mismatch for bound IP: expected " + itMac->second + ", got " + std::string { sourceMac });
        return false;
    }

    // 2. Zone Policy and Whitelist Evaluation
    bool matchedZoneRule { false };
    bool passedAllFilters { true };

    for (const auto& rule : m_rules) {
        if (rule.allowedZone != zone) {
            continue;
        }
        matchedZoneRule = true;

        // Check IP / CIDR filter
        if (!rule.ipCidr.empty() && !matchesCidr(sourceIp, rule.ipCidr)) {
            passedAllFilters = false;
            recordViolation(SecurityViolationType::UnauthorizedIp, zone, sourceIp, sourceMac,
                            "Source IP rejected by zone CIDR policy");
            return false;
        }

        // Check MAC filter if rule explicitly demands it
        if (!rule.macAddress.empty() && !sourceMac.empty() && rule.macAddress != sourceMac) {
            passedAllFilters = false;
            recordViolation(SecurityViolationType::MacIpMismatch, zone, sourceIp, sourceMac,
                            "Source MAC does not match rule requirement");
            return false;
        }

        // Check Rate Limiting
        if (rule.maxPacketsPerSec > 0U && !checkRateLimit(sourceIp, rule.maxPacketsPerSec)) {
            passedAllFilters = false;
            recordViolation(SecurityViolationType::RateLimitExceeded, zone, sourceIp, sourceMac,
                            "Traffic rate limit exceeded (> " + std::to_string(rule.maxPacketsPerSec) + " PPS)");
            return false;
        }

        // Check Allowed Sentence Formatters
        if (!rule.allowedFormatters.empty()) {
            const std::string formatter { extractFormatter(payload) };
            if (!formatter.empty()) {
                const auto itFmt = std::find(rule.allowedFormatters.begin(), rule.allowedFormatters.end(), formatter);
                if (itFmt == rule.allowedFormatters.end()) {
                    passedAllFilters = false;
                    recordViolation(SecurityViolationType::DisallowedSentence, zone, sourceIp, sourceMac,
                                    "Sentence formatter " + formatter + " disallowed by zone policy");
                    return false;
                }
            }
        }
        break; // Evaluated matching zone rule
    }

    // If rules are configured but this zone has no matching rule, block untrusted zones
    if (!matchedZoneRule && !m_rules.empty() && zone == SecurityZone::InternetShore) {
        recordViolation(SecurityViolationType::UnauthorizedIp, zone, sourceIp, sourceMac,
                        "Untrusted shore network denied ingress by default");
        return false;
    }

    return passedAllFilters;
}

bool Iec61162_460Firewall::inspectOutbound(SecurityZone targetZone, std::string_view payload)
{
    std::lock_guard<std::mutex> lock { m_mutex };
    // Block sensitive internal security alerts or proprietary camera commands from egressing to Internet
    if (targetZone == SecurityZone::InternetShore) {
        const std::string formatter { extractFormatter(payload) };
        if (formatter == "ALF" || formatter == "ARC" || formatter == "PFLIR" || formatter == "PFEC") {
            recordViolation(SecurityViolationType::DisallowedSentence, targetZone, "localhost", "",
                            "Egress leak prevented: " + formatter + " blocked to untrusted network");
            return false;
        }
    }
    return true;
}

std::vector<SecurityIncident> Iec61162_460Firewall::getIncidentHistory() const
{
    std::lock_guard<std::mutex> lock { m_mutex };
    return m_incidents;
}

void Iec61162_460Firewall::clearIncidentHistory()
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_incidents.clear();
}

std::size_t Iec61162_460Firewall::getViolationCount() const
{
    std::lock_guard<std::mutex> lock { m_mutex };
    return m_violationCount;
}

bool Iec61162_460Firewall::checkRateLimit(std::string_view ip, std::uint32_t maxPps)
{
    const auto now = std::chrono::steady_clock::now();
    auto& tracker = m_rateTrackers[std::string { ip }];

    if (tracker.windowStart.time_since_epoch().count() == 0 ||
        std::chrono::duration_cast<std::chrono::seconds>(now - tracker.windowStart).count() >= 1) {
        tracker.windowStart = now;
        tracker.packetCount = 1U;
        return true;
    }

    ++tracker.packetCount;
    return tracker.packetCount <= maxPps;
}

bool Iec61162_460Firewall::matchesCidr(std::string_view ip, std::string_view cidr)
{
    if (cidr.empty() || ip.empty()) {
        return true;
    }
    const auto slashPos = cidr.find('/');
    if (slashPos == std::string_view::npos) {
        return ip == cidr;
    }

    // Check subnet prefix (e.g. "192.168.1." for "192.168.1.0/24")
    const std::string_view prefix = cidr.substr(0U, slashPos);
    const auto lastDot = prefix.rfind('.');
    if (lastDot != std::string_view::npos) {
        const std::string_view basePrefix = prefix.substr(0U, lastDot + 1U);
        return ip.substr(0U, basePrefix.size()) == basePrefix;
    }
    return ip == prefix;
}

std::string Iec61162_460Firewall::extractFormatter(std::string_view payload)
{
    const auto startPos = payload.find_first_of("$!");
    if (startPos == std::string_view::npos || startPos + 2U >= payload.size()) {
        return "";
    }
    const auto commaPos = payload.find(',', startPos);
    if (commaPos == std::string_view::npos || commaPos <= startPos + 1U) {
        return "";
    }
    const std::string_view addressField = payload.substr(startPos + 1U, commaPos - (startPos + 1U));
    if (addressField.empty()) {
        return "";
    }
    // Proprietary sentence (e.g. $PFEC, $PFLIR)
    if (addressField[0] == 'P' || addressField[0] == 'p') {
        return std::string { addressField };
    }
    // Standard sentence: 2-char talker + 3-char formatter (e.g. GPGGA -> GGA)
    if (addressField.size() >= 3U) {
        return std::string { addressField.substr(addressField.size() - 3U) };
    }
    return std::string { addressField };
}

void Iec61162_460Firewall::recordViolation(SecurityViolationType type, SecurityZone zone, std::string_view ip,
                                          std::string_view mac, std::string_view desc)
{
    ++m_violationCount;

    SecurityIncident incident {};
    incident.timestamp = std::chrono::system_clock::now();
    incident.violationType = type;
    incident.sourceZone = zone;
    incident.sourceIp = std::string { ip };
    incident.sourceMac = std::string { mac };
    incident.description = std::string { desc };

    if (m_incidents.size() >= 500U) {
        m_incidents.erase(m_incidents.begin());
    }
    m_incidents.push_back(incident);

    if (m_alertCallback) {
        m_alertCallback(incident);
    }

    if (m_bam) {
        if (type == SecurityViolationType::RateLimitExceeded) {
            m_bam->registerAlert(kAlertIdNetworkFlood, 1U, Bam::AlertPriority::Warning,
                                 Bam::AlertCategory::CategoryB, "Network Flood",
                                 "Bridge Network Flood Detected");
        } else {
            m_bam->registerAlert(kAlertIdSecurityViolation, 1U, Bam::AlertPriority::Warning,
                                 Bam::AlertCategory::CategoryB, "Security Violation",
                                 std::string { desc });
        }
    }
}

} // namespace Nmea::Network
