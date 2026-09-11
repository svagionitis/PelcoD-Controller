#pragma once

/// @file PelcoDTypes.h
/// @brief Type definitions, enumerations, and constants for Pelco-D protocol.

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
    QueryDiagnostics = 0x6FU
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
    QueryDiagnostics = 0x71U
};

/// @enum AutoMode
/// @brief Generic tri-state auto/on/off configuration.
enum class AutoMode : std::uint8_t { Off = 0x00U, On = 0x01U, Auto = 0x02U };

/// @enum SwitchState
/// @brief Binary switch state.
enum class SwitchState : std::uint8_t { Off = 0x00U, On = 0x01U };

/// @enum TransportState
/// @brief Operational state of communication transport.
enum class TransportState : std::uint8_t { Disconnected = 0x00U, Connecting = 0x01U, Connected = 0x02U, Error = 0x03U };

/// @struct DeviceInfo
/// @brief Identification information received from device query.
struct DeviceInfo {
    std::uint8_t softwareType { 0x00U };
    std::uint8_t hardwareType { 0x00U };
    std::string modelName {};
    std::string serialNumber {};
};

} // namespace PelcoD
