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
    std::string result {};
    result.reserve(text.size());
    for (const char ch : text) {
        result.push_back(toLowerAscii(ch));
    }
    return result;
}

/// @brief Safely converts a character range into a std::vector<std::uint8_t> with explicit casting.
/// @details Prevents MSVC C4365 template conversion errors. Zero CPU instructions beyond vector reserve and copy.
/// @param[in] text Input string view.
/// @return Vector of unsigned bytes.
[[nodiscard]] inline std::vector<std::uint8_t> stringToBytes(std::string_view text)
{
    std::vector<std::uint8_t> bytes {};
    bytes.reserve(text.size());
    for (const char ch : text) {
        bytes.push_back(static_cast<std::uint8_t>(ch));
    }
    return bytes;
}

} // namespace Onvif
