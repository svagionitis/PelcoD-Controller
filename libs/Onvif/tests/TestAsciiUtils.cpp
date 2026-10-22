/// @file TestAsciiUtils.cpp
/// @brief Unit tests for shared ASCII utilities module (AsciiUtils.h).

#include "AsciiUtils.h"

#include <cstddef>
#include <cstdint>

#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

namespace Onvif {
namespace {

    // ==============================================================================
    // Compile-time constexpr static assertions
    // ==============================================================================
    static_assert(toLowerAscii('A') == 'a', "toLowerAscii('A') must fold to 'a'");
    static_assert(toLowerAscii('Z') == 'z', "toLowerAscii('Z') must fold to 'z'");
    static_assert(toLowerAscii('a') == 'a', "toLowerAscii('a') must remain 'a'");
    static_assert(toLowerAscii('0') == '0', "toLowerAscii('0') must remain '0'");
    static_assert(toLowerAscii('-') == '-', "toLowerAscii('-') must remain '-'");

    static_assert(toUpperAscii('a') == 'A', "toUpperAscii('a') must fold to 'A'");
    static_assert(toUpperAscii('z') == 'Z', "toUpperAscii('z') must fold to 'Z'");
    static_assert(toUpperAscii('A') == 'A', "toUpperAscii('A') must remain 'A'");
    static_assert(toUpperAscii('9') == '9', "toUpperAscii('9') must remain '9'");

    static_assert(isAsciiDigit('0'), "'0' is an ASCII digit");
    static_assert(isAsciiDigit('9'), "'9' is an ASCII digit");
    static_assert(!isAsciiDigit('a'), "'a' is not an ASCII digit");
    static_assert(!isAsciiDigit('/'), "'/' is not an ASCII digit");
    static_assert(!isAsciiDigit(':'), "':' is not an ASCII digit");

    static_assert(isAsciiAlpha('A'), "'A' is ASCII alpha");
    static_assert(isAsciiAlpha('z'), "'z' is ASCII alpha");
    static_assert(!isAsciiAlpha('1'), "'1' is not ASCII alpha");
    static_assert(!isAsciiAlpha(' '), "' ' is not ASCII alpha");

    static_assert(isAsciiHexDigit('0'), "'0' is ASCII hex digit");
    static_assert(isAsciiHexDigit('9'), "'9' is ASCII hex digit");
    static_assert(isAsciiHexDigit('a'), "'a' is ASCII hex digit");
    static_assert(isAsciiHexDigit('f'), "'f' is ASCII hex digit");
    static_assert(isAsciiHexDigit('A'), "'A' is ASCII hex digit");
    static_assert(isAsciiHexDigit('F'), "'F' is ASCII hex digit");
    static_assert(!isAsciiHexDigit('g'), "'g' is not ASCII hex digit");
    static_assert(!isAsciiHexDigit('G'), "'G' is not ASCII hex digit");

    static_assert(isAsciiSpace(' '), "' ' is ASCII space");
    static_assert(isAsciiSpace('\t'), "'\\t' is ASCII space");
    static_assert(isAsciiSpace('\r'), "'\\r' is ASCII space");
    static_assert(isAsciiSpace('\n'), "'\\n' is ASCII space");
    static_assert(!isAsciiSpace('a'), "'a' is not ASCII space");
    static_assert(!isAsciiSpace('\v'), "'\\v' is not considered basic ASCII space");

    // ==============================================================================
    // Character Conversion Tests
    // ==============================================================================

    TEST(AsciiUtilsTest, ToLowerAsciiFoldsUppercaseRange)
    {
        for (char c { 'A' }; c <= 'Z'; ++c) {
            const char expected { static_cast<char>(c + 32) };
            EXPECT_EQ(toLowerAscii(c), expected);
        }
    }

