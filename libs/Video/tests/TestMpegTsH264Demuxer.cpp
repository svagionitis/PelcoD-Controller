/// @file TestMpegTsH264Demuxer.cpp
/// @brief Unit tests for MpegTsH264Demuxer.

#include "MpegTsH264Demuxer.h"

#include <cstddef>
#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

namespace Video {
namespace {

    /// @brief Helper to generate a 188-byte MPEG-TS packet.
    std::vector<std::uint8_t> makeTsPacket(
        std::uint16_t pid, bool pusi, std::uint8_t cc, const std::vector<std::uint8_t>& payload)
    {
        std::vector<std::uint8_t> pkt(188U, 0xFFU);
        pkt[0] = 0x47U; // Sync byte
        pkt[1] = static_cast<std::uint8_t>((pusi ? 0x40U : 0x00U) | ((pid >> 8) & 0x1FU));
        pkt[2] = static_cast<std::uint8_t>(pid & 0xFFU);

        if (payload.size() >= 184U) {
            pkt[3] = static_cast<std::uint8_t>(0x10U | (cc & 0x0FU)); // Payload only (AFC=01)
            for (std::size_t i { 0U }; i < 184U; ++i) {
                pkt[4U + i] = payload[i];
            }
        } else {
            // Adaptation field with stuffing (AFC=11)
            pkt[3] = static_cast<std::uint8_t>(0x30U | (cc & 0x0FU));
            const std::size_t stuffingNeeded { 184U - payload.size() };
            const std::uint8_t adaptLen { static_cast<std::uint8_t>(stuffingNeeded - 1U) };
            pkt[4] = adaptLen;
            if (adaptLen > 0U) {
                pkt[5] = 0x00U; // flags = 0
                for (std::size_t i { 1U }; i < adaptLen; ++i) {
                    pkt[5U + i] = 0xFFU; // stuffing
                }
            }
            const std::size_t payloadOffset { 5U + adaptLen };
            for (std::size_t i { 0U }; i < payload.size(); ++i) {
                pkt[payloadOffset + i] = payload[i];
            }
        }
        return pkt;
    }

    /// @brief Helper to generate a PAT packet mapping program 1 to pmtPid.
    std::vector<std::uint8_t> makePatPacket(std::uint16_t pmtPid)
    {
        std::vector<std::uint8_t> payload;
        payload.push_back(0x00U); // pointer_field = 0

        // PAT section
        payload.push_back(0x00U); // table_id = 0x00
        payload.push_back(0xB0U); // section_syntax_indicator=1, section_length high 4 bits = 0
        payload.push_back(0x0DU); // section_length = 13 (5 bytes syntax + 4 bytes prog + 4 bytes CRC)
        payload.push_back(0x00U); // transport_stream_id
        payload.push_back(0x01U);
        payload.push_back(0x01U); // version=0, current_next=1
        payload.push_back(0x00U); // section_number
        payload.push_back(0x00U); // last_section_number

        // Program 1 entry
        payload.push_back(0x00U); // program_number = 1
        payload.push_back(0x01U);
        payload.push_back(static_cast<std::uint8_t>(0xE0U | ((pmtPid >> 8) & 0x1FU)));
        payload.push_back(static_cast<std::uint8_t>(pmtPid & 0xFFU));

        // CRC32 placeholder
        payload.push_back(0x00U);
        payload.push_back(0x00U);
        payload.push_back(0x00U);
        payload.push_back(0x00U);

        return makeTsPacket(0x0000U, true, 0U, payload);
    }

