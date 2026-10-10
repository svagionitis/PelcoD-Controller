/// @file ProtocolParser.cpp
/// @brief Implementation of Pelco-D protocol response packet parser.

#include "ProtocolParser.h"
#include "PelcoDFrame.h"

#include <cctype>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace PelcoD {

bool ProtocolParser::parseGeneral(
    const std::vector<std::uint8_t>& frame, std::uint8_t& address, std::uint8_t& alarms) noexcept
{
    if (frame.size() != PelcoDFrame::GeneralResponseSize || frame[0] != PelcoDFrame::SyncByte) {
        return false;
    }
    address = frame[1];
    alarms = frame[2];
    return true;
}

bool ProtocolParser::parsePan(const std::vector<std::uint8_t>& frame, std::uint16_t& panCentidegrees) noexcept
{
    if (frame.size() != PelcoDFrame::StandardFrameSize || frame[0] != PelcoDFrame::SyncByte) {
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
    if (frame.size() != PelcoDFrame::StandardFrameSize || frame[0] != PelcoDFrame::SyncByte) {
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
    if (frame.size() != PelcoDFrame::StandardFrameSize || frame[0] != PelcoDFrame::SyncByte) {
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
    if (frame.size() != PelcoDFrame::StandardFrameSize || frame[0] != PelcoDFrame::SyncByte) {
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
    if (frame.size() != PelcoDFrame::StandardFrameSize || frame[0] != PelcoDFrame::SyncByte) {
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
    if (frame.size() != PelcoDFrame::StandardFrameSize || frame[0] != PelcoDFrame::SyncByte) {
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
    if (frame.size() != PelcoDFrame::StandardFrameSize || frame[0] != PelcoDFrame::SyncByte) {
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
    if (frame.size() != PelcoDFrame::StandardFrameSize || frame[0] != PelcoDFrame::SyncByte) {
        return false;
    }
    if (frame[3] != static_cast<std::uint8_t>(ResponseOpcode::VersionInfo)) {
        return false;
    }
    subOpcode = static_cast<VersionInfoSubOpcode>(frame[2]);
    data1 = frame[4];
    data2 = frame[5];
    return true;
}

bool ProtocolParser::parseTimeResponse(const std::vector<std::uint8_t>& frame,
    TimeSubOpcode& subOpcode, std::uint8_t& data1, std::uint8_t& data2) noexcept
{
    if (frame.size() != PelcoDFrame::StandardFrameSize || frame[0] != PelcoDFrame::SyncByte) {
        return false;
    }
    if (frame[3] != static_cast<std::uint8_t>(ResponseOpcode::TimeMacro)) {
        return false;
    }
    subOpcode = static_cast<TimeSubOpcode>(frame[2]);
    data1 = frame[4];
    data2 = frame[5];
    return true;
}

bool ProtocolParser::parseEverestResponse(const std::vector<std::uint8_t>& frame,
    EverestSubOpcode& subOpcode, std::uint8_t& data1, std::uint8_t& data2) noexcept
{
    if (frame.size() != PelcoDFrame::StandardFrameSize || frame[0] != PelcoDFrame::SyncByte) {
        return false;
    }
    if (frame[3] != static_cast<std::uint8_t>(ResponseOpcode::Everest)) {
        return false;
    }
    subOpcode = static_cast<EverestSubOpcode>(frame[2]);
    data1 = frame[4];
    data2 = frame[5];
    return true;
}

bool ProtocolParser::parseQuery(const std::vector<std::uint8_t>& frame, std::string& payload)
{
    if (frame.size() != PelcoDFrame::QueryResponseSize || frame[0] != PelcoDFrame::SyncByte) {
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

bool ProtocolParser::updateStatus(const std::vector<std::uint8_t>& frame, DeviceStatus& status, DeviceInfo& info)
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
        const auto opcode = static_cast<ResponseOpcode>(frame[3]);
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

        case ResponseOpcode::TimeMacro:
            return true;

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
                    status.alarms = d2;
                    return true;
                case EverestSubOpcode::LimitResponse:
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
            const auto opcode = static_cast<CommandOpcode>(cmd2);
            switch (opcode) {
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
