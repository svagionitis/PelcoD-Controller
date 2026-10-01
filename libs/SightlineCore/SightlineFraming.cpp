/// @file SightlineFraming.cpp
/// @brief Implementation of core packet framing, CRC calculation, and endian utilities.

#include "SightlineFraming.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineFraming::buildPacket(MessageId id, ByteView payload)
{
    // Length covers Message ID + Payload + Checksum
    const std::size_t payloadAndCsLen { 1U + payload.size() + 1U };

    std::vector<std::uint8_t> packet {};
    packet.reserve(payloadAndCsLen + 4U);

    packet.push_back(HeaderByte1);
    packet.push_back(HeaderByte2);

    if (payloadAndCsLen < 128U) {
        // Normal 1-byte length
        packet.push_back(static_cast<std::uint8_t>(payloadAndCsLen));
    } else {
        // Extended 2-byte length: bit 7 set on low byte
        const auto lenLow = static_cast<std::uint8_t>((payloadAndCsLen & 0x7FU) | 0x80U);
        const auto lenHigh = static_cast<std::uint8_t>((payloadAndCsLen >> 7U) & 0xFFU);
        packet.push_back(lenLow);
        packet.push_back(lenHigh);
    }

    const std::size_t crcStartIdx { packet.size() };
    packet.push_back(static_cast<std::uint8_t>(id));
    packet.insert(packet.end(), payload.begin(), payload.end());

    const std::uint8_t crc { SightlineCrc8::compute(packet.data() + crcStartIdx, packet.size() - crcStartIdx) };
    packet.push_back(crc);

    return packet;
}

std::size_t SightlineFraming::getHeaderLength(ByteView packet) noexcept
{
    if (packet.size() < 3U) {
        return 0U;
    }
    if ((packet[2U] & 0x80U) == 0U) {
        return 3U; // 1-byte length
    }
    if (packet.size() >= 4U) {
        return 4U; // 2-byte extended length
    }
    return 0U;
}

MessageId SightlineFraming::identifyMessage(ByteView packet) noexcept
{
    const std::size_t hLen { getHeaderLength(packet) };
    if (hLen == 0U || packet.size() <= hLen) {
        return MessageId::Unknown;
    }
    return static_cast<MessageId>(packet[hLen]);
}

ByteView SightlineFraming::extractPayload(ByteView packet) noexcept
{
    const std::size_t hLen { getHeaderLength(packet) };
    // Packet must have header + MessageId (1) + Checksum (1)
    if (hLen == 0U || packet.size() < (hLen + 2U)) {
        return {};
    }
    // Payload is strictly between MessageId and Checksum
    return packet.subspan(hLen + 1U, packet.size() - (hLen + 2U));
}

void SightlineFraming::appendU16Le(std::vector<std::uint8_t>& buf, std::uint16_t val)
{
    buf.push_back(static_cast<std::uint8_t>(val & 0xFFU));
    buf.push_back(static_cast<std::uint8_t>((val >> 8U) & 0xFFU));
}

void SightlineFraming::appendS16Le(std::vector<std::uint8_t>& buf, std::int16_t val)
{
    appendU16Le(buf, static_cast<std::uint16_t>(val));
}

void SightlineFraming::appendU32Le(std::vector<std::uint8_t>& buf, std::uint32_t val)
{
    buf.push_back(static_cast<std::uint8_t>(val & 0xFFU));
    buf.push_back(static_cast<std::uint8_t>((val >> 8U) & 0xFFU));
    buf.push_back(static_cast<std::uint8_t>((val >> 16U) & 0xFFU));
    buf.push_back(static_cast<std::uint8_t>((val >> 24U) & 0xFFU));
}

void SightlineFraming::appendS32Le(std::vector<std::uint8_t>& buf, std::int32_t val)
{
    appendU32Le(buf, static_cast<std::uint32_t>(val));
}

void SightlineFraming::appendU64Le(std::vector<std::uint8_t>& buf, std::uint64_t val)
{
    appendU32Le(buf, static_cast<std::uint32_t>(val & 0xFFFFFFFFU));
    appendU32Le(buf, static_cast<std::uint32_t>((val >> 32U) & 0xFFFFFFFFU));
}

void SightlineFraming::appendFloat32Le(std::vector<std::uint8_t>& buf, float val)
{
    std::uint32_t raw { 0U };
    std::memcpy(&raw, &val, sizeof(float));
    appendU32Le(buf, raw);
}

void SightlineFraming::appendDouble64Le(std::vector<std::uint8_t>& buf, double val)
{
    std::uint64_t raw { 0U };
    std::memcpy(&raw, &val, sizeof(double));
    appendU64Le(buf, raw);
}

void SightlineFraming::appendString(std::vector<std::uint8_t>& buf, const std::string& str)
{
    buf.insert(buf.end(), str.begin(), str.end());
    buf.push_back(0x00U); // Null terminator
}

std::uint16_t SightlineFraming::readU16Le(const std::uint8_t* ptr) noexcept
{
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(ptr[0]) | (static_cast<std::uint16_t>(ptr[1]) << 8U));
}

std::int16_t SightlineFraming::readS16Le(const std::uint8_t* ptr) noexcept
{
    return static_cast<std::int16_t>(readU16Le(ptr));
}

std::uint32_t SightlineFraming::readU32Le(const std::uint8_t* ptr) noexcept
{
    return static_cast<std::uint32_t>(ptr[0]) | (static_cast<std::uint32_t>(ptr[1]) << 8U)
        | (static_cast<std::uint32_t>(ptr[2]) << 16U) | (static_cast<std::uint32_t>(ptr[3]) << 24U);
}

std::int32_t SightlineFraming::readS32Le(const std::uint8_t* ptr) noexcept
{
    return static_cast<std::int32_t>(readU32Le(ptr));
}

std::uint64_t SightlineFraming::readU64Le(const std::uint8_t* ptr) noexcept
{
    return static_cast<std::uint64_t>(readU32Le(ptr)) | (static_cast<std::uint64_t>(readU32Le(ptr + 4U)) << 32U);
}

float SightlineFraming::readFloat32Le(const std::uint8_t* ptr) noexcept
{
    const std::uint32_t raw { readU32Le(ptr) };
    float val { 0.0F };
    std::memcpy(&val, &raw, sizeof(float));
    return val;
}

double SightlineFraming::readDouble64Le(const std::uint8_t* ptr) noexcept
{
    const std::uint64_t raw { readU64Le(ptr) };
    double val { 0.0 };
    std::memcpy(&val, &raw, sizeof(double));
    return val;
}

} // namespace Sightline
