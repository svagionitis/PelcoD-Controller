#pragma once

/// @file Iec61162_460Firewall.h
/// @brief Stateful maritime network firewall and interface isolator conforming to IEC 61162-460.

#include "Iec61162_460Types.h"
#include "bam/BridgeAlertManager.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Nmea::Network {

/// @class Iec61162_460Firewall
/// @brief Stateful maritime network firewall and interface isolator conforming to IEC 61162-460.
/// @details Validates source IP, MAC address bindings, transmission group permissions, sentence formatters,
///          and enforces sliding-window rate limits to prevent packet storms and denial-of-service.
class Iec61162_460Firewall {
public:
    using SecurityAlertCallback = std::function<void(const SecurityIncident& incident)>;

    Iec61162_460Firewall();
    ~Iec61162_460Firewall() = default;

    // Non-copyable, non-movable
    Iec61162_460Firewall(const Iec61162_460Firewall&) = delete;
    Iec61162_460Firewall& operator=(const Iec61162_460Firewall&) = delete;
    Iec61162_460Firewall(Iec61162_460Firewall&&) = delete;
    Iec61162_460Firewall& operator=(Iec61162_460Firewall&&) = delete;

    /// @brief Registers an ingress/egress filtering rule.
    /// @param[in] rule Firewall rule definition.
    void addRule(const FirewallRule& rule);

    /// @brief Clears all configured firewall rules.
    void clearRules();

    /// @brief Binds a known MAC address to an IP address for anti-spoofing validation.
    /// @param[in] ip Source IPv4 address string.
    /// @param[in] mac Expected MAC address string.
    void bindMacAddress(std::string_view ip, std::string_view mac);

    /// @brief Attaches a Bridge Alert Manager to automatically raise BAM alerts on security violations.
    /// @param[in] bam Shared pointer to BridgeAlertManager instance.
    void attachAlertManager(std::shared_ptr<Bam::BridgeAlertManager> bam);

    /// @brief Sets callback for notifying application logic upon security violations.
    /// @param[in] cb Callback invoked on security incidents.
    void setAlertCallback(SecurityAlertCallback cb);

    /// @brief Inspects an inbound network packet against configured security policies.
    /// @param[in] zone Source network zone.
    /// @param[in] sourceIp Source IPv4 address string.
    /// @param[in] sourceMac Source MAC address string.
    /// @param[in] payload Raw packet payload or NMEA sentence.
    /// @return True if packet is permitted, false if rejected/dropped.
    [[nodiscard]] bool inspectInbound(SecurityZone zone, std::string_view sourceIp,
                                      std::string_view sourceMac, std::string_view payload);

    /// @brief Validates if an outbound sentence is permitted to egress towards a target zone.
    /// @param[in] targetZone Destination network zone.
    /// @param[in] payload Outbound sentence payload.
    /// @return True if permitted, false if blocked.
    [[nodiscard]] bool inspectOutbound(SecurityZone targetZone, std::string_view payload);

    /// @brief Retrieves the recent security incident audit history.
    /// @return Vector of recorded security incidents.
    [[nodiscard]] std::vector<SecurityIncident> getIncidentHistory() const;

    /// @brief Clears the incident history log.
    void clearIncidentHistory();

    /// @brief Returns the total number of security violations detected.
    [[nodiscard]] std::size_t getViolationCount() const;

private:
    struct SourceRateTracker {
        std::chrono::steady_clock::time_point windowStart {};
        std::uint32_t packetCount { 0U };
    };

    [[nodiscard]] bool checkRateLimit(std::string_view ip, std::uint32_t maxPps);
    [[nodiscard]] static bool matchesCidr(std::string_view ip, std::string_view cidr);
    [[nodiscard]] static std::string extractFormatter(std::string_view payload);

    void recordViolation(SecurityViolationType type, SecurityZone zone, std::string_view ip,
                         std::string_view mac, std::string_view desc);

    mutable std::mutex m_mutex {};
    std::vector<FirewallRule> m_rules {};
    std::unordered_map<std::string, std::string> m_macBindings {};
    std::shared_ptr<Bam::BridgeAlertManager> m_bam {};
    SecurityAlertCallback m_alertCallback {};
    std::vector<SecurityIncident> m_incidents {};
    std::unordered_map<std::string, SourceRateTracker> m_rateTrackers {};
    std::size_t m_violationCount { 0U };
};

} // namespace Nmea::Network
