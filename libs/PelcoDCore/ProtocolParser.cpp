/// @file ProtocolParser.cpp
/// @brief Implementation of Pelco-D protocol response packet parser.

#include "ProtocolParser.h"
#include "PelcoDFrame.h"

#include <cctype>
#include <chrono>

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

} // namespace PelcoD
