#pragma once

/// @file PelcoDTypes.h
/// @brief Type definitions, enumerations, and constants for Pelco-D protocol.

#include <Transport/TransportTypes.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace PelcoD {

/// @brief Standard Pelco-D command frame length in bytes per specification v5.0.1.
inline constexpr std::size_t kStandardFrameSize { 7U };

/// @brief General response / alarm frame length in bytes.
inline constexpr std::size_t kGeneralResponseSize { 4U };

/// @brief Extended ASCII query response frame length in bytes.
inline constexpr std::size_t kQueryResponseSize { 18U };

/// @brief Zero-allocation fixed 7-byte Pelco-D command frame.
/// @details Represents all standard Pelco-D transmitted commands as a fixed-size stack array.
///          Eliminates heap allocation on hot command dispatch paths (MISRA C++:2023 Rule 21.6.1).
using StandardFrame = std::array<std::uint8_t, kStandardFrameSize>;

/// @class RawFrame
/// @brief Bounded, stack-allocated protocol frame container for 4, 7, or 18-byte packets.
/// @details Provides a uniform value type capable of storing any valid Pelco-D packet
///          without dynamic heap allocation.
class RawFrame final {
public:
    static constexpr std::size_t MaxCapacity { kQueryResponseSize };

    constexpr RawFrame() noexcept = default;

    /// @brief Construct a RawFrame from a 7-byte StandardFrame.
    /// @param[in] frame Source standard frame array.
    constexpr RawFrame(const StandardFrame& frame) noexcept
        : m_size { static_cast<std::uint8_t>(kStandardFrameSize) }
    {
        for (std::size_t i { 0U }; i < kStandardFrameSize; ++i) {
            m_data[i] = frame[i];
        }
    }

    /// @brief Construct a RawFrame from raw memory pointer and length.
    /// @param[in] bytes Pointer to raw byte buffer.
    /// @param[in] length Number of bytes in buffer.
    RawFrame(const std::uint8_t* bytes, std::size_t length) noexcept
        : m_size { static_cast<std::uint8_t>(length <= MaxCapacity ? length : MaxCapacity) }
    {
        if (bytes != nullptr) {
            for (std::size_t i { 0U }; i < m_size; ++i) {
                m_data[i] = bytes[i];
            }
        }
    }

    /// @brief Retrieves pointer to underlying buffer.
    [[nodiscard]] constexpr const std::uint8_t* data() const noexcept
    {
        return m_data.data();
    }

    /// @brief Retrieves mutable pointer to underlying buffer.
    [[nodiscard]] constexpr std::uint8_t* data() noexcept
    {
        return m_data.data();
    }

    /// @brief Retrieves number of active bytes stored.
    [[nodiscard]] constexpr std::size_t size() const noexcept
    {
        return m_size;
    }

    /// @brief Checks whether the frame contains zero bytes.
    [[nodiscard]] constexpr bool empty() const noexcept
    {
        return m_size == 0U;
    }

    /// @brief Subscript access operator.
    [[nodiscard]] constexpr std::uint8_t operator[](std::size_t idx) const noexcept
    {
        return m_data[idx];
    }

    /// @brief Mutable subscript access operator.
    [[nodiscard]] constexpr std::uint8_t& operator[](std::size_t idx) noexcept
    {
        return m_data[idx];
    }

    /// @brief Beginning iterator.
    [[nodiscard]] constexpr const std::uint8_t* begin() const noexcept
    {
        return m_data.data();
    }

    /// @brief End iterator.
    [[nodiscard]] constexpr const std::uint8_t* end() const noexcept
    {
        return m_data.data() + m_size;
    }

    /// @brief Converts to a 7-byte StandardFrame if size matches.
    [[nodiscard]] StandardFrame toStandardFrame() const noexcept
    {
        StandardFrame out {};
        for (std::size_t i { 0U }; i < kStandardFrameSize && i < m_size; ++i) {
            out[i] = m_data[i];
        }
        return out;
    }

