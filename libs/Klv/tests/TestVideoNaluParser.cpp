#include "VideoNaluParser.h"
#include <gtest/gtest.h>
#include <vector>

using namespace Klv;

TEST(TestVideoNaluParser, StartCodeScanning) {
    // 00 00 01 (3-byte) and 00 00 00 01 (4-byte)
    const std::vector<std::uint8_t> buffer = {
        0xAAU, 0xBBU,
        0x00U, 0x00U, 0x01U, 0x07U, 0x01U, // 3-byte prefix at offset 2
        0x00U, 0x00U, 0x00U, 0x01U, 0x08U  // 4-byte prefix at offset 7
    };

    std::size_t prefixLen = 0U;
    const std::size_t offset1 = VideoNaluParser::findStartCode(buffer.data(), buffer.size(), 0U, prefixLen);
    EXPECT_EQ(offset1, 2U);
    EXPECT_EQ(prefixLen, 3U);

    const std::size_t offset2 = VideoNaluParser::findStartCode(buffer.data(), buffer.size(), offset1 + prefixLen, prefixLen);
    EXPECT_EQ(offset2, 7U);
    EXPECT_EQ(prefixLen, 4U);

    const std::size_t offset3 = VideoNaluParser::findStartCode(buffer.data(), buffer.size(), offset2 + prefixLen, prefixLen);
    EXPECT_EQ(offset3, buffer.size());
}

TEST(TestVideoNaluParser, H264NaluClassification) {
    EXPECT_EQ(VideoNaluParser::extractNaluType(0x67U, VideoCodec::H264), 7U); // SPS
    EXPECT_EQ(VideoNaluParser::extractNaluType(0x68U, VideoCodec::H264), 8U); // PPS
    EXPECT_EQ(VideoNaluParser::extractNaluType(0x65U, VideoCodec::H264), 5U); // IDR
    EXPECT_EQ(VideoNaluParser::extractNaluType(0x41U, VideoCodec::H264), 1U); // Non-IDR
    EXPECT_EQ(VideoNaluParser::extractNaluType(0x09U, VideoCodec::H264), 9U); // AUD

    EXPECT_TRUE(VideoNaluParser::isKeyframeType(5U, VideoCodec::H264));
    EXPECT_FALSE(VideoNaluParser::isKeyframeType(1U, VideoCodec::H264));
    EXPECT_FALSE(VideoNaluParser::isKeyframeType(7U, VideoCodec::H264));
}

TEST(TestVideoNaluParser, H265NaluClassification) {
    // H.265: NAL type in bits 9..14 (byte 0 shifted right 1, mask 0x3F)
    // VPS (32): 32 << 1 = 0x40
    EXPECT_EQ(VideoNaluParser::extractNaluType(0x40U, VideoCodec::H265), 32U);
    // SPS (33): 33 << 1 = 0x42
    EXPECT_EQ(VideoNaluParser::extractNaluType(0x42U, VideoCodec::H265), 33U);
    // PPS (34): 34 << 1 = 0x44
    EXPECT_EQ(VideoNaluParser::extractNaluType(0x44U, VideoCodec::H265), 34U);
    // IDR_W_RADL (19): 19 << 1 = 0x26
    EXPECT_EQ(VideoNaluParser::extractNaluType(0x26U, VideoCodec::H265), 19U);
    // Trail_R (1): 1 << 1 = 0x02
    EXPECT_EQ(VideoNaluParser::extractNaluType(0x02U, VideoCodec::H265), 1U);

    EXPECT_TRUE(VideoNaluParser::isKeyframeType(19U, VideoCodec::H265));
    EXPECT_TRUE(VideoNaluParser::isKeyframeType(20U, VideoCodec::H265));
    EXPECT_TRUE(VideoNaluParser::isKeyframeType(21U, VideoCodec::H265));
    EXPECT_FALSE(VideoNaluParser::isKeyframeType(1U, VideoCodec::H265));
    EXPECT_FALSE(VideoNaluParser::isKeyframeType(32U, VideoCodec::H265));
}

