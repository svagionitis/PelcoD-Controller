#include "SonyViscaBuilder.h"

namespace Visca::Sony {

ViscaFrame SonyViscaBuilder::zoomStop(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x07, 0x00, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::zoomTele(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x07, 0x02, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::zoomWide(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x07, 0x03, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::zoomTeleVariable(uint8_t cameraAddress, uint8_t speed)
{
    const uint8_t speedByte = static_cast<uint8_t>(0x20 | (speed & 0x07));
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x07, speedByte, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::zoomWideVariable(uint8_t cameraAddress, uint8_t speed)
{
    const uint8_t speedByte = static_cast<uint8_t>(0x30 | (speed & 0x07));
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x07, speedByte, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::zoomDirect(uint8_t cameraAddress, uint16_t position)
{
    const auto nibbles = ViscaFrame::packWordNibbles(position);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x47, nibbles[0], nibbles[1], nibbles[2], nibbles[3],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::dzoomOn(uint8_t cameraAddress, bool on)
{
    const uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x06, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::dzoomMode(uint8_t cameraAddress, bool combine)
{
    const uint8_t mode = combine ? 0x00 : 0x01;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x36, mode, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusStop(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x08, 0x00, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusFar(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x08, 0x02, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusNear(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x08, 0x03, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusFarVariable(uint8_t cameraAddress, uint8_t speed)
{
    const uint8_t speedByte = static_cast<uint8_t>(0x20 | (speed & 0x07));
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x08, speedByte, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusNearVariable(uint8_t cameraAddress, uint8_t speed)
{
    const uint8_t speedByte = static_cast<uint8_t>(0x30 | (speed & 0x07));
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x08, speedByte, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusDirect(uint8_t cameraAddress, uint16_t position)
{
    const auto nibbles = ViscaFrame::packWordNibbles(position);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x48, nibbles[0], nibbles[1], nibbles[2], nibbles[3],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusAuto(uint8_t cameraAddress, bool autoMode)
{
    const uint8_t mode = autoMode ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x38, mode, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusOnePush(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x18, 0x01, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusNearLimit(uint8_t cameraAddress, uint16_t limit)
{
    const auto nibbles = ViscaFrame::packWordNibbles(limit);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x28, nibbles[0], nibbles[1], nibbles[2], nibbles[3],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::exposureMode(uint8_t cameraAddress, SonyExposureMode mode)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x39, static_cast<uint8_t>(mode), kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::shutterDirect(uint8_t cameraAddress, uint8_t position)
{
    const auto nibbles = ViscaFrame::packByteNibbles(position);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x4A, 0x00, 0x00, nibbles[0], nibbles[1],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::irisDirect(uint8_t cameraAddress, uint8_t position)
{
    const auto nibbles = ViscaFrame::packByteNibbles(position);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x4B, 0x00, 0x00, nibbles[0], nibbles[1],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::gainDirect(uint8_t cameraAddress, uint8_t position)
{
    const auto nibbles = ViscaFrame::packByteNibbles(position);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x4C, 0x00, 0x00, nibbles[0], nibbles[1],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::exposureComp(uint8_t cameraAddress, bool on)
{
    const uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x3E, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::exposureCompDirect(uint8_t cameraAddress, uint8_t position)
{
    const auto nibbles = ViscaFrame::packByteNibbles(position);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x4E, 0x00, 0x00, nibbles[0], nibbles[1],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::wbMode(uint8_t cameraAddress, SonyWhiteBalanceMode mode)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x35, static_cast<uint8_t>(mode), kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::wbOnePushTrigger(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x10, 0x05, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::rGainDirect(uint8_t cameraAddress, uint8_t position)
{
    const auto nibbles = ViscaFrame::packByteNibbles(position);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x43, 0x00, 0x00, nibbles[0], nibbles[1],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::bGainDirect(uint8_t cameraAddress, uint8_t position)
{
    const auto nibbles = ViscaFrame::packByteNibbles(position);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x44, 0x00, 0x00, nibbles[0], nibbles[1],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::stabilizer(uint8_t cameraAddress, SonyStabilizerMode mode)
{
    uint8_t modeByte = 0x00;
    switch (mode) {
    case SonyStabilizerMode::Off:
        modeByte = 0x00;
        break;
    case SonyStabilizerMode::Normal:
        modeByte = 0x02;
        break;
    case SonyStabilizerMode::Super:
        modeByte = 0x04;
        break;
    case SonyStabilizerMode::SuperPlus:
        modeByte = 0x05;
        break;
    }
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x34, modeByte, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::defog(uint8_t cameraAddress, SonyDefogMode mode)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x37, static_cast<uint8_t>(mode), kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::icr(uint8_t cameraAddress, bool on)
{
    const uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x01, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::autoIcr(uint8_t cameraAddress, bool on)
{
    const uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x51, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::zoomPositionInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x47, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::dzoomModeInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x06, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusPositionInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x48, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusModeInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x38, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusNearLimitInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x28, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::exposureModeInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x39, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::shutterPositionInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x4A, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::irisPositionInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x4B, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::gainPositionInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x4C, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::exposureCompModeInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x3E, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::exposureCompPositionInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x4E, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::wbModeInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x35, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::rGainInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x43, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::bGainInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x44, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::stabilizerModeInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x34, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::defogModeInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x37, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::icrModeInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x01, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::autoIcrModeInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x51, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::cameraIdInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x22, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::apertureReset(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x02, 0x00, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::apertureUp(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x02, 0x02, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::apertureDown(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x02, 0x03, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::apertureDirect(uint8_t cameraAddress, uint8_t level)
{
    const auto nibbles = ViscaFrame::packByteNibbles(static_cast<uint8_t>(level & 0x0FU));
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x42, 0x00, 0x00, nibbles[0], nibbles[1],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::apertureInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x42, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::backlight(uint8_t cameraAddress, bool on)
{
    const uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x33, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::backlightInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x33, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::autoSlowShutter(uint8_t cameraAddress, bool on)
{
    const uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x5A, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::autoSlowShutterInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x5A, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::highSensitivity(uint8_t cameraAddress, bool on)
{
    const uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x5E, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::highSensitivityInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x5E, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::noiseReduction2D(uint8_t cameraAddress, uint8_t level)
{
    const uint8_t val = static_cast<uint8_t>(level & 0x07U);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x53, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::noiseReduction2DInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x53, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::noiseReduction3D(uint8_t cameraAddress, uint8_t level)
{
    const uint8_t val = static_cast<uint8_t>(level & 0x07U);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x54, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::noiseReduction3DInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x54, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::wideD(uint8_t cameraAddress, SonyWideDMode mode)
{
    uint8_t modeByte = 0x03; // Off
    if (mode == SonyWideDMode::WideD) {
        modeByte = 0x02; // Wide-D On
    } else if (mode == SonyWideDMode::VisibilityEnhancer) {
        modeByte = 0x06; // VE On
    }
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x3D, modeByte, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::wideDInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x3D, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::freeze(uint8_t cameraAddress, bool on)
{
    const uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x62, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::freezeInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x62, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::pictureFlip(uint8_t cameraAddress, bool on)
{
    const uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x66, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::pictureFlipInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x66, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::lrReverse(uint8_t cameraAddress, bool on)
{
    const uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x61, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::lrReverseInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x61, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::colorGain(uint8_t cameraAddress, uint8_t gain)
{
    const uint8_t val = static_cast<uint8_t>(gain & 0x0FU);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x49, 0x00, 0x00, 0x00, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::colorGainInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x49, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::colorHue(uint8_t cameraAddress, uint8_t hue)
{
    const uint8_t val = static_cast<uint8_t>(hue & 0x0FU);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x4F, 0x00, 0x00, 0x00, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::colorHueInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x4F, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::chromaSuppress(uint8_t cameraAddress, uint8_t level)
{
    const uint8_t val = static_cast<uint8_t>(level & 0x03U);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x5F, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::chromaSuppressInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x5F, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::gamma(uint8_t cameraAddress, uint8_t mode)
{
    const uint8_t val = static_cast<uint8_t>(mode & 0x07U);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x5B, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::gammaInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x5B, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::memory(uint8_t cameraAddress, SonyMemoryAction action, uint8_t channel)
{
    const uint8_t ch = static_cast<uint8_t>(channel & 0x0FU);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x3F, static_cast<uint8_t>(action), ch,
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::memorySet(uint8_t cameraAddress, uint8_t channel)
{
    return memory(cameraAddress, SonyMemoryAction::Set, channel);
}

ViscaFrame SonyViscaBuilder::memoryRecall(uint8_t cameraAddress, uint8_t channel)
{
    return memory(cameraAddress, SonyMemoryAction::Recall, channel);
}

ViscaFrame SonyViscaBuilder::memoryReset(uint8_t cameraAddress, uint8_t channel)
{
    return memory(cameraAddress, SonyMemoryAction::Reset, channel);
}

ViscaFrame SonyViscaBuilder::memoryInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x3F, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::spotAe(uint8_t cameraAddress, bool on)
{
    const uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x59, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::spotAePosition(uint8_t cameraAddress, uint8_t x, uint8_t y)
{
    const auto xNib = ViscaFrame::packByteNibbles(static_cast<uint8_t>(x & 0x0FU));
    const auto yNib = ViscaFrame::packByteNibbles(static_cast<uint8_t>(y & 0x0FU));
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x29, xNib[0], xNib[1], yNib[0], yNib[1],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::spotAeInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x59, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::spotAePositionInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x29, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::spotFocusPosition(uint8_t cameraAddress, uint8_t x, uint8_t y)
{
    const auto xNib = ViscaFrame::packByteNibbles(static_cast<uint8_t>(x & 0x0FU));
    const auto yNib = ViscaFrame::packByteNibbles(static_cast<uint8_t>(y & 0x0FU));
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x2A, xNib[0], xNib[1], yNib[0], yNib[1],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::spotFocusPositionInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x2A, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::spotAwbPosition(uint8_t cameraAddress, uint8_t x, uint8_t y)
{
    const auto xNib = ViscaFrame::packByteNibbles(static_cast<uint8_t>(x & 0x0FU));
    const auto yNib = ViscaFrame::packByteNibbles(static_cast<uint8_t>(y & 0x0FU));
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x2B, xNib[0], xNib[1], yNib[0], yNib[1],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::spotAwbPositionInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x2B, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::writeRegister(uint8_t cameraAddress, uint8_t reg, uint8_t value)
{
    const auto nibbles = ViscaFrame::packByteNibbles(value);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x24, reg, nibbles[0], nibbles[1], kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::registerInquiry(uint8_t cameraAddress, uint8_t reg)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x24, reg, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::blockInquiry(uint8_t cameraAddress, uint8_t blockIndex)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x7E, 0x7E, static_cast<uint8_t>(blockIndex & 0x0F),
        kViscaTerminator };
}

} // namespace Visca::Sony
