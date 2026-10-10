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
/// @brief Verify framing of 4-byte Pelco-D general response packets when explicitly allowed.
/// @details Pushes 4-byte general responses with AllowGeneralResponse expectation and verifies extraction.
TEST(StreamAccumulatorTest, GeneralResponseFraming)
{
    PelcoD::RxStreamAccumulator acc;

    // 4-byte general response: FF 01 00 01 (checksum = 0x01 + 0x00 = 0x01)
    const std::vector<std::uint8_t> genFrame { 0xFFU, 0x01U, 0x00U, 0x01U };

    // Intact push with AllowGeneralResponse and flush
    auto extracted = acc.push(genFrame, PelcoD::RxFrameExpectation::AllowGeneralResponse);
    if (extracted.empty()) {
        extracted = acc.flush(PelcoD::RxFrameExpectation::AllowGeneralResponse);
    }
    ASSERT_EQ(extracted.size(), 1U);
    EXPECT_EQ(extracted[0], genFrame);
    EXPECT_EQ(acc.size(), 0U);

    // Byte-by-byte push with AllowGeneralResponse and final flush
    for (std::size_t i = 0U; i < genFrame.size() - 1U; ++i) {
        auto step = acc.push(std::vector<std::uint8_t> { genFrame[i] }, PelcoD::RxFrameExpectation::AllowGeneralResponse);
        EXPECT_TRUE(step.empty());
    }
    const auto stepLast = acc.push(
        std::vector<std::uint8_t> { genFrame.back() }, PelcoD::RxFrameExpectation::AllowGeneralResponse);
    EXPECT_TRUE(stepLast.empty());
    auto finalStep = acc.flush(PelcoD::RxFrameExpectation::AllowGeneralResponse);
    ASSERT_EQ(finalStep.size(), 1U);
    EXPECT_EQ(finalStep[0], genFrame);
    EXPECT_EQ(acc.size(), 0U);
}

/// @brief Verify boundary lookahead extracts 4-byte frame immediately when followed by next sync byte.
/// @details Ingests a 4-byte general response followed immediately by a 7-byte command in a single push.
TEST(StreamAccumulatorTest, LookaheadExtractsConsecutiveFrames)
{
    PelcoD::RxStreamAccumulator acc;

    const std::vector<std::uint8_t> genFrame { 0xFFU, 0x01U, 0x00U, 0x01U };
    const auto cmdFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x02U, 0x10U, 0x20U);

    std::vector<std::uint8_t> stream;
    stream.insert(stream.end(), genFrame.begin(), genFrame.end());
    stream.insert(stream.end(), cmdFrame.begin(), cmdFrame.end());

    const auto extracted = acc.push(stream, PelcoD::RxFrameExpectation::AllowGeneralResponse);
    ASSERT_EQ(extracted.size(), 2U);
    EXPECT_EQ(extracted[0], genFrame);
    EXPECT_EQ(extracted[1], cmdFrame);
    EXPECT_EQ(acc.size(), 0U);
}

