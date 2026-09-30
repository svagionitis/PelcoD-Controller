/// @file TestSightlineAccumulator.cpp
/// @brief Unit tests for SightlineStreamAccumulator normal/extended framing, sync recovery, and stats.

#include "SightlineProtocolBuilder.h"
#include "SightlineStreamAccumulator.h"
#include "SightlineTypes.h"

#include <gtest/gtest.h>

#include <vector>

namespace Sightline {
namespace {

    /// @brief Verify extraction of a single normal (<128 bytes) framed SLA packet.
    TEST(TestSightlineAccumulator, SingleNormalPacket)
    {
        SightlineStreamAccumulator acc;

        const auto pkt = SightlineProtocolBuilder::buildGetParameters(0x00U);
        ASSERT_EQ(pkt.size(), 6U); // 0x51, 0xAC, 0x03, 0x28, 0x00, 0x73

        const auto extracted = acc.push(pkt, true);
        ASSERT_EQ(extracted.size(), 1U);
        EXPECT_EQ(extracted[0], pkt);
        EXPECT_EQ(acc.size(), 0U);
        EXPECT_EQ(acc.packetsExtracted(), 1U);
    }

    /// @brief Verify reassembly of fragmented chunks arriving one byte at a time.
    TEST(TestSightlineAccumulator, FragmentedChunkIngest)
    {
        SightlineStreamAccumulator acc;

        const auto pkt = SightlineProtocolBuilder::buildGetParameters(0x00U);

        for (std::size_t i = 0U; i < pkt.size() - 1U; ++i) {
            const std::vector<std::uint8_t> singleByte { pkt[i] };
            EXPECT_TRUE(acc.push(singleByte, true).empty());
        }

        const std::vector<std::uint8_t> lastByte { pkt.back() };
        const auto res = acc.push(lastByte, true);
        ASSERT_EQ(res.size(), 1U);
        EXPECT_EQ(res[0], pkt);
        EXPECT_EQ(acc.size(), 0U);
    }

    /// @brief Verify recovery when valid frame is preceded by unaligned noise bytes.
    TEST(TestSightlineAccumulator, NoisePreambleRecovery)
    {
        SightlineStreamAccumulator acc;

        const auto pkt = SightlineProtocolBuilder::buildGetParameters(0x00U);
        std::vector<std::uint8_t> stream { 0x12U, 0x34U, 0x51U, 0x99U, 0xFFU };
        stream.insert(stream.end(), pkt.begin(), pkt.end());

        const auto res = acc.push(stream, true);
        ASSERT_EQ(res.size(), 1U);
        EXPECT_EQ(res[0], pkt);
        EXPECT_EQ(acc.discardedBytes(), 5U);
        EXPECT_EQ(acc.packetsExtracted(), 1U);
    }

    /// @brief Verify rejection of packet with corrupted CRC and forward scanning.
    TEST(TestSightlineAccumulator, CorruptedChecksumRejection)
    {
        SightlineStreamAccumulator acc;

        auto badPkt = SightlineProtocolBuilder::buildGetParameters(0x00U);
        badPkt.back() ^= 0xFFU; // Corrupt checksum byte

        const auto goodPkt = SightlineProtocolBuilder::buildGetParameters(0x01U);

        std::vector<std::uint8_t> stream {};
        stream.insert(stream.end(), badPkt.begin(), badPkt.end());
        stream.insert(stream.end(), goodPkt.begin(), goodPkt.end());

        const auto res = acc.push(stream, true);
        ASSERT_EQ(res.size(), 1U);
        EXPECT_EQ(res[0], goodPkt);
        EXPECT_GE(acc.checksumErrors(), 1U);
    }

    /// @brief Verify extended 2-byte length encoding for packets with payload >= 128 bytes.
    TEST(TestSightlineAccumulator, ExtendedLengthPacket)
    {
        SightlineStreamAccumulator acc;

        // Create a large payload with 150 bytes
        std::vector<std::uint8_t> largePayload(150U, 0xAAU);
        const auto largePkt = SightlineProtocolBuilder::buildRawPacket(MessageId::CommandPassThrough, largePayload);

        // Header size should be 4 bytes (0x51, 0xAC, lenLow, lenHigh)
        ASSERT_GE(largePkt.size(), 154U);
        EXPECT_TRUE((largePkt[2U] & 0x80U) != 0U); // MSB set on length low

        const auto extracted = acc.push(largePkt, true);
        ASSERT_EQ(extracted.size(), 1U);
        EXPECT_EQ(extracted[0], largePkt);
        EXPECT_EQ(acc.size(), 0U);
    }

    /// @brief Verify telemetry counters resetStats().
    TEST(TestSightlineAccumulator, TelemetryReset)
    {
        SightlineStreamAccumulator acc;

        const std::vector<std::uint8_t> noise { 0x00U, 0x11U, 0x22U, 0x33U, 0x44U };
        static_cast<void>(acc.push(noise, true));
        EXPECT_EQ(acc.discardedBytes(), 5U);

        acc.resetStats();
        EXPECT_EQ(acc.discardedBytes(), 0U);
        EXPECT_EQ(acc.checksumErrors(), 0U);
        EXPECT_EQ(acc.packetsExtracted(), 0U);
    }

} // namespace
} // namespace Sightline
