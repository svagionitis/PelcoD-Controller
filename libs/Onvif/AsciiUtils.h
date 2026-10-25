/// @file AsciiUtils.h
/// @brief Fast, locale-independent ASCII character and string utilities.
/// @details Designed for safety-critical protocol token parsing (RFC 3986, RFC 7616, WS-Discovery).
/// Complies with MISRA C++:2023 Rule 7.0.6, CERT STR37-C, and CERT ERR55-CPP.

#pragma once

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace Onvif {

/// @brief Folds an ASCII uppercase letter ('A'..'Z') to lowercase ('a'..'z').
/// @details Does not throw, does not allocate, and is independent of host C/C++ locale.
/// @param[in] ch Input character.
/// @return Lowercase ASCII character if in range 'A'..'Z', otherwise returns @p ch unmodified.
[[nodiscard]] constexpr char toLowerAscii(char ch) noexcept
{
    return ((ch >= 'A') && (ch <= 'Z')) ? static_cast<char>((ch - 'A') + 'a') : ch;
}

/// @brief Folds an ASCII lowercase letter ('a'..'z') to uppercase ('A'..'Z').
/// @details Does not throw, does not allocate, and is independent of host C/C++ locale.
/// @param[in] ch Input character.
/// @return Uppercase ASCII character if in range 'a'..'z', otherwise returns @p ch unmodified.
[[nodiscard]] constexpr char toUpperAscii(char ch) noexcept
{
    return ((ch >= 'a') && (ch <= 'z')) ? static_cast<char>((ch - 'a') + 'A') : ch;
}

/// @brief Checks whether a character is an ASCII decimal digit ('0'..'9').
/// @details Does not throw, does not allocate, and is independent of host C/C++ locale.
/// @param[in] ch Input character.
/// @return true if character is in '0'..'9', false otherwise.
[[nodiscard]] constexpr bool isAsciiDigit(char ch) noexcept
{
    return (ch >= '0') && (ch <= '9');
}

/// @brief Checks whether a character is an ASCII alphabet character ('A'..'Z' or 'a'..'z').
/// @details Does not throw, does not allocate, and is independent of host C/C++ locale.
/// @param[in] ch Input character.
/// @return true if character is in 'A'..'Z' or 'a'..'z', false otherwise.
[[nodiscard]] constexpr bool isAsciiAlpha(char ch) noexcept
{
    return ((ch >= 'A') && (ch <= 'Z')) || ((ch >= 'a') && (ch <= 'z'));
}

/// @brief Checks whether a character is an ASCII hexadecimal digit ('0'..'9', 'a'..'f', 'A'..'F').
/// @details Does not throw, does not allocate, and is independent of host C/C++ locale.
/// @param[in] ch Input character.
/// @return true if character is a valid hexadecimal digit, false otherwise.
[[nodiscard]] constexpr bool isAsciiHexDigit(char ch) noexcept
{
    return ((ch >= '0') && (ch <= '9')) || ((ch >= 'a') && (ch <= 'f')) || ((ch >= 'A') && (ch <= 'F'));
}

/// @brief Converts an ASCII hexadecimal digit ('0'..'9', 'a'..'f', 'A'..'F') to its numeric value (0..15).
/// @details Does not throw, does not allocate, and is independent of host C/C++ locale.
/// @param[in] ch Input character.
/// @return Integer value 0..15 if valid hex digit, otherwise -1.
[[nodiscard]] constexpr int hexDigitValue(char ch) noexcept
{
    if ((ch >= '0') && (ch <= '9')) {
        return ch - '0';
    }
    if ((ch >= 'a') && (ch <= 'f')) {
        return (ch - 'a') + 10;
    }
    if ((ch >= 'A') && (ch <= 'F')) {
        return (ch - 'A') + 10;
    }
    return -1;
}

/// @brief Parses two hexadecimal ASCII characters into a byte value.
/// @details Does not throw, does not allocate.
/// @param[in] hi Upper nibble hex char ('0'..'9', 'A'..'F', 'a'..'f').
/// @param[in] lo Lower nibble hex char ('0'..'9', 'A'..'F', 'a'..'f').
/// @param[out] outByte Output byte.
/// @return true if both characters were valid hexadecimal digits, false otherwise.
[[nodiscard]] constexpr bool parseHexByte(char hi, char lo, std::uint8_t& outByte) noexcept
{
    const int h { hexDigitValue(hi) };
    const int l { hexDigitValue(lo) };
    if ((h < 0) || (l < 0)) {
        return false;
    }
    outByte = static_cast<std::uint8_t>((static_cast<unsigned int>(h) << 4U) | static_cast<unsigned int>(l));
    return true;
}

/// @brief Checks whether a character is standard ASCII whitespace (space, tab, CR, LF).
/// @details Does not throw, does not allocate, and is independent of host C/C++ locale.
/// @param[in] ch Input character.
/// @return true if character is space, tab, carriage return, or newline.
[[nodiscard]] constexpr bool isAsciiSpace(char ch) noexcept
{
    return (ch == ' ') || (ch == '\t') || (ch == '\r') || (ch == '\n');
}

/// @brief Performs a locale-independent case-insensitive comparison of two ASCII strings.
/// @details Allocation-free, noexcept; safe for high-frequency protocol matching.
/// @param[in] lhs First string view.
/// @param[in] rhs Second string view.
/// @return true if strings are equal ignoring ASCII case, false otherwise.
[[nodiscard]] inline bool equalsIgnoreCaseAscii(std::string_view lhs, std::string_view rhs) noexcept
{
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i { 0U }; i < lhs.size(); ++i) {
        if (toLowerAscii(lhs[i]) != toLowerAscii(rhs[i])) {
            return false;
        }
    }
    return true;
}