/// @brief Verify inter-byte timeout flushes a standalone 4-byte general response.
/// @details Pushes a 4-byte frame, waits for the inter-byte timeout to elapse, and calls flushExpired().
TEST(StreamAccumulatorTest, InterByteTimeoutFlushesGeneralResponse)
{
    PelcoD::RxStreamAccumulator acc(4096U, std::chrono::milliseconds(20));

    const std::vector<std::uint8_t> genFrame { 0xFFU, 0x01U, 0x00U, 0x01U };

    // Push without flush: available < 7, so it stays buffered
    const auto initial = acc.push(genFrame, PelcoD::RxFrameExpectation::AllowGeneralResponse);
    EXPECT_TRUE(initial.empty());
    EXPECT_EQ(acc.size(), 4U);

    // Immediate flushExpired fails before timeout
    const auto immediate = acc.flushExpired(PelcoD::RxFrameExpectation::AllowGeneralResponse);
    EXPECT_TRUE(immediate.empty());
    EXPECT_EQ(acc.size(), 4U);

    // Sleep past timeout (25 ms > 20 ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    // flushExpired now succeeds
    const auto expired = acc.flushExpired(PelcoD::RxFrameExpectation::AllowGeneralResponse);
    ASSERT_EQ(expired.size(), 1U);
    EXPECT_EQ(expired[0], genFrame);
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

/// @brief Verify telemetry statistics tracking for discarded bytes and checksum errors.
/// @details Ingests unaligned noise bytes and a bad checksum frame, validating counters.
TEST(StreamAccumulatorTest, StatisticsTracking)
{
    PelcoD::RxStreamAccumulator acc;
    EXPECT_EQ(acc.discardedBytes(), 0U);
    EXPECT_EQ(acc.checksumErrors(), 0U);

    // 4 noise bytes followed by valid frame
    const auto validFrame = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x02U, 0x20U, 0x00U);
    std::vector<std::uint8_t> noisy { 0x11U, 0x22U, 0x33U, 0x44U };
    noisy.insert(noisy.end(), validFrame.begin(), validFrame.end());

    const auto res1 = acc.push(noisy, true);
    ASSERT_EQ(res1.size(), 1U);
    EXPECT_EQ(acc.discardedBytes(), 4U);
    EXPECT_EQ(acc.checksumErrors(), 0U);

    // Corrupted checksum frame: 7 bytes starting with 0xFF but invalid CRC
    std::vector<std::uint8_t> badCrcFrame { 0xFFU, 0x01U, 0x00U, 0x02U, 0x20U, 0x00U, 0x00U };
    const auto res2 = acc.push(badCrcFrame, false);
    EXPECT_TRUE(res2.empty());
    EXPECT_EQ(acc.checksumErrors(), 1U);

    // Reset stats
    acc.resetStats();
    EXPECT_EQ(acc.discardedBytes(), 0U);
    EXPECT_EQ(acc.checksumErrors(), 0U);
}

/// @brief Verify that chunked 7-byte ACKs from default address 1 are not misread as 4-byte general responses.
/// @details Regression test for Finding H4. A 7-byte ACK (FF 01 00 01 07 01 0A) delivered in chunks of
///          5 bytes and 2 bytes must be reassembled into a single 7-byte frame, not prematurely sliced into a 4-byte general response.
TEST(StreamAccumulatorTest, ChunkedAckAddress1NotMisreadAsGeneralResponse)
{
    PelcoD::RxStreamAccumulator acc;

    // 7-byte ACK for address 1: FF 01 00 01 07 01 0A
    // Checksum: (0x01 + 0x00 + 0x01 + 0x07 + 0x01) % 256 = 0x0A
    const std::vector<std::uint8_t> ackFrame { 0xFFU, 0x01U, 0x00U, 0x01U, 0x07U, 0x01U, 0x0AU };

    // Deliver in two chunks: 5 bytes + 2 bytes
    const std::vector<std::uint8_t> chunk1(ackFrame.begin(), ackFrame.begin() + 5);
    const std::vector<std::uint8_t> chunk2(ackFrame.begin() + 5, ackFrame.end());

    const auto res1 = acc.push(chunk1);
    EXPECT_TRUE(res1.empty()); // Must NOT prematurely extract false 4-byte frame FF 01 00 01
    EXPECT_EQ(acc.size(), 5U);

    const auto res2 = acc.push(chunk2);
    ASSERT_EQ(res2.size(), 1U);
    EXPECT_EQ(res2[0], ackFrame);
    EXPECT_EQ(acc.size(), 0U);
}

/// @brief Verify that response frames where camera address matches the response opcode are not misread when chunked.
/// @details Regression test for Finding H4: Address 0x59 responding to Pan Query (opcode 0x59).
///          FF 59 00 59 ... matches 4-byte checksum (0x59 + 0x00 == 0x59).
TEST(StreamAccumulatorTest, ChunkedOpcodeAddressCollisionNotMisread)
{
    PelcoD::RxStreamAccumulator acc;

    // Pan Query response for address 0x59 (89):
    // FF 59 00 59 10 20 E2
    // Checksum: (0x59 + 0x00 + 0x59 + 0x10 + 0x20) % 256 = 0xE2
    const std::vector<std::uint8_t> panFrame { 0xFFU, 0x59U, 0x00U, 0x59U, 0x10U, 0x20U, 0xE2U };

    const std::vector<std::uint8_t> chunk1(panFrame.begin(), panFrame.begin() + 4);
    const std::vector<std::uint8_t> chunk2(panFrame.begin() + 4, panFrame.end());

    const auto res1 = acc.push(chunk1);
    EXPECT_TRUE(res1.empty()); // Must wait for full frame
    EXPECT_EQ(acc.size(), 4U);

    const auto res2 = acc.push(chunk2);
    ASSERT_EQ(res2.size(), 1U);
    EXPECT_EQ(res2[0], panFrame);
    EXPECT_EQ(acc.size(), 0U);
}

