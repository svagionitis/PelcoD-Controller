/// @file TestStreamAccumulator.cpp
/// @brief Unit test verifying RxStreamAccumulator framing, fragmentation, and edge cases.

#include "PelcoDFrame.h"
#include "RxStreamAccumulator.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

/// @brief Verify accumulation and extraction of an intact single 7-byte Pelco-D frame.
/// @details Checks that a clean 7-byte frame pushed to an empty accumulator is immediately emitted,
///          leaving the internal accumulator buffer empty.
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

/// @brief Verify reassembly of a 7-byte frame received in fragmented chunks.
/// @details Pushes a frame split into a 3-byte prefix and a 4-byte suffix, verifying that
///          the first push yields no frame while buffering, and the second completes extraction.
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

/// @brief Verify parsing multiple complete frames delivered within a single contiguous buffer.
/// @details Feeds two consecutive 7-byte frames in a single push call, ensuring both discrete
///          frames are recognized and returned in proper sequence.
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

/// @brief Verify automatic recovery and sync detection when preceded by random noise bytes.
/// @details Feeds arbitrary garbage bytes followed by a valid 7-byte frame, verifying that the
///          sync byte search correctly discards noise and extracts the intact frame.
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

/// @brief Verify buffer overflow protection and automatic buffer reset.
/// @details Pushes data exceeding maxBufferSize without finding valid frames, verifying that the
///          internal buffer is flushed, preventing unbounded memory growth.
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

/// @brief Verify framing of 4-byte Pelco-D general response packets.
/// @details Pushes 4-byte general responses (e.g. ACK/alarms) both as a single block and byte-by-byte,
///          verifying extraction without waiting for 7 bytes.
TEST(StreamAccumulatorTest, GeneralResponseFraming)
{
    PelcoD::RxStreamAccumulator acc;

    // 4-byte general response: FF 01 00 01 (checksum = 0x01 + 0x00 = 0x01)
    const std::vector<std::uint8_t> genFrame { 0xFFU, 0x01U, 0x00U, 0x01U };

    // Intact push
    auto extracted = acc.push(genFrame);
    ASSERT_EQ(extracted.size(), 1U);
    EXPECT_EQ(extracted[0], genFrame);
    EXPECT_EQ(acc.size(), 0U);

    // Byte-by-byte push
    for (std::size_t i = 0U; i < genFrame.size() - 1U; ++i) {
        auto step = acc.push(std::vector<std::uint8_t> { genFrame[i] });
        EXPECT_TRUE(step.empty());
    }
    auto finalStep = acc.push(std::vector<std::uint8_t> { genFrame.back() });
    ASSERT_EQ(finalStep.size(), 1U);
    EXPECT_EQ(finalStep[0], genFrame);
    EXPECT_EQ(acc.size(), 0U);
}

/// @brief Verify accumulation and extraction of extended 18-byte query response frames.
/// @details Verifies that when awaitingQuery is true, an 18-byte query response packet is correctly
///          framed and emitted rather than truncated.
TEST(StreamAccumulatorTest, QueryResponseFraming)
{
    PelcoD::RxStreamAccumulator acc;

    std::vector<std::uint8_t> qFrame(18U, 0x00U);
    qFrame[0] = 0xFFU;
    qFrame[1] = 0x01U;
    qFrame[2] = 'T';
    qFrame[3] = 'E';
    qFrame[4] = 'S';
    qFrame[5] = 'T';
    qFrame[17] = PelcoD::PelcoDFrame::calculateChecksum(&qFrame[1], 16U);

    // Push with awaitingQuery = true
    const auto extracted = acc.push(qFrame, true);
    ASSERT_EQ(extracted.size(), 1U);
    EXPECT_EQ(extracted[0], qFrame);
    EXPECT_EQ(acc.size(), 0U);
}

/// @brief Verify resilience when payload contains 0xFF byte that looks like a sync byte.
/// @details Pushes a frame where a data byte is 0xFF, followed by a valid frame, ensuring the
///          accumulator does not get misaligned on internal payload bytes.
TEST(StreamAccumulatorTest, FalseSyncBytes)
{
    PelcoD::RxStreamAccumulator acc;

    // Frame with 0xFF in data byte (e.g. pan speed 0xFF turbo): FF 01 00 02 FF 00 02
    const auto turboFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x02U, 0xFFU, 0x00U);
    const auto secondFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x00U, 0x00U, 0x00U);

    std::vector<std::uint8_t> stream;
    stream.insert(stream.end(), turboFrame.begin(), turboFrame.end());
    stream.insert(stream.end(), secondFrame.begin(), secondFrame.end());

    const auto extracted = acc.push(stream);
    ASSERT_EQ(extracted.size(), 2U);
    EXPECT_EQ(extracted[0], turboFrame);
    EXPECT_EQ(extracted[1], secondFrame);
    EXPECT_EQ(acc.size(), 0U);
}

/// @brief Verify the raw pointer overload of push().
/// @details Ensures push(const uint8_t*, size_t, bool) behaves identically to the vector overload,
///          including handling nullptr safely.
TEST(StreamAccumulatorTest, RawPointerPush)
{
    PelcoD::RxStreamAccumulator acc;

    const auto frame = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x04U, 0x20U, 0x00U);
    const auto extracted = acc.push(frame.data(), frame.size(), false);
    ASSERT_EQ(extracted.size(), 1U);
    EXPECT_EQ(extracted[0], frame);

    // Nullptr or zero size should return empty safely
    const auto emptyExtracted = acc.push(nullptr, 0U, false);
    EXPECT_TRUE(emptyExtracted.empty());
}

/// @brief Verify manual buffer clearing via clear().
/// @details Pushes partial byte sequence, verifies size > 0, calls clear(), and verifies size == 0
///          and subsequent complete frame pushes successfully.
TEST(StreamAccumulatorTest, ClearAndReset)
{
    PelcoD::RxStreamAccumulator acc;

    // Push partial bytes
    const std::vector<std::uint8_t> partial { 0xFFU, 0x01U, 0x00U };
    const auto partialRes = acc.push(partial);
    EXPECT_TRUE(partialRes.empty());
    EXPECT_EQ(acc.size(), 3U);

    // Clear buffer
    acc.clear();
    EXPECT_EQ(acc.size(), 0U);

    // Push complete frame to verify clean state
    const auto validFrame = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x02U, 0x10U, 0x00U);
    const auto extracted = acc.push(validFrame);
    ASSERT_EQ(extracted.size(), 1U);
    EXPECT_EQ(extracted[0], validFrame);
}

} // namespace