    TEST(AsciiUtilsTest, ToLowerAsciiLeavesNonUppercaseUnchanged)
    {
        // Lowercase letters
        for (char c { 'a' }; c <= 'z'; ++c) {
            EXPECT_EQ(toLowerAscii(c), c);
        }
        // Digits
        for (char c { '0' }; c <= '9'; ++c) {
            EXPECT_EQ(toLowerAscii(c), c);
        }
        // Punctuation and control characters
        EXPECT_EQ(toLowerAscii('\0'), '\0');
        EXPECT_EQ(toLowerAscii(' '), ' ');
        EXPECT_EQ(toLowerAscii('-'), '-');
        EXPECT_EQ(toLowerAscii(':'), ':');
        EXPECT_EQ(toLowerAscii('/'), '/');

        // Extended bytes / negative char values (CERT STR37-C compliance)
        const char highAscii { static_cast<char>(0x80) };
        const char maxByte { static_cast<char>(0xFF) };
        EXPECT_EQ(toLowerAscii(highAscii), highAscii);
        EXPECT_EQ(toLowerAscii(maxByte), maxByte);
    }

    TEST(AsciiUtilsTest, ToUpperAsciiFoldsLowercaseRange)
    {
        for (char c { 'a' }; c <= 'z'; ++c) {
            const char expected { static_cast<char>(c - 32) };
            EXPECT_EQ(toUpperAscii(c), expected);
        }
    }

    TEST(AsciiUtilsTest, ToUpperAsciiLeavesNonLowercaseUnchanged)
    {
        // Uppercase letters
        for (char c { 'A' }; c <= 'Z'; ++c) {
            EXPECT_EQ(toUpperAscii(c), c);
        }
        // Digits
        for (char c { '0' }; c <= '9'; ++c) {
            EXPECT_EQ(toUpperAscii(c), c);
        }
        // Extended bytes / negative char values
        const char highAscii { static_cast<char>(0x80) };
        const char maxByte { static_cast<char>(0xFF) };
        EXPECT_EQ(toUpperAscii(highAscii), highAscii);
        EXPECT_EQ(toUpperAscii(maxByte), maxByte);
    }

    // ==============================================================================
    // Character Classification Tests
    // ==============================================================================

    TEST(AsciiUtilsTest, IsAsciiDigitClassification)
    {
        for (int i { 0 }; i < 256; ++i) {
            const char c { static_cast<char>(i) };
            const bool expected { (c >= '0') && (c <= '9') };
            EXPECT_EQ(isAsciiDigit(c), expected);
        }
    }

    TEST(AsciiUtilsTest, IsAsciiAlphaClassification)
    {
        for (int i { 0 }; i < 256; ++i) {
            const char c { static_cast<char>(i) };
            const bool expected { ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) };
            EXPECT_EQ(isAsciiAlpha(c), expected);
        }
    }

    TEST(AsciiUtilsTest, IsAsciiHexDigitClassification)
    {
        for (int i { 0 }; i < 256; ++i) {
            const char c { static_cast<char>(i) };
            const bool expected { ((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'f'))
                || ((c >= 'A') && (c <= 'F')) };
            EXPECT_EQ(isAsciiHexDigit(c), expected);
        }
    }

    TEST(AsciiUtilsTest, IsAsciiSpaceClassification)
    {
        EXPECT_TRUE(isAsciiSpace(' '));
        EXPECT_TRUE(isAsciiSpace('\t'));
        EXPECT_TRUE(isAsciiSpace('\r'));
        EXPECT_TRUE(isAsciiSpace('\n'));
        EXPECT_FALSE(isAsciiSpace('\v'));
        EXPECT_FALSE(isAsciiSpace('\f'));
        EXPECT_FALSE(isAsciiSpace('0'));
        EXPECT_FALSE(isAsciiSpace('a'));
    }

    // ==============================================================================
    // String Comparison and Search Tests
    // ==============================================================================

    TEST(AsciiUtilsTest, EqualsIgnoreCaseAsciiIdentical)
    {
        EXPECT_TRUE(equalsIgnoreCaseAscii("", ""));
        EXPECT_TRUE(equalsIgnoreCaseAscii("admin", "admin"));
        EXPECT_TRUE(equalsIgnoreCaseAscii("http://localhost:8080/onvif", "http://localhost:8080/onvif"));
    }

    TEST(AsciiUtilsTest, EqualsIgnoreCaseAsciiMixedCase)
    {
        EXPECT_TRUE(equalsIgnoreCaseAscii("ADMIN", "admin"));
        EXPECT_TRUE(equalsIgnoreCaseAscii("Admin", "admin"));
        EXPECT_TRUE(equalsIgnoreCaseAscii("rtsp://192.168.1.100:554/media", "RTSP://192.168.1.100:554/MEDIA"));
        EXPECT_TRUE(equalsIgnoreCaseAscii(
            "urn:uuid:01234567-89ab-cdef-0123-456789abcdef", "URN:UUID:01234567-89AB-CDEF-0123-456789ABCDEF"));
    }

