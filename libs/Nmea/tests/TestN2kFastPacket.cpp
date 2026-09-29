#include "n2k/N2kFastPacketAssembler.h"
#include "n2k/N2kTypes.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace Nmea::N2k {

TEST(TestN2kFastPacket, SingleFramePgnImmediateReturn)
{
    N2kFastPacketAssembler assembler;

    // PGN 129025 is single-frame (8 bytes)
    N2kHeader hdr {};
    hdr.priority = 2U;
    hdr.pgn = static_cast<std::uint32_t>(Pgn::PositionRapidUpdate);
    hdr.sourceAddress = 35U;
    hdr.destinationAddress = 0xFFU;

    CanFrame frame {};
    frame.id = hdr.toCanId();
    frame.dlc = 8U;
    frame.data = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };

    const auto result = assembler.processCanFrame(frame);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->header.pgn, static_cast<std::uint32_t>(Pgn::PositionRapidUpdate));
    EXPECT_EQ(result->header.sourceAddress, 35U);
    EXPECT_EQ(result->payload.size(), 8U);
    EXPECT_EQ(result->payload[0], 0x11);
    EXPECT_EQ(result->payload[7], 0x88);
}

TEST(TestN2kFastPacket, MultiFrameReassembly)
{
    N2kFastPacketAssembler assembler;

    // PGN 129038 is Fast Packet (28 bytes)
    N2kHeader hdr {};
    hdr.priority = 4U;
    hdr.pgn = static_cast<std::uint32_t>(Pgn::AisClassAPositionReport);
    hdr.sourceAddress = 42U;
    hdr.destinationAddress = 0xFFU;

    const std::uint32_t canId = hdr.toCanId();
    const std::uint8_t seq = 3U; // arbitrary sequence 0..31

    // 28-byte synthetic test payload
    std::vector<std::uint8_t> expectedPayload(28U);
    for (std::size_t i = 0; i < 28U; ++i) {
        expectedPayload[i] = static_cast<std::uint8_t>(i + 1U);
    }

    // Frame 0: header byte = (seq << 3) | 0, totalBytes = 28, 6 data bytes
    CanFrame f0 {};
    f0.id = canId;
    f0.dlc = 8U;
    f0.data[0] = static_cast<std::uint8_t>((seq << 3) | 0U);
    f0.data[1] = 28U;
    for (std::size_t i = 0; i < 6U; ++i) {
        f0.data[2 + i] = expectedPayload[i];
    }

    // Frame 1: header byte = (seq << 3) | 1, 7 data bytes
    CanFrame f1 {};
    f1.id = canId;
    f1.dlc = 8U;
    f1.data[0] = static_cast<std::uint8_t>((seq << 3) | 1U);
    for (std::size_t i = 0; i < 7U; ++i) {
        f1.data[1 + i] = expectedPayload[6 + i];
    }

    // Frame 2: header byte = (seq << 3) | 2, 7 data bytes
    CanFrame f2 {};
    f2.id = canId;
    f2.dlc = 8U;
    f2.data[0] = static_cast<std::uint8_t>((seq << 3) | 2U);
    for (std::size_t i = 0; i < 7U; ++i) {
        f2.data[1 + i] = expectedPayload[13 + i];
    }

    // Frame 3: header byte = (seq << 3) | 3, 7 data bytes
    CanFrame f3 {};
    f3.id = canId;
    f3.dlc = 8U;
    f3.data[0] = static_cast<std::uint8_t>((seq << 3) | 3U);
    for (std::size_t i = 0; i < 7U; ++i) {
        f3.data[1 + i] = expectedPayload[20 + i];
    }

    // Frame 4: header byte = (seq << 3) | 4, remaining 1 data byte
    CanFrame f4 {};
    f4.id = canId;
    f4.dlc = 8U;
    f4.data[0] = static_cast<std::uint8_t>((seq << 3) | 4U);
    f4.data[1] = expectedPayload[27];

    EXPECT_FALSE(assembler.processCanFrame(f0).has_value());
    EXPECT_FALSE(assembler.processCanFrame(f1).has_value());
    EXPECT_FALSE(assembler.processCanFrame(f2).has_value());
    EXPECT_FALSE(assembler.processCanFrame(f3).has_value());

    const auto finalMsg = assembler.processCanFrame(f4);
    ASSERT_TRUE(finalMsg.has_value());
    EXPECT_EQ(finalMsg->header.pgn, static_cast<std::uint32_t>(Pgn::AisClassAPositionReport));
    EXPECT_EQ(finalMsg->payload.size(), 28U);
    EXPECT_EQ(finalMsg->payload, expectedPayload);
}

TEST(TestN2kFastPacket, OutOfOrderFrameRejection)
{
    N2kFastPacketAssembler assembler;

    N2kHeader hdr {};
    hdr.pgn = static_cast<std::uint32_t>(Pgn::AisClassAPositionReport);
    hdr.sourceAddress = 10U;

    const std::uint32_t canId = hdr.toCanId();
    const std::uint8_t seq = 1U;

    // Frame 0:
    CanFrame f0 {};
    f0.id = canId;
    f0.dlc = 8U;
    f0.data[0] = static_cast<std::uint8_t>((seq << 3) | 0U);
    f0.data[1] = 20U;

    // Frame 2 (Skipping Frame 1)
    CanFrame f2 {};
    f2.id = canId;
    f2.dlc = 8U;
    f2.data[0] = static_cast<std::uint8_t>((seq << 3) | 2U);

    EXPECT_FALSE(assembler.processCanFrame(f0).has_value());
    // Should reject Frame 2 due to sequence order error
    EXPECT_FALSE(assembler.processCanFrame(f2).has_value());

    // Session should be aborted, so subsequent Frame 1 is now rejected as orphaned
    CanFrame f1 {};
    f1.id = canId;
    f1.dlc = 8U;
    f1.data[0] = static_cast<std::uint8_t>((seq << 3) | 1U);
    EXPECT_FALSE(assembler.processCanFrame(f1).has_value());
}

TEST(TestN2kFastPacket, SessionTimeoutPruning)
{
    N2kFastPacketAssembler assembler;

    N2kHeader hdr {};
    hdr.pgn = static_cast<std::uint32_t>(Pgn::AisClassAPositionReport);
    hdr.sourceAddress = 15U;

    CanFrame f0 {};
    f0.id = hdr.toCanId();
    f0.dlc = 8U;
    f0.data[0] = 0x00;
    f0.data[1] = 50U;

    EXPECT_FALSE(assembler.processCanFrame(f0).has_value());

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    assembler.pruneTimedOutSessions(std::chrono::milliseconds(10));

    // Attempting to deliver frame 1 after timeout prune should be rejected
    CanFrame f1 {};
    f1.id = hdr.toCanId();
    f1.dlc = 8U;
    f1.data[0] = 0x01;
    EXPECT_FALSE(assembler.processCanFrame(f1).has_value());
}

} // namespace Nmea::N2k
