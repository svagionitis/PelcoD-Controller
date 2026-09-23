#include "KlvBer.h"
#include <gtest/gtest.h>
#include <vector>

using namespace Klv;

TEST(KlvBerTest, ShortFormLength) {
    std::vector<std::uint8_t> out;
    KlvBer::encodeLength(72U, out);
    ASSERT_EQ(out.size(), 1U);
    EXPECT_EQ(out[0], 72U);

    std::size_t decodedLen = 0U;
    std::size_t consumed = 0U;
    EXPECT_TRUE(KlvBer::decodeLength(out.data(), out.size(), decodedLen, consumed));
    EXPECT_EQ(decodedLen, 72U);
    EXPECT_EQ(consumed, 1U);
}

TEST(KlvBerTest, LongFormLength1Byte) {
    std::vector<std::uint8_t> out;
    KlvBer::encodeLength(200U, out);
    ASSERT_EQ(out.size(), 2U);
    EXPECT_EQ(out[0], 0x81U);
    EXPECT_EQ(out[1], 200U);

    std::size_t decodedLen = 0U;
    std::size_t consumed = 0U;
    EXPECT_TRUE(KlvBer::decodeLength(out.data(), out.size(), decodedLen, consumed));
    EXPECT_EQ(decodedLen, 200U);
    EXPECT_EQ(consumed, 2U);
}

TEST(KlvBerTest, LongFormLength2Bytes) {
    std::vector<std::uint8_t> out;
    KlvBer::encodeLength(1000U, out);
    ASSERT_EQ(out.size(), 3U);
    EXPECT_EQ(out[0], 0x82U);
    EXPECT_EQ(out[1], (1000U >> 8U) & 0xFFU);
    EXPECT_EQ(out[2], 1000U & 0xFFU);

    std::size_t decodedLen = 0U;
    std::size_t consumed = 0U;
    EXPECT_TRUE(KlvBer::decodeLength(out.data(), out.size(), decodedLen, consumed));
    EXPECT_EQ(decodedLen, 1000U);
    EXPECT_EQ(consumed, 3U);
}

TEST(KlvBerTest, LongFormLength3Bytes) {
    std::vector<std::uint8_t> out;
    KlvBer::encodeLength(70000U, out);
    ASSERT_EQ(out.size(), 4U);
    EXPECT_EQ(out[0], 0x83U);

    std::size_t decodedLen = 0U;
    std::size_t consumed = 0U;
    EXPECT_TRUE(KlvBer::decodeLength(out.data(), out.size(), decodedLen, consumed));
    EXPECT_EQ(decodedLen, 70000U);
    EXPECT_EQ(consumed, 4U);
}

TEST(KlvBerTest, DecodeUnderflowProtection) {
    const std::uint8_t truncatedLongForm[] = { 0x82, 0x01 }; // Needs 3 bytes, only 2 provided
    std::size_t decodedLen = 0U;
    std::size_t consumed = 0U;
    EXPECT_FALSE(KlvBer::decodeLength(truncatedLongForm, 2U, decodedLen, consumed));
    EXPECT_FALSE(KlvBer::decodeLength(nullptr, 10U, decodedLen, consumed));
    EXPECT_FALSE(KlvBer::decodeLength(truncatedLongForm, 0U, decodedLen, consumed));
}

TEST(KlvBerTest, TagEncodingAndDecoding) {
    // 1-byte tag (< 128)
    {
        std::vector<std::uint8_t> out;
        KlvBer::encodeTag(42U, out);
        ASSERT_EQ(out.size(), 1U);
        EXPECT_EQ(out[0], 42U);

        std::uint32_t decodedTag = 0U;
        std::size_t consumed = 0U;
        EXPECT_TRUE(KlvBer::decodeTag(out.data(), out.size(), decodedTag, consumed));
        EXPECT_EQ(decodedTag, 42U);
        EXPECT_EQ(consumed, 1U);
    }

    // Multi-byte BER-OID tag (>= 128)
    {
        std::vector<std::uint8_t> out;
        KlvBer::encodeTag(128U, out);
        ASSERT_EQ(out.size(), 2U);
        EXPECT_EQ(out[0], 0x81U);
        EXPECT_EQ(out[1], 0x00U);

        std::uint32_t decodedTag = 0U;
        std::size_t consumed = 0U;
        EXPECT_TRUE(KlvBer::decodeTag(out.data(), out.size(), decodedTag, consumed));
        EXPECT_EQ(decodedTag, 128U);
        EXPECT_EQ(consumed, 2U);
    }
}
