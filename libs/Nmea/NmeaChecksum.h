#pragma once

/// @file NmeaChecksum.h
/// @brief High-performance constexpr 8-bit XOR checksum calculation and validation for NMEA 0183 sentences.

#include <cstdint>
#include <string>
#include <string_view>

namespace Nmea {

/// @class NmeaChecksum
/// @brief Checksum utility verifying and computing standard NMEA 0183 8-bit XOR checksums.
class NmeaChecksum {
public:
    /// @brief Computes 8-bit XOR checksum over sentence characters between start delimiter and asterisk.
    /// @param[in] sentence String view containing NMEA sentence.
    /// @return Computed 8-bit XOR checksum value.
    [[nodiscard]] static constexpr std::uint8_t calculate(std::string_view sentence) noexcept
    {
        std::size_t startIdx { 0U };
        if (!sentence.empty() && (sentence.front() == '$' || sentence.front() == '!')) {
            startIdx = 1U;
        }

        std::uint8_t crc { 0U };
        for (std::size_t i { startIdx }; i < sentence.size(); ++i) {
            const char c { sentence[i] };
            if (c == '*' || c == '\r' || c == '\n') {
                break;
            }
            crc ^= static_cast<std::uint8_t>(c);
        }
        return crc;
    }

    /// @brief Converts two hexadecimal ASCII characters to a byte value.
    /// @param[in] hi Upper nibble hex char (0-9, A-F, a-f).
    /// @param[in] lo Lower nibble hex char (0-9, A-F, a-f).
    /// @param[out] outByte Output byte.
    /// @return True if both characters were valid hexadecimal.
    [[nodiscard]] static constexpr bool parseHexByte(char hi, char lo, std::uint8_t& outByte) noexcept
    {
        auto hexVal = [](char c) noexcept -> int {
            if (c >= '0' && c <= '9') {
                return c - '0';
            }
            if (c >= 'A' && c <= 'F') {
                return c - 'A' + 10;
            }
            if (c >= 'a' && c <= 'f') {
                return c - 'a' + 10;
            }
            return -1;
        };

        const int h { hexVal(hi) };
        const int l { hexVal(lo) };
        if (h < 0 || l < 0) {
            return false;
        }
        outByte = static_cast<std::uint8_t>((static_cast<unsigned int>(h) << 4U) | static_cast<unsigned int>(l));
        return true;
    }

    /// @brief Validates an NMEA sentence against its trailing *HH checksum.
    /// @param[in] sentence Complete NMEA sentence, e.g. "$GPGGA,...*4F\r\n" or "\\s:...*hh\\$GPGGA,...*4F\r\n".
    /// @return True if sentence possesses a valid asterisk followed by matching two-digit hex checksum.
    [[nodiscard]] static constexpr bool validate(std::string_view sentence) noexcept
    {
        if (!sentence.empty() && sentence.front() == '\\') {
            const auto secondBackslash = sentence.find('\\', 1U);
            if (secondBackslash != std::string_view::npos) {
                sentence = sentence.substr(secondBackslash + 1U);
                while (!sentence.empty() && (sentence.front() == ' ' || sentence.front() == '\t')) {
                    sentence.remove_prefix(1U);
                }
            }
        }

        const auto starPos = sentence.rfind('*');
        if (starPos == std::string_view::npos || starPos + 2U >= sentence.size()) {
            return false;
        }

        std::uint8_t expectedCrc { 0U };
        if (!parseHexByte(sentence[starPos + 1U], sentence[starPos + 2U], expectedCrc)) {
            return false;
        }

        const std::uint8_t calculatedCrc { calculate(sentence.substr(0U, starPos)) };
        return calculatedCrc == expectedCrc;
    }

    /// @brief Formats an 8-bit byte into two uppercase hexadecimal characters.
    /// @param[in] value Byte value to format.
    /// @return Two-character uppercase hex string (e.g. "4F").
    [[nodiscard]] static std::string toHexString(std::uint8_t value)
    {
        static constexpr char HexDigits[] { "0123456789ABCDEF" };
        std::string result {};
        result.push_back(HexDigits[(value >> 4U) & 0x0FU]);
        result.push_back(HexDigits[value & 0x0FU]);
        return result;
    }

    /// @brief Appends *HH\\r\\n checksum to an unchecksummed sentence payload.
    /// @param[in] payload Sentence payload with or without leading $/! (e.g. "GPGGA,...").
    /// @param[in] prefix Optional leading delimiter ('$' or '!').
    /// @return Fully framed NMEA sentence with checksum and line terminators.
    [[nodiscard]] static std::string frameSentence(std::string_view payload, char prefix = '$')
    {
        std::string body {};
        body.reserve(payload.size() + 8U);
        if (payload.empty() || (payload.front() != '$' && payload.front() != '!')) {
            body.push_back(prefix);
        }
        body.append(payload);

        // Strip any existing *HH or \r\n if present
        const auto starPos = body.find('*');
        if (starPos != std::string::npos) {
            body.resize(starPos);
        }

        const std::uint8_t crc { calculate(body) };
        body.push_back('*');
        body.append(toHexString(crc));
        body.append("\r\n");
        return body;
    }
};

} // namespace Nmea
