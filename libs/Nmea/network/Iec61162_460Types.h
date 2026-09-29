#pragma once

/// @file Iec61162_460Types.h
/// @brief Types, enumerations, and structures for IEC 61162-460 Secure Marine Gateway compliance.

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace Nmea::Network {

/// @brief Security classification zones defined by IEC 61162-460.
enum class SecurityZone : std::uint8_t {
    BridgeNetwork = 0U,   ///< Secure navigation network (IEC 61162-450 LWE, radar, gyro, GNSS)
    ExternalCamera = 1U,  ///< PTZ and thermal sensor payload network
    GeneralShipLan = 2U,  ///< General shipboard LAN, workstations, crew access
    InternetShore = 3U    ///< Shore connection, satellite link (untrusted)
};

/// @brief Violation categories triggering IEC 61162-460 security alarms.
enum class SecurityViolationType : std::uint8_t {
    UnauthorizedIp,          ///< Packet received from non-whitelisted IP address
    MacIpMismatch,           ///< Inbound packet MAC does not match known ARP/whitelist binding
    DisallowedGroup,         ///< Packet targeted unauthorized LWE transmission group
    DisallowedSentence,      ///< Inbound sentence formatter not permitted by zone policy
    RateLimitExceeded,       ///< Transmission frequency exceeds packets-per-second threshold
    MalformedTagBlock,       ///< Tag block syntax invalid or corrupted
    UnauthorizedPtzCommand   ///< Gimbal control attempted from untrusted network zone
};

/// @brief Detailed security incident log record.
struct SecurityIncident {
    std::chrono::system_clock::time_point timestamp {};
    SecurityViolationType violationType { SecurityViolationType::UnauthorizedIp };
    SecurityZone sourceZone { SecurityZone::GeneralShipLan };
    std::string sourceIp {};
    std::string sourceMac {};
    std::uint16_t port { 0U };
    std::string description {};
};

/// @brief Ingress and egress firewall rule specification.
struct FirewallRule {
    SecurityZone allowedZone { SecurityZone::BridgeNetwork };
    std::string ipCidr {};                         ///< Allowed CIDR block or IP (e.g. "192.168.1.0/24", empty = any)
    std::string macAddress {};                     ///< Expected MAC (e.g. "00:1A:2B:3C:4D:5E", empty = any)
    std::vector<std::string> allowedFormatters {}; ///< Allowed NMEA formatters (e.g. {"GGA", "RMC", "HDT"})
    std::uint32_t maxPacketsPerSec { 100U };       ///< Maximum allowable PPS before rate-limiting
};

/// @brief Standard BAM Alert IDs for IEC 61162-460 security violations.
inline constexpr std::uint32_t kAlertIdSecurityViolation = 46001U;
inline constexpr std::uint32_t kAlertIdNetworkFlood = 46002U;

} // namespace Nmea::Network
