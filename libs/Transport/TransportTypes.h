#pragma once

/// @file TransportTypes.h
/// @brief Core connection state types and enumerations for streaming transports (protocol-independent).

#include <cstdint>

namespace Transport {

/// @enum TransportState
/// @brief Operational state of communication transport.
enum class TransportState : std::uint8_t { Disconnected = 0x00U, Connecting = 0x01U, Connected = 0x02U, Error = 0x03U };

} // namespace Transport