    /// @brief Helper to generate a PMT packet mapping streamType to videoPid.
    std::vector<std::uint8_t> makePmtPacket(
        std::uint16_t pmtPid, std::uint16_t videoPid, std::uint8_t streamType = 0x1BU)
    {
        std::vector<std::uint8_t> payload;
        payload.push_back(0x00U); // pointer_field = 0

        // PMT section
        payload.push_back(0x02U); // table_id = 0x02 (PMT)
        payload.push_back(0xB0U); // section_syntax_indicator=1
        payload.push_back(0x12U); // section_length = 18 (9 syntax + 5 ES + 4 CRC)
        payload.push_back(0x00U); // program_number = 1
        payload.push_back(0x01U);
        payload.push_back(0x01U); // version=0, current_next=1
        payload.push_back(0x00U); // section_number
        payload.push_back(0x00U); // last_section_number
        payload.push_back(static_cast<std::uint8_t>(0xE0U | ((videoPid >> 8) & 0x1FU))); // PCR PID
        payload.push_back(static_cast<std::uint8_t>(videoPid & 0xFFU));
        payload.push_back(0xF0U); // program_info_length = 0
        payload.push_back(0x00U);

        // Elementary stream descriptor
        payload.push_back(streamType); // stream_type (0x1B = H.264)
        payload.push_back(static_cast<std::uint8_t>(0xE0U | ((videoPid >> 8) & 0x1FU)));
        payload.push_back(static_cast<std::uint8_t>(videoPid & 0xFFU));
        payload.push_back(0xF0U); // ES_info_length = 0
        payload.push_back(0x00U);

        // CRC32 placeholder
        payload.push_back(0x00U);
        payload.push_back(0x00U);
        payload.push_back(0x00U);
        payload.push_back(0x00U);

        return makeTsPacket(pmtPid, true, 0U, payload);
    }

    /// @brief Helper to generate PES header bytes with PTS.
    std::vector<std::uint8_t> makePesHeader(std::uint64_t pts90k)
    {
        std::vector<std::uint8_t> pes;
        pes.push_back(0x00U); // start code prefix
        pes.push_back(0x00U);
        pes.push_back(0x01U);
        pes.push_back(0xE0U); // video stream ID
        pes.push_back(0x00U); // packet length = 0 (unspecified)
        pes.push_back(0x00U);
        pes.push_back(0x80U); // flags 1
        pes.push_back(0x80U); // flags 2: PTS present (bit 7)
        pes.push_back(0x05U); // header data length = 5

        // 5-byte PTS encoding: 0010 (4 bits) + PTS[32..30] (3 bits) + 1 (1 bit)
        pes.push_back(static_cast<std::uint8_t>(0x21U | (((pts90k >> 30) & 0x07U) << 1)));
        pes.push_back(static_cast<std::uint8_t>((pts90k >> 22) & 0xFFU));
        pes.push_back(static_cast<std::uint8_t>(0x01U | (((pts90k >> 15) & 0x7FU) << 1)));
        pes.push_back(static_cast<std::uint8_t>((pts90k >> 7) & 0xFFU));
        pes.push_back(static_cast<std::uint8_t>(0x01U | ((pts90k & 0x7FU) << 1)));

        return pes;
    }

} // namespace

/// @brief Test fixture for MpegTsH264Demuxer.
class MpegTsH264DemuxerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_nalus.clear();
        m_timestamps.clear();
    }

    struct CapturedNalu {
        std::vector<std::uint8_t> data;
        double pts { 0.0 };
    };

    std::vector<CapturedNalu> m_nalus;
    std::vector<double> m_timestamps;

    MpegTsH264Demuxer::NaluCallback createCallback()
    {
        return [this](const std::uint8_t* data, std::size_t size, double pts) {
            CapturedNalu captured;
            captured.data.assign(data, data + size);
            captured.pts = pts;
            m_nalus.push_back(std::move(captured));
            m_timestamps.push_back(pts);
        };
    }
};

TEST_F(MpegTsH264DemuxerTest, InitialStateAndReset)
{
    MpegTsH264Demuxer demuxer(createCallback());
    EXPECT_FALSE(demuxer.videoPid().has_value());
    EXPECT_FALSE(demuxer.pmtPid().has_value());

    demuxer.setVideoPid(0x0100U);
    EXPECT_EQ(demuxer.videoPid(), 0x0100U);

    demuxer.reset();
    EXPECT_FALSE(demuxer.videoPid().has_value());
    EXPECT_FALSE(demuxer.pmtPid().has_value());
}

