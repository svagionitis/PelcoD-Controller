#include "VideoPesPacketizer.h"
#include <gtest/gtest.h>
#include <vector>

using namespace Klv;

TEST(TestVideoPesPacketizer, HeaderStructurePtsOnly) {
    VideoPesPacketizer packetizer(/*prependAud=*/false);

    const std::vector<std::uint8_t> frameData = {
        0x00U, 0x00U, 0x00U, 0x01U, 0x65U, 0x88U, 0x84U, 0x00U
    };

    constexpr std::uint64_t kTimestampUs = 10000000ULL; // 10.0 seconds
    const auto pes = packetizer.buildPesPacket(frameData.data(),
                                               frameData.size(),
                                               kTimestampUs,
                                               std::nullopt,
                                               /*isKeyframe=*/true);

    ASSERT_GE(pes.size(), 14U + frameData.size());

    // 1. Start code prefix: 00 00 01
    EXPECT_EQ(pes[0], 0x00U);
    EXPECT_EQ(pes[1], 0x00U);
    EXPECT_EQ(pes[2], 0x01U);

    // 2. Stream ID 0xE0
    EXPECT_EQ(pes[3], VideoPesPacketizer::kVideoStreamId);

    // 3. PES packet length (3 + 5 + 8 = 16 bytes = 0x0010)
    const auto length = static_cast<std::uint16_t>(
        (static_cast<std::uint32_t>(pes[4]) << 8U) | static_cast<std::uint32_t>(pes[5]));
    EXPECT_EQ(length, 16U);

    // 4. Header flags: 0x84 (data alignment), 0x80 (PTS present, DTS absent)
    EXPECT_EQ(pes[6], 0x84U);
    EXPECT_EQ(pes[7], 0x80U);
    EXPECT_EQ(pes[8], 0x05U); // PES header data length = 5

    // 5. 90 kHz PTS validation
    // 10,000,000 us * 90 / 1000 = 900,000 ticks
    constexpr std::uint64_t expectedPts = (kTimestampUs * 90ULL) / 1000ULL;
    EXPECT_EQ(expectedPts, 900000ULL);

    const std::uint64_t parsedPts =
        ((static_cast<std::uint64_t>(pes[9] & 0x0EU) >> 1U) << 30U) |
        (static_cast<std::uint64_t>(pes[10]) << 22U) |
        ((static_cast<std::uint64_t>(pes[11] & 0xFEU) >> 1U) << 15U) |
        (static_cast<std::uint64_t>(pes[12]) << 7U) |
        (static_cast<std::uint64_t>(pes[13] & 0xFEU) >> 1U);

    EXPECT_EQ(parsedPts, expectedPts);
}

TEST(TestVideoPesPacketizer, HeaderStructurePtsAndDts) {
    VideoPesPacketizer packetizer(/*prependAud=*/false);

    const std::vector<std::uint8_t> frameData = { 0x00U, 0x00U, 0x00U, 0x01U, 0x41U, 0x01U };
    constexpr std::uint64_t kPtsUs = 2000000ULL; // 2.0 s (180,000 ticks)
    constexpr std::uint64_t kDtsUs = 1966666ULL; // 1.966666 s

    const auto pes = packetizer.buildPesPacket(frameData.data(),
                                               frameData.size(),
                                               kPtsUs,
                                               kDtsUs,
                                               /*isKeyframe=*/false);

    ASSERT_GE(pes.size(), 19U + frameData.size());

    // PTS + DTS flags: 0xC0
    EXPECT_EQ(pes[7], 0xC0U);
    // PES header data length = 10
    EXPECT_EQ(pes[8], 10U);

    // PTS prefix should be 0x31 (bits '0011' in high nibble)
    EXPECT_EQ(pes[9] & 0xF0U, 0x30U);

    // DTS prefix should be 0x11 (bits '0001' in high nibble)
    EXPECT_EQ(pes[14] & 0xF0U, 0x10U);
}

TEST(TestVideoPesPacketizer, AutomaticAudPrepending) {
    VideoPesPacketizer packetizerWithAud(/*prependAud=*/true);

    VideoAccessUnit au;
    au.codec = VideoCodec::H264;
    au.ptsUs = 500000ULL;
    au.primarySliceType = VideoSliceType::I;
    au.isKeyframe = true;
    // Frame without leading AUD
    au.data = { 0x00U, 0x00U, 0x00U, 0x01U, 0x65U, 0x88U };

    const auto pes = packetizerWithAud.packetize(au);
    ASSERT_GE(pes.size(), 14U + 6U + au.data.size());

    // Byte 14 should begin the prepended AUD (00 00 00 01 09 10)
    EXPECT_EQ(pes[14], 0x00U);
    EXPECT_EQ(pes[15], 0x00U);
    EXPECT_EQ(pes[16], 0x00U);
    EXPECT_EQ(pes[17], 0x01U);
    EXPECT_EQ(pes[18], 0x09U); // AUD NAL type
}

TEST(TestVideoPesPacketizer, UnboundedPesLengthForLargeFrames) {
    VideoPesPacketizer packetizer(/*prependAud=*/false);

    // Create 70 KB payload (> 65535 bytes)
    const std::vector<std::uint8_t> largePayload(70000U, 0x55U);
    const auto pes = packetizer.buildPesPacket(largePayload.data(),
                                               largePayload.size(),
                                               1000ULL);

    // Unbounded PES length field must be 0x0000 per ISO/IEC 13818-1
    EXPECT_EQ(pes[4], 0x00U);
    EXPECT_EQ(pes[5], 0x00U);
}
