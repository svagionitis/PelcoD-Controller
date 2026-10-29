#include "KlvEncoder.h"
#include "StanagStreamIndexer.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace Klv {
namespace {

    // Helper to encode a 33-bit PTS into 5 standard MPEG-2 PES bytes
    void encodePts(std::uint64_t pts, std::uint8_t* out)
    {
        out[0] = static_cast<std::uint8_t>(0x21U | (((pts >> 30U) & 0x07U) << 1U));
        out[1] = static_cast<std::uint8_t>((pts >> 22U) & 0xFFU);
        out[2] = static_cast<std::uint8_t>(0x01U | (((pts >> 15U) & 0x7FU) << 1U));
        out[3] = static_cast<std::uint8_t>((pts >> 7U) & 0xFFU);
        out[4] = static_cast<std::uint8_t>(0x01U | ((pts & 0x7FU) << 1U));
    }

    // Helper to construct a 188-byte TS packet
    std::vector<std::uint8_t> makeTsPacket(std::uint16_t pid, bool pusi, const std::vector<std::uint8_t>& payload)
    {
        std::vector<std::uint8_t> pkt(188U, 0xFFU);
        pkt[0] = 0x47U;
        pkt[1] = static_cast<std::uint8_t>((pusi ? 0x40U : 0x00U) | ((pid >> 8U) & 0x1FU));
        pkt[2] = static_cast<std::uint8_t>(pid & 0xFFU);
        pkt[3] = 0x10U; // No adaptation, payload only, counter 0

        const std::size_t copyLen = std::min(payload.size(), static_cast<std::size_t>(184U));
        std::copy_n(payload.begin(), copyLen, pkt.begin() + 4U);
        return pkt;
    }

