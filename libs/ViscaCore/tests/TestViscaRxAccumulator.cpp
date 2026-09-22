/// @file TestViscaRxAccumulator.cpp
/// @brief Unit tests for ViscaRxAccumulator stream framing, fragmentation, and noise recovery.

#include "ViscaRxAccumulator.h"
#include <gtest/gtest.h>

using namespace Visca;

/// @brief Tests receiving a single complete frame in one byte buffer.
/// @details Verifies that a clean packet is framed and available via popFrame().
TEST(TestViscaRxAccumulator, SingleCompleteFrame)
{
    ViscaRxAccumulator accumulator;

    const std::vector<uint8_t> packet = { 0x90, 0x41, 0xFF };
    accumulator.addData(packet);

    EXPECT_TRUE(accumulator.hasFrames());
    EXPECT_EQ(accumulator.pendingFrameCount(), 1U);

    const auto frame = accumulator.popFrame();
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->size(), 3U);
    EXPECT_EQ((*frame)[0], 0x90);
    EXPECT_EQ((*frame)[1], 0x41);
    EXPECT_EQ((*frame)[2], 0xFF);

    EXPECT_FALSE(accumulator.hasFrames());
}

/// @brief Verifies that fragmented byte arrival reassembles into a complete frame.
/// @details Simulates bytes arriving one-by-one from a serial or network transport.
TEST(TestViscaRxAccumulator, FragmentedBytesArrival)
{
    ViscaRxAccumulator accumulator;

    const std::vector<uint8_t> packet = { 0x90, 0x50, 0x02, 0xFF };
    for (size_t i = 0; i < packet.size() - 1; ++i) {
        accumulator.addData(&packet[i], 1);
        EXPECT_FALSE(accumulator.hasFrames());
    }

    // Supply the final 0xFF delimiter byte
    accumulator.addData(&packet.back(), 1);
    EXPECT_TRUE(accumulator.hasFrames());

    const auto frame = accumulator.popFrame();
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->size(), 4U);
    EXPECT_EQ((*frame)[2], 0x02);
}

/// @brief Tests back-to-back concatenated frames within a single received chunk.
/// @details Verifies that multiple packets delimited by 0xFF are correctly split.
TEST(TestViscaRxAccumulator, MultipleFramesInSingleBuffer)
{
    ViscaRxAccumulator accumulator;

    // Two back-to-back packets: ACK (90 41 FF) and Completion (90 51 FF)
    const std::vector<uint8_t> stream = { 0x90, 0x41, 0xFF, 0x90, 0x51, 0xFF };

    accumulator.addData(stream);
    EXPECT_EQ(accumulator.pendingFrameCount(), 2U);

    const auto f1 = accumulator.popFrame();
    ASSERT_TRUE(f1.has_value());
    EXPECT_TRUE(f1->isAck());

    const auto f2 = accumulator.popFrame();
    ASSERT_TRUE(f2.has_value());
    EXPECT_TRUE(f2->isCompletion());

    EXPECT_FALSE(accumulator.hasFrames());
}

/// @brief Tests noise recovery when invalid bytes precede a valid packet.
/// @details Verifies that leading garbage bytes without 0xFF or short fragments are discarded.
TEST(TestViscaRxAccumulator, NoiseAndRecovery)
{
    ViscaRxAccumulator accumulator;

    // Stream with garbage noise, an invalid 2-byte fragment, then a valid 3-byte packet
    const std::vector<uint8_t> stream = {
        0x12, 0x34, 0x00, 0xFF, // Invalid: < 3 bytes, MSB not set
        0x90, 0x42, 0xFF // Valid packet
    };

    accumulator.addData(stream);
    EXPECT_EQ(accumulator.pendingFrameCount(), 1U);

    const auto frame = accumulator.popFrame();
    ASSERT_TRUE(frame.has_value());
    EXPECT_TRUE(frame->isAck());
    EXPECT_EQ(frame->socket(), ViscaSocket::Socket2);
}

/// @brief Verifies asynchronous callback dispatch upon frame reception.
/// @details Tests that registering a FrameCallback notifies immediately upon frame assembly.
TEST(TestViscaRxAccumulator, CallbackNotification)
{
    ViscaRxAccumulator accumulator;

    std::vector<ViscaFrame> received;
    accumulator.setFrameCallback([&](const ViscaFrame& frame) { received.push_back(frame); });

    const std::vector<uint8_t> packet = { 0x90, 0x50, 0x00, 0x20, 0x07, 0x11, 0x01, 0x00, 0x02, 0xFF };
    accumulator.addData(packet);

    ASSERT_EQ(received.size(), 1U);
    EXPECT_EQ(received[0].size(), 10U);
    EXPECT_TRUE(received[0].isInquiryResponse());
}
