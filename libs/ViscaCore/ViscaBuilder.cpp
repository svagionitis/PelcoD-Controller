#include "ViscaBuilder.h"

namespace Visca {

ViscaFrame ViscaBuilder::addressSet()
{
    return ViscaFrame { 0x88, 0x30, 0x01, kViscaTerminator };
}

ViscaFrame ViscaBuilder::ifClear(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x00, 0x01, kViscaTerminator };
}

ViscaFrame ViscaBuilder::ifClearBroadcast()
{
    return ViscaFrame { 0x88, 0x01, 0x00, 0x01, kViscaTerminator };
}

ViscaFrame ViscaBuilder::commandCancel(uint8_t cameraAddress, ViscaSocket socket)
{
    const uint8_t socketByte = (socket == ViscaSocket::Socket2) ? 0x22 : 0x21;
    return ViscaFrame { makeHeader(cameraAddress), socketByte, kViscaTerminator };
}

ViscaFrame ViscaBuilder::power(uint8_t cameraAddress, bool on)
{
    const uint8_t powerMode = on ? 0x02 : 0x03;
    return ViscaFrame { makeHeader(cameraAddress), 0x01, 0x04, 0x00, powerMode, kViscaTerminator };
}

ViscaFrame ViscaBuilder::versionInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x00, 0x02, kViscaTerminator };
}

ViscaFrame ViscaBuilder::powerInquiry(uint8_t cameraAddress)
{
    return ViscaFrame { makeHeader(cameraAddress), 0x09, 0x04, 0x00, kViscaTerminator };
}

} // namespace Visca