    TEST(TestStanagStreamIndexer, SyntheticPatPmtAndStreamIndexing)
    {
        StanagStreamIndexer indexer {};

        // 1. Construct PAT packet (PID 0): Program 1 -> PMT PID 0x0100
        std::vector<std::uint8_t> patPayload = {
            0x00U, // Pointer
            0x00U, 0xB0U, 0x0DU, // Table 0, syntax 1, length 13
            0x00U, 0x01U, 0xC1U, 0x00U, 0x00U, // Transport stream ID, version, section
            0x00U, 0x01U, 0xE1U, 0x00U, // Program 1 -> PMT PID 0x0100
            0x00U, 0x00U, 0x00U, 0x00U // Dummy CRC
        };
        auto patPkt = makeTsPacket(0x0000U, true, patPayload);

        // 2. Construct PMT packet (PID 0x0100): Video PID 0x0101 (H.264 = 0x1B), KLV PID 0x0102 (Metadata = 0x15)
        std::vector<std::uint8_t> pmtPayload = {
            0x00U, // Pointer
            0x02U, 0xB0U, 0x17U, // Table 2, length 23
            0x00U, 0x01U, 0xC1U, 0x00U, 0x00U, // Program 1
            0xE1U, 0x00U, // PCR PID 0x0100
            0xF0U, 0x00U, // Program info length = 0
            0x1BU, 0xE1U, 0x01U, 0xF0U, 0x00U, // Stream 0x1B (H.264) -> PID 0x0101
            0x15U, 0xE1U, 0x02U, 0xF0U, 0x00U, // Stream 0x15 (Metadata) -> PID 0x0102
            0x00U, 0x00U, 0x00U, 0x00U // Dummy CRC
        };
        auto pmtPkt = makeTsPacket(0x0100U, true, pmtPayload);

        // 3. Construct Video PES packet on PID 0x0101 (PTS = 90000, IDR slice NAL 0x0000000165)
        std::uint8_t vidPtsBytes[5] {};
        encodePts(90000U, vidPtsBytes);

        std::vector<std::uint8_t> vidPayload = { 0x00U, 0x00U, 0x01U, 0xE0U, // PES start, stream 0xE0
            0x00U, 0x20U, // Length
            0x80U, 0x80U, 0x05U, // Flags: PTS present, header len = 5
            vidPtsBytes[0], vidPtsBytes[1], vidPtsBytes[2], vidPtsBytes[3], vidPtsBytes[4], 0x00U, 0x00U, 0x00U, 0x01U,
            0x65U, // H.264 IDR Slice (0x65 -> NAL type 5)
            0x88U, 0x88U, 0x88U };
        auto vidPkt = makeTsPacket(0x0101U, true, vidPayload);

        // 4. Construct KLV PES packet on PID 0x0102 (PTS = 90000, MISB ST 0601 UL + Tag 2)
        std::uint8_t klvPtsBytes[5] {};
        encodePts(90000U, klvPtsBytes);

        std::vector<std::uint8_t> klvPayload = {
            0x00U, 0x00U, 0x01U, 0xBDU, // PES start, stream 0xBD
            0x00U, 0x30U, // Length
            0x80U, 0x80U, 0x05U, // Flags: PTS present, header len = 5
            klvPtsBytes[0], klvPtsBytes[1], klvPtsBytes[2], klvPtsBytes[3], klvPtsBytes[4],
            // MISB ST 0601 Universal Label:
            0x06U, 0x0EU, 0x2BU, 0x34U, 0x02U, 0x0BU, 0x01U, 0x01U, 0x0EU, 0x01U, 0x03U, 0x01U, 0x01U, 0x00U, 0x00U,
            0x00U,
            0x0AU, // Length = 10 bytes
            0x02U, 0x08U, // Tag 2, len 8
            0x00U, 0x05U, 0xF5U, 0xE1U, 0x00U, 0x00U, 0x00U, 0x00U // Timestamp = 1677721600000000
        };
        auto klvPkt = makeTsPacket(0x0102U, true, klvPayload);

        // Combine all packets
        std::vector<std::uint8_t> streamData {};
        streamData.insert(streamData.end(), patPkt.begin(), patPkt.end());
        streamData.insert(streamData.end(), pmtPkt.begin(), pmtPkt.end());
        streamData.insert(streamData.end(), vidPkt.begin(), vidPkt.end());
        streamData.insert(streamData.end(), klvPkt.begin(), klvPkt.end());

        const std::size_t count = indexer.indexChunk(streamData.data(), streamData.size(), 0U);
        EXPECT_EQ(count, 4U);

        EXPECT_EQ(indexer.videoPid(), 0x0101U);
        EXPECT_EQ(indexer.metadataPid(), 0x0102U);

        indexer.timeIndex().finalize();
        EXPECT_EQ(indexer.timeIndex().videoFrameCount(), 1U);
        EXPECT_EQ(indexer.timeIndex().keyframeCount(), 1U);
        EXPECT_EQ(indexer.timeIndex().klvPacketCount(), 1U);

        auto kf = indexer.timeIndex().findPrecedingKeyframe(90000U);
        ASSERT_TRUE(kf.has_value());
        EXPECT_TRUE(kf->isKeyframe);
        EXPECT_EQ(kf->ptsTicks, 90000U);

        auto klv = indexer.timeIndex().findTelemetry(90000U);
        ASSERT_TRUE(klv.has_value());
        EXPECT_EQ(klv->ptsTicks, 90000U);
        EXPECT_GT(klv->utcTimestampUs, 0U);
    }

