#include "KlvCrc.h"
#include "KlvEncoder.h"
#include "MpegTsKlvExtractor.h"
#include "MpegTsKlvMuxer.h"
#include <gtest/gtest.h>
#include <vector>

using namespace Klv;

TEST(MpegTsKlvMuxerTest, PatAndPmtGeneration) {
    MpegTsMuxerConfig config;
    config.programNumber = 100U;
    config.pmtPid = 0x0200U;
    config.metadataPid = 0x0250U;
    config.streamType = KlvStreamType::MetadataInPes; // 0x15
    config.formatIdentifier = "KLVA";

    MpegTsKlvMuxer muxer(config);

    std::vector<std::vector<std::uint8_t>> packets;
    muxer.setPacketCallback([&packets](const std::uint8_t* data, std::size_t size) {
        packets.emplace_back(data, data + size);
    });

    const std::size_t emitted = muxer.emitPsiTables();
    ASSERT_EQ(emitted, 2U);
    ASSERT_EQ(packets.size(), 2U);

    // Verify PAT packet (Packet 0)
    const auto& patPacket = packets[0];
    EXPECT_EQ(patPacket.size(), MpegTsKlvMuxer::kTsPacketSize);
    EXPECT_EQ(patPacket[0], MpegTsKlvMuxer::kTsSyncByte);
    // PID 0x0000, PUSI = 1
    EXPECT_EQ(patPacket[1] & 0x40U, 0x40U);
    EXPECT_EQ(patPacket[1] & 0x1FU, 0x00U);
    EXPECT_EQ(patPacket[2], 0x00U);
    // Pointer field at byte 4
    EXPECT_EQ(patPacket[4], 0x00U);
    // PAT section at byte 5: Table ID = 0x00
    EXPECT_EQ(patPacket[5], 0x00U);

    // Verify PAT CRC32
    const std::size_t patSectionLen = ((static_cast<std::size_t>(patPacket[6] & 0x0FU)) << 8U) |
                                      static_cast<std::size_t>(patPacket[7]);
    const std::size_t patTotalSectionBytes = 3U + patSectionLen;
    const std::uint32_t patCrc = KlvCrc::calculateCrc32Mpeg(patPacket.data() + 5, patTotalSectionBytes);
    EXPECT_EQ(patCrc, 0x00000000U); // A valid section with CRC appended yields remainder 0

    // Verify PMT packet (Packet 1)
    const auto& pmtPacket = packets[1];
    EXPECT_EQ(pmtPacket.size(), MpegTsKlvMuxer::kTsPacketSize);
    EXPECT_EQ(pmtPacket[0], MpegTsKlvMuxer::kTsSyncByte);
    // PID 0x0200, PUSI = 1
    const std::uint16_t pmtPid = static_cast<std::uint16_t>(((pmtPacket[1] & 0x1FU) << 8U) | pmtPacket[2]);
    EXPECT_EQ(pmtPid, 0x0200U);
    // PMT Table ID = 0x02
    EXPECT_EQ(pmtPacket[5], 0x02U);

    // Verify PMT CRC32
    const std::size_t pmtSectionLen = ((static_cast<std::size_t>(pmtPacket[6] & 0x0FU)) << 8U) |
                                      static_cast<std::size_t>(pmtPacket[7]);
    const std::size_t pmtTotalSectionBytes = 3U + pmtSectionLen;
    const std::uint32_t pmtCrc = KlvCrc::calculateCrc32Mpeg(pmtPacket.data() + 5, pmtTotalSectionBytes);
    EXPECT_EQ(pmtCrc, 0x00000000U);
}