/// @brief Checks if @p haystack contains @p needle ignoring ASCII case without dynamic allocation.
/// @details Allocation-free, noexcept; safe for WS-Discovery message and UUID filtering.
/// @param[in] haystack Text to search.
/// @param[in] needle Substring to find.
/// @return true if @p needle occurs within @p haystack, false if not found or if either view is empty.
[[nodiscard]] inline bool containsIgnoreCaseAscii(std::string_view haystack, std::string_view needle) noexcept
{
    if (needle.empty() || (haystack.size() < needle.size())) {
        return false;
    }
    const auto it = std::search(haystack.cbegin(), haystack.cend(), needle.cbegin(), needle.cend(),
        [](char a, char b) noexcept { return toLowerAscii(a) == toLowerAscii(b); });
    return it != haystack.cend();
}

/// @brief Converts an ASCII string view to a lowercase std::string.
/// @details Single allocation with reserve(); safe for protocol URI and header normalizations.
/// @param[in] text Input string view.
/// @return Newly allocated lowercase std::string with single reservation.
[[nodiscard]] inline std::string toLowerAsciiString(std::string_view text)
{
    std::string result { };
    result.reserve(text.size());
    for (const char ch : text) {
        result.push_back(toLowerAscii(ch));
    }
    return result;
}

/// @brief Decodes a percent-encoded (URL-encoded) ASCII string view per RFC 3986.
/// @details Replaces '+' with space and '%XX' hexadecimal sequences with their corresponding byte.
/// Malformed sequences (e.g. invalid hex digits or incomplete '%') are preserved as literal characters
/// without throwing exceptions (CERT ERR55-CPP / CERT ERR51-CPP).
/// @param[in] in Input URL-encoded string view.
/// @return Decoded std::string.
[[nodiscard]] inline std::string urlDecode(std::string_view in)
{
    std::string out { };
    out.reserve(in.size());
    for (std::size_t i { 0U }; i < in.size(); ++i) {
        if ((in[i] == '%') && (i + 2U < in.size())) {
            std::uint8_t byteVal { 0U };
            if (parseHexByte(in[i + 1U], in[i + 2U], byteVal)) {
                out.push_back(static_cast<char>(byteVal));
                i += 2U;
                continue;
            }
        }
        if (in[i] == '+') {
            out.push_back(' ');
        } else {
            out.push_back(in[i]);
        }
    }
    return out;
}

/// @brief Safely converts a character range into a std::vector<std::uint8_t> with explicit casting.
/// @details Prevents MSVC C4365 template conversion errors. Zero CPU instructions beyond vector reserve and copy.
/// @param[in] text Input string view.
/// @return Vector of unsigned bytes.
[[nodiscard]] inline std::vector<std::uint8_t> stringToBytes(std::string_view text)
{
    std::vector<std::uint8_t> bytes { };
    bytes.reserve(text.size());
    for (const char ch : text) {
        bytes.push_back(static_cast<std::uint8_t>(ch));
    }
    return bytes;
}

} // namespace Onvif