    TEST(TestStanagStreamIndexer, SaveAndLoadSidxFile)
    {
        StanagStreamIndexer indexer {};

        VideoIndexEntry v {};
        v.ptsTicks = 90000U;
        v.isKeyframe = true;
        indexer.timeIndex().addVideoEntry(v);

        KlvIndexEntry k {};
        k.ptsTicks = 90000U;
        k.utcTimestampUs = 1600000000000000ULL;
        UasDatalinkMessage msg {};
        msg.missionId = "TEST-MISSION-SIDX";
        msg.platformHeadingDeg = 123.45;
        k.message = msg;
        indexer.timeIndex().addKlvEntry(k);
        indexer.timeIndex().finalize();

        const std::string testFile = "test_index_temp.sidx";
        EXPECT_TRUE(indexer.saveIndex(testFile));
        EXPECT_TRUE(std::filesystem::exists(testFile));

        StanagStreamIndexer loader {};
        EXPECT_TRUE(loader.loadIndex(testFile));
        EXPECT_EQ(loader.timeIndex().videoFrameCount(), 1U);
        EXPECT_EQ(loader.timeIndex().keyframeCount(), 1U);
        EXPECT_EQ(loader.timeIndex().klvPacketCount(), 1U);
        EXPECT_EQ(loader.timeIndex().basePts(), 90000U);

        const auto optKlv = loader.timeIndex().findTelemetry(90000U);
        ASSERT_TRUE(optKlv.has_value());
        ASSERT_TRUE(optKlv->message.has_value());
        EXPECT_EQ(optKlv->message->missionId.value_or(""), "TEST-MISSION-SIDX");
        if (optKlv->message->platformHeadingDeg) {
            EXPECT_NEAR(*optKlv->message->platformHeadingDeg, 123.45, 0.05);
        }

        std::filesystem::remove(testFile);
    }

    TEST(TestStanagStreamIndexer, MultiPacketKlvPesIndexing)
    {
        StanagStreamIndexer indexer {};

        // Construct a large ST 0601 message (> 200 bytes)
        UasDatalinkMessage msg {};
        msg.precisionTimeStampUs = 1600000000000000ULL;
        msg.missionId = "MULTI-PACKET-KLV-INGEST-MISSION-VERY-LONG-STRING-FOR-OVERFLOW";
        msg.platformDesignation = "MQ-9-REAPER-EXTENDED-PAYLOAD-TEST";
        msg.platformHeadingDeg = 270.5;
        msg.platformPitchDeg = 5.2;
        msg.platformRollDeg = -2.1;
        msg.sensorLatitudeDeg = 34.123456;
        msg.sensorLongitudeDeg = -117.654321;
        msg.sensorTrueAltitudeM = 5400.0;
        msg.slantRangeM = 12500.0;
        msg.targetWidthM = 15.0;

        const auto klvBytes = KlvEncoder::encode(msg);
        ASSERT_GT(klvBytes.size(), 180U); // Ensure it exceeds a single TS packet payload

        // Build PES packet wrapping klvBytes
        const auto pesLen = static_cast<std::uint16_t>(klvBytes.size() + 8U);
        std::vector<std::uint8_t> pesPacket = {
            0x00U, 0x00U, 0x01U, 0xBDU, static_cast<std::uint8_t>((pesLen >> 8U) & 0xFFU),
            static_cast<std::uint8_t>(pesLen & 0xFFU), 0x80U, 0x80U, 0x05U, // Flags: PTS present
            0x21U, 0x00U, 0x01U, 0x00U, 0x01U // Dummy 33-bit PTS
        };
        pesPacket.insert(pesPacket.end(), klvBytes.begin(), klvBytes.end());

        // Fragment PES across TS packets on PID 0x0102
        std::vector<std::uint8_t> streamData {};
        std::size_t offset = 0U;
        bool isFirst = true;

        while (offset < pesPacket.size()) {
            const std::size_t chunk = std::min(pesPacket.size() - offset, static_cast<std::size_t>(184U));
            std::vector<std::uint8_t> chunkData(pesPacket.begin() + static_cast<std::ptrdiff_t>(offset),
                pesPacket.begin() + static_cast<std::ptrdiff_t>(offset + chunk));
            auto ts = makeTsPacket(0x0102U, isFirst, chunkData);
            streamData.insert(streamData.end(), ts.begin(), ts.end());
            offset += chunk;
            isFirst = false;
        }

        indexer.setMetadataPid(0x0102U);
        static_cast<void>(indexer.indexChunk(streamData.data(), streamData.size(), 0U));
        indexer.flush();

        EXPECT_EQ(indexer.timeIndex().klvPacketCount(), 1U);
        const auto optEntry = indexer.timeIndex().findTelemetry(indexer.timeIndex().basePts());
        ASSERT_TRUE(optEntry.has_value());
        ASSERT_TRUE(optEntry->message.has_value());
        EXPECT_EQ(optEntry->message->missionId.value_or(""), msg.missionId);
        EXPECT_EQ(optEntry->message->platformDesignation.value_or(""), msg.platformDesignation);
        if (optEntry->message->platformHeadingDeg) {
            EXPECT_NEAR(*optEntry->message->platformHeadingDeg, 270.5, 0.05);
        }
    }