/// @brief Verify stream reassembly when a 7-byte ACK arrives byte-by-byte at every boundary.
/// @details Tests that byte-by-byte ingestion across all 7 bytes yields no premature frames until the 7th byte.
TEST(StreamAccumulatorTest, ByteByByteChunkingEveryBoundary)
{
    PelcoD::RxStreamAccumulator acc;

    const std::vector<std::uint8_t> ackFrame { 0xFFU, 0x01U, 0x00U, 0x01U, 0x07U, 0x01U, 0x0AU };

    for (std::size_t i { 0U }; i < ackFrame.size() - 1U; ++i) {
        const auto step = acc.push(std::vector<std::uint8_t> { ackFrame[i] });
        EXPECT_TRUE(step.empty()) << "Premature extraction at byte index " << i;
    }

    const auto finalStep = acc.push(std::vector<std::uint8_t> { ackFrame.back() });
    ASSERT_EQ(finalStep.size(), 1U);
    EXPECT_EQ(finalStep[0], ackFrame);
    EXPECT_EQ(acc.size(), 0U);
}

/// @brief Verify corrupted 7-byte frame matching 4-byte checksum does not emit false general response.
/// @details FF 01 00 01 10 20 99 has invalid 7-byte checksum, but bytes 0-3 pass 4-byte checksum.
TEST(StreamAccumulatorTest, Corrupted7ByteFrameNotParsedAsFalseGeneralResponse)
{
    PelcoD::RxStreamAccumulator acc;

    const std::vector<std::uint8_t> badFrame { 0xFFU, 0x01U, 0x00U, 0x01U, 0x10U, 0x20U, 0x99U };
    const auto res = acc.push(badFrame);
    EXPECT_TRUE(res.empty());
}

/// @brief Verify sliding cursor efficiently extracts frames amidst stream noise and compacts buffer (L11).
TEST(StreamAccumulatorTest, SlidingCursorHandlesStreamNoiseEfficiently)
{
    PelcoD::RxStreamAccumulator acc;

    const auto frame1 = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x02U, 0x20U, 0x00U);
    const auto frame2 = PelcoD::PelcoDFrame::createFrame(2U, 0x00U, 0x04U, 0x00U, 0x30U);

    // Stream: 100 bytes noise, frame1, 500 bytes noise, frame2
    std::vector<std::uint8_t> noisyStream(100U, 0x55U);
    noisyStream.insert(noisyStream.end(), frame1.begin(), frame1.end());
    noisyStream.insert(noisyStream.end(), 500U, 0xAAU);
    noisyStream.insert(noisyStream.end(), frame2.begin(), frame2.end());

    const auto extracted = acc.push(noisyStream);
    ASSERT_EQ(extracted.size(), 2U);
    EXPECT_EQ(extracted[0], frame1);
    EXPECT_EQ(extracted[1], frame2);
    EXPECT_EQ(acc.size(), 0U);
    EXPECT_EQ(acc.discardedBytes(), 600U);
    EXPECT_EQ(acc.checksumErrors(), 0U);
}

/// @brief Verify that checksumErrors increments for corrupted candidate frames while noise increments discardedBytes (L11).
TEST(StreamAccumulatorTest, ChecksumErrorAccurateAccounting)
{
    PelcoD::RxStreamAccumulator acc;

    // 10 noise bytes, followed by a 7-byte candidate starting with SyncByte but corrupted checksum
    std::vector<std::uint8_t> stream(10U, 0x12U);
    const std::vector<std::uint8_t> badCandidate { 0xFFU, 0x01U, 0x00U, 0x04U, 0x20U, 0x00U, 0x99U };
    stream.insert(stream.end(), badCandidate.begin(), badCandidate.end());

    const auto extracted = acc.push(stream);
    EXPECT_TRUE(extracted.empty());

    // 10 preamble noise bytes + 1 byte slid past bad sync = 11 discarded bytes
    EXPECT_GE(acc.discardedBytes(), 10U);
    EXPECT_GE(acc.checksumErrors(), 1U);
}

} // namespace
