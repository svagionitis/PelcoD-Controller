#pragma once

/// @file Base64.h
/// @brief Self-contained, portable RFC 4648 Base64 encoder and decoder (zero external dependencies).

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Nmea::Network {

/// @class Base64
/// @brief RFC 4648 compliant Base64 encoding and decoding utility.
class Base64 {
public:
    /// @brief Encodes raw byte buffer into a Base64 ASCII string.
    /// @param[in] data Span or vector of bytes.
    /// @return Standard Base64 encoded string with padding.
    [[nodiscard]] static std::string encode(const std::vector<std::uint8_t>& data)
    {
        return encode(std::string_view { reinterpret_cast<const char*>(data.data()), data.size() });
    }

    /// @brief Encodes a string view or character buffer into a Base64 ASCII string.
    /// @param[in] text Input string view.
    /// @return Standard Base64 encoded string with padding.
    [[nodiscard]] static std::string encode(std::string_view text)
    {
        static constexpr char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string result {};
        const std::size_t len { text.size() };
        result.reserve(((len + 2U) / 3U) * 4U);

        for (std::size_t i { 0U }; i < len; i += 3U) {
            const auto b0 = static_cast<std::uint32_t>(static_cast<std::uint8_t>(text[i]));
            const auto b1 = (i + 1U < len) ? static_cast<std::uint32_t>(static_cast<std::uint8_t>(text[i + 1U])) : 0U;
            const auto b2 = (i + 2U < len) ? static_cast<std::uint32_t>(static_cast<std::uint8_t>(text[i + 2U])) : 0U;

            const std::uint32_t triple { (b0 << 16U) | (b1 << 8U) | b2 };

            result.push_back(kTable[(triple >> 18U) & 0x3FU]);
            result.push_back(kTable[(triple >> 12U) & 0x3FU]);

            if (i + 1U < len) {
                result.push_back(kTable[(triple >> 6U) & 0x3FU]);
            } else {
                result.push_back('=');
            }

            if (i + 2U < len) {
                result.push_back(kTable[triple & 0x3FU]);
            } else {
                result.push_back('=');
            }
        }
        return result;
    }

    /// @brief Decodes a Base64 ASCII string into raw binary bytes.
    /// @param[in] base64Text Input Base64 string (ignoring whitespace).
    /// @return Decoded byte vector.
    [[nodiscard]] static std::vector<std::uint8_t> decode(std::string_view base64Text)
    {
        std::vector<std::uint8_t> result {};
        result.reserve((base64Text.size() * 3U) / 4U);

        std::uint32_t buffer { 0U };
        int bitsCollected { 0 };

        for (const char c : base64Text) {
            if (c == ' ' || c == '\r' || c == '\n' || c == '\t') {
                continue;
            }
            if (c == '=') {
                break;
            }

            const int val { decodeChar(c) };
            if (val < 0) {
                continue; // Ignore invalid characters
            }

            buffer = (buffer << 6U) | static_cast<std::uint32_t>(val);
            bitsCollected += 6;

            if (bitsCollected >= 8) {
                bitsCollected -= 8;
                result.push_back(static_cast<std::uint8_t>((buffer >> static_cast<unsigned int>(bitsCollected)) & 0xFFU));
            }
        }
        return result;
    }

private:
    [[nodiscard]] static int decodeChar(char c) noexcept
    {
        if (c >= 'A' && c <= 'Z') {
            return c - 'A';
        }
        if (c >= 'a' && c <= 'z') {
            return c - 'a' + 26;
        }
        if (c >= '0' && c <= '9') {
            return c - '0' + 52;
        }
        if (c == '+') {
            return 62;
        }
        if (c == '/') {
            return 63;
        }
        return -1;
    }
};

} // namespace Nmea::Network
