#pragma once

/// @file SightlineSerial.h
/// @brief Sightline SLA Serial Port Module (Serial port communications, passthrough, and GPIO).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__serial.html

#include "../SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @struct MsgSetPortConfiguration
/// @brief Serial port mode and baud rate configuration (Message ID 0x3E).
/// @details Conforms to official Sightline SLASetPortConfiguration_t struct layout.
struct MsgSetPortConfiguration {
    std::uint8_t portIndex { 0U };
    std::uint32_t baudRate { 57600U };
    std::uint8_t dataBits { 8U };
    std::uint8_t stopBits { 1U };
    std::uint8_t parity { 0U };
    std::uint8_t mode { 0U }; // 0: SLA Protocol, 1: Aquarius & SLA, 2: SLA no telemetry, etc.
};

/// @struct MsgCommandPassThrough
/// @brief Transparent serial/network passthrough tunnel (Message ID 0x3D).
struct MsgCommandPassThrough {
    std::uint8_t destPort { 0U };
    std::vector<std::uint8_t> data {};
};

/// @struct MsgGPIO
/// @brief General Purpose I/O pin control and state queries (Message ID 0xB6).
struct MsgGPIO {
    std::uint8_t pinMask { 0xFFU };
    std::uint8_t pinValues { 0x00U };
    std::uint8_t directionMask { 0xFFU }; // 1 = Output, 0 = Input
};

} // namespace Sightline
