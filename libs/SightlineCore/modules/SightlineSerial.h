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

/// @struct MsgI2CCommand
/// @brief Master I2C transaction across buses 0..3 for sensor daughterboards (Message ID 0x94).
/// @details Conforms to official Sightline SLAI2CCommand_t struct layout.
struct MsgI2CCommand {
    std::uint8_t busIndex { 0U }; ///< Hardware I2C bus index (0..3)
    std::uint8_t deviceAddress { 0U }; ///< 7-bit / 8-bit slave I2C device address
    std::uint8_t subAddress { 0U }; ///< Register sub-address / pointer
    std::uint8_t writeLength { 0U }; ///< Number of bytes to write
    std::uint8_t readLength { 0U }; ///< Number of bytes to read
    std::vector<std::uint8_t> data {}; ///< Payload buffer for write data or returned read data
};

/// @struct MsgSendToBTS
/// @brief Transparent packet forwarding bridge to Base Transceiver Station / RF modem (Message ID 0xBE).
/// @details Conforms to official Sightline SLASendToBTS_t struct layout.
struct MsgSendToBTS {
    std::uint8_t btsPort { 0U }; ///< Target BTS hardware channel or RF link ID
    std::vector<std::uint8_t> data {}; ///< Transparent raw data payload
};

} // namespace Sightline
