#pragma once

/// @file WsDiscoveryCommon.h
/// @brief Shared constants and UUID generation utility for WS-Discovery multicast services.

#include <cstdint>
#include <string>

namespace Onvif {

/// @brief Default IPv4 multicast group address for WS-Discovery.
inline constexpr const char* kMulticastIp { "239.255.255.250" };

/// @brief Standard UDP multicast port for WS-Discovery (RFC 3927 / ONVIF Profile S).
inline constexpr std::uint16_t kMulticastPort { 3702 };

/// @brief Generates a random RFC 4122 version 4 UUID string.
/// @return Hyphen-formatted UUID string (e.g., "550e8400-e29b-41d4-a716-446655440000").
[[nodiscard]] std::string generateRandomUuid();

} // namespace Onvif
