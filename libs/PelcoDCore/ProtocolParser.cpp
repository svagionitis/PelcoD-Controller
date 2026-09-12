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
    panCentidegrees = static_cast<std::uint16_t>((frame[4] << 8U) | frame[5]);
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
    tiltCentidegrees = static_cast<std::uint16_t>((frame[4] << 8U) | frame[5]);
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
    zoomPosition = static_cast<std::uint16_t>((frame[4] << 8U) | frame[5]);
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
    magnification = static_cast<std::uint16_t>((frame[4] << 8U) | frame[5]);
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
            if (frame[i] != 0U) {
                payload.push_back(static_cast<char>(frame[i]));
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
                if ((cmd1 & 0x08U) != 0U) {
                    acts.push_back("IrisOpen");
                }
                if ((cmd1 & 0x04U) != 0U) {
                    acts.push_back("IrisClose");
                }
                if (acts.empty()) {
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
            switch (cmd2) {
            case 0x03U:
                return "Set Preset " + std::to_string(d2);
            case 0x05U:
                return "Clear Preset " + std::to_string(d2);
            case 0x07U:
                return "GoTo Preset " + std::to_string(d2);
            case 0x09U:
                return "Set Aux " + std::to_string(d2) + " ON";
            case 0x0BU:
                return "Clear Aux " + std::to_string(d2) + " OFF";
            case 0x49U:
                return "Set Zero Position";
            case 0x51U:
                return "Query Pan Position";
            case 0x53U:
                return "Query Tilt Position";
            case 0x55U:
                return "Query Zoom Position";
            case 0x61U:
                return "Query Magnification";
            case 0x67U:
                return "Set Baud Rate";
            case 0x6FU:
                return "Query Diagnostics";
            case 0x0FU:
                return "Remote Reset";
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
            oss << "Pan Response: " << std::fixed << std::setprecision(2) << (static_cast<double>(cdeg) / 100.0) << "°";
            return oss.str();
        }
        if (cmd2 == static_cast<std::uint8_t>(ResponseOpcode::QueryTilt)) {
            const auto cdeg = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
            std::ostringstream oss;
            oss << "Tilt Response: " << std::fixed << std::setprecision(2) << (static_cast<double>(cdeg) / 100.0) << "°";
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
            const auto temp = static_cast<int>(static_cast<std::int8_t>(d1));
            return "Diagnostics Response: Temp=" + std::to_string(temp) + "°C Sensor=0x" + toHexByte(d2);
        }

        return "Response (Opcode 0x" + toHexByte(cmd2) + ")";
    }

    return "Raw Frame (" + std::to_string(frame.size()) + " bytes)";
}

} // namespace PelcoD
