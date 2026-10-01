/// @file TestSightlineZeroCopy.cpp
/// @brief Unit tests verifying zero-copy byte view semantics and payload extraction.

#include "SightlineFraming.h"
#include "SightlineProtocolBuilder.h"
#include "SightlineTypes.h"

#include <gtest/gtest.h>
#include <vector>

namespace Sightline {
namespace {

    /// @brief Verify ByteView core construction and range access.
    TEST(TestSightlineZeroCopy, ByteViewBasicOperations)
    {
        const std::vector<std::uint8_t> data { 0x01U, 0x02U, 0x03U, 0x04U, 0x05U };
        const ByteView view { data };

        EXPECT_FALSE(view.empty());
        EXPECT_EQ(view.size(), 5U);
        EXPECT_EQ(view.data(), data.data());
        EXPECT_EQ(view[0], 0x01U);
        EXPECT_EQ(view[4], 0x05U);

        // Vector equality operator
        EXPECT_EQ(view, data);
        EXPECT_EQ(data, view);

        // Subspan slicing
        const ByteView mid = view.subspan(1U, 3U);
        EXPECT_EQ(mid.size(), 3U);
        EXPECT_EQ(mid[0], 0x02U);
        EXPECT_EQ(mid[2], 0x04U);

        // Out-of-bounds subspan clamps safely
        const ByteView oob = view.subspan(10U);
        EXPECT_TRUE(oob.empty());
    }

    /// @brief Verify extractPayload performs true zero-copy extraction pointing to original buffer.
    TEST(TestSightlineZeroCopy, ExtractZeroCopyPointerMatch)
    {
        const std::vector<std::uint8_t> payload { 0x10U, 0x20U, 0x30U, 0x40U };
        const auto pkt = SightlineFraming::buildPacket(MessageId::SetVideoParameters, payload);

        const ByteView extracted = SightlineFraming::extractPayload(pkt);
        ASSERT_EQ(extracted.size(), payload.size());
        EXPECT_EQ(extracted, payload);

        // Verify zero-copy: extracted pointer must directly alias pkt buffer
        // hLen for short packet is 3 (header 2 + len 1). ID is at offset 3. Payload begins at offset 4.
        EXPECT_EQ(extracted.data(), pkt.data() + 4U);
    }

    /// @brief Verify empty payload extraction on messages without payload bytes.
    TEST(TestSightlineZeroCopy, ExtractEmptyPayload)
    {
        const MsgSaveParameters msg {};
        const auto savePkt = SightlineProtocolBuilder::buildSaveParameters(msg);
        const ByteView extracted = SightlineFraming::extractPayload(savePkt);

        EXPECT_TRUE(extracted.empty());
        EXPECT_EQ(extracted.size(), 0U);
    }

    /// @brief Verify extended length packets (>= 128 bytes) extract zero-copy correctly.
    TEST(TestSightlineZeroCopy, ExtractExtendedPacket)
    {
        std::vector<std::uint8_t> largePayload(150U, 0x5AU);
        largePayload[0] = 0x11U;
        largePayload[149] = 0x99U;

        const auto pkt = SightlineFraming::buildPacket(MessageId::SetEthernetDisplayParameters, largePayload);
        EXPECT_EQ(SightlineFraming::getHeaderLength(pkt), 4U); // 2-byte extended header

        const ByteView extracted = SightlineFraming::extractPayload(pkt);
        ASSERT_EQ(extracted.size(), 150U);
        EXPECT_EQ(extracted[0], 0x11U);
        EXPECT_EQ(extracted[149], 0x99U);

        // Header 2 + len 2 + id 1 = 5
        EXPECT_EQ(extracted.data(), pkt.data() + 5U);
    }

    /// @brief Verify malformed or truncated packet buffers return empty view safely.
    TEST(TestSightlineZeroCopy, TruncatedPacketSafety)
    {
        const std::vector<std::uint8_t> emptyPkt {};
        EXPECT_TRUE(SightlineFraming::extractPayload(emptyPkt).empty());

        const std::vector<std::uint8_t> shortPkt { 0x51U, 0xACU };
        EXPECT_TRUE(SightlineFraming::extractPayload(shortPkt).empty());

        // Header length is 3, but total size is only 4 (missing CRC)
        const std::vector<std::uint8_t> noCrcPkt { 0x51U, 0xACU, 0x02U, 0x00U };
        EXPECT_TRUE(SightlineFraming::extractPayload(noCrcPkt).empty());
    }

} // namespace
} // namespace Sightline