TEST_F(MpegTsH264DemuxerTest, PatPmtAutoDiscovery)
{
    MpegTsH264Demuxer demuxer(createCallback());

    const std::uint16_t expectedPmtPid { 0x1000U };
    const std::uint16_t expectedVideoPid { 0x0100U };

    const auto patPkt { makePatPacket(expectedPmtPid) };
    const auto pmtPkt { makePmtPacket(expectedPmtPid, expectedVideoPid, 0x1BU) };

    EXPECT_EQ(demuxer.processPacket(patPkt.data()), 0U);
    EXPECT_EQ(demuxer.pmtPid(), expectedPmtPid);
    EXPECT_FALSE(demuxer.videoPid().has_value());

    EXPECT_EQ(demuxer.processPacket(pmtPkt.data()), 0U);
    EXPECT_EQ(demuxer.videoPid(), expectedVideoPid);
}

TEST_F(MpegTsH264DemuxerTest, SinglePacketPesDemuxingAndPts)
{
    MpegTsH264Demuxer demuxer(createCallback());
    const std::uint16_t videoPid { 0x0100U };
    demuxer.setVideoPid(videoPid);

    // 90 kHz PTS corresponding to 2.0 seconds: 2.0 * 90000 = 180000
    const std::uint64_t pts90k { 180000ULL };
    auto pes { makePesHeader(pts90k) };

    // Append Annex-B NAL unit (0x00 0x00 0x00 0x01 followed by IDR slice byte 0x65)
    pes.push_back(0x00U);
    pes.push_back(0x00U);
    pes.push_back(0x00U);
    pes.push_back(0x01U);
    pes.push_back(0x65U);
    pes.push_back(0x88U);
    pes.push_back(0x84U);

    const auto pkt1 { makeTsPacket(videoPid, true, 0U, pes) };
    static_cast<void>(demuxer.processPacket(pkt1.data()));

    // PES is in progress; flush to finish
    demuxer.flush();

    ASSERT_EQ(m_nalus.size(), 1U);
    EXPECT_DOUBLE_EQ(m_nalus[0].pts, 2.0);
    ASSERT_GE(m_nalus[0].data.size(), 7U);
    EXPECT_EQ(m_nalus[0].data[0], 0x00U);
    EXPECT_EQ(m_nalus[0].data[1], 0x00U);
    EXPECT_EQ(m_nalus[0].data[2], 0x00U);
    EXPECT_EQ(m_nalus[0].data[3], 0x01U);
    EXPECT_EQ(m_nalus[0].data[4], 0x65U);
}

TEST_F(MpegTsH264DemuxerTest, MultiPacketPesReassembly)
{
    MpegTsH264Demuxer demuxer(createCallback());
    const std::uint16_t videoPid { 0x0100U };
    demuxer.setVideoPid(videoPid);

    const std::uint64_t pts90k { 90000ULL }; // 1.0 second
    auto pesHeader { makePesHeader(pts90k) };

    // First packet: PES header + start code + partial NAL payload
    pesHeader.push_back(0x00U);
    pesHeader.push_back(0x00U);
    pesHeader.push_back(0x00U);
    pesHeader.push_back(0x01U);
    pesHeader.push_back(0x67U); // SPS
    for (std::size_t i { 0U }; i < 50U; ++i) {
        pesHeader.push_back(static_cast<std::uint8_t>(i));
    }
    const auto pkt1 { makeTsPacket(videoPid, true, 0U, pesHeader) };

    // Second packet: continuation of the same PES (PUSI = false)
    std::vector<std::uint8_t> continuation;
    for (std::size_t i { 50U }; i < 150U; ++i) {
        continuation.push_back(static_cast<std::uint8_t>(i));
    }
    const auto pkt2 { makeTsPacket(videoPid, false, 1U, continuation) };

    static_cast<void>(demuxer.processPacket(pkt1.data()));
    static_cast<void>(demuxer.processPacket(pkt2.data()));
    demuxer.flush();

    ASSERT_EQ(m_nalus.size(), 1U);
    EXPECT_DOUBLE_EQ(m_nalus[0].pts, 1.0);
    EXPECT_EQ(m_nalus[0].data[4], 0x67U);
    EXPECT_EQ(m_nalus[0].data.size(), 155U); // 4 (startcode) + 1 (SPS) + 150 (data)
}

