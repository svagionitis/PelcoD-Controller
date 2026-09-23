#include "KlvBer.h"

namespace Klv {

std::size_t KlvBer::encodedLengthSize(std::size_t length) noexcept {
    if (length < 128U) {
        return 1U;
    }
    if (length <= 0xFFU) {
        return 2U; // 0x81 + 1 byte
    }
    if (length <= 0xFFFFU) {
        return 3U; // 0x82 + 2 bytes
    }
    if (length <= 0xFFFFFFU) {
        return 4U; // 0x83 + 3 bytes
    }
    return 5U; // 0x84 + 4 bytes
}

void KlvBer::encodeLength(std::size_t length, std::vector<std::uint8_t>& out) {
    if (length < 128U) {
        out.push_back(static_cast<std::uint8_t>(length));
    } else if (length <= 0xFFU) {
        out.push_back(0x81U);
        out.push_back(static_cast<std::uint8_t>(length));
    } else if (length <= 0xFFFFU) {
        out.push_back(0x82U);
        out.push_back(static_cast<std::uint8_t>((length >> 8U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>(length & 0xFFU));
    } else if (length <= 0xFFFFFFU) {
        out.push_back(0x83U);
        out.push_back(static_cast<std::uint8_t>((length >> 16U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>((length >> 8U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>(length & 0xFFU));
    } else {
        out.push_back(0x84U);
        out.push_back(static_cast<std::uint8_t>((length >> 24U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>((length >> 16U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>((length >> 8U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>(length & 0xFFU));
    }
}

bool KlvBer::decodeLength(const std::uint8_t* data,
                          std::size_t size,
                          std::size_t& decodedLength,
                          std::size_t& bytesConsumed) noexcept {
    if (data == nullptr || size == 0U) {
        return false;
    }

    const std::uint8_t initialByte = data[0];
    if ((initialByte & 0x80U) == 0U) {
        // Short form: 1 byte
        decodedLength = static_cast<std::size_t>(initialByte);
        bytesConsumed = 1U;
        return true;
    }

    const std::size_t numBytes = initialByte & 0x7FU;
    if (numBytes == 0U || numBytes > 4U) {
        // 0x80 = Indefinite form (disallowed in SMPTE 336M / ST 0601), > 4 bytes unsupported
        return false;
    }

    if (size < 1U + numBytes) {
        return false; // Underflow
    }

    std::size_t val { 0U };
    for (std::size_t i = 1U; i <= numBytes; ++i) {
        val = (val << 8U) | static_cast<std::size_t>(data[i]);
    }

    decodedLength = val;
    bytesConsumed = 1U + numBytes;
    return true;
}

std::size_t KlvBer::encodedTagSize(std::uint32_t tag) noexcept {
    if (tag < 128U) {
        return 1U;
    }
    if (tag < 16384U) {
        return 2U;
    }
    if (tag < 2097152U) {
        return 3U;
    }
    if (tag < 268435456U) {
        return 4U;
    }
    return 5U;
}

void KlvBer::encodeTag(std::uint32_t tag, std::vector<std::uint8_t>& out) {
    if (tag < 128U) {
        out.push_back(static_cast<std::uint8_t>(tag));
        return;
    }

    std::uint8_t buffer[5];
    std::size_t count = 0;
    buffer[count++] = static_cast<std::uint8_t>(tag & 0x7FU);
    tag >>= 7U;

    while (tag > 0U && count < 5U) {
        buffer[count++] = static_cast<std::uint8_t>((tag & 0x7FU) | 0x80U);
        tag >>= 7U;
    }

    for (std::size_t i = count; i > 0U; --i) {
        out.push_back(buffer[i - 1U]);
    }
}

bool KlvBer::decodeTag(const std::uint8_t* data,
                       std::size_t size,
                       std::uint32_t& decodedTag,
                       std::size_t& bytesConsumed) noexcept {
    if (data == nullptr || size == 0U) {
        return false;
    }

    std::uint32_t val { 0U };
    std::size_t consumed { 0U };

    while (consumed < size && consumed < 5U) {
        const std::uint8_t b = data[consumed++];
        val = (val << 7U) | static_cast<std::uint32_t>(b & 0x7FU);
        if ((b & 0x80U) == 0U) {
            decodedTag = val;
            bytesConsumed = consumed;
            return true;
        }
    }

    return false;
}

} // namespace Klv
