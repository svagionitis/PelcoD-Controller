#pragma once

/// @file WsDiscoveryCommon.h
/// @brief Shared constants and UUID generation utility for WS-Discovery multicast services.

#include <atomic>
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

/// @class AppSequenceTracker
/// @brief Monotonically tracks WS-Discovery AppSequence headers (InstanceId and MessageNumber).
class AppSequenceTracker {
public:
    /// @brief Initializes tracker with epoch-based instance ID and message number 1.
    AppSequenceTracker() noexcept;

    /// @brief Initializes tracker with explicit instance ID.
    /// @param[in] instanceId Instance identifier.
    explicit AppSequenceTracker(std::uint32_t instanceId) noexcept;

    /// @brief Increments and returns the next message number.
    /// @return Incrementing 32-bit message number.
    [[nodiscard]] std::uint32_t nextMessageNum() noexcept;

    /// @brief Gets the current instance identifier.
    /// @return 32-bit instance identifier.
    [[nodiscard]] std::uint32_t instanceId() const noexcept;

    /// @brief Formats an AppSequence XML header fragment.
    /// @return XML string e.g. `<d:AppSequence InstanceId="..." MessageNumber="..."/>`.
    [[nodiscard]] std::string formatHeader();

private:
    std::uint32_t m_instanceId { 1U };
    std::atomic<std::uint32_t> m_messageNumber { 1U };
};

} // namespace Onvif
