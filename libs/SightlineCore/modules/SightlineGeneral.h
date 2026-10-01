#pragma once

/// @file SightlineGeneral.h
/// @brief Sightline SLA General Module (System configuration, parameters, status, warnings).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__general.html

#include "../SightlineTypes.h"

#include <cstdint>
#include <string>
#include <vector>

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
/// @details Conforms to official Sightline SLACurrentConfiguration_t struct layout.
struct MsgCurrentConfiguration {
    std::uint8_t maxCameras { 1U };
    std::uint8_t maxVirtCameras { 0U };
    std::uint8_t maxStreams { 1U };
    std::uint8_t maxProcessed { 1U };
    std::uint16_t cameraConfiguredBits { 0U };
    std::uint16_t cameraConnectedBits { 0U };
    std::uint32_t displayPresentBits { 0U };
    std::uint32_t captureStateBits { 0U };

    // Backward-compatibility aliases
    std::uint8_t numVideoInputs { 1U };
    std::uint8_t numVideoOutputs { 0U };
    std::uint8_t numDisplays { 1U };
    std::uint8_t hardwareType { 1U };
};

/// @struct MsgSystemValue
/// @brief Read/write system register value (Message ID 0x92 / 0x93).
/// @details Conforms to official Sightline SLASetSystemValue_t / SLACurrentSystemValue_t layout.
struct MsgSystemValue {
    std::uint8_t systemValueId { 0U }; ///< System register key identifier
    std::uint32_t value { 0U }; ///< 32-bit register value or bitmask
};

/// @struct MsgTagData
/// @brief Custom binary tags embedded synchronously into video frame headers (Message ID 0x96).
/// @details Conforms to official Sightline SLATagData_t struct layout.
struct MsgTagData {
    std::uint16_t tagId { 0U }; ///< Tag identifier
    std::vector<std::uint8_t> data {}; ///< Custom binary payload bytes
};

/// @struct MsgTagDataRate
/// @brief Controls periodic broadcast rate for a specific Tag ID (Message ID 0x97).
/// @details Conforms to official Sightline SLATagDataRate_t struct layout.
struct MsgTagDataRate {
    std::uint16_t tagId { 0U }; ///< Target tag identifier
    std::uint8_t rate { 0U }; ///< Decimation rate / frequency (0 = disabled, 1 = every frame)
};

/// @struct MsgTagSourceSelector
/// @brief Sensor/camera source channel selector for Tag data (Message ID 0x98).
/// @details Conforms to official Sightline SLATagSourceSelector_t struct layout.
struct MsgTagSourceSelector {
    std::uint16_t tagId { 0U }; ///< Target tag identifier
    std::uint8_t source { 0U }; ///< Camera / processing channel index
};

/// @struct MsgDetailedTiming
/// @brief Frame-by-frame latency profiler telemetry (Message ID 0x88).
/// @details Conforms to official Sightline SLADetailedTimingMessage_t struct layout.
struct MsgDetailedTiming {
    std::uint32_t frameNumber { 0U }; ///< Sequence frame index
    std::uint32_t captureLatencyUs { 0U }; ///< Sensor ingestion latency in microseconds
    std::uint32_t processLatencyUs { 0U }; ///< Video processing pipeline latency in microseconds
    std::uint32_t transmitLatencyUs { 0U }; ///< Network / display transmit queue latency in microseconds
};

/// @struct MsgAppendedMetadata
/// @brief Per-frame appended metadata stream configuration (Message ID 0x89).
/// @details Conforms to official Sightline SLAAppendedMetadata_t struct layout.
struct MsgAppendedMetadata {
    std::uint8_t cameraIndex { 0U }; ///< Camera channel index (0..3)
    std::uint8_t enable { 0U }; ///< 1 = Enable appended metadata, 0 = Disable
};

/// @struct MsgFrameIndex
/// @brief Frame synchronization sequence and timestamp telemetry (Message ID 0x8A).
/// @details Conforms to official Sightline SLAFrameIndex_t struct layout.
struct MsgFrameIndex {
    std::uint8_t cameraIndex { 0U }; ///< Camera channel index (0..3)
    std::uint32_t frameIndex { 0U }; ///< Monotonic frame sequence counter
    std::uint64_t timestampUs { 0ULL }; ///< UTC / sensor timestamp in microseconds
};

} // namespace Sightline