    TEST(TestStanagStreamIndexer, MultiPacketVideoKeyframeDetection)
    {
        StanagStreamIndexer indexer {};
        indexer.setVideoPid(0x0101U);

        // First packet: PES header + AUD (Access Unit Delimiter) NAL (non-IDR)
        std::vector<std::uint8_t> pesPart1 = { 0x00U, 0x00U, 0x01U, 0xE0U, 0x00U, 0x00U, // Length = 0 (unbounded video)
            0x80U, 0x80U, 0x05U, 0x21U, 0x00U, 0x01U, 0x00U, 0x01U, // PTS
            // AUD NAL: 0x00 0x00 0x00 0x01 0x09
            0x00U, 0x00U, 0x00U, 0x01U, 0x09U, 0xF0U };
        // Pad part1 to 184 bytes
        pesPart1.resize(184U, 0xFFU);
        auto ts1 = makeTsPacket(0x0101U, true, pesPart1);

        // Second packet: IDR NAL: 0x00 0x00 0x00 0x01 0x65
        std::vector<std::uint8_t> pesPart2 = { 0x00U, 0x00U, 0x00U, 0x01U, 0x65U, 0x88U, 0x88U, 0x88U };
        auto ts2 = makeTsPacket(0x0101U, false, pesPart2);

        std::vector<std::uint8_t> streamData {};
        streamData.insert(streamData.end(), ts1.begin(), ts1.end());
        streamData.insert(streamData.end(), ts2.begin(), ts2.end());

        static_cast<void>(indexer.indexChunk(streamData.data(), streamData.size(), 0U));
        indexer.flush();

        EXPECT_EQ(indexer.timeIndex().videoFrameCount(), 1U);
        EXPECT_EQ(indexer.timeIndex().keyframeCount(), 1U);
        const auto* vf = indexer.timeIndex().videoEntryAt(0U);
        ASSERT_NE(vf, nullptr);
        EXPECT_TRUE(vf->isKeyframe);
        EXPECT_GT(vf->packetSizeBytes, 184U);
    }

    TEST(TestStanagStreamIndexer, RealSampleVideoIndexingIfAvailable)
    {
        std::string samplePath = "sample-videos/mpegts-klv-day-flight.ts";
        if (!std::filesystem::exists(samplePath)) {
            samplePath = "../../../sample-videos/mpegts-klv-day-flight.ts";
        }
        if (!std::filesystem::exists(samplePath)) {
            samplePath = "../sample-videos/mpegts-klv-day-flight.ts";
        }
        if (!std::filesystem::exists(samplePath)) {
            GTEST_SKIP() << "Sample video not found at " << samplePath;
        }

        StanagStreamIndexer indexer {};
        bool progressCalled = false;
        bool ok = indexer.indexFile(samplePath, [&progressCalled](int pct) {
            if (pct >= 50) {
                progressCalled = true;
            }
        });

        EXPECT_TRUE(ok);
        EXPECT_TRUE(progressCalled);
        EXPECT_GT(indexer.timeIndex().videoFrameCount(), 0U);
        EXPECT_GT(indexer.timeIndex().keyframeCount(), 0U);
        EXPECT_GT(indexer.timeIndex().klvPacketCount(), 0U);
        EXPECT_GT(indexer.timeIndex().durationSeconds(), 10.0);

        const auto k0 = indexer.timeIndex().findTelemetry(indexer.timeIndex().basePts());
        ASSERT_TRUE(k0.has_value());
        EXPECT_TRUE(k0->message.has_value());
        if (k0->message && k0->message->platformHeadingDeg) {
            EXPECT_NEAR(*k0->message->platformHeadingDeg, 86.1067, 0.01);
        }
    }

} // namespace
} // namespace Klv