    TEST(AsciiUtilsTest, EqualsIgnoreCaseAsciiDifferentLengths)
    {
        EXPECT_FALSE(equalsIgnoreCaseAscii("admin", "administrator"));
        EXPECT_FALSE(equalsIgnoreCaseAscii("admin", ""));
        EXPECT_FALSE(equalsIgnoreCaseAscii("", "admin"));
    }

    TEST(AsciiUtilsTest, EqualsIgnoreCaseAsciiDifferentContent)
    {
        EXPECT_FALSE(equalsIgnoreCaseAscii("admin", "admin1"));
        EXPECT_FALSE(equalsIgnoreCaseAscii("password", "passward"));
        EXPECT_FALSE(equalsIgnoreCaseAscii("http", "https"));
    }

    TEST(AsciiUtilsTest, ContainsIgnoreCaseAsciiBasic)
    {
        EXPECT_TRUE(containsIgnoreCaseAscii("urn:uuid:01234567-89ab-cdef", "01234567-89ab-cdef"));
        EXPECT_TRUE(containsIgnoreCaseAscii("urn:uuid:01234567-89AB-CDEF", "01234567-89ab-cdef"));
        EXPECT_TRUE(containsIgnoreCaseAscii("URN:UUID:01234567-89AB-CDEF", "uuid"));
        EXPECT_TRUE(containsIgnoreCaseAscii("prefix_MATCH_suffix", "match"));
        EXPECT_TRUE(containsIgnoreCaseAscii("exact_match", "exact_match"));
    }

    TEST(AsciiUtilsTest, ContainsIgnoreCaseAsciiEdgeCases)
    {
        // Empty needle or haystack
        EXPECT_FALSE(containsIgnoreCaseAscii("", ""));
        EXPECT_FALSE(containsIgnoreCaseAscii("haystack", ""));
        EXPECT_FALSE(containsIgnoreCaseAscii("", "needle"));

        // Needle longer than haystack
        EXPECT_FALSE(containsIgnoreCaseAscii("short", "longer_needle"));

        // Not found
        EXPECT_FALSE(containsIgnoreCaseAscii("urn:uuid:1111", "2222"));
        EXPECT_FALSE(containsIgnoreCaseAscii("admin", "root"));
    }

    // ==============================================================================
    // String Conversion Tests
    // ==============================================================================

    TEST(AsciiUtilsTest, ToLowerAsciiStringConversion)
    {
        EXPECT_EQ(toLowerAsciiString(""), "");
        EXPECT_EQ(toLowerAsciiString("already lower"), "already lower");
        EXPECT_EQ(toLowerAsciiString("ALL CAPS"), "all caps");
        EXPECT_EQ(toLowerAsciiString("MiXeD cAsE 123!"), "mixed case 123!");
        EXPECT_EQ(toLowerAsciiString("RTSP://CAMERA:554/ONVIF"), "rtsp://camera:554/onvif");
    }

    TEST(AsciiUtilsTest, StringToBytesConversion)
    {
        const std::string text { "Hello\x00\xFF\x7F", 8 };
        const std::vector<std::uint8_t> bytes { stringToBytes(text) };

        ASSERT_EQ(bytes.size(), 8U);
        EXPECT_EQ(bytes[0], static_cast<std::uint8_t>('H'));
        EXPECT_EQ(bytes[1], static_cast<std::uint8_t>('e'));
        EXPECT_EQ(bytes[2], static_cast<std::uint8_t>('l'));
        EXPECT_EQ(bytes[3], static_cast<std::uint8_t>('l'));
        EXPECT_EQ(bytes[4], static_cast<std::uint8_t>('o'));
        EXPECT_EQ(bytes[5], 0x00U);
        EXPECT_EQ(bytes[6], 0xFFU);
        EXPECT_EQ(bytes[7], 0x7FU);

        // Empty view returns empty vector
        const std::vector<std::uint8_t> emptyBytes { stringToBytes("") };
        EXPECT_TRUE(emptyBytes.empty());
    }

} // namespace
} // namespace Onvif
