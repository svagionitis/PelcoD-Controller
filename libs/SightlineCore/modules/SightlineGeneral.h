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
struct MsgVersionNumber {
    std::uint8_t hardwareType { 0U };
    std::uint8_t softwareMajor { 0U };
    std::uint8_t softwareMinor { 0U };
    std::uint8_t softwarePatch { 0U };
    std::uint32_t appBits { 0U };
    std::uint32_t boardRevision { 0U };
    std::string versionString {};
};

/// @struct MsgUserWarningMessage
/// @brief Unsolicited diagnostic notifications and error warnings (Message ID 0x86).
struct MsgUserWarningMessage {
    std::uint16_t warningCode { 0U };
    std::string message {};
};

/// @struct MsgSystemStatusMessage
/// @brief Periodic system health, CPU load, and temperature message (Message ID 0x87).
struct MsgSystemStatusMessage {
    std::uint16_t cpuLoadPercent { 0U };
    std::int16_t coreTempC { 0 };
    std::uint32_t uptimeSeconds { 0U };
    std::uint32_t errorFlags { 0U };
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