TEST(TestVideoNaluParser, ParseCompleteAccessUnitH264) {
    // Synthesize an H.264 Access Unit:
    // [Start Code 4B] [AUD]
    // [Start Code 4B] [SPS]
    // [Start Code 4B] [PPS]
    // [Start Code 4B] [IDR Slice]
    const std::vector<std::uint8_t> frameData = {
        0x00U, 0x00U, 0x00U, 0x01U, 0x09U, 0x10U,                   // AUD
        0x00U, 0x00U, 0x00U, 0x01U, 0x67U, 0x42U, 0x00U, 0x1FU,     // SPS
        0x00U, 0x00U, 0x00U, 0x01U, 0x68U, 0xCEU, 0x3CU, 0x80U,     // PPS
        0x00U, 0x00U, 0x00U, 0x01U, 0x65U, 0x88U, 0x84U, 0x00U      // IDR
    };

    constexpr std::uint64_t kPts = 12345678ULL;
    const auto au = VideoNaluParser::parseAccessUnit(frameData.data(),
                                                     frameData.size(),
                                                     VideoCodec::H264,
                                                     kPts);

    EXPECT_EQ(au.codec, VideoCodec::H264);
    EXPECT_EQ(au.ptsUs, kPts);
    EXPECT_EQ(au.dtsUs, kPts);
    EXPECT_FALSE(au.hasDts);
    EXPECT_TRUE(au.isKeyframe);
    EXPECT_EQ(au.primarySliceType, VideoSliceType::I);
    ASSERT_EQ(au.nalus.size(), 4U);

    EXPECT_EQ(au.nalus[0].naluType, 9U); // AUD
    EXPECT_EQ(au.nalus[1].naluType, 7U); // SPS
    EXPECT_EQ(au.nalus[2].naluType, 8U); // PPS
    EXPECT_EQ(au.nalus[3].naluType, 5U); // IDR
    EXPECT_TRUE(au.nalus[3].isKeyframe);
}

TEST(TestVideoNaluParser, StreamingChunkPush) {
    VideoNaluParser parser(VideoCodec::H264);

    std::vector<VideoAccessUnit> emittedAus;
    parser.setUnitCallback([&emittedAus](const VideoAccessUnit& au) {
        emittedAus.push_back(au);
    });

    // Frame 1: AUD + IDR Slice (first_mb_in_slice = 0)
    const std::vector<std::uint8_t> frame1 = {
        0x00U, 0x00U, 0x00U, 0x01U, 0x09U, 0x10U,
        0x00U, 0x00U, 0x00U, 0x01U, 0x65U, 0xB8U, 0x00U
    };

    // Frame 2: AUD + Non-IDR Slice
    const std::vector<std::uint8_t> frame2 = {
        0x00U, 0x00U, 0x00U, 0x01U, 0x09U, 0x30U,
        0x00U, 0x00U, 0x00U, 0x01U, 0x41U, 0x9AU, 0x00U
    };

    // Push Frame 1 in two separate chunks
    const std::size_t chunk1Size = 4U;
    const std::size_t n1 = parser.pushChunk(frame1.data(), chunk1Size, 1000ULL);
    EXPECT_EQ(n1, 0U);
    EXPECT_EQ(emittedAus.size(), 0U);

    const std::size_t n2 = parser.pushChunk(frame1.data() + chunk1Size, frame1.size() - chunk1Size, 1000ULL);
    EXPECT_EQ(n2, 0U);
    EXPECT_EQ(emittedAus.size(), 0U); // Boundary is triggered when start of Frame 2 is detected

    // Push Frame 2
    const std::size_t n3 = parser.pushChunk(frame2.data(), frame2.size(), 2000ULL);
    EXPECT_EQ(n3, 1U);
    ASSERT_EQ(emittedAus.size(), 1U); // Frame 1 emitted!
    EXPECT_EQ(emittedAus[0].ptsUs, 1000ULL);
    EXPECT_TRUE(emittedAus[0].isKeyframe);

    // Flush Frame 2
    const std::size_t flushed = parser.flush();
    EXPECT_EQ(flushed, 1U);
    ASSERT_EQ(emittedAus.size(), 2U);
    EXPECT_EQ(emittedAus[1].ptsUs, 2000ULL);
}
