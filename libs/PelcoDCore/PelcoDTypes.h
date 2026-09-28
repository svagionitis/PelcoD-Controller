#pragma once

/// @file PelcoDTypes.h
/// @brief Type definitions, enumerations, and constants for Pelco-D protocol.

#include <Transport/TransportTypes.h>
#include <cstdint>
#include <string>

namespace PelcoD {

/// @enum PanDirection
/// @brief Standard Pan motion directions.
enum class PanDirection : std::uint8_t { Stop = 0x00U, Right = 0x02U, Left = 0x04U };

/// @enum TiltDirection
/// @brief Standard Tilt motion directions.
enum class TiltDirection : std::uint8_t { Stop = 0x00U, Up = 0x08U, Down = 0x10U };

/// @enum IrisAction
/// @brief Standard Iris actions.
enum class IrisAction : std::uint8_t { Stop = 0x00U, Open = 0x02U, Close = 0x04U };

/// @enum FocusAction
/// @brief Standard Focus actions.
enum class FocusAction : std::uint8_t { Stop = 0x00U, Near = 0x01U, Far = 0x80U };

/// @enum ZoomAction
/// @brief Standard Zoom actions.
enum class ZoomAction : std::uint8_t { Stop = 0x00U, Tele = 0x20U, Wide = 0x40U };

/// @enum ScanSense
/// @brief Sense bit interpretations for Command 1.
enum class ScanSense : std::uint8_t { AutoScanOn = 0x90U, ManualScanOn = 0x10U, DeviceOn = 0x88U, DeviceOff = 0x08U };

/// @enum CommandOpcode
/// @brief Extended command opcode values (Command 2 byte).
enum class CommandOpcode : std::uint8_t {
    SetPreset = 0x03U,
    ClearPreset = 0x05U,
    GoToPreset = 0x07U,
    SetAuxiliary = 0x09U,
    ClearAuxiliary = 0x0BU,
    Dummy = 0x0DU,
    RemoteReset = 0x0FU,
    SetZoneStart = 0x11U,
    SetZoneEnd = 0x13U,
    WriteCharacter = 0x15U,
    ClearScreen = 0x17U,
    AlarmAcknowledge = 0x19U,
    ZoneScanOn = 0x1BU,
    ZoneScanOff = 0x1DU,
    RecordPatternStart = 0x1FU,
    RecordPatternStop = 0x21U,
    RunPattern = 0x23U,
    SetZoomSpeed = 0x25U,
    SetFocusSpeed = 0x27U,
    ResetDefaults = 0x29U,
    AutoFocus = 0x2BU,
    AutoIris = 0x2DU,
    Agc = 0x2FU,
    BacklightComp = 0x31U,
    AutoWhiteBalance = 0x33U,
    PhaseDelayMode = 0x35U,
    SetShutterSpeed = 0x37U,
    AdjustLineLock = 0x39U,
    AdjustWbRedBlue = 0x3BU,
    AdjustWbMg = 0x3DU,
    AdjustGain = 0x3FU,
    AdjustAutoIrisLevel = 0x41U,
    AdjustAutoIrisPeak = 0x43U,
    Query = 0x45U,
    PresetScan = 0x47U,
    SetZeroPosition = 0x49U,
    SetPanPosition = 0x4BU,
    SetTiltPosition = 0x4DU,
    SetZoomPosition = 0x4FU,
    QueryPanPosition = 0x51U,
    QueryTiltPosition = 0x53U,
    QueryZoomPosition = 0x55U,
    PrepareForDownload = 0x57U,
    SetMagnification = 0x5FU,
    QueryMagnification = 0x61U,
    EchoMode = 0x65U,
    SetBaudRate = 0x67U,
    StartDownload = 0x69U,
    QueryDeviceType = 0x6BU,
    QueryDiagnostics = 0x6FU,
    VersionInfo = 0x73U,
    TimeMacro = 0x77U,
    ScreenMove = 0x79U
};

/// @enum ResponseOpcode
/// @brief Extended response opcode values.
enum class ResponseOpcode : std::uint8_t {
    StandardExtended = 0x01U,
    QueryPan = 0x59U,
    QueryTilt = 0x5BU,
    QueryZoom = 0x5DU,
    QueryMagnification = 0x63U,
    QueryDeviceType = 0x6DU,
    QueryDiagnostics = 0x71U,
    VersionInfo = 0x73U,
    TimeMacro = 0x77U
};

/// @enum VersionInfoSubOpcode
/// @brief Sub-opcodes for Version Information Macro (opcode 0x73, Command 1 byte).
enum class VersionInfoSubOpcode : std::uint8_t {
    RequestSoftwareVersion = 0x00U,
    SoftwareVersionResponse = 0x01U,
    RequestBuildNumber = 0x02U,
    BuildNumberResponse = 0x03U
};

/// @enum TimeSubOpcode
/// @brief Sub-opcodes for Time Commands Macro (opcode 0x77, Command 1 byte).
enum class TimeSubOpcode : std::uint8_t {
    SetSeconds = 0x00U,
    ReportSeconds = 0x01U,
    SetHourMinute = 0x02U,
    ReportHourMinute = 0x03U,
    SetMonthDay = 0x04U,
    ReportMonthDay = 0x05U,
    SetYear = 0x06U,
    ReportYear = 0x07U
};

/// @struct PelcoDTime
/// @brief Date and time container for Pelco-D clock synchronization (opcode 0x77).
struct PelcoDTime {
    std::uint8_t hour { 0U };
    std::uint8_t minute { 0U };
    std::uint8_t second { 0U };
    std::uint8_t month { 0U };
    std::uint8_t day { 0U };
    std::uint16_t year { 0U };
};

/// @enum AutoMode
/// @brief Generic tri-state auto/on/off configuration.
enum class AutoMode : std::uint8_t { Off = 0x00U, On = 0x01U, Auto = 0x02U };

/// @enum SwitchState
/// @brief Binary switch state.
enum class SwitchState : std::uint8_t { Off = 0x00U, On = 0x01U };

/// @brief Operational state of communication transport.
using TransportState = ::Transport::TransportState;

/// @enum CommandPriority
/// @brief Priority levels for command execution in PelcoDDevice.
enum class CommandPriority : std::uint8_t {
    Low = 0U, ///< Background queries and non-essential telemetry polling
    Normal = 1U, ///< Standard motion, presets, and configuration commands
    Urgent = 2U ///< Safety-critical commands (e.g. stopMotion) that preempt in-flight queries
};

/// @struct DeviceInfo
/// @brief Identification information received from device query.
struct DeviceInfo {
    std::uint8_t softwareType { 0x00U };
    std::uint8_t hardwareType { 0x00U };
    std::string modelName {};
    std::string serialNumber {};
    std::uint8_t softwareMajor { 0x00U };
    std::uint8_t softwareMinor { 0x00U };
    std::uint16_t buildNumber { 0x0000U };
};

} // namespace PelcoD
