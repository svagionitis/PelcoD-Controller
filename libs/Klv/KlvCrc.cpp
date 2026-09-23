#include "KlvCrc.h"
#include <array>

namespace Klv {

namespace {

/// @brief Generates the 256-entry CRC-16-CCITT table at compile-time.
constexpr std::array<std::uint16_t, 256> generateCrcTable() noexcept {
    std::array<std::uint16_t, 256> table {};
    for (std::size_t i = 0U; i < 256U; ++i) {
        std::uint16_t curr = static_cast<std::uint16_t>(i << 8U);
        for (std::size_t bit = 0U; bit < 8U; ++bit) {
            if ((curr & 0x8000U) != 0U) {
                curr = static_cast<std::uint16_t>((curr << 1U) ^ 0x1021U);
            } else {
                curr = static_cast<std::uint16_t>(curr << 1U);
            }
        }
        table[i] = curr;
    }
    return table;
}

constexpr auto kCrcTable = generateCrcTable();

} // namespace

std::uint16_t KlvCrc::calculate(const std::uint8_t* data, std::size_t size) noexcept {
    if (data == nullptr || size == 0U) {
        return 0U;
    }

    std::uint16_t crc { 0U };
    for (std::size_t i = 0U; i < size; ++i) {
        const std::uint8_t idx = static_cast<std::uint8_t>((crc >> 8U) ^ data[i]);
        crc = static_cast<std::uint16_t>((crc << 8U) ^ kCrcTable[idx]);
    }
    return crc;
}

bool KlvCrc::verifyPacket(const std::uint8_t* packet, std::size_t packetSize) noexcept {
    if (packet == nullptr || packetSize < 4U) {
        return false;
    }

    // A valid MISB ST 0601 packet evaluated with its 2-byte CRC yields 0x0000.
    return calculate(packet, packetSize) == 0x0000U;
}

std::uint16_t KlvCrc::computeChecksumForTag1(const std::uint8_t* packetWithoutCrc,
                                             std::size_t size) noexcept {
    return calculate(packetWithoutCrc, size);
}

} // namespace Klv
