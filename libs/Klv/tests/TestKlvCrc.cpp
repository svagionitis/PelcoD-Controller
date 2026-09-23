#include "KlvCrc.h"
#include <gtest/gtest.h>
#include <vector>

using namespace Klv;

TEST(KlvCrcTest, BasicCrcCalculation) {
    // Standard ASCII "123456789" with CCITT poly 0x1021, init 0x0000
    // Check value is 0x31C3
    const std::uint8_t data[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    const std::uint16_t crc = KlvCrc::calculate(data, sizeof(data));
    EXPECT_EQ(crc, 0x31C3U);
}

TEST(KlvCrcTest, Tag1ChecksumAndVerification) {
    std::vector<std::uint8_t> testPacket = {
        0x06, 0x0E, 0x2B, 0x34, 0x02, 0x0B, 0x01, 0x01, 0x0E, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0x00,
        0x06, // BER length (6 bytes: tag 65 + tag 1)
        0x41, 0x01, 0x05, // Tag 65 (0x41), len 1, val 5
        0x01, 0x02 // Tag 1 (0x01), len 2
    };

    const std::uint16_t tag1Crc = KlvCrc::computeChecksumForTag1(testPacket.data(), testPacket.size());
    testPacket.push_back(static_cast<std::uint8_t>((tag1Crc >> 8U) & 0xFFU));
    testPacket.push_back(static_cast<std::uint8_t>(tag1Crc & 0xFFU));

    // Verifying complete packet with CRC appended should succeed
    EXPECT_TRUE(KlvCrc::verifyPacket(testPacket.data(), testPacket.size()));

    // Corrupt one byte
    testPacket[18] ^= 0xFFU;
    EXPECT_FALSE(KlvCrc::verifyPacket(testPacket.data(), testPacket.size()));
}

TEST(KlvCrcTest, NullOrEmptyData) {
    EXPECT_EQ(KlvCrc::calculate(nullptr, 10U), 0U);
    EXPECT_EQ(KlvCrc::calculate(nullptr, 0U), 0U);
    EXPECT_FALSE(KlvCrc::verifyPacket(nullptr, 10U));
    EXPECT_FALSE(KlvCrc::verifyPacket(nullptr, 0U));
}
