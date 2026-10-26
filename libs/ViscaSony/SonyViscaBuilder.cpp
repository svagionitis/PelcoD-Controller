#include "SonyViscaBuilder.h"
#include <cstdint>

namespace Visca::Sony {

ViscaFrame SonyViscaBuilder::zoomStop(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x07, 0x00, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::zoomTele(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x07, 0x02, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::zoomWide(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x07, 0x03, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::zoomTeleVariable(std::uint8_t cameraAddress, std::uint8_t speed)
{
    const std::uint8_t speedByte = static_cast<std::uint8_t>(0x20 | (speed & 0x07));
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x07, speedByte, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::zoomWideVariable(std::uint8_t cameraAddress, std::uint8_t speed)
{
    const std::uint8_t speedByte = static_cast<std::uint8_t>(0x30 | (speed & 0x07));
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x07, speedByte, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::zoomDirect(std::uint8_t cameraAddress, std::uint16_t position)
{
    const auto nibbles = ViscaFrame::packWordNibbles(position);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x47, nibbles[0], nibbles[1], nibbles[2], nibbles[3],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::dzoomOn(std::uint8_t cameraAddress, bool on)
{
    const std::uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x06, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::dzoomMode(std::uint8_t cameraAddress, bool combine)
{
    const std::uint8_t mode = combine ? 0x00 : 0x01;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x36, mode, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusStop(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x08, 0x00, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusFar(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x08, 0x02, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusNear(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x08, 0x03, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusFarVariable(std::uint8_t cameraAddress, std::uint8_t speed)
{
    const std::uint8_t speedByte = static_cast<std::uint8_t>(0x20 | (speed & 0x07));
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x08, speedByte, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusNearVariable(std::uint8_t cameraAddress, std::uint8_t speed)
{
    const std::uint8_t speedByte = static_cast<std::uint8_t>(0x30 | (speed & 0x07));
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x08, speedByte, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusDirect(std::uint8_t cameraAddress, std::uint16_t position)
{
    const auto nibbles = ViscaFrame::packWordNibbles(position);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x48, nibbles[0], nibbles[1], nibbles[2], nibbles[3],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusAuto(std::uint8_t cameraAddress, bool autoMode)
{
    const std::uint8_t mode = autoMode ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x38, mode, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusOnePush(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x18, 0x01, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusNearLimit(std::uint8_t cameraAddress, std::uint16_t limit)
{
    const auto nibbles = ViscaFrame::packWordNibbles(limit);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x28, nibbles[0], nibbles[1], nibbles[2], nibbles[3],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::exposureMode(std::uint8_t cameraAddress, SonyExposureMode mode)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x39, static_cast<std::uint8_t>(mode),
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::shutterDirect(std::uint8_t cameraAddress, std::uint8_t position)
{
    const auto nibbles = ViscaFrame::packByteNibbles(position);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x4A, 0x00, 0x00, nibbles[0], nibbles[1],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::irisDirect(std::uint8_t cameraAddress, std::uint8_t position)
{
    const auto nibbles = ViscaFrame::packByteNibbles(position);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x4B, 0x00, 0x00, nibbles[0], nibbles[1],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::gainDirect(std::uint8_t cameraAddress, std::uint8_t position)
{
    const auto nibbles = ViscaFrame::packByteNibbles(position);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x4C, 0x00, 0x00, nibbles[0], nibbles[1],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::exposureComp(std::uint8_t cameraAddress, bool on)
{
    const std::uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x3E, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::exposureCompDirect(std::uint8_t cameraAddress, std::uint8_t position)
{
    const auto nibbles = ViscaFrame::packByteNibbles(position);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x4E, 0x00, 0x00, nibbles[0], nibbles[1],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::wbMode(std::uint8_t cameraAddress, SonyWhiteBalanceMode mode)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x35, static_cast<std::uint8_t>(mode),
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::wbOnePushTrigger(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x10, 0x05, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::rGainDirect(std::uint8_t cameraAddress, std::uint8_t position)
{
    const auto nibbles = ViscaFrame::packByteNibbles(position);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x43, 0x00, 0x00, nibbles[0], nibbles[1],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::bGainDirect(std::uint8_t cameraAddress, std::uint8_t position)
{
    const auto nibbles = ViscaFrame::packByteNibbles(position);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x44, 0x00, 0x00, nibbles[0], nibbles[1],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::stabilizer(std::uint8_t cameraAddress, SonyStabilizerMode mode)
{
    std::uint8_t modeByte = 0x00;
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

ViscaFrame SonyViscaBuilder::defog(std::uint8_t cameraAddress, SonyDefogMode mode)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x37, static_cast<std::uint8_t>(mode),
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::icr(std::uint8_t cameraAddress, bool on)
{
    const std::uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x01, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::autoIcr(std::uint8_t cameraAddress, bool on)
{
    const std::uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x51, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::zoomPositionInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x47, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::dzoomModeInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x06, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusPositionInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x48, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusModeInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x38, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::focusNearLimitInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x28, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::exposureModeInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x39, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::shutterPositionInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x4A, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::irisPositionInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x4B, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::gainPositionInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x4C, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::exposureCompModeInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x3E, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::exposureCompPositionInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x4E, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::wbModeInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x35, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::rGainInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x43, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::bGainInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x44, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::stabilizerModeInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x34, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::defogModeInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x37, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::icrModeInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x01, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::autoIcrModeInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x51, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::cameraIdInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x22, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::apertureReset(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x02, 0x00, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::apertureUp(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x02, 0x02, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::apertureDown(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x02, 0x03, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::apertureDirect(std::uint8_t cameraAddress, std::uint8_t level)
{
    const auto nibbles = ViscaFrame::packByteNibbles(static_cast<std::uint8_t>(level & 0x0FU));
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x42, 0x00, 0x00, nibbles[0], nibbles[1],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::apertureInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x42, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::backlight(std::uint8_t cameraAddress, bool on)
{
    const std::uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x33, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::backlightInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x33, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::autoSlowShutter(std::uint8_t cameraAddress, bool on)
{
    const std::uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x5A, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::autoSlowShutterInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x5A, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::highSensitivity(std::uint8_t cameraAddress, bool on)
{
    const std::uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x5E, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::highSensitivityInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x5E, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::noiseReduction2D(std::uint8_t cameraAddress, std::uint8_t level)
{
    const std::uint8_t val = static_cast<std::uint8_t>(level & 0x07U);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x53, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::noiseReduction2DInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x53, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::noiseReduction3D(std::uint8_t cameraAddress, std::uint8_t level)
{
    const std::uint8_t val = static_cast<std::uint8_t>(level & 0x07U);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x54, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::noiseReduction3DInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x54, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::wideD(std::uint8_t cameraAddress, SonyWideDMode mode)
{
    std::uint8_t modeByte = 0x03; // Off
    if (mode == SonyWideDMode::WideD) {
        modeByte = 0x02; // Wide-D On
    } else if (mode == SonyWideDMode::VisibilityEnhancer) {
        modeByte = 0x06; // VE On
    }
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x3D, modeByte, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::wideDInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x3D, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::freeze(std::uint8_t cameraAddress, bool on)
{
    const std::uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x62, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::freezeInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x62, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::pictureFlip(std::uint8_t cameraAddress, bool on)
{
    const std::uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x66, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::pictureFlipInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x66, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::lrReverse(std::uint8_t cameraAddress, bool on)
{
    const std::uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x61, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::lrReverseInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x61, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::colorGain(std::uint8_t cameraAddress, std::uint8_t gain)
{
    const std::uint8_t val = static_cast<std::uint8_t>(gain & 0x0FU);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x49, 0x00, 0x00, 0x00, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::colorGainInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x49, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::colorHue(std::uint8_t cameraAddress, std::uint8_t hue)
{
    const std::uint8_t val = static_cast<std::uint8_t>(hue & 0x0FU);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x4F, 0x00, 0x00, 0x00, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::colorHueInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x4F, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::chromaSuppress(std::uint8_t cameraAddress, std::uint8_t level)
{
    const std::uint8_t val = static_cast<std::uint8_t>(level & 0x03U);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x5F, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::chromaSuppressInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x5F, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::gamma(std::uint8_t cameraAddress, std::uint8_t mode)
{
    const std::uint8_t val = static_cast<std::uint8_t>(mode & 0x07U);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x5B, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::gammaInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x5B, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::memory(std::uint8_t cameraAddress, SonyMemoryAction action, std::uint8_t channel)
{
    const std::uint8_t ch = static_cast<std::uint8_t>(channel & 0x0FU);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x3F, static_cast<std::uint8_t>(action), ch,
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::memorySet(std::uint8_t cameraAddress, std::uint8_t channel)
{
    return memory(cameraAddress, SonyMemoryAction::Set, channel);
}

ViscaFrame SonyViscaBuilder::memoryRecall(std::uint8_t cameraAddress, std::uint8_t channel)
{
    return memory(cameraAddress, SonyMemoryAction::Recall, channel);
}

ViscaFrame SonyViscaBuilder::memoryReset(std::uint8_t cameraAddress, std::uint8_t channel)
{
    return memory(cameraAddress, SonyMemoryAction::Reset, channel);
}

ViscaFrame SonyViscaBuilder::memoryInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x3F, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::spotAe(std::uint8_t cameraAddress, bool on)
{
    const std::uint8_t val = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x59, val, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::spotAePosition(std::uint8_t cameraAddress, std::uint8_t x, std::uint8_t y)
{
    const auto xNib = ViscaFrame::packByteNibbles(static_cast<std::uint8_t>(x & 0x0FU));
    const auto yNib = ViscaFrame::packByteNibbles(static_cast<std::uint8_t>(y & 0x0FU));
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x29, xNib[0], xNib[1], yNib[0], yNib[1],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::spotAeInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x59, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::spotAePositionInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x29, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::spotFocusPosition(std::uint8_t cameraAddress, std::uint8_t x, std::uint8_t y)
{
    const auto xNib = ViscaFrame::packByteNibbles(static_cast<std::uint8_t>(x & 0x0FU));
    const auto yNib = ViscaFrame::packByteNibbles(static_cast<std::uint8_t>(y & 0x0FU));
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x2A, xNib[0], xNib[1], yNib[0], yNib[1],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::spotFocusPositionInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x2A, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::spotAwbPosition(std::uint8_t cameraAddress, std::uint8_t x, std::uint8_t y)
{
    const auto xNib = ViscaFrame::packByteNibbles(static_cast<std::uint8_t>(x & 0x0FU));
    const auto yNib = ViscaFrame::packByteNibbles(static_cast<std::uint8_t>(y & 0x0FU));
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x2B, xNib[0], xNib[1], yNib[0], yNib[1],
        kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::spotAwbPositionInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x2B, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::writeRegister(std::uint8_t cameraAddress, std::uint8_t reg, std::uint8_t value)
{
    const auto nibbles = ViscaFrame::packByteNibbles(value);
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x24, reg, nibbles[0], nibbles[1], kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::registerInquiry(std::uint8_t cameraAddress, std::uint8_t reg)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x24, reg, kViscaTerminator };
}

ViscaFrame SonyViscaBuilder::blockInquiry(std::uint8_t cameraAddress, std::uint8_t blockIndex)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x7E, 0x7E, static_cast<std::uint8_t>(blockIndex & 0x0F),
        kViscaTerminator };
}

} // namespace Visca::Sony