    /// @brief Converts stored bytes to a heap std::vector for legacy consumers.
    [[nodiscard]] std::vector<std::uint8_t> toVector() const
    {
        return std::vector<std::uint8_t>(m_data.begin(), m_data.begin() + m_size);
    }

private:
    std::array<std::uint8_t, MaxCapacity> m_data {};
    std::uint8_t m_size { 0U };
};

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
    Everest = 0x75U,
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
    Everest = 0x75U,
    TimeMacro = 0x77U
};

/// @enum AuxSubOpcode
/// @brief Sub-opcodes for Auxiliary commands (opcodes 0x09/0x0B, Command 1 byte).
enum class AuxSubOpcode : std::uint8_t { Relay = 0x00U, Led = 0x01U };

/// @enum AuxLedColor
/// @brief Spectra IV two-color indicator LED color codes (opcode 0x09/0x0B with sub 0x01).
enum class AuxLedColor : std::uint8_t { Off = 0x00U, Amber = 0xFCU, Red = 0xFDU, Green = 0xFEU };

/// @enum EverestSubOpcode
/// @brief Sub-opcodes for Everest Macro (opcode 0x75, Command 1 byte).
enum class EverestSubOpcode : std::uint8_t {
    QueryAzimuthZero = 0x00U,
    AzimuthZeroResponse = 0x01U,
    SetZoomLimit = 0x02U,
    QueryZoomLimit = 0x03U,
    ZoomLimitResponse = 0x04U,
    QueryAlarms = 0x05U,
    AlarmsResponse = 0x06U,
    DeletePattern = 0x07U,
    SetManualLeftPanLimit = 0x08U,
    SetManualRightPanLimit = 0x09U,
    SetScanLeftPanLimit = 0x0AU,
    SetScanRightPanLimit = 0x0BU,
    QueryLimit = 0x0CU,
    LimitResponse = 0x0DU,
    EnableLimits = 0x0EU,
    QueryDefinedPresets = 0x0FU,
    DefinedPresetsResponse = 0x10U,
    QueryDefinedPatterns = 0x11U,
    DefinedPatternsResponse = 0x12U
};

