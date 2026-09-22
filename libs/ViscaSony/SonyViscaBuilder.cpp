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
