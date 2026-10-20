#include "KlvCrc.h"

#include <cstddef>
#include <cstdint>

#include <array>

namespace Klv {

namespace {

/// @brief Generates the 256-entry CRC-16-CCITT table at compile-time.
constexpr std::array<std::uint16_t, 256> generateCrcTable() noexcept {
    std::array<std::uint16_t, 256> table {};
    for (std::size_t i = 0U; i < 256U; ++i) {
        auto curr = static_cast<std::uint32_t>(i << 8U);
        for (std::size_t bit = 0U; bit < 8U; ++bit) {
            if ((curr & 0x8000U) != 0U) {
                curr = (curr << 1U) ^ 0x1021U;
            } else {
                curr = curr << 1U;
            }
        }
        table[i] = static_cast<std::uint16_t>(curr);
    }
    return table;
}

constexpr auto kCrcTable = generateCrcTable();

} // namespace

std::uint16_t KlvCrc::calculate(const std::uint8_t* data, std::size_t size) noexcept {
    if (data == nullptr || size == 0U) {
        return 0U;
    }

    std::uint32_t crc { 0U };
    for (std::size_t i = 0U; i < size; ++i) {
        const auto idx = static_cast<std::size_t>((crc >> 8U) ^ static_cast<std::uint32_t>(data[i]));
        crc = ((crc << 8U) ^ static_cast<std::uint32_t>(kCrcTable[idx])) & 0xFFFFU;
    }
    return static_cast<std::uint16_t>(crc);
}

std::uint16_t KlvCrc::computeBcc16(const std::uint8_t* data, std::size_t size) noexcept {
    if (data == nullptr || size == 0U) {
        return 0U;
    }

    std::uint32_t bcc { 0U };
    for (std::size_t i = 0U; i < size; ++i) {
        const auto shift = static_cast<std::uint32_t>(8U * ((i + 1U) % 2U));
        bcc = (bcc + (static_cast<std::uint32_t>(data[i]) << shift)) & 0xFFFFU;
    }
    return static_cast<std::uint16_t>(bcc);
}

bool KlvCrc::verifyPacket(const std::uint8_t* packet, std::size_t packetSize) noexcept {
    if (packet == nullptr || packetSize < 4U) {
        return false;
    }

    // 1. Check MISB ST 0601 BCC-16 checksum
    // Tag 1 (Checksum) stores a 2-byte big-endian value at the end of the packet.
    const auto storedChecksum = static_cast<std::uint16_t>(
        (static_cast<std::uint32_t>(packet[packetSize - 2U]) << 8U) |
        static_cast<std::uint32_t>(packet[packetSize - 1U]));

    const std::uint16_t calculatedBcc = computeBcc16(packet, packetSize - 2U);
    if (calculatedBcc == storedChecksum) {
        return true;
    }

    // 2. Check MISB ST 0903 / CCITT CRC-16 (evaluates to 0x0000 over entire packet)
    return calculate(packet, packetSize) == 0x0000U;
}

std::uint16_t KlvCrc::computeChecksumForTag1(const std::uint8_t* packetWithoutCrc,
                                             std::size_t size) noexcept {
    return computeBcc16(packetWithoutCrc, size);
}

namespace {

/// @brief Generates the 256-entry CRC-32 MPEG-2 (ISO/IEC 13818-1) table at compile-time.
constexpr std::array<std::uint32_t, 256> generateCrc32MpegTable() noexcept {
    std::array<std::uint32_t, 256> table {};
    for (std::size_t i = 0U; i < 256U; ++i) {
        std::uint32_t curr = static_cast<std::uint32_t>(i << 24U);
        for (std::size_t bit = 0U; bit < 8U; ++bit) {
            if ((curr & 0x80000000U) != 0U) {
                curr = (curr << 1U) ^ 0x04C11DB7U;
            } else {
                curr = (curr << 1U);
            }
        }
        table[i] = curr;
    }
    return table;
}

constexpr auto kCrc32MpegTable = generateCrc32MpegTable();

} // namespace

std::uint32_t KlvCrc::calculateCrc32Mpeg(const std::uint8_t* data, std::size_t size) noexcept {
    if (data == nullptr || size == 0U) {
        return 0U;
    }

    std::uint32_t crc { 0xFFFFFFFFU };
    for (std::size_t i = 0U; i < size; ++i) {
        const auto idx = static_cast<std::uint8_t>((crc >> 24U) ^ data[i]);
        crc = (crc << 8U) ^ kCrc32MpegTable[idx];
    }
    return crc;
}

} // namespace Klv
