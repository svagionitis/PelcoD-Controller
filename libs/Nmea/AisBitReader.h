#pragma once

/// @file AisBitReader.h
/// @brief Fast, zero-allocation bitstream reader for ITU-R M.1371 6-bit armored ASCII payloads.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Nmea {

/// @class AisBitReader
/// @brief Sequential bit extractor reading variable-length signed and unsigned integers and ASCII strings.
class AisBitReader {
public:
    /// @brief Converts a 6-bit armored ASCII character to a 6-bit unsigned integer (0-63).
    /// @param[in] c ASCII character in range ['0' .. 'w'].
    /// @return 6-bit value in range [0 .. 63].
    [[nodiscard]] static constexpr std::uint8_t charToSixBit(char c) noexcept
    {
        const auto uc = static_cast<unsigned char>(c);
        if (uc < 48U || uc > 119U) {
            return 0U;
        }
        std::uint8_t val = static_cast<std::uint8_t>(uc - 48U);
        if (val > 40U) {
            val = static_cast<std::uint8_t>(val - 8U);
        }
        return static_cast<std::uint8_t>(val & 0x3FU);
    }

    /// @brief Converts a 6-bit value (0-63) to its corresponding 8-bit ASCII character.
    /// @param[in] val 6-bit value.
    /// @return Decoded ASCII character.
    [[nodiscard]] static constexpr char sixBitToAscii(std::uint8_t val) noexcept
    {
        val &= 0x3FU;
        if (val < 32U) {
            return static_cast<char>(val + 64U); // '@', 'A'-'_'
        }
        return static_cast<char>(val); // ' ', '0'-'?'
    }

    /// @brief Constructs bit reader from an armored ASCII payload string.
    /// @param[in] armoredPayload String view of 6-bit ASCII payload characters.
    /// @param[in] fillBits Number of padding bits at the end of the payload (0-5).
    explicit AisBitReader(std::string_view armoredPayload, std::size_t fillBits = 0U)
        : m_totalBits { (armoredPayload.size() * 6U > fillBits) ? (armoredPayload.size() * 6U - fillBits) : 0U }
    {
        m_bytes.reserve(armoredPayload.size());
        for (const char c : armoredPayload) {
            m_bytes.push_back(charToSixBit(c));
        }
    }

    /// @brief Constructs bit reader from pre-unpacked 6-bit byte vector.
    /// @param[in] bytes Vector of 6-bit values.
    /// @param[in] fillBits Padding bits to exclude.
    explicit AisBitReader(std::vector<std::uint8_t> bytes, std::size_t fillBits = 0U)
        : m_bytes { std::move(bytes) }
        , m_totalBits { (m_bytes.size() * 6U > fillBits) ? (m_bytes.size() * 6U - fillBits) : 0U }
    {
    }

    /// @brief Total available bits in the payload.
    [[nodiscard]] std::size_t totalBits() const noexcept
    {
        return m_totalBits;
    }

    /// @brief Remaining unread bits.
    [[nodiscard]] std::size_t remainingBits() const noexcept
    {
        return (m_bitPos < m_totalBits) ? (m_totalBits - m_bitPos) : 0U;
    }

    /// @brief Current bit reading position.
    [[nodiscard]] std::size_t bitPosition() const noexcept
    {
        return m_bitPos;
    }

    /// @brief Seeks to a specific absolute bit offset.
    /// @param[in] pos Target bit offset.
    void seekBit(std::size_t pos) noexcept
    {
        m_bitPos = (pos <= m_totalBits) ? pos : m_totalBits;
    }

    /// @brief Reads an unsigned integer of up to 32 bits, MSB first.
    /// @param[in] numBits Number of bits to read (1 to 32).
    /// @return Unpacked unsigned integer.
    [[nodiscard]] std::uint32_t readBits(std::size_t numBits) noexcept
    {
        if (numBits == 0U || numBits > 32U || m_bitPos >= m_totalBits) {
            return 0U;
        }

        std::uint32_t result { 0U };
        const std::size_t count = (numBits <= remainingBits()) ? numBits : remainingBits();

        for (std::size_t i { 0U }; i < count; ++i) {
            const std::size_t byteIdx = m_bitPos / 6U;
            const std::size_t bitInByte = 5U - (m_bitPos % 6U);
            const std::uint32_t bit = (m_bytes[byteIdx] >> bitInByte) & 0x01U;
            result = (result << 1U) | bit;
            ++m_bitPos;
        }

        // If requested more bits than available, pad with zeros
        if (numBits > count) {
            result <<= (numBits - count);
        }

        return result;
    }

    /// @brief Reads a signed 2's complement integer of up to 32 bits.
    /// @param[in] numBits Number of bits to read (1 to 32).
    /// @return Sign-extended signed integer.
    [[nodiscard]] std::int32_t readSignedBits(std::size_t numBits) noexcept
    {
        if (numBits == 0U || numBits > 32U) {
            return 0;
        }
        const std::uint32_t rawVal = readBits(numBits);
        if (numBits == 32U) {
            return static_cast<std::int32_t>(rawVal);
        }
        // Check sign bit (MSB)
        const std::uint32_t signMask = 1U << (numBits - 1U);
        if ((rawVal & signMask) != 0U) {
            // Sign extend upper bits with 1s
            const std::uint32_t extensionMask = ~((1U << numBits) - 1U);
            return static_cast<std::int32_t>(rawVal | extensionMask);
        }
        return static_cast<std::int32_t>(rawVal);
    }

    /// @brief Reads a 6-bit ASCII string of specified character count and strips trailing '@' and whitespace.
    /// @param[in] numChars Number of 6-bit characters to extract.
    /// @return Trimmed ASCII string.
    [[nodiscard]] std::string readString(std::size_t numChars) noexcept
    {
        std::string str {};
        str.reserve(numChars);

        for (std::size_t i { 0U }; i < numChars; ++i) {
            const auto val = static_cast<std::uint8_t>(readBits(6U));
            str.push_back(sixBitToAscii(val));
        }

        // Trim trailing '@', spaces, and control characters
        while (!str.empty() && (str.back() == '@' || str.back() == ' ' || str.back() == '\0')) {
            str.pop_back();
        }

        return str;
    }

private:
    std::vector<std::uint8_t> m_bytes {};
    std::size_t m_totalBits { 0U };
    std::size_t m_bitPos { 0U };
};

} // namespace Nmea