TEST(MpegTsKlvMuxerTest, SinglePacketMuxAndExtractRoundTrip) {
    UasDatalinkMessage msg;
    msg.missionId = "MUXER_SINGLE_TEST";
    msg.platformHeadingDeg = 240.5;
    msg.platformPitchDeg = 3.2;
    msg.platformRollDeg = -1.5;
    msg.sensorLatitudeDeg = 37.7749;
    msg.sensorLongitudeDeg = -122.4194;
    msg.sensorTrueAltitudeM = 1200.0;
    msg.precisionTimeStampUs = 1696400000000000ULL;

    MpegTsMuxerConfig config;
    config.metadataPid = 0x01E0U;
    MpegTsKlvMuxer muxer(config);

    const auto tsBuffer = muxer.muxMessageToBuffer(msg);
    ASSERT_FALSE(tsBuffer.empty());
    ASSERT_EQ(tsBuffer.size() % MpegTsKlvMuxer::kTsPacketSize, 0U);

    // Extract using MpegTsKlvExtractor
    MpegTsKlvExtractor extractor;
    std::size_t messageCount = 0U;
    UasDatalinkMessage extractedMsg;

    extractor.setMessageCallback([&](const UasDatalinkMessage& decoded) {
        messageCount++;
        extractedMsg = decoded;
    });

    const std::size_t dispatched = extractor.processStream(tsBuffer.data(), tsBuffer.size());
    EXPECT_EQ(dispatched, 1U);
    EXPECT_EQ(messageCount, 1U);
    EXPECT_TRUE(extractor.metadataPid().has_value());
    EXPECT_EQ(*extractor.metadataPid(), 0x01E0U);

    EXPECT_EQ(extractedMsg.missionId.value_or(""), "MUXER_SINGLE_TEST");
    ASSERT_TRUE(extractedMsg.platformHeadingDeg.has_value());
    EXPECT_NEAR(*extractedMsg.platformHeadingDeg, 240.5, 0.01);
    ASSERT_TRUE(extractedMsg.sensorLatitudeDeg.has_value());
    EXPECT_NEAR(*extractedMsg.sensorLatitudeDeg, 37.7749, 0.0001);
    ASSERT_TRUE(extractedMsg.sensorLongitudeDeg.has_value());
    EXPECT_NEAR(*extractedMsg.sensorLongitudeDeg, -122.4194, 0.0001);
    ASSERT_TRUE(extractedMsg.precisionTimeStampUs.has_value());
    EXPECT_EQ(*extractedMsg.precisionTimeStampUs, 1696400000000000ULL);
}

TEST(MpegTsKlvMuxerTest, LargeFragmentedPesRoundTrip) {
    UasDatalinkMessage msg;
    msg.missionId = "EXTENDED_FRAGMENTED_PAYLOAD_MISSION_ID_LONG_STRING_0123456789";
    msg.platformDesignation = "TACTICAL_UAV_SURVEILLANCE_SYSTEM_MARK_IV";
    msg.imageSourceSensor = "LONG_RANGE_EO_IR_DUAL_IMAGER_WITH_LASER_RANGE_FINDER";
    msg.imageCoordinateSystem = "Geodetic WGS-84 Universal Transverse Mercator (UTM) Zone 32 North";
    msg.platformHeadingDeg = 315.0;
    msg.platformPitchDeg = -4.5;
    msg.platformRollDeg = 12.0;
    msg.sensorLatitudeDeg = 48.8566;
    msg.sensorLongitudeDeg = 2.3522;
    msg.sensorTrueAltitudeM = 3500.0;
    msg.sensorRelAzimuthDeg = 180.0;
    msg.sensorRelElevationDeg = -25.0;
    msg.slantRangeM = 4500.0;
    msg.targetWidthM = 25.0;
    msg.precisionTimeStampUs = 1700000000123456ULL;

    MpegTsMuxerConfig config;
    config.metadataPid = 0x0300U;
    config.pmtPid = 0x0100U;
    MpegTsKlvMuxer muxer(config);

    const auto tsBuffer = muxer.muxMessageToBuffer(msg);
    ASSERT_GT(tsBuffer.size(), 3U * MpegTsKlvMuxer::kTsPacketSize);

    MpegTsKlvExtractor extractor;
    std::size_t messageCount = 0U;
    UasDatalinkMessage extractedMsg;

    extractor.setMessageCallback([&](const UasDatalinkMessage& decoded) {
        messageCount++;
        extractedMsg = decoded;
    });

    const std::size_t dispatched = extractor.processStream(tsBuffer.data(), tsBuffer.size());
    EXPECT_EQ(dispatched, 1U);
    EXPECT_EQ(messageCount, 1U);
    EXPECT_EQ(extractedMsg.missionId.value_or(""), "EXTENDED_FRAGMENTED_PAYLOAD_MISSION_ID_LONG_STRING_0123456789");
    EXPECT_EQ(extractedMsg.platformDesignation.value_or(""), "TACTICAL_UAV_SURVEILLANCE_SYSTEM_MARK_IV");
    ASSERT_TRUE(extractedMsg.sensorTrueAltitudeM.has_value());
    EXPECT_NEAR(*extractedMsg.sensorTrueAltitudeM, 3500.0, 1.0);
}

