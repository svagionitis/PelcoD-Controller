/// @file ProtocolParser.cpp
/// @brief Implementation of Pelco-D protocol response packet parser.

#include "ProtocolParser.h"
#include "PelcoDFrame.h"

#include <cctype>
#include <chrono>
#include <iomanip>
#include <optional>
#include <sstream>

namespace PelcoD {

namespace {

[[nodiscard]] constexpr std::optional<ResponseOpcode> toResponseOpcode(std::uint8_t val) noexcept
{
    switch (val) {
    case static_cast<std::uint8_t>(ResponseOpcode::StandardExtended):
        return ResponseOpcode::StandardExtended;
    case static_cast<std::uint8_t>(ResponseOpcode::QueryPan):
        return ResponseOpcode::QueryPan;
    case static_cast<std::uint8_t>(ResponseOpcode::QueryTilt):
        return ResponseOpcode::QueryTilt;
    case static_cast<std::uint8_t>(ResponseOpcode::QueryZoom):
        return ResponseOpcode::QueryZoom;
    case static_cast<std::uint8_t>(ResponseOpcode::QueryMagnification):
        return ResponseOpcode::QueryMagnification;
    case static_cast<std::uint8_t>(ResponseOpcode::QueryDeviceType):
        return ResponseOpcode::QueryDeviceType;
    case static_cast<std::uint8_t>(ResponseOpcode::QueryDiagnostics):
        return ResponseOpcode::QueryDiagnostics;
    case static_cast<std::uint8_t>(ResponseOpcode::VersionInfo):
        return ResponseOpcode::VersionInfo;
    case static_cast<std::uint8_t>(ResponseOpcode::Everest):
        return ResponseOpcode::Everest;
    case static_cast<std::uint8_t>(ResponseOpcode::TimeMacro):
        return ResponseOpcode::TimeMacro;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] constexpr std::optional<VersionInfoSubOpcode> toVersionInfoSubOpcode(std::uint8_t val) noexcept
{
    switch (val) {
    case static_cast<std::uint8_t>(VersionInfoSubOpcode::RequestSoftwareVersion):
        return VersionInfoSubOpcode::RequestSoftwareVersion;
    case static_cast<std::uint8_t>(VersionInfoSubOpcode::SoftwareVersionResponse):
        return VersionInfoSubOpcode::SoftwareVersionResponse;
    case static_cast<std::uint8_t>(VersionInfoSubOpcode::RequestBuildNumber):
        return VersionInfoSubOpcode::RequestBuildNumber;
    case static_cast<std::uint8_t>(VersionInfoSubOpcode::BuildNumberResponse):
        return VersionInfoSubOpcode::BuildNumberResponse;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] constexpr std::optional<TimeSubOpcode> toTimeSubOpcode(std::uint8_t val) noexcept
{
    switch (val) {
    case static_cast<std::uint8_t>(TimeSubOpcode::SetSeconds):
        return TimeSubOpcode::SetSeconds;
    case static_cast<std::uint8_t>(TimeSubOpcode::ReportSeconds):
        return TimeSubOpcode::ReportSeconds;
    case static_cast<std::uint8_t>(TimeSubOpcode::SetHourMinute):
        return TimeSubOpcode::SetHourMinute;
    case static_cast<std::uint8_t>(TimeSubOpcode::ReportHourMinute):
        return TimeSubOpcode::ReportHourMinute;
    case static_cast<std::uint8_t>(TimeSubOpcode::SetMonthDay):
        return TimeSubOpcode::SetMonthDay;
    case static_cast<std::uint8_t>(TimeSubOpcode::ReportMonthDay):
        return TimeSubOpcode::ReportMonthDay;
    case static_cast<std::uint8_t>(TimeSubOpcode::SetYear):
        return TimeSubOpcode::SetYear;
    case static_cast<std::uint8_t>(TimeSubOpcode::ReportYear):
        return TimeSubOpcode::ReportYear;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] constexpr std::optional<EverestSubOpcode> toEverestSubOpcode(std::uint8_t val) noexcept
{
    switch (val) {
    case static_cast<std::uint8_t>(EverestSubOpcode::QueryAzimuthZero):
        return EverestSubOpcode::QueryAzimuthZero;
    case static_cast<std::uint8_t>(EverestSubOpcode::AzimuthZeroResponse):
        return EverestSubOpcode::AzimuthZeroResponse;
    case static_cast<std::uint8_t>(EverestSubOpcode::SetZoomLimit):
        return EverestSubOpcode::SetZoomLimit;
    case static_cast<std::uint8_t>(EverestSubOpcode::QueryZoomLimit):
        return EverestSubOpcode::QueryZoomLimit;
    case static_cast<std::uint8_t>(EverestSubOpcode::ZoomLimitResponse):
        return EverestSubOpcode::ZoomLimitResponse;
    case static_cast<std::uint8_t>(EverestSubOpcode::QueryAlarms):
        return EverestSubOpcode::QueryAlarms;
    case static_cast<std::uint8_t>(EverestSubOpcode::AlarmsResponse):
        return EverestSubOpcode::AlarmsResponse;
    case static_cast<std::uint8_t>(EverestSubOpcode::DeletePattern):
        return EverestSubOpcode::DeletePattern;
    case static_cast<std::uint8_t>(EverestSubOpcode::SetManualLeftPanLimit):
        return EverestSubOpcode::SetManualLeftPanLimit;
    case static_cast<std::uint8_t>(EverestSubOpcode::SetManualRightPanLimit):
        return EverestSubOpcode::SetManualRightPanLimit;
    case static_cast<std::uint8_t>(EverestSubOpcode::SetScanLeftPanLimit):
        return EverestSubOpcode::SetScanLeftPanLimit;
    case static_cast<std::uint8_t>(EverestSubOpcode::SetScanRightPanLimit):
        return EverestSubOpcode::SetScanRightPanLimit;
    case static_cast<std::uint8_t>(EverestSubOpcode::QueryLimit):
        return EverestSubOpcode::QueryLimit;
    case static_cast<std::uint8_t>(EverestSubOpcode::LimitResponse):
        return EverestSubOpcode::LimitResponse;
    case static_cast<std::uint8_t>(EverestSubOpcode::EnableLimits):
        return EverestSubOpcode::EnableLimits;
    case static_cast<std::uint8_t>(EverestSubOpcode::QueryDefinedPresets):
        return EverestSubOpcode::QueryDefinedPresets;
    case static_cast<std::uint8_t>(EverestSubOpcode::DefinedPresetsResponse):
        return EverestSubOpcode::DefinedPresetsResponse;
    case static_cast<std::uint8_t>(EverestSubOpcode::QueryDefinedPatterns):
        return EverestSubOpcode::QueryDefinedPatterns;
    case static_cast<std::uint8_t>(EverestSubOpcode::DefinedPatternsResponse):
        return EverestSubOpcode::DefinedPatternsResponse;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] constexpr std::optional<CommandOpcode> toCommandOpcode(std::uint8_t val) noexcept
{
    switch (val) {
    case static_cast<std::uint8_t>(CommandOpcode::SetPreset):
        return CommandOpcode::SetPreset;
    case static_cast<std::uint8_t>(CommandOpcode::ClearPreset):
        return CommandOpcode::ClearPreset;
    case static_cast<std::uint8_t>(CommandOpcode::GoToPreset):
        return CommandOpcode::GoToPreset;
    case static_cast<std::uint8_t>(CommandOpcode::SetAuxiliary):
        return CommandOpcode::SetAuxiliary;
    case static_cast<std::uint8_t>(CommandOpcode::ClearAuxiliary):
        return CommandOpcode::ClearAuxiliary;
    case static_cast<std::uint8_t>(CommandOpcode::Dummy):
        return CommandOpcode::Dummy;
    case static_cast<std::uint8_t>(CommandOpcode::RemoteReset):
        return CommandOpcode::RemoteReset;
    case static_cast<std::uint8_t>(CommandOpcode::SetZoneStart):
        return CommandOpcode::SetZoneStart;
    case static_cast<std::uint8_t>(CommandOpcode::SetZoneEnd):
        return CommandOpcode::SetZoneEnd;
    case static_cast<std::uint8_t>(CommandOpcode::WriteCharacter):
        return CommandOpcode::WriteCharacter;
    case static_cast<std::uint8_t>(CommandOpcode::ClearScreen):
        return CommandOpcode::ClearScreen;
    case static_cast<std::uint8_t>(CommandOpcode::AlarmAcknowledge):
        return CommandOpcode::AlarmAcknowledge;
    case static_cast<std::uint8_t>(CommandOpcode::ZoneScanOn):
        return CommandOpcode::ZoneScanOn;
    case static_cast<std::uint8_t>(CommandOpcode::ZoneScanOff):
        return CommandOpcode::ZoneScanOff;
    case static_cast<std::uint8_t>(CommandOpcode::RecordPatternStart):
        return CommandOpcode::RecordPatternStart;
    case static_cast<std::uint8_t>(CommandOpcode::RecordPatternStop):
        return CommandOpcode::RecordPatternStop;
    case static_cast<std::uint8_t>(CommandOpcode::RunPattern):
        return CommandOpcode::RunPattern;
    case static_cast<std::uint8_t>(CommandOpcode::SetZoomSpeed):
        return CommandOpcode::SetZoomSpeed;
    case static_cast<std::uint8_t>(CommandOpcode::SetFocusSpeed):
        return CommandOpcode::SetFocusSpeed;
    case static_cast<std::uint8_t>(CommandOpcode::ResetDefaults):
        return CommandOpcode::ResetDefaults;
    case static_cast<std::uint8_t>(CommandOpcode::AutoFocus):
        return CommandOpcode::AutoFocus;
    case static_cast<std::uint8_t>(CommandOpcode::AutoIris):
        return CommandOpcode::AutoIris;
    case static_cast<std::uint8_t>(CommandOpcode::Agc):
        return CommandOpcode::Agc;
    case static_cast<std::uint8_t>(CommandOpcode::BacklightComp):
        return CommandOpcode::BacklightComp;
    case static_cast<std::uint8_t>(CommandOpcode::AutoWhiteBalance):
        return CommandOpcode::AutoWhiteBalance;
    case static_cast<std::uint8_t>(CommandOpcode::PhaseDelayMode):
        return CommandOpcode::PhaseDelayMode;
    case static_cast<std::uint8_t>(CommandOpcode::SetShutterSpeed):
        return CommandOpcode::SetShutterSpeed;
    case static_cast<std::uint8_t>(CommandOpcode::AdjustLineLock):
        return CommandOpcode::AdjustLineLock;
    case static_cast<std::uint8_t>(CommandOpcode::AdjustWbRedBlue):
        return CommandOpcode::AdjustWbRedBlue;
    case static_cast<std::uint8_t>(CommandOpcode::AdjustWbMg):
        return CommandOpcode::AdjustWbMg;
    case static_cast<std::uint8_t>(CommandOpcode::AdjustGain):
        return CommandOpcode::AdjustGain;
    case static_cast<std::uint8_t>(CommandOpcode::AdjustAutoIrisLevel):
        return CommandOpcode::AdjustAutoIrisLevel;
    case static_cast<std::uint8_t>(CommandOpcode::AdjustAutoIrisPeak):
        return CommandOpcode::AdjustAutoIrisPeak;
    case static_cast<std::uint8_t>(CommandOpcode::Query):
        return CommandOpcode::Query;
    case static_cast<std::uint8_t>(CommandOpcode::PresetScan):
        return CommandOpcode::PresetScan;
    case static_cast<std::uint8_t>(CommandOpcode::SetZeroPosition):
        return CommandOpcode::SetZeroPosition;
    case static_cast<std::uint8_t>(CommandOpcode::SetPanPosition):
        return CommandOpcode::SetPanPosition;
    case static_cast<std::uint8_t>(CommandOpcode::SetTiltPosition):
        return CommandOpcode::SetTiltPosition;
    case static_cast<std::uint8_t>(CommandOpcode::SetZoomPosition):
        return CommandOpcode::SetZoomPosition;
    case static_cast<std::uint8_t>(CommandOpcode::QueryPanPosition):
        return CommandOpcode::QueryPanPosition;
    case static_cast<std::uint8_t>(CommandOpcode::QueryTiltPosition):
        return CommandOpcode::QueryTiltPosition;
    case static_cast<std::uint8_t>(CommandOpcode::QueryZoomPosition):
        return CommandOpcode::QueryZoomPosition;
    case static_cast<std::uint8_t>(CommandOpcode::PrepareForDownload):
        return CommandOpcode::PrepareForDownload;
    case static_cast<std::uint8_t>(CommandOpcode::SetMagnification):
        return CommandOpcode::SetMagnification;
    case static_cast<std::uint8_t>(CommandOpcode::QueryMagnification):
        return CommandOpcode::QueryMagnification;
    case static_cast<std::uint8_t>(CommandOpcode::EchoMode):
        return CommandOpcode::EchoMode;
    case static_cast<std::uint8_t>(CommandOpcode::SetBaudRate):
        return CommandOpcode::SetBaudRate;
    case static_cast<std::uint8_t>(CommandOpcode::StartDownload):
        return CommandOpcode::StartDownload;
    case static_cast<std::uint8_t>(CommandOpcode::QueryDeviceType):
        return CommandOpcode::QueryDeviceType;
    case static_cast<std::uint8_t>(CommandOpcode::QueryDiagnostics):
        return CommandOpcode::QueryDiagnostics;
    case static_cast<std::uint8_t>(CommandOpcode::VersionInfo):
        return CommandOpcode::VersionInfo;
    case static_cast<std::uint8_t>(CommandOpcode::Everest):
        return CommandOpcode::Everest;
    case static_cast<std::uint8_t>(CommandOpcode::TimeMacro):
        return CommandOpcode::TimeMacro;
    case static_cast<std::uint8_t>(CommandOpcode::ScreenMove):
        return CommandOpcode::ScreenMove;
    default:
        return std::nullopt;
    }
}

} // namespace

bool ProtocolParser::parseGeneral(
    const std::vector<std::uint8_t>& frame, std::uint8_t& address, std::uint8_t& alarms) noexcept
{
    if (!PelcoDFrame::isValidFrame(frame) || frame.size() != PelcoDFrame::GeneralResponseSize) {
        return false;
    }
    address = frame[1];
    alarms = frame[2];
    return true;
}

bool ProtocolParser::parsePan(const std::vector<std::uint8_t>& frame, std::uint16_t& panCentidegrees) noexcept
{
    if (!PelcoDFrame::isValidFrame(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[3] != static_cast<std::uint8_t>(ResponseOpcode::QueryPan)) {
        return false;
    }
    panCentidegrees = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(frame[4]) << 8U) | static_cast<std::uint16_t>(frame[5]));
    return true;
}

bool ProtocolParser::parseTilt(const std::vector<std::uint8_t>& frame, std::uint16_t& tiltCentidegrees) noexcept
{
    if (!PelcoDFrame::isValidFrame(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[3] != static_cast<std::uint8_t>(ResponseOpcode::QueryTilt)) {
        return false;
    }
    tiltCentidegrees = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(frame[4]) << 8U) | static_cast<std::uint16_t>(frame[5]));
    return true;
}

bool ProtocolParser::parseZoom(const std::vector<std::uint8_t>& frame, std::uint16_t& zoomPosition) noexcept
{
    if (!PelcoDFrame::isValidFrame(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[3] != static_cast<std::uint8_t>(ResponseOpcode::QueryZoom)) {
        return false;
    }
    zoomPosition = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(frame[4]) << 8U) | static_cast<std::uint16_t>(frame[5]));
    return true;
}

bool ProtocolParser::parseMag(const std::vector<std::uint8_t>& frame, std::uint16_t& magnification) noexcept
{
    if (!PelcoDFrame::isValidFrame(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[3] != static_cast<std::uint8_t>(ResponseOpcode::QueryMagnification)) {
        return false;
    }
    magnification = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(frame[4]) << 8U) | static_cast<std::uint16_t>(frame[5]));
    return true;
}

bool ProtocolParser::parseDevType(
    const std::vector<std::uint8_t>& frame, std::uint8_t& swType, std::uint8_t& hwType) noexcept
{
    if (!PelcoDFrame::isValidFrame(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[3] != static_cast<std::uint8_t>(ResponseOpcode::QueryDeviceType)) {
        return false;
    }
    swType = frame[4];
    hwType = frame[5];
    return true;
}

bool ProtocolParser::parseAck(const std::vector<std::uint8_t>& frame, std::uint8_t& echoOpcode, bool& ack) noexcept
{
    if (!PelcoDFrame::isValidFrame(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[3] != static_cast<std::uint8_t>(ResponseOpcode::StandardExtended)) {
        return false;
    }
    echoOpcode = frame[4];
    ack = (frame[5] == 0x01U); // 0x01 = ACK, 0x00 = NAK
    return true;
}

bool ProtocolParser::parseDiagnostics(
    const std::vector<std::uint8_t>& frame, std::uint8_t& temp, std::uint8_t& sensorId) noexcept
{
    if (!PelcoDFrame::isValidFrame(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[3] != static_cast<std::uint8_t>(ResponseOpcode::QueryDiagnostics)) {
        return false;
    }
    temp = frame[4];
    sensorId = frame[5];
    return true;
}

bool ProtocolParser::parseVersionInfo(const std::vector<std::uint8_t>& frame,
    VersionInfoSubOpcode& subOpcode, std::uint8_t& data1, std::uint8_t& data2) noexcept
{
    if (!PelcoDFrame::isValidFrame(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[3] != static_cast<std::uint8_t>(ResponseOpcode::VersionInfo)) {
        return false;
    }
    const auto maybeSub = toVersionInfoSubOpcode(frame[2]);
    if (!maybeSub.has_value()) {
        return false;
    }
    subOpcode = *maybeSub;
    data1 = frame[4];
    data2 = frame[5];
    return true;
}

bool ProtocolParser::parseTimeResponse(const std::vector<std::uint8_t>& frame,
    TimeSubOpcode& subOpcode, std::uint8_t& data1, std::uint8_t& data2) noexcept
{
    if (!PelcoDFrame::isValidFrame(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[3] != static_cast<std::uint8_t>(ResponseOpcode::TimeMacro)) {
        return false;
    }
    const auto maybeSub = toTimeSubOpcode(frame[2]);
    if (!maybeSub.has_value()) {
        return false;
    }
    subOpcode = *maybeSub;
    data1 = frame[4];
    data2 = frame[5];
    return true;
}

bool ProtocolParser::parseEverestResponse(const std::vector<std::uint8_t>& frame,
    EverestSubOpcode& subOpcode, std::uint8_t& data1, std::uint8_t& data2) noexcept
{
    if (!PelcoDFrame::isValidFrame(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[3] != static_cast<std::uint8_t>(ResponseOpcode::Everest)) {
        return false;
    }
    const auto maybeSub = toEverestSubOpcode(frame[2]);
    if (!maybeSub.has_value()) {
        return false;
    }
    subOpcode = *maybeSub;
    data1 = frame[4];
    data2 = frame[5];
    return true;
}

bool ProtocolParser::parseLimitResponse(
    const std::vector<std::uint8_t>& frame, std::uint16_t& limitCentidegrees) noexcept
{
    if (!PelcoDFrame::isValidFrame(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[3] != static_cast<std::uint8_t>(ResponseOpcode::Everest)
        || frame[2] != static_cast<std::uint8_t>(EverestSubOpcode::LimitResponse)) {
        return false;
    }
    limitCentidegrees = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(frame[4]) << 8U) | static_cast<std::uint16_t>(frame[5]));
    return true;
}

bool ProtocolParser::parseQuery(const std::vector<std::uint8_t>& frame, std::string& payload)
{
    if (!PelcoDFrame::isValidFrame(frame) || frame.size() != PelcoDFrame::QueryResponseSize) {
        return false;
    }

    payload.clear();
    for (std::size_t i { 2U }; i < 17U; ++i) {
        const std::uint8_t byte = frame[i];
        if (byte == 0x00U) {
            break;
        }
        if (std::isprint(static_cast<unsigned char>(byte))) {
            payload.push_back(static_cast<char>(byte));
        }
    }
    return true;
}

bool ProtocolParser::updateStatus(
    const std::vector<std::uint8_t>& frame,
    DeviceStatus& status,
    DeviceInfo& info,
    [[maybe_unused]] std::optional<EverestLimitId> limitId)
{
    if (!PelcoDFrame::isValidFrame(frame)) {
        return false;
    }

    status.lastRxTime = std::chrono::steady_clock::now();

    if (frame.size() == PelcoDFrame::GeneralResponseSize) {
        std::uint8_t addr { 0U };
        std::uint8_t alarms { 0U };
        if (parseGeneral(frame, addr, alarms)) {
            status.address = addr;
            status.alarms = alarms;
            return true;
        }
    } else if (frame.size() == PelcoDFrame::StandardFrameSize) {
        status.address = frame[1];
        const auto maybeOpcode = toResponseOpcode(frame[3]);
        if (!maybeOpcode.has_value()) {
            return false;
        }
        const auto opcode = *maybeOpcode;
        switch (opcode) {
        case ResponseOpcode::StandardExtended: {
            std::uint8_t echoOpcode { 0U };
            bool ack { false };
            if (parseAck(frame, echoOpcode, ack)) {
                status.lastAckOk = ack;
                status.lastAckOpcode = echoOpcode;
                return true;
            }
            return false;
        }
        case ResponseOpcode::QueryPan:
            return parsePan(frame, status.panCentidegrees);

        case ResponseOpcode::QueryTilt:
            return parseTilt(frame, status.tiltCentidegrees);

        case ResponseOpcode::QueryZoom:
            return parseZoom(frame, status.zoomPosition);

        case ResponseOpcode::QueryMagnification:
            return parseMag(frame, status.magnification);

        case ResponseOpcode::QueryDeviceType:
            return parseDevType(frame, info.softwareType, info.hardwareType);

        case ResponseOpcode::QueryDiagnostics:
            return parseDiagnostics(frame, status.diagnosticTemp, status.diagnosticSensorId);

        case ResponseOpcode::VersionInfo: {
            VersionInfoSubOpcode sub { VersionInfoSubOpcode::RequestSoftwareVersion };
            std::uint8_t d1 { 0U };
            std::uint8_t d2 { 0U };
            if (parseVersionInfo(frame, sub, d1, d2)) {
                if (sub == VersionInfoSubOpcode::SoftwareVersionResponse) {
                    info.softwareMajor = d1;
                    info.softwareMinor = d2;
                    return true;
                }
                if (sub == VersionInfoSubOpcode::BuildNumberResponse) {
                    info.buildNumber = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
                    return true;
                }
            }
            return false;
        }

        case ResponseOpcode::TimeMacro: {
            TimeSubOpcode sub {};
            std::uint8_t d1 { 0U };
            std::uint8_t d2 { 0U };
            if (parseTimeResponse(frame, sub, d1, d2)) {
                switch (sub) {
                case TimeSubOpcode::ReportSeconds:
                    status.deviceTime.second = d2;
                    return true;
                case TimeSubOpcode::ReportHourMinute:
                    status.deviceTime.hour = d1;
                    status.deviceTime.minute = d2;
                    return true;
                case TimeSubOpcode::ReportMonthDay:
                    status.deviceTime.month = d1;
                    status.deviceTime.day = d2;
                    return true;
                case TimeSubOpcode::ReportYear:
                    status.deviceTime.year = static_cast<std::uint16_t>(
                        (static_cast<std::uint16_t>(d1) << 8U) | static_cast<std::uint16_t>(d2));
                    return true;
                default:
                    return false;
                }
            }
            return false;
        }

        case ResponseOpcode::Everest: {
            EverestSubOpcode sub {};
            std::uint8_t d1 { 0U };
            std::uint8_t d2 { 0U };
            if (parseEverestResponse(frame, sub, d1, d2)) {
                const auto val16 = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
                switch (sub) {
                case EverestSubOpcode::AzimuthZeroResponse:
                    status.azimuthZeroOffsetCentidegrees = val16;
                    return true;
                case EverestSubOpcode::ZoomLimitResponse:
                    status.zoomLimit = val16;
                    return true;
                case EverestSubOpcode::AlarmsResponse:
                    status.everestAlarms = d2;
                    return true;
                case EverestSubOpcode::LimitResponse:
                    status.lastLimitCentidegrees = val16;
                    if (limitId.has_value()) {
                        switch (*limitId) {
                        case EverestLimitId::ManualLeftPan:
                            status.manualLeftLimitCentidegrees = val16;
                            break;
                        case EverestLimitId::ManualRightPan:
                            status.manualRightLimitCentidegrees = val16;
                            break;
                        case EverestLimitId::ScanLeftPan:
                            status.scanLeftLimitCentidegrees = val16;
                            break;
                        case EverestLimitId::ScanRightPan:
                            status.scanRightLimitCentidegrees = val16;
                            break;
                        }
                    }
                    return true;
                case EverestSubOpcode::DefinedPresetsResponse:
                    status.definedPresetsMask = val16;
                    return true;
                case EverestSubOpcode::DefinedPatternsResponse:
                    status.definedPatternsMask = val16;
                    return true;
                default:
                    return true;
                }
            }
            return false;
        }

        default:
            return false;
        }
    } else if (frame.size() == PelcoDFrame::QueryResponseSize) {
        status.address = frame[1];
        std::string payload;
        if (parseQuery(frame, payload)) {
            info.modelName = payload;
            return true;
        }
    }

    return false;
}

static std::string toHexByte(std::uint8_t val)
{
    return PelcoDFrame::toHexString(&val, 1U, '\0');
}

std::string ProtocolParser::describeFrame(bool isTx, const std::vector<std::uint8_t>& frame)
{
    if (frame.empty()) {
        return "Empty";
    }

    if (frame.size() == PelcoDFrame::GeneralResponseSize) {
        return "General Response (Addr " + std::to_string(frame[1]) + ", Alarms: 0x" + toHexByte(frame[2]) + ")";
    }

    if (frame.size() == PelcoDFrame::QueryResponseSize) {
        std::string payload;
        for (std::size_t i { 2U }; i < 17U; ++i) {
            const std::uint8_t b = frame[i];
            if (b == 0U) {
                continue;
            }
            if (b >= 0x20U && b <= 0x7EU) {
                payload.push_back(static_cast<char>(b));
            } else {
                payload += "\\x" + toHexByte(b);
            }
        }
        return "Query Response (Addr " + std::to_string(frame[1]) + "): \"" + payload + "\"";
    }

    if (frame.size() == PelcoDFrame::StandardFrameSize) {
        const std::uint8_t cmd1 = frame[2];
        const std::uint8_t cmd2 = frame[3];
        const std::uint8_t d1 = frame[4];
        const std::uint8_t d2 = frame[5];

        if (isTx) {
            if ((cmd2 & 0x01U) == 0U) {
                std::vector<std::string> acts;
                if ((cmd2 & 0x02U) != 0U) {
                    acts.push_back("Right(spd " + std::to_string(d1) + ")");
                }
                if ((cmd2 & 0x04U) != 0U) {
                    acts.push_back("Left(spd " + std::to_string(d1) + ")");
                }
                if ((cmd2 & 0x08U) != 0U) {
                    acts.push_back("Up(spd " + std::to_string(d2) + ")");
                }
                if ((cmd2 & 0x10U) != 0U) {
                    acts.push_back("Down(spd " + std::to_string(d2) + ")");
                }
                if ((cmd2 & 0x20U) != 0U) {
                    acts.push_back("ZoomTele");
                }
                if ((cmd2 & 0x40U) != 0U) {
                    acts.push_back("ZoomWide");
                }
                if ((cmd1 & 0x01U) != 0U) {
                    acts.push_back("FocusNear");
                }
                if ((cmd2 & 0x80U) != 0U) {
                    acts.push_back("FocusFar");
                }
                if ((cmd1 & 0x02U) != 0U) {
                    acts.push_back("IrisOpen");
                }
                if ((cmd1 & 0x04U) != 0U) {
                    acts.push_back("IrisClose");
                }
                if (acts.empty()) {
                    if (cmd1 == static_cast<std::uint8_t>(ScanSense::DeviceOn)) {
                        return "Power On";
                    }
                    if (cmd1 == static_cast<std::uint8_t>(ScanSense::DeviceOff)) {
                        return "Power Off";
                    }
                    if (cmd1 == static_cast<std::uint8_t>(ScanSense::AutoScanOn)) {
                        return "Auto Scan On";
                    }
                    if (cmd1 == static_cast<std::uint8_t>(ScanSense::ManualScanOn)) {
                        return "Manual Scan On";
                    }
                    if (cmd1 != 0x00U) {
                        return "Camera Command (Cmd1=0x" + toHexByte(cmd1) + ")";
                    }
                    return "PTZ Stop";
                }
                std::string result = "PTZ: ";
                for (std::size_t i { 0U }; i < acts.size(); ++i) {
                    if (i > 0U) {
                        result += ", ";
                    }
                    result += acts[i];
                }
                return result;
            }

            // Extended command
            const auto maybeOpcode = toCommandOpcode(cmd2);
            if (!maybeOpcode.has_value()) {
                return "Extended Command (Cmd2=0x" + toHexByte(cmd2) + ")";
            }
            switch (*maybeOpcode) {
            case CommandOpcode::SetPreset:
                return "Set Preset " + std::to_string(d2);
            case CommandOpcode::ClearPreset:
                return "Clear Preset " + std::to_string(d2);
            case CommandOpcode::GoToPreset:
                return "GoTo Preset " + std::to_string(d2);
            case CommandOpcode::SetAuxiliary:
                if (cmd1 == static_cast<std::uint8_t>(AuxSubOpcode::Led)) {
                    return "Set Aux LED (ID/Color=0x" + toHexByte(d2) + ", Rate=" + std::to_string(d1) + ")";
                }
                return "Set Aux " + std::to_string(d2) + " ON";
            case CommandOpcode::ClearAuxiliary:
                if (cmd1 == static_cast<std::uint8_t>(AuxSubOpcode::Led)) {
                    return "Clear Aux LED (ID/Color=0x" + toHexByte(d2) + ", Rate=" + std::to_string(d1) + ")";
                }
                return "Clear Aux " + std::to_string(d2) + " OFF";
            case CommandOpcode::Dummy:
                return "Dummy / Ping";
            case CommandOpcode::RemoteReset:
                return "Remote Reset";
            case CommandOpcode::SetZoneStart:
                return "Set Zone " + std::to_string(d2) + " Start";
            case CommandOpcode::SetZoneEnd:
                return "Set Zone " + std::to_string(d2) + " End";
            case CommandOpcode::WriteCharacter:
                return std::string("Write Char '") + static_cast<char>(d2) + "' at Col " + std::to_string(d1);
            case CommandOpcode::ClearScreen:
                return "Clear Screen";
            case CommandOpcode::AlarmAcknowledge:
                return "Alarm Acknowledge (Alarm " + std::to_string(d2) + ")";
            case CommandOpcode::ZoneScanOn:
                return "Zone Scan On";
            case CommandOpcode::ZoneScanOff:
                return "Zone Scan Off";
            case CommandOpcode::RecordPatternStart:
                return "Record Pattern " + std::to_string(d2) + " Start";
            case CommandOpcode::RecordPatternStop:
                return "Record Pattern Stop";
            case CommandOpcode::RunPattern:
                return "Run Pattern " + std::to_string(d2);
            case CommandOpcode::SetZoomSpeed:
                return "Set Zoom Speed (" + std::to_string(d2) + ")";
            case CommandOpcode::SetFocusSpeed:
                return "Set Focus Speed (" + std::to_string(d2) + ")";
            case CommandOpcode::ResetDefaults:
                return "Reset Defaults";
            case CommandOpcode::AutoFocus:
                return "Auto Focus (" + std::to_string(d2) + ")";
            case CommandOpcode::AutoIris:
                return "Auto Iris (" + std::to_string(d2) + ")";
            case CommandOpcode::Agc:
                return "AGC (" + std::to_string(d2) + ")";
            case CommandOpcode::BacklightComp:
                return "Backlight Comp (" + std::to_string(d2) + ")";
            case CommandOpcode::AutoWhiteBalance:
                return "Auto White Balance (" + std::to_string(d2) + ")";
            case CommandOpcode::PhaseDelayMode:
                return "Phase Delay Mode (" + std::to_string(d2) + ")";
            case CommandOpcode::SetShutterSpeed: {
                const auto val16 = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
                return "Set Shutter Speed (" + std::to_string(val16) + ")";
            }
            case CommandOpcode::AdjustLineLock: {
                const auto val16 = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
                return "Adjust Line Lock (" + std::to_string(val16) + ")";
            }
            case CommandOpcode::AdjustWbRedBlue: {
                const auto val16 = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
                return "Adjust WB Red/Blue (" + std::to_string(val16) + ")";
            }
            case CommandOpcode::AdjustWbMg: {
                const auto val16 = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
                return "Adjust WB Magenta/Green (" + std::to_string(val16) + ")";
            }
            case CommandOpcode::AdjustGain: {
                const auto val16 = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
                return "Adjust Gain (" + std::to_string(val16) + ")";
            }
            case CommandOpcode::AdjustAutoIrisLevel:
                return "Adjust Auto Iris Level (" + std::to_string(d2) + ")";
            case CommandOpcode::AdjustAutoIrisPeak:
                return "Adjust Auto Iris Peak (" + std::to_string(d2) + ")";
            case CommandOpcode::Query:
                return "Query (General)";
            case CommandOpcode::PresetScan:
                return "Preset Scan (Dwell " + std::to_string(d2) + "s)";
            case CommandOpcode::SetZeroPosition:
                return "Set Zero Position";
            case CommandOpcode::SetPanPosition: {
                const auto val16 = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
                return "Set Pan Position (" + std::to_string(val16) + ")";
            }
            case CommandOpcode::SetTiltPosition: {
                const auto val16 = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
                return "Set Tilt Position (" + std::to_string(val16) + ")";
            }
            case CommandOpcode::SetZoomPosition: {
                const auto val16 = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
                return "Set Zoom Position (" + std::to_string(val16) + ")";
            }
            case CommandOpcode::QueryPanPosition:
                return "Query Pan Position";
            case CommandOpcode::QueryTiltPosition:
                return "Query Tilt Position";
            case CommandOpcode::QueryZoomPosition:
                return "Query Zoom Position";
            case CommandOpcode::PrepareForDownload:
                return "Prepare For Download";
            case CommandOpcode::SetMagnification: {
                const auto val16 = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
                return "Set Magnification (" + std::to_string(val16) + ")";
            }
            case CommandOpcode::QueryMagnification:
                return "Query Magnification";
            case CommandOpcode::EchoMode:
                return "Activate Echo Mode";
            case CommandOpcode::SetBaudRate:
                return "Set Baud Rate";
            case CommandOpcode::StartDownload:
                return "Start Download";
            case CommandOpcode::QueryDeviceType:
                return "Query Device Type";
            case CommandOpcode::QueryDiagnostics:
                return "Query Diagnostics";
            case CommandOpcode::ScreenMove: {
                const auto panVal = static_cast<std::int32_t>(static_cast<std::int8_t>(d1));
                const auto tiltVal = static_cast<std::int32_t>(static_cast<std::int8_t>(d2));
                const std::string modeStr = (cmd1 == 0x01U) ? "Rel" : "Abs";
                return "Screen Move (" + modeStr + ", Pan " + std::to_string(panVal) + "%, Tilt "
                    + std::to_string(tiltVal) + "%)";
            }
            case CommandOpcode::VersionInfo: {
                if (cmd1 == 0x00U) {
                    return "Query Software Version";
                }
                if (cmd1 == 0x02U) {
                    return "Query Build Number";
                }
                return "Version Info Macro (Sub 0x" + toHexByte(cmd1) + ")";
            }
            case CommandOpcode::TimeMacro: {
                switch (cmd1) {
                case 0x00U:
                    return "Set Seconds (" + std::to_string(d2) + "s)";
                case 0x01U:
                    return "Query Seconds";
                case 0x02U:
                    return "Set Hour/Minute (" + std::to_string(d1) + ":" + std::to_string(d2) + ")";
                case 0x03U:
                    return "Query Hour/Minute";
                case 0x04U:
                    return "Set Month/Day (" + std::to_string(d1) + "/" + std::to_string(d2) + ")";
                case 0x05U:
                    return "Query Month/Day";
                case 0x06U: {
                    const auto yr = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
                    return "Set Year (" + std::to_string(yr) + ")";
                }
                case 0x07U:
                    return "Query Year";
                default:
                    return "Time Command (Sub 0x" + toHexByte(cmd1) + ")";
                }
            }
            case CommandOpcode::Everest: {
                const auto val16 = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
                switch (cmd1) {
                case 0x00U:
                    return "Query Azimuth Zero Offset";
                case 0x02U:
                    return "Set Zoom Limit (" + std::to_string(val16) + ")";
                case 0x03U:
                    return "Query Zoom Limit";
                case 0x05U:
                    return "Query Alarms (Everest)";
                case 0x07U:
                    return "Delete Pattern " + std::to_string(d2);
                case 0x08U:
                    return "Set Manual Left Pan Limit (" + std::to_string(val16) + ")";
                case 0x09U:
                    return "Set Manual Right Pan Limit (" + std::to_string(val16) + ")";
                case 0x0AU:
                    return "Set Scan Left Pan Limit (" + std::to_string(val16) + ")";
                case 0x0BU:
                    return "Set Scan Right Pan Limit (" + std::to_string(val16) + ")";
                case 0x0CU:
                    return "Query Limit (ID=" + std::to_string(d2) + ")";
                case 0x0EU:
                    return "Set Limits " + std::string(d2 == 1U ? "Enabled" : "Disabled");
                case 0x0FU:
                    return "Query Defined Presets (Group " + std::to_string(d2) + ")";
                case 0x11U:
                    return "Query Defined Patterns (Group " + std::to_string(d2) + ")";
                default:
                    return "Everest Macro (Sub 0x" + toHexByte(cmd1) + ")";
                }
            }
            default:
                break;
            }
            return "Extended Command (Cmd2=0x" + toHexByte(cmd2) + ")";
        }

        // Responses (!isTx)
        if (cmd2 == 0x01U) {
            return (d2 == 0x01U) ? ("ACK (OK) for Opcode 0x" + toHexByte(d1))
                                 : ("NAK (Error) for Opcode 0x" + toHexByte(d1));
        }
        if (cmd2 == static_cast<std::uint8_t>(ResponseOpcode::QueryPan)) {
            const auto cdeg = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
            std::ostringstream oss;
            oss << "Pan Response: " << std::fixed << std::setprecision(2) << (static_cast<double>(cdeg) / 100.0) << " deg";
            return oss.str();
        }
        if (cmd2 == static_cast<std::uint8_t>(ResponseOpcode::QueryTilt)) {
            const auto cdeg = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
            std::ostringstream oss;
            oss << "Tilt Response: " << std::fixed << std::setprecision(2) << (static_cast<double>(cdeg) / 100.0) << " deg";
            return oss.str();
        }
        if (cmd2 == static_cast<std::uint8_t>(ResponseOpcode::QueryZoom)) {
            const auto pos = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
            return "Zoom Response: " + std::to_string(pos);
        }
        if (cmd2 == static_cast<std::uint8_t>(ResponseOpcode::QueryMagnification)) {
            const auto mag = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
            return "Magnification Response: " + std::to_string(mag);
        }
        if (cmd2 == static_cast<std::uint8_t>(ResponseOpcode::QueryDeviceType)) {
            return "Device Type Response: SW=0x" + toHexByte(d1) + " HW=0x" + toHexByte(d2);
        }
        if (cmd2 == static_cast<std::uint8_t>(ResponseOpcode::QueryDiagnostics)) {
            const auto temp = static_cast<std::int32_t>(static_cast<std::int8_t>(d1));
            return "Diagnostics Response: Temp=" + std::to_string(temp) + " C Sensor=0x" + toHexByte(d2);
        }
        if (cmd2 == static_cast<std::uint8_t>(ResponseOpcode::VersionInfo)) {
            if (cmd1 == 0x01U) {
                return "Software Version Response: " + std::to_string(d1) + "." + std::to_string(d2);
            }
            if (cmd1 == 0x03U) {
                const auto build = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
                return "Build Number Response: " + std::to_string(build);
            }
            return "Version Info Response (Sub 0x" + toHexByte(cmd1) + ")";
        }
        if (cmd2 == static_cast<std::uint8_t>(ResponseOpcode::TimeMacro)) {
            switch (cmd1) {
            case 0x01U:
                return "Seconds Response: " + std::to_string(d2) + "s";
            case 0x03U:
                return "Hour/Minute Response: " + std::to_string(d1) + ":" + std::to_string(d2);
            case 0x05U:
                return "Month/Day Response: " + std::to_string(d1) + "/" + std::to_string(d2);
            case 0x07U: {
                const auto yr = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
                return "Year Response: " + std::to_string(yr);
            }
            default:
                return "Time Response (Sub 0x" + toHexByte(cmd1) + ")";
            }
        }
        if (cmd2 == static_cast<std::uint8_t>(ResponseOpcode::Everest)) {
            const auto val16 = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
            switch (cmd1) {
            case 0x01U:
                return "Azimuth Zero Offset Response: " + std::to_string(val16) + " centidegrees";
            case 0x04U:
                return "Zoom Limit Response: " + std::to_string(val16);
            case 0x06U:
                return "Alarms Response (Everest): Mask=0x" + toHexByte(d2);
            case 0x0DU:
                return "Limit Response: " + std::to_string(val16) + " centidegrees";
            case 0x10U:
                return "Defined Presets Response: Mask=0x" + toHexByte(d1) + toHexByte(d2);
            case 0x12U:
                return "Defined Patterns Response: Mask=0x" + toHexByte(d1) + toHexByte(d2);
            default:
                return "Everest Response (Sub 0x" + toHexByte(cmd1) + ")";
            }
        }

        return "Response (Opcode 0x" + toHexByte(cmd2) + ")";
    }

    return "Raw Frame (" + std::to_string(frame.size()) + " bytes)";
}

ResponseClassification ProtocolParser::classifyResponse(const std::vector<std::uint8_t>& frame) noexcept
{
    if (frame.empty() || frame[0] != PelcoDFrame::SyncByte) {
        return ResponseClassification::Unknown;
    }

    if (frame.size() == PelcoDFrame::GeneralResponseSize) {
        return ResponseClassification::General;
    }

    if (frame.size() == PelcoDFrame::QueryResponseSize) {
        return ResponseClassification::QueryReply;
    }

    if (frame.size() == PelcoDFrame::StandardFrameSize) {
        const std::uint8_t op = frame[3];
        if (op == static_cast<std::uint8_t>(ResponseOpcode::StandardExtended)) {
            return ResponseClassification::StandardExtendedAckNak;
        }
        if (op == static_cast<std::uint8_t>(ResponseOpcode::QueryPan)
            || op == static_cast<std::uint8_t>(ResponseOpcode::QueryTilt)
            || op == static_cast<std::uint8_t>(ResponseOpcode::QueryZoom)
            || op == static_cast<std::uint8_t>(ResponseOpcode::QueryMagnification)
            || op == static_cast<std::uint8_t>(ResponseOpcode::QueryDeviceType)
            || op == static_cast<std::uint8_t>(ResponseOpcode::QueryDiagnostics)
            || op == static_cast<std::uint8_t>(ResponseOpcode::VersionInfo)
            || op == static_cast<std::uint8_t>(ResponseOpcode::TimeMacro)
            || op == static_cast<std::uint8_t>(ResponseOpcode::Everest)) {
            return ResponseClassification::ExtendedTelemetry;
        }
    }

    return ResponseClassification::Unknown;
}

bool ProtocolParser::isResponseMatchingQuery(
    const std::string& queryTag, const std::vector<std::uint8_t>& frame) noexcept
{
    if (frame.empty()) {
        return false;
    }

    if (queryTag == "QueryPan") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::QueryPan);
    }
    if (queryTag == "QueryTilt") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::QueryTilt);
    }
    if (queryTag == "QueryZoom") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::QueryZoom);
    }
    if (queryTag == "QueryMagnification") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::QueryMagnification);
    }
    if (queryTag == "QueryDeviceType") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::QueryDeviceType);
    }
    if (queryTag == "QueryDiagnostics") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::QueryDiagnostics);
    }
    if (queryTag == "QuerySoftwareVersion") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::VersionInfo)
            && frame[2] == static_cast<std::uint8_t>(VersionInfoSubOpcode::SoftwareVersionResponse);
    }
    if (queryTag == "QueryBuildNumber") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::VersionInfo)
            && frame[2] == static_cast<std::uint8_t>(VersionInfoSubOpcode::BuildNumberResponse);
    }
    if (queryTag == "QueryTime") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::TimeMacro);
    }
    if (queryTag == "QueryAzimuthZero") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::Everest)
            && frame[2] == static_cast<std::uint8_t>(EverestSubOpcode::AzimuthZeroResponse);
    }
    if (queryTag == "QueryZoomLimit") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::Everest)
            && frame[2] == static_cast<std::uint8_t>(EverestSubOpcode::ZoomLimitResponse);
    }
    if (queryTag == "QueryEverestAlarms") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::Everest)
            && frame[2] == static_cast<std::uint8_t>(EverestSubOpcode::AlarmsResponse);
    }
    if (queryTag == "QueryLimit") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::Everest)
            && frame[2] == static_cast<std::uint8_t>(EverestSubOpcode::LimitResponse);
    }
    if (queryTag == "QueryDefinedPresets") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::Everest)
            && frame[2] == static_cast<std::uint8_t>(EverestSubOpcode::DefinedPresetsResponse);
    }
    if (queryTag == "QueryDefinedPatterns") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::Everest)
            && frame[2] == static_cast<std::uint8_t>(EverestSubOpcode::DefinedPatternsResponse);
    }
    if (queryTag == "QueryEverest") {
        return frame.size() == PelcoDFrame::StandardFrameSize
            && frame[3] == static_cast<std::uint8_t>(ResponseOpcode::Everest);
    }
    if (queryTag == "QueryGeneral") {
        return frame.size() == PelcoDFrame::QueryResponseSize;
    }

    // Generic query fallback: any recognized standard query response opcode or 18-byte query response
    if (frame.size() == PelcoDFrame::StandardFrameSize) {
        const std::uint8_t op = frame[3];
        return op == static_cast<std::uint8_t>(ResponseOpcode::QueryPan)
            || op == static_cast<std::uint8_t>(ResponseOpcode::QueryTilt)
            || op == static_cast<std::uint8_t>(ResponseOpcode::QueryZoom)
            || op == static_cast<std::uint8_t>(ResponseOpcode::QueryMagnification)
            || op == static_cast<std::uint8_t>(ResponseOpcode::QueryDeviceType)
            || op == static_cast<std::uint8_t>(ResponseOpcode::QueryDiagnostics)
            || op == static_cast<std::uint8_t>(ResponseOpcode::VersionInfo)
            || op == static_cast<std::uint8_t>(ResponseOpcode::TimeMacro)
            || op == static_cast<std::uint8_t>(ResponseOpcode::Everest);
    }
    if (frame.size() == PelcoDFrame::QueryResponseSize) {
        return true;
    }

    return false;
}

} // namespace PelcoD
