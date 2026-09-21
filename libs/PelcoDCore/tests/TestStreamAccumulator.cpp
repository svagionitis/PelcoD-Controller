/// @file TestStreamAccumulator.cpp
/// @brief Unit test verifying RxStreamAccumulator framing, fragmentation, and edge cases.

#include "PelcoDFrame.h"
#include "RxStreamAccumulator.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

TEST(StreamAccumulatorTest, SingleFrame)
{
    PelcoD::RxStreamAccumulator acc;
    EXPECT_EQ(acc.size(), 0U);

    const auto frame = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x02U, 0x20U, 0x00U);
    ASSERT_EQ(frame.size(), 7U);

    const auto extracted = acc.push(frame);
    ASSERT_EQ(extracted.size(), 1U);
    EXPECT_EQ(extracted[0], frame);
    EXPECT_EQ(acc.size(), 0U);
}

TEST(StreamAccumulatorTest, FragmentedFrame)
{
    PelcoD::RxStreamAccumulator acc;

    const auto frame = PelcoD::PelcoDFrame::createFrame(2U, 0x00U, 0x04U, 0x00U, 0x30U);
    const std::vector<std::uint8_t> chunk1(frame.begin(), frame.begin() + 3);
    const std::vector<std::uint8_t> chunk2(frame.begin() + 3, frame.end());

    auto res1 = acc.push(chunk1);
    EXPECT_TRUE(res1.empty());
    EXPECT_EQ(acc.size(), 3U);

    auto res2 = acc.push(chunk2);
    ASSERT_EQ(res2.size(), 1U);
    EXPECT_EQ(res2[0], frame);
    EXPECT_EQ(acc.size(), 0U);
}

TEST(StreamAccumulatorTest, MultipleFramesInOneChunk)
{
    PelcoD::RxStreamAccumulator acc;

    const auto frame1 = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x02U, 0x20U, 0x00U);
    const auto frame2 = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x04U, 0x00U, 0x20U);

    std::vector<std::uint8_t> stream;
    stream.insert(stream.end(), frame1.begin(), frame1.end());
    stream.insert(stream.end(), frame2.begin(), frame2.end());

    const auto extracted = acc.push(stream);
    ASSERT_EQ(extracted.size(), 2U);
    EXPECT_EQ(extracted[0], frame1);
    EXPECT_EQ(extracted[1], frame2);
    EXPECT_EQ(acc.size(), 0U);
}

TEST(StreamAccumulatorTest, GarbagePreamble)
{
    PelcoD::RxStreamAccumulator acc;

    const auto frame = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x00U, 0x00U, 0x00U);
    std::vector<std::uint8_t> noisyData { 0x00U, 0x12U, 0x34U, 0xAAU };
    noisyData.insert(noisyData.end(), frame.begin(), frame.end());

    const auto extracted = acc.push(noisyData);
    ASSERT_EQ(extracted.size(), 1U);
    EXPECT_EQ(extracted[0], frame);
    EXPECT_EQ(acc.size(), 0U);
}

TEST(StreamAccumulatorTest, BufferOverflowReset)
{
    PelcoD::RxStreamAccumulator acc(100U);
    EXPECT_EQ(acc.maxBufferSize(), 100U);

    std::vector<std::uint8_t> garbage(120U, 0x11U);
    const auto extracted = acc.push(garbage);
    EXPECT_TRUE(extracted.empty());
    EXPECT_EQ(acc.size(), 0U);

    const auto validFrame = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x08U, 0x00U, 0x00U);
    const auto extractedValid = acc.push(validFrame);
    ASSERT_EQ(extractedValid.size(), 1U);
    EXPECT_EQ(extractedValid[0], validFrame);
}

} // namespace