TEST(MpegTsKlvMuxerTest, PtsAndPcrGeneration) {
    UasDatalinkMessage msg;
    msg.missionId = "TIMING_TEST";
    msg.platformHeadingDeg = 90.0;
    constexpr std::uint64_t kTimestamp = 10000000ULL; // 10 seconds in us
    msg.precisionTimeStampUs = kTimestamp;

    MpegTsMuxerConfig config;
    config.emitPts = true;
    config.pcrIntervalMs = 0U; // Force PCR on every packet
    config.metadataPid = 0x01E0U;
    MpegTsKlvMuxer muxer(config);

    std::vector<std::vector<std::uint8_t>> tsPackets;
    muxer.setPacketCallback([&tsPackets](const std::uint8_t* p, std::size_t sz) {
        tsPackets.emplace_back(p, p + sz);
    });

    const std::size_t emitted = muxer.muxMessage(msg);
    EXPECT_GT(emitted, 0U);

    // Expect at least PAT, PMT, and 1 or more metadata packets
    ASSERT_GE(tsPackets.size(), 3U);

    // Find the first metadata TS packet (PUSI = 1, PID = 0x01E0)
    bool foundMeta = false;
    for (const auto& pkt : tsPackets) {
        const std::uint16_t pid = static_cast<std::uint16_t>(((pkt[1] & 0x1FU) << 8U) | pkt[2]);
        const bool pusi = (pkt[1] & 0x40U) != 0U;

        if (pid == 0x01E0U && pusi) {
            foundMeta = true;
            // Adaptation field present (bits 5..4 of byte 3 should be 11)
            EXPECT_EQ((pkt[3] >> 4U) & 0x03U, 0x03U);

            // Check adaptation field PCR flag
            const std::uint8_t afLen = pkt[4];
            EXPECT_GE(afLen, 7U);
            const std::uint8_t afFlags = pkt[5];
            EXPECT_NE(afFlags & 0x10U, 0x00U); // PCR flag set

            // Parse PCR base from bytes 6..9
            const std::uint64_t pcrBase = (static_cast<std::uint64_t>(pkt[6]) << 25U) |
                                          (static_cast<std::uint64_t>(pkt[7]) << 17U) |
                                          (static_cast<std::uint64_t>(pkt[8]) << 9U) |
                                          (static_cast<std::uint64_t>(pkt[9]) << 1U) |
                                          (static_cast<std::uint64_t>(pkt[10] >> 7U));
            const std::uint64_t expectedPts = ((kTimestamp * 90ULL) / 1000ULL) & 0x1FFFFFFFFULL;
            EXPECT_EQ(pcrBase, expectedPts);

            // Check PES header starts after adaptation field
            const std::size_t pesOffset = 5U + static_cast<std::size_t>(afLen);
            EXPECT_EQ(pkt[pesOffset], 0x00U);
            EXPECT_EQ(pkt[pesOffset + 1], 0x00U);
            EXPECT_EQ(pkt[pesOffset + 2], 0x01U);
            EXPECT_EQ(pkt[pesOffset + 3], static_cast<std::uint8_t>(config.streamId)); // 0xBD
            // PTS flag in PES header (byte 7 of PES)
            EXPECT_EQ(pkt[pesOffset + 7] & 0x80U, 0x80U);
            break;
        }
    }
    EXPECT_TRUE(foundMeta);
}

TEST(MpegTsKlvMuxerTest, ContinuityCounterCycling) {
    MpegTsMuxerConfig config;
    config.metadataPid = 0x01E0U;
    MpegTsKlvMuxer muxer(config);

    UasDatalinkMessage msg;
    msg.missionId = "CC_TEST";

    std::vector<std::uint8_t> metaCcs;
    muxer.setPacketCallback([&metaCcs](const std::uint8_t* p, std::size_t) {
        const std::uint16_t pid = static_cast<std::uint16_t>(((p[1] & 0x1FU) << 8U) | p[2]);
        if (pid == 0x01E0U) {
            metaCcs.push_back(p[3] & 0x0FU);
        }
    });

    // Mux 35 single-packet messages
    for (int i = 0; i < 35; ++i) {
        const std::size_t emitted = muxer.muxMessage(msg);
        EXPECT_GT(emitted, 0U);
    }

    ASSERT_EQ(metaCcs.size(), 35U);
    for (std::size_t i = 1; i < metaCcs.size(); ++i) {
        const std::uint8_t expectedCc = static_cast<std::uint8_t>((metaCcs[i - 1] + 1U) & 0x0FU);
        EXPECT_EQ(metaCcs[i], expectedCc);
    }
}

TEST(MpegTsKlvMuxerTest, StreamTypeAndIdVariations) {
    MpegTsMuxerConfig config;
    config.streamType = KlvStreamType::PesPrivateData; // 0x06
    config.streamId = KlvStreamId::PrivateStream1;     // 0xBD
    config.metadataPid = 0x0150U;

    MpegTsKlvMuxer muxer(config);

    UasDatalinkMessage msg;
    msg.missionId = "STREAM_TYPE_06";
    msg.platformHeadingDeg = 180.0;

    const auto tsBuffer = muxer.muxMessageToBuffer(msg);
    ASSERT_FALSE(tsBuffer.empty());

    MpegTsKlvExtractor extractor;
    std::string receivedMission;
    extractor.setMessageCallback([&](const UasDatalinkMessage& m) {
        if (m.missionId) receivedMission = *m.missionId;
    });

    const std::size_t dispatched = extractor.processStream(tsBuffer.data(), tsBuffer.size());
    EXPECT_EQ(dispatched, 1U);
    EXPECT_EQ(receivedMission, "STREAM_TYPE_06");
}
