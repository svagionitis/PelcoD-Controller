#pragma once

/// @file SightlineGeneral.h
/// @brief Sightline SLA General Module (System configuration, parameters, status, warnings).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__general.html

#include "../SightlineTypes.h"

#include <cstdint>
#include <string>

namespace Sightline {

/// @struct MsgGetParameters
/// @brief Unified generic getter request (Message ID 0x28).
struct MsgGetParameters {
    std::uint8_t queryId { 0U };
};

/// @struct MsgResetAllParameters
/// @brief System reset to factory defaults (Message ID 0x01).
struct MsgResetAllParameters {
    std::uint8_t resetType { 0U }; // 0: Reset all, 1: Keep network, 2: Save to flash
};

/// @struct MsgSaveParameters
/// @brief Commit active parameters to flash memory (Message ID 0x25).
struct MsgSaveParameters {
    std::uint8_t commitType { 0U };
};

/// @struct MsgVersionNumber
/// @brief System hardware, firmware, and capability flags reply (Message ID 0x40 / 0x00).
/// @details Conforms to official Sightline SLAVersionNumber_t struct specification.
struct MsgVersionNumber {
    std::uint8_t softwareMajor { 0U }; ///< Software major version number (byte 0)
    std::uint8_t softwareMinor { 0U }; ///< Software minor version number (byte 1)
    std::uint8_t hardwareVersion { 0U }; ///< Second FPGA / HW version (byte 2)
    std::uint8_t degreesF { 0U }; ///< Temperature in degrees F (byte 3)
    std::uint32_t hardwareId { 0U }; ///< Hardware UID (bytes 4..6, 24-bit)
    std::uint32_t appBits { 0U }; ///< Application capability bits (bytes 7..10)
    std::uint8_t boardType { 0U }; ///< Hardware board type (byte 11)
    std::uint8_t hardwareType { 0U }; ///< Alias for boardType for backward compatibility
    std::uint8_t softwareRelease { 0U }; ///< Software revision / release number (byte 12)
    std::uint8_t softwarePatch { 0U }; ///< Alias for softwareRelease
    std::uint16_t otherVersion { 0U }; ///< FPGA & hardware revisions (bytes 13..14)
    std::uint32_t boardRevision { 0U }; ///< Hardware board revision from otherVersion
    std::uint32_t srcRevision { 0U }; ///< Source code revision (bytes 15..18)
    std::uint32_t buildDate { 0U }; ///< Build date (bytes 19..22)
    std::uint32_t buildTime { 0U }; ///< Build time (bytes 23..26)
    std::uint16_t softwareBuild { 0U }; ///< Build number (bytes 27..28)
    std::uint16_t v4AppBits { 0U }; ///< Version 4 application bits (bytes 29..30)
    std::int16_t degreesC { 0 }; ///< Temperature in degrees C (bytes 31..32)
    std::uint32_t adapters { 0U }; ///< Attached camera adapters (bytes 33..36)
    std::string versionString {}; ///< Human-readable version string (e.g. "3.11.6")
};

/// @struct MsgUserWarningMessage
/// @brief Unsolicited diagnostic notifications and error warnings (Message ID 0x86).
struct MsgUserWarningMessage {
    std::uint16_t warningCode { 0U };
    std::string message {};
};

/// @struct MsgSystemStatusMessage
/// @brief Periodic system health, CPU load, and temperature message (Message ID 0x87).
/// @details Conforms to official Sightline SLASystemStatusMessage_t struct specification.
struct MsgSystemStatusMessage {
    std::uint64_t errorFlags { 0ULL }; ///< Reserved error and status flags (bytes 0..7)
    std::int16_t temperatureF { 0 }; ///< Temperature in degrees F (bytes 8..9)
    std::uint8_t load0 { 0U }; ///< Core 0 / ARM 0 load percentage (byte 10)
    std::uint8_t load1 { 0U }; ///< Core 1 / ARM 1 load percentage (byte 11)
    std::uint8_t load2 { 0U }; ///< Core 2 load percentage (byte 12)
    std::uint8_t load3 { 0U }; ///< Core 3 load percentage (byte 13)
    std::int16_t coreTempC { 0 }; ///< Temperature in degrees C (bytes 14..15)
    std::uint8_t missedFrames0 { 0U }; ///< Missed frames: Cam 0 (byte 16)
    std::uint8_t missedFrames1 { 0U }; ///< Missed frames: Cam 1 (byte 17)
    std::uint8_t missedFrames2 { 0U }; ///< Missed frames: Cam 2 (byte 18)
    std::uint8_t missedFrames3 { 0U }; ///< Missed frames: Cam 3 (byte 19)
    std::uint16_t cpuLoadPercent { 0U }; ///< Aggregated CPU load percentage (0..100)
    std::uint32_t uptimeSeconds { 0U }; ///< Application-tracked session uptime in seconds
};

/// @struct MsgSystemStatusMode
/// @brief Enable or disable 1Hz periodic system status messages (Message ID 0x80).
/// @details Official Sightline SLASystemStatusMode_t struct layout.
struct MsgSystemStatusMode {
    std::uint16_t systemStatusBits { 0x0001U }; ///< Bit 0 = 1: Enable status messages
    std::uint32_t systemDebugBits { 0U }; ///< Debug measurement flags
};

/// @struct MsgCurrentConfiguration
/// @brief Current hardware configuration, camera count, and display paths (Message ID 0x8E).
struct MsgCurrentConfiguration {
    std::uint8_t numVideoInputs { 1U };
    std::uint8_t numVideoOutputs { 1U };
    std::uint8_t numDisplays { 1U };
    std::uint8_t hardwareType { 0U };
};

} // namespace Sightline
