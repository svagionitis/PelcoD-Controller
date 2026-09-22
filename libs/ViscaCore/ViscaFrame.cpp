#include "ViscaFrame.h"

#include <iomanip>
#include <sstream>
#include <utility>

namespace Visca {

ViscaFrame::ViscaFrame(std::vector<uint8_t> bytes)
    : m_bytes(std::move(bytes))
{
}

ViscaFrame::ViscaFrame(std::initializer_list<uint8_t> init)
    : m_bytes(init)
{
}

ViscaFrame::ViscaFrame(const uint8_t* data, size_t length)
{
    if (data != nullptr && length > 0) {
        m_bytes.assign(data, data + length);
    }
}

bool ViscaFrame::isValid() const noexcept
{
    return m_bytes.size() >= kMinPacketLength && m_bytes.size() <= kMaxPacketLength
        && m_bytes.back() == kViscaTerminator && (m_bytes[0] & 0x80) != 0;
}

uint8_t ViscaFrame::sourceAddress() const noexcept
{
    if (m_bytes.empty()) {
        return 0;
    }
    return static_cast<uint8_t>((m_bytes[0] >> 4) & 0x07);
}

uint8_t ViscaFrame::destinationAddress() const noexcept
{
    if (m_bytes.empty()) {
        return 0;
    }
    return static_cast<uint8_t>(m_bytes[0] & 0x0F);
}

bool ViscaFrame::isBroadcast() const noexcept
{
    if (m_bytes.empty()) {
        return false;
    }
    return (m_bytes[0] & 0x0F) == kBroadcastAddress;
}

ViscaMessageType ViscaFrame::messageType() const noexcept
{
    if (!isValid()) {
        return ViscaMessageType::Unknown;
    }

    if (m_bytes[0] == 0x88 && m_bytes.size() == 4 && m_bytes[1] == 0x30) {
        return ViscaMessageType::AddressSet;
    }

    // Controller command dispatch (source == 0)
    if (sourceAddress() == kControllerAddress) {
        if (m_bytes[1] == 0x09) {
            return ViscaMessageType::Inquiry;
        }
        return ViscaMessageType::Command;
    }

    // Camera responses (source != 0)
    if (isAck()) {
        return ViscaMessageType::Ack;
    }
    if (isCompletion()) {
        return ViscaMessageType::Completion;
    }
    if (isError()) {
        return ViscaMessageType::Error;
    }
    if (isNetworkChange()) {
        return ViscaMessageType::NetworkChange;
    }

    return ViscaMessageType::Unknown;
}

bool ViscaFrame::isAck() const noexcept
{
    return m_bytes.size() == 3 && (m_bytes[1] & 0xF0) == 0x40 && m_bytes[2] == kViscaTerminator;
}

bool ViscaFrame::isCompletion() const noexcept
{
    if (m_bytes.size() < 3 || m_bytes.back() != kViscaTerminator) {
        return false;
    }
    // Command completion: y0 5s FF (size == 3, socket 1 or 2)
    // Inquiry completion: y0 50 ... FF (size >= 3)
    return (m_bytes[1] & 0xF0) == 0x50;
}

bool ViscaFrame::isError() const noexcept
{
    return m_bytes.size() == 4 && (m_bytes[1] & 0xF0) == 0x60 && m_bytes[3] == kViscaTerminator;
}

bool ViscaFrame::isInquiryResponse() const noexcept
{
    return m_bytes.size() >= 4 && m_bytes[1] == 0x50 && m_bytes.back() == kViscaTerminator;
}

bool ViscaFrame::isNetworkChange() const noexcept
{
    return m_bytes.size() == 3 && m_bytes[1] == 0x38 && m_bytes[2] == kViscaTerminator;
}

ViscaSocket ViscaFrame::socket() const noexcept
{
    if (m_bytes.size() < 2) {
        return ViscaSocket::None;
    }
    const uint8_t highNibble = static_cast<uint8_t>(m_bytes[1] & 0xF0);
    const uint8_t lowNibble = static_cast<uint8_t>(m_bytes[1] & 0x0F);

    if (highNibble == 0x40 || highNibble == 0x50 || highNibble == 0x60) {
        if (lowNibble == 1) {
            return ViscaSocket::Socket1;
        }
        if (lowNibble == 2) {
            return ViscaSocket::Socket2;
        }
    }
    return ViscaSocket::None;
}

ViscaErrorCode ViscaFrame::errorCode() const noexcept
{
    if (isError() && m_bytes.size() >= 3) {
        return static_cast<ViscaErrorCode>(m_bytes[2]);
    }
    return ViscaErrorCode::None;
}

std::vector<uint8_t> ViscaFrame::packWordNibbles(uint16_t value)
{
    return { static_cast<uint8_t>((value >> 12) & 0x0F), static_cast<uint8_t>((value >> 8) & 0x0F),
        static_cast<uint8_t>((value >> 4) & 0x0F), static_cast<uint8_t>(value & 0x0F) };
}

uint16_t ViscaFrame::unpackWordNibbles(const uint8_t* data) noexcept
{
    if (data == nullptr) {
        return 0;
    }
    return static_cast<uint16_t>(
        ((data[0] & 0x0F) << 12) | ((data[1] & 0x0F) << 8) | ((data[2] & 0x0F) << 4) | (data[3] & 0x0F));
}

std::vector<uint8_t> ViscaFrame::packByteNibbles(uint8_t value)
{
    return { static_cast<uint8_t>((value >> 4) & 0x0F), static_cast<uint8_t>(value & 0x0F) };
}

uint8_t ViscaFrame::unpackByteNibbles(const uint8_t* data) noexcept
{
    if (data == nullptr) {
        return 0;
    }
    return static_cast<uint8_t>(((data[0] & 0x0F) << 4) | (data[1] & 0x0F));
}

std::string ViscaFrame::toHexString() const
{
    if (m_bytes.empty()) {
        return {};
    }
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0');
    for (size_t i = 0; i < m_bytes.size(); ++i) {
        if (i > 0) {
            oss << ' ';
        }
        oss << std::setw(2) << static_cast<unsigned int>(m_bytes[i]);
    }
    return oss.str();
}

namespace {

constexpr int hexCharToNibble(char c) noexcept
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

} // namespace

ViscaFrame ViscaFrame::fromHexString(std::string_view hexStr)
{
    std::vector<uint8_t> bytes {};
    int highNibble = -1;

    for (size_t i = 0; i < hexStr.size(); ++i) {
        const char c = hexStr[i];
        if (c == '0' && (i + 1 < hexStr.size()) && (hexStr[i + 1] == 'x' || hexStr[i + 1] == 'X')) {
            ++i;
            continue;
        }

        const int val = hexCharToNibble(c);
        if (val < 0) {
            continue;
        }

        if (highNibble < 0) {
            highNibble = val;
        } else {
            bytes.push_back(static_cast<uint8_t>((highNibble << 4) | val));
            highNibble = -1;
        }
    }

    return ViscaFrame(std::move(bytes));
}

} // namespace Visca
