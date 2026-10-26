#include "ViscaBuilder.h"
#include <cstdint>

namespace Visca {

ViscaFrame ViscaBuilder::addressSet()
{
    return ViscaFrame { 0x88, 0x30, 0x01, kViscaTerminator };
}

ViscaFrame ViscaBuilder::ifClear(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x00, 0x01, kViscaTerminator };
}

ViscaFrame ViscaBuilder::ifClearBroadcast()
{
    return ViscaFrame { 0x88, 0x01, 0x00, 0x01, kViscaTerminator };
}

ViscaFrame ViscaBuilder::commandCancel(std::uint8_t cameraAddress, ViscaSocket socket)
{
    const std::uint8_t socketByte = (socket == ViscaSocket::Socket2) ? 0x22 : 0x21;
    return ViscaFrame { makeHeader(cameraAddress), socketByte, kViscaTerminator };
}

ViscaFrame ViscaBuilder::power(std::uint8_t cameraAddress, bool on)
{
    const std::uint8_t powerMode = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x00, powerMode, kViscaTerminator };
}

ViscaFrame ViscaBuilder::versionInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x00, 0x02, kViscaTerminator };
}

ViscaFrame ViscaBuilder::powerInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x00, kViscaTerminator };
}

ViscaFrame ViscaBuilder::panTiltDrive(std::uint8_t cameraAddress, std::uint8_t panSpeed, std::uint8_t tiltSpeed,
    ViscaPanDirection panDir, ViscaTiltDirection tiltDir)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x06, 0x01, panSpeed, tiltSpeed,
        static_cast<std::uint8_t>(panDir), static_cast<std::uint8_t>(tiltDir), kViscaTerminator };
}

ViscaFrame ViscaBuilder::panTiltStop(std::uint8_t cameraAddress, std::uint8_t panSpeed, std::uint8_t tiltSpeed)
{
    return panTiltDrive(cameraAddress, panSpeed, tiltSpeed, ViscaPanDirection::Stop, ViscaTiltDirection::Stop);
}

ViscaFrame ViscaBuilder::panTiltUp(std::uint8_t cameraAddress, std::uint8_t tiltSpeed)
{
    return panTiltDrive(cameraAddress, 0x00, tiltSpeed, ViscaPanDirection::Stop, ViscaTiltDirection::Up);
}

ViscaFrame ViscaBuilder::panTiltDown(std::uint8_t cameraAddress, std::uint8_t tiltSpeed)
{
    return panTiltDrive(cameraAddress, 0x00, tiltSpeed, ViscaPanDirection::Stop, ViscaTiltDirection::Down);
}

ViscaFrame ViscaBuilder::panTiltLeft(std::uint8_t cameraAddress, std::uint8_t panSpeed)
{
    return panTiltDrive(cameraAddress, panSpeed, 0x00, ViscaPanDirection::Left, ViscaTiltDirection::Stop);
}

ViscaFrame ViscaBuilder::panTiltRight(std::uint8_t cameraAddress, std::uint8_t panSpeed)
{
    return panTiltDrive(cameraAddress, panSpeed, 0x00, ViscaPanDirection::Right, ViscaTiltDirection::Stop);
}

ViscaFrame ViscaBuilder::panTiltUpLeft(std::uint8_t cameraAddress, std::uint8_t panSpeed, std::uint8_t tiltSpeed)
{
    return panTiltDrive(cameraAddress, panSpeed, tiltSpeed, ViscaPanDirection::Left, ViscaTiltDirection::Up);
}

ViscaFrame ViscaBuilder::panTiltUpRight(std::uint8_t cameraAddress, std::uint8_t panSpeed, std::uint8_t tiltSpeed)
{
    return panTiltDrive(cameraAddress, panSpeed, tiltSpeed, ViscaPanDirection::Right, ViscaTiltDirection::Up);
}

ViscaFrame ViscaBuilder::panTiltDownLeft(std::uint8_t cameraAddress, std::uint8_t panSpeed, std::uint8_t tiltSpeed)
{
    return panTiltDrive(cameraAddress, panSpeed, tiltSpeed, ViscaPanDirection::Left, ViscaTiltDirection::Down);
}

ViscaFrame ViscaBuilder::panTiltDownRight(std::uint8_t cameraAddress, std::uint8_t panSpeed, std::uint8_t tiltSpeed)
{
    return panTiltDrive(cameraAddress, panSpeed, tiltSpeed, ViscaPanDirection::Right, ViscaTiltDirection::Down);
}

ViscaFrame ViscaBuilder::panTiltAbsolute(std::uint8_t cameraAddress, std::uint8_t panSpeed, std::uint8_t tiltSpeed,
    std::int16_t panPos, std::int16_t tiltPos)
{
    const auto panNibbles = ViscaFrame::packWordNibbles(static_cast<std::uint16_t>(panPos));
    const auto tiltNibbles = ViscaFrame::packWordNibbles(static_cast<std::uint16_t>(tiltPos));
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x06, 0x02, panSpeed, tiltSpeed, panNibbles[0], panNibbles[1],
        panNibbles[2], panNibbles[3], tiltNibbles[0], tiltNibbles[1], tiltNibbles[2], tiltNibbles[3],
        kViscaTerminator };
}

ViscaFrame ViscaBuilder::panTiltRelative(std::uint8_t cameraAddress, std::uint8_t panSpeed, std::uint8_t tiltSpeed,
    std::int16_t deltaPan, std::int16_t deltaTilt)
{
    const auto panNibbles = ViscaFrame::packWordNibbles(static_cast<std::uint16_t>(deltaPan));
    const auto tiltNibbles = ViscaFrame::packWordNibbles(static_cast<std::uint16_t>(deltaTilt));
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x06, 0x03, panSpeed, tiltSpeed, panNibbles[0], panNibbles[1],
        panNibbles[2], panNibbles[3], tiltNibbles[0], tiltNibbles[1], tiltNibbles[2], tiltNibbles[3],
        kViscaTerminator };
}

ViscaFrame ViscaBuilder::panTiltHome(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x06, 0x04, kViscaTerminator };
}

ViscaFrame ViscaBuilder::panTiltReset(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x06, 0x05, kViscaTerminator };
}

ViscaFrame ViscaBuilder::panTiltLimitSet(
    std::uint8_t cameraAddress, ViscaPanTiltCorner corner, std::int16_t panPos, std::int16_t tiltPos)
{
    const auto panNibbles = ViscaFrame::packWordNibbles(static_cast<std::uint16_t>(panPos));
    const auto tiltNibbles = ViscaFrame::packWordNibbles(static_cast<std::uint16_t>(tiltPos));
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x06, 0x07, 0x00, static_cast<std::uint8_t>(corner),
        panNibbles[0], panNibbles[1], panNibbles[2], panNibbles[3], tiltNibbles[0], tiltNibbles[1], tiltNibbles[2],
        tiltNibbles[3], kViscaTerminator };
}

ViscaFrame ViscaBuilder::panTiltLimitClear(std::uint8_t cameraAddress, ViscaPanTiltCorner corner)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x06, 0x07, 0x01, static_cast<std::uint8_t>(corner), 0x07,
        0x0F, 0x0F, 0x0F, 0x07, 0x0F, 0x0F, 0x0F, kViscaTerminator };
}

ViscaFrame ViscaBuilder::panTiltPositionInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x06, 0x12, kViscaTerminator };
}

ViscaFrame ViscaBuilder::panTiltStatusInquiry(std::uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x06, 0x10, kViscaTerminator };
}

} // namespace Visca