/// @enum EverestLimitId
/// @brief Limit identifier for Everest limit query (opcode 0x75, sub 0x0C).
enum class EverestLimitId : std::uint8_t {
    ManualLeftPan = 0x00U,
    ManualRightPan = 0x01U,
    ScanLeftPan = 0x02U,
    ScanRightPan = 0x03U
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

/// @enum SendResult
/// @brief Status return code for command submission across session and queue.
/// @details Marked [[nodiscard]] to enforce caller handling of safety and transport outcomes.
enum class [[nodiscard]] SendResult : std::uint8_t {
    Success = 0U, ///< Frame accepted and queued/transmitted successfully
    TransportClosed, ///< Transport link is down or not opened
    QueueFull, ///< Paced command queue capacity exceeded
    InvalidFrame, ///< Frame failed validation or checksum check
    MotionPurged ///< Replaced by newer motion generation or preempted
};

/// @brief Converts a SendResult code to human-readable string view.
/// @param[in] result Outcome status code.
/// @return String representation suitable for logging.
[[nodiscard]] constexpr std::string_view sendResultToString(SendResult result) noexcept
{
    switch (result) {
    case SendResult::Success:
        return "Success";
    case SendResult::TransportClosed:
        return "TransportClosed";
    case SendResult::QueueFull:
        return "QueueFull";
    case SendResult::InvalidFrame:
        return "InvalidFrame";
    case SendResult::MotionPurged:
        return "MotionPurged";
    default:
        return "Unknown";
    }
}

/// @enum QueryKind
/// @brief Strongly-typed enumeration of all recognized Pelco-D query response categories.
/// @details Eliminates stringly-typed query correlation defects (Finding A2) by enforcing
///          compile-time safety, bounded categories, and exact opcode dispatch.
enum class QueryKind : std::uint16_t {
    None = 0x0000U, ///< No query expected (unsolicited command / motion frame).
    Pan = 0x0001U, ///< Standard Pan position query (opcode 0x59).
    Tilt = 0x0002U, ///< Standard Tilt position query (opcode 0x5B).
    Zoom = 0x0003U, ///< Standard Zoom position query (opcode 0x5D).
    Magnification = 0x0004U, ///< Standard Magnification query (opcode 0x63).
    DeviceType = 0x0005U, ///< Device type query (opcode 0x6D).
    Diagnostics = 0x0006U, ///< Camera diagnostics query (opcode 0x71).
    SoftwareVersion = 0x0007U, ///< Software version query (opcode 0x73, sub 0x00).
    BuildNumber = 0x0008U, ///< Build number query (opcode 0x73, sub 0x01).
    Time = 0x0009U, ///< Time Macro query (opcode 0x77).
    AzimuthZero = 0x000AU, ///< Everest Azimuth zero query (opcode 0x75, sub 0x00).
    ZoomLimit = 0x000BU, ///< Everest Zoom limit query (opcode 0x75, sub 0x01).
    EverestAlarms = 0x000CU, ///< Everest Alarms query (opcode 0x75, sub 0x02).
    Limit = 0x000DU, ///< Everest Limit query (opcode 0x75, sub 0x03).
    DefinedPresets = 0x000EU, ///< Everest Defined Presets query (opcode 0x75, sub 0x04).
    DefinedPatterns = 0x000FU, ///< Everest Defined Patterns query (opcode 0x75, sub 0x05).
    EverestGeneric = 0x0010U, ///< Generic Everest query (opcode 0x75).
    General = 0x0011U, ///< Extended 18-byte general query response.
    SerialNumber = 0x0012U, ///< Serial number query (18-byte general response).
    AnyStandard = 0x00F0U, ///< Explicit wildcard matching any standard query response.
    Custom = 0x00FFU ///< Custom / Vendor extension query (e.g. Fujinon).
};

/// @brief Converts a QueryKind enumeration to its canonical string representation.
/// @param[in] kind Query enumeration value.
/// @return String view of the canonical query tag name.
[[nodiscard]] constexpr std::string_view toQueryTagName(QueryKind kind) noexcept
{
    switch (kind) {
    case QueryKind::Pan:
        return "QueryPan";
    case QueryKind::Tilt:
        return "QueryTilt";
    case QueryKind::Zoom:
        return "QueryZoom";
    case QueryKind::Magnification:
        return "QueryMagnification";
    case QueryKind::DeviceType:
        return "QueryDeviceType";
    case QueryKind::Diagnostics:
        return "QueryDiagnostics";
    case QueryKind::SoftwareVersion:
        return "QuerySoftwareVersion";
    case QueryKind::BuildNumber:
        return "QueryBuildNumber";
    case QueryKind::Time:
        return "QueryTime";
    case QueryKind::AzimuthZero:
        return "QueryAzimuthZero";
    case QueryKind::ZoomLimit:
        return "QueryZoomLimit";
    case QueryKind::EverestAlarms:
        return "QueryEverestAlarms";
    case QueryKind::Limit:
        return "QueryLimit";
    case QueryKind::DefinedPresets:
        return "QueryDefinedPresets";
    case QueryKind::DefinedPatterns:
        return "QueryDefinedPatterns";
    case QueryKind::EverestGeneric:
        return "QueryEverest";
    case QueryKind::General:
        return "QueryGeneral";
    case QueryKind::SerialNumber:
        return "QuerySerialNumber";
    case QueryKind::AnyStandard:
        return "GenericQuery";
    case QueryKind::Custom:
        return "Custom";
    case QueryKind::None:
    default:
        return "";
    }
}

/// @brief Parses a string tag into its corresponding QueryKind enumeration value.
/// @details Handles legacy query tags, active RTT probe prefixes ("RttProbe:"),
///          and explicit wildcards ("GenericQuery"). Unknown tags return QueryKind::Custom
///          (if non-empty) or QueryKind::None (if empty).
/// @param[in] tag Query tag string view.
/// @return Resolved QueryKind enumeration value.
[[nodiscard]] QueryKind parseQueryKind(std::string_view tag) noexcept;

/// @brief Validates whether a QueryKind corresponds to an active query expectation.
/// @param[in] kind Query enumeration value.
/// @return True if kind represents an active query expectation.
[[nodiscard]] constexpr bool isQueryExpected(QueryKind kind) noexcept
{
    return kind != QueryKind::None;
}

} // namespace PelcoD
