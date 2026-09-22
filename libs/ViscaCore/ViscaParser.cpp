#include "ViscaParser.h"

namespace Visca {

std::optional<ViscaAckResponse> ViscaParser::parseAck(const ViscaFrame& frame) noexcept
{
    if (!frame.isAck()) {
        return std::nullopt;
    }
    ViscaAckResponse resp;
    resp.cameraAddress = frame.sourceAddress();
    resp.socket = frame.socket();
    return resp;
}

std::optional<ViscaCompletionResponse> ViscaParser::parseCompletion(const ViscaFrame& frame) noexcept
{
    if (frame.size() != 3 || !frame.isCompletion()) {
        return std::nullopt;
    }
    ViscaCompletionResponse resp;
    resp.cameraAddress = frame.sourceAddress();
    resp.socket = frame.socket();
    return resp;
}

std::optional<ViscaErrorResponse> ViscaParser::parseError(const ViscaFrame& frame) noexcept
{
    if (!frame.isError()) {
        return std::nullopt;
    }
    ViscaErrorResponse resp;
    resp.cameraAddress = frame.sourceAddress();
    resp.socket = frame.socket();
    resp.code = frame.errorCode();
    return resp;
}

std::optional<ViscaVersionInfo> ViscaParser::parseVersionInquiry(const ViscaFrame& frame) noexcept
{
    if (frame.size() != 10 || !frame.isInquiryResponse()) {
        return std::nullopt;
    }
    ViscaVersionInfo info;
    info.cameraAddress = frame.sourceAddress();
    info.vendorId = static_cast<uint16_t>((frame[2] << 8) | frame[3]);
    info.modelId = static_cast<uint16_t>((frame[4] << 8) | frame[5]);
    info.romVersion = static_cast<uint16_t>((frame[6] << 8) | frame[7]);
    info.maxSockets = frame[8];
    return info;
}

std::optional<ViscaAddressSetResponse> ViscaParser::parseAddressSet(const ViscaFrame& frame) noexcept
{
    if (frame.size() != 4 || frame[0] != 0x88 || frame[1] != 0x30 || frame[3] != kViscaTerminator) {
        return std::nullopt;
    }
    const uint8_t p = static_cast<uint8_t>(frame[2] & 0x0F);
    ViscaAddressSetResponse resp;
    resp.cameraCount = (p >= 2) ? static_cast<uint8_t>(p - 1) : 0;
    return resp;
}

std::optional<bool> ViscaParser::parsePowerInquiry(const ViscaFrame& frame) noexcept
{
    if (frame.size() != 4 || frame[1] != 0x50 || frame[3] != kViscaTerminator) {
        return std::nullopt;
    }
    if (frame[2] == 0x02) {
        return true;
    }
    if (frame[2] == 0x03) {
        return false;
    }
    return std::nullopt;
}

} // namespace Visca
