#include "SonyViscaParser.h"

namespace Visca::Sony {

bool SonyViscaParser::parseBlock00(const ViscaFrame& frame, SonyFCBStatus& status) noexcept
{
    if (frame.size() != 16 || !frame.isInquiryResponse()) {
        return false;
    }

    status.focusAuto = (frame[2] & 0x01) != 0;
    status.digitalZoomOn = (frame[2] & 0x02) != 0;
    status.digitalZoomCombine = (frame[2] & 0x20) == 0;
    status.lowContrastDetected = (frame[2] & 0x40) != 0;

    status.zoomPosition = ViscaFrame::unpackWordNibbles(frame.data() + 3);
    status.focusPosition = ViscaFrame::unpackWordNibbles(frame.data() + 7);
    status.focusNearLimit = static_cast<uint16_t>(((frame[11] & 0x0F) << 12) | ((frame[12] & 0x0F) << 8));

    status.zoomCommandExecuting = (frame[13] & 0x01) != 0;
    status.focusCommandExecuting = (frame[13] & 0x02) != 0;

    return true;
}

bool SonyViscaParser::parseBlock01(const ViscaFrame& frame, SonyFCBStatus& status) noexcept
{
    if (frame.size() != 16 || !frame.isInquiryResponse()) {
        return false;
    }

    status.slowShutterOn = (frame[2] & 0x01) != 0;
    status.exposureCompOn = (frame[2] & 0x02) != 0;
    status.backlightOn = (frame[2] & 0x04) != 0;
    status.spotAeOn = (frame[2] & 0x08) != 0;
    status.veOn = (frame[2] & 0x10) != 0;
    status.spotFocusOn = (frame[2] & 0x40) != 0;
    status.spotAwbOn = (frame[2] & 0x80) != 0;

    status.wbMode = static_cast<SonyWhiteBalanceMode>(frame[3] & 0x0F);
    status.apertureGain = static_cast<uint8_t>(frame[4] & 0x0F);
    status.exposureMode = static_cast<SonyExposureMode>(frame[5] & 0x0F);

    status.shutterPosition = static_cast<uint8_t>(frame[6] & 0x3F);
    status.irisPosition = static_cast<uint8_t>(frame[7] & 0x1F);
    status.gainPosition = static_cast<uint8_t>(frame[8] & 0x1F);
    status.exposureCompPosition = static_cast<uint8_t>(frame[9] & 0x0F);

    status.rGain = static_cast<uint16_t>(((frame[11] & 0x0F) << 4) | (frame[12] & 0x0F));
    status.bGain = static_cast<uint16_t>(((frame[13] & 0x0F) << 4) | (frame[14] & 0x0F));

    return true;
}

bool SonyViscaParser::parseBlock02(const ViscaFrame& frame, SonyFCBStatus& status) noexcept
{
    if (frame.size() != 16 || !frame.isInquiryResponse()) {
        return false;
    }

    status.powerOn = (frame[2] & 0x01) != 0;
    status.autoIcrOn = (frame[2] & 0x04) != 0;
    status.icrColor = (frame[2] & 0x10) != 0;

    const uint8_t stabLevelNibble = static_cast<uint8_t>(frame[3] & 0x03);
    if (stabLevelNibble == 3) {
        status.stabilizerLevel = SonyStabilizerMode::SuperPlus;
    } else if (stabLevelNibble == 2) {
        status.stabilizerLevel = SonyStabilizerMode::Super;
    } else {
        status.stabilizerLevel = SonyStabilizerMode::Normal;
    }

    status.lrReverseOn = (frame[3] & 0x04) != 0;
    status.freezeOn = (frame[3] & 0x08) != 0;
    status.icrOn = (frame[3] & 0x10) != 0;
    status.stabilizerOn = (frame[3] & 0x40) != 0;

    status.system50Hz = (frame[12] & 0x01) != 0;

    return true;
}

bool SonyViscaParser::parseBlock03(const ViscaFrame& frame, SonyFCBStatus& status) noexcept
{
    if (frame.size() != 16 || !frame.isInquiryResponse()) {
        return false;
    }

    status.digitalZoomPosition = static_cast<uint16_t>(((frame[3] & 0x0F) << 4) | (frame[4] & 0x0F));
    status.gamma = static_cast<uint8_t>(frame[9] & 0x07);
    status.nrLevel = static_cast<uint8_t>(frame[10] & 0x07);
    status.colorGain = static_cast<uint8_t>(frame[11] & 0x0F);
    status.chromaSuppress = static_cast<uint8_t>(frame[12] & 0x03);

    return true;
}

bool SonyViscaParser::parseBlock04(const ViscaFrame& frame, SonyFCBStatus& status) noexcept
{
    if (frame.size() != 16 || !frame.isInquiryResponse()) {
        return false;
    }

    status.defogOn = (frame[8] & 0x01) != 0;
    status.defogLevel = static_cast<SonyDefogMode>(frame[13] & 0x03);
    status.wideDMode = static_cast<SonyWideDMode>(frame[9] & 0x03);
    status.veCompensationLevel = static_cast<uint8_t>(frame[11] & 0x03);
    status.veBrightnessCompensation = static_cast<uint8_t>(frame[5] & 0x03);

    return true;
}

bool SonyViscaParser::parseBlock(uint8_t blockIndex, const ViscaFrame& frame, SonyFCBStatus& status) noexcept
{
    switch (blockIndex) {
    case 0:
        return parseBlock00(frame, status);
    case 1:
        return parseBlock01(frame, status);
    case 2:
        return parseBlock02(frame, status);
    case 3:
        return parseBlock03(frame, status);
    case 4:
        return parseBlock04(frame, status);
    default:
        return false;
    }
}

bool SonyViscaParser::parseRegisterInquiry(const ViscaFrame& frame, uint8_t& outValue) noexcept
{
    if (frame.size() != 5 || frame[1] != 0x50 || frame[4] != kViscaTerminator) {
        return false;
    }
    outValue = static_cast<uint8_t>(((frame[2] & 0x0F) << 4) | (frame[3] & 0x0F));
    return true;
}

} // namespace Visca::Sony