TEST_F(MpegTsH264DemuxerTest, MultiNaluInSinglePesDispatchedSeparately)
{
    MpegTsH264Demuxer demuxer(createCallback());
    const std::uint16_t videoPid { 0x0100U };
    demuxer.setVideoPid(videoPid);

    auto pes { makePesHeader(270000ULL) }; // 3.0 seconds

    // SPS (NAL type 7)
    pes.push_back(0x00U);
    pes.push_back(0x00U);
    pes.push_back(0x00U);
    pes.push_back(0x01U);
    pes.push_back(0x67U);
    pes.push_back(0x42U);

    // PPS (NAL type 8)
    pes.push_back(0x00U);
    pes.push_back(0x00U);
    pes.push_back(0x00U);
    pes.push_back(0x01U);
    pes.push_back(0x68U);
    pes.push_back(0xCEU);

    // IDR slice (NAL type 5)
    pes.push_back(0x00U);
    pes.push_back(0x00U);
    pes.push_back(0x00U);
    pes.push_back(0x01U);
    pes.push_back(0x65U);
    pes.push_back(0xB8U);

    const auto pkt { makeTsPacket(videoPid, true, 0U, pes) };
    static_cast<void>(demuxer.processPacket(pkt.data()));
    demuxer.flush();

    ASSERT_EQ(m_nalus.size(), 3U);
    EXPECT_EQ(m_nalus[0].data[4], 0x67U);
    EXPECT_EQ(m_nalus[1].data[4], 0x68U);
    EXPECT_EQ(m_nalus[2].data[4], 0x65U);
    EXPECT_DOUBLE_EQ(m_nalus[0].pts, 3.0);
    EXPECT_DOUBLE_EQ(m_nalus[1].pts, 3.0);
    EXPECT_DOUBLE_EQ(m_nalus[2].pts, 3.0);
}

TEST_F(MpegTsH264DemuxerTest, StreamResyncToSyncByte)
{
    MpegTsH264Demuxer demuxer(createCallback());
    const std::uint16_t videoPid { 0x0100U };
    demuxer.setVideoPid(videoPid);

    auto pes { makePesHeader(90000ULL) };
    pes.push_back(0x00U);
    pes.push_back(0x00U);
    pes.push_back(0x00U);
    pes.push_back(0x01U);
    pes.push_back(0x65U);

    const auto pkt { makeTsPacket(videoPid, true, 0U, pes) };

    // Prepend 17 bytes of arbitrary garbage data before valid TS packet
    std::vector<std::uint8_t> corruptedStream;
    for (std::size_t i { 0U }; i < 17U; ++i) {
        corruptedStream.push_back(0xAAU);
    }
    corruptedStream.insert(corruptedStream.end(), pkt.begin(), pkt.end());

    static_cast<void>(demuxer.processData(corruptedStream.data(), corruptedStream.size()));
    demuxer.flush();

    ASSERT_EQ(m_nalus.size(), 1U);
    EXPECT_EQ(m_nalus[0].data[4], 0x65U);
}

TEST_F(MpegTsH264DemuxerTest, IgnoreCorruptAndScrambledPackets)
{
    MpegTsH264Demuxer demuxer(createCallback());
    const std::uint16_t videoPid { 0x0100U };
    demuxer.setVideoPid(videoPid);

    auto pes { makePesHeader(90000ULL) };
    pes.push_back(0x00U);
    pes.push_back(0x00U);
    pes.push_back(0x00U);
    pes.push_back(0x01U);
    pes.push_back(0x65U);

    auto corruptPkt { makeTsPacket(videoPid, true, 0U, pes) };
    corruptPkt[1] |= 0x80U; // Set TEI bit (Transport Error Indicator)

    EXPECT_EQ(demuxer.processPacket(corruptPkt.data()), 0U);
    demuxer.flush();
    EXPECT_EQ(m_nalus.size(), 0U);

    auto scrambledPkt { makeTsPacket(videoPid, true, 0U, pes) };
    scrambledPkt[3] |= 0x80U; // Set TSC bits (Scrambled)

    EXPECT_EQ(demuxer.processPacket(scrambledPkt.data()), 0U);
    demuxer.flush();
    EXPECT_EQ(m_nalus.size(), 0U);
}

} // namespace Video
