#pragma once

/// @file Sha1.h
/// @brief Self-contained, portable RFC 3174 SHA-1 cryptographic hash implementation (zero external dependencies).

#include "Base64.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace Nmea::Network {

/// @class Sha1
/// @brief RFC 3174 compliant SHA-1 message digest engine and WebSocket accept token generator.
class Sha1 {
public:
    using Digest = std::array<std::uint8_t, 20>;

    /// @brief Computes raw 20-byte binary SHA-1 digest from a byte buffer or string view.
    /// @param[in] input Input text or data buffer.
    /// @return 20-byte SHA-1 digest array.
    [[nodiscard]] static Digest compute(std::string_view input) noexcept
    {
        std::uint32_t h0 { 0x67452301U };
        std::uint32_t h1 { 0xEFCDAB89U };
        std::uint32_t h2 { 0x98BADCFEU };
        std::uint32_t h3 { 0x10325476U };
        std::uint32_t h4 { 0xC3D2E1F0U };

        const std::uint64_t bitLen { static_cast<std::uint64_t>(input.size()) * 8ULL };
        const std::size_t paddedLen { ((input.size() + 8U) / 64U + 1U) * 64U };

        // Process in 64-byte chunks
        std::array<std::uint32_t, 80> w {};

        for (std::size_t chunkStart { 0U }; chunkStart < paddedLen; chunkStart += 64U) {
            for (std::size_t i { 0U }; i < 16U; ++i) {
                std::uint32_t word { 0U };
                for (std::size_t j { 0U }; j < 4U; ++j) {
                    const std::size_t idx { chunkStart + i * 4U + j };
                    std::uint8_t byte { 0U };
                    if (idx < input.size()) {
                        byte = static_cast<std::uint8_t>(input[idx]);
                    } else if (idx == input.size()) {
                        byte = 0x80U;
                    } else if (idx >= paddedLen - 8U) {
                        const auto shift = static_cast<unsigned int>((paddedLen - 1U - idx) * 8U);
                        byte = static_cast<std::uint8_t>((bitLen >> shift) & 0xFFU);
                    }
                    word = (word << 8U) | static_cast<std::uint32_t>(byte);
                }
                w[i] = word;
            }

            for (std::size_t i { 16U }; i < 80U; ++i) {
                w[i] = rotl(w[i - 3U] ^ w[i - 8U] ^ w[i - 14U] ^ w[i - 16U], 1U);
            }

            std::uint32_t a { h0 };
            std::uint32_t b { h1 };
            std::uint32_t c { h2 };
            std::uint32_t d { h3 };
            std::uint32_t e { h4 };

            for (std::size_t i { 0U }; i < 80U; ++i) {
                std::uint32_t f { 0U };
                std::uint32_t k { 0U };
                if (i < 20U) {
                    f = (b & c) | ((~b) & d);
                    k = 0x5A827999U;
                } else if (i < 40U) {
                    f = b ^ c ^ d;
                    k = 0x6ED9EBA1U;
                } else if (i < 60U) {
                    f = (b & c) | (b & d) | (c & d);
                    k = 0x8F1BBCDCU;
                } else {
                    f = b ^ c ^ d;
                    k = 0xCA62C1D6U;
                }

                const std::uint32_t temp { rotl(a, 5U) + f + e + k + w[i] };
                e = d;
                d = c;
                c = rotl(b, 30U);
                b = a;
                a = temp;
            }

            h0 += a;
            h1 += b;
            h2 += c;
            h3 += d;
            h4 += e;
        }

        Digest digest {};
        const std::array<std::uint32_t, 5> state { h0, h1, h2, h3, h4 };
        for (std::size_t i { 0U }; i < 5U; ++i) {
            digest[i * 4U + 0U] = static_cast<std::uint8_t>((state[i] >> 24U) & 0xFFU);
            digest[i * 4U + 1U] = static_cast<std::uint8_t>((state[i] >> 16U) & 0xFFU);
            digest[i * 4U + 2U] = static_cast<std::uint8_t>((state[i] >> 8U) & 0xFFU);
            digest[i * 4U + 3U] = static_cast<std::uint8_t>(state[i] & 0xFFU);
        }
        return digest;
    }

    /// @brief Computes lowercase 40-character hexadecimal representation of SHA-1 hash.
    /// @param[in] input Input string view.
    /// @return 40-hex-character string.
    [[nodiscard]] static std::string computeHex(std::string_view input)
    {
        const Digest d { compute(input) };
        static constexpr char kHex[] = "0123456789abcdef";
        std::string hex {};
        hex.reserve(40U);
        for (const std::uint8_t b : d) {
            hex.push_back(kHex[(b >> 4U) & 0x0FU]);
            hex.push_back(kHex[b & 0x0FU]);
        }
        return hex;
    }

    /// @brief Computes RFC 6455 Sec-WebSocket-Accept response token from a client's Sec-WebSocket-Key.
    /// @param[in] clientKey Base64 encoded client nonce from HTTP Upgrade header.
    /// @return 28-character Base64 encoded accept token.
    [[nodiscard]] static std::string computeWebSocketAccept(std::string_view clientKey)
    {
        static constexpr std::string_view kGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        std::string concatenated { clientKey };
        concatenated.append(kGuid);

        const Digest digest { compute(concatenated) };
        return Base64::encode(std::string_view { reinterpret_cast<const char*>(digest.data()), digest.size() });
    }

private:
    [[nodiscard]] static constexpr std::uint32_t rotl(std::uint32_t val, unsigned int bits) noexcept
    {
        return (val << bits) | (val >> (32U - bits));
    }
};

} // namespace Nmea::Network
