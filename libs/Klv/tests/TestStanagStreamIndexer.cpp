#include "StanagStreamIndexer.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <vector>

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
        0x00U,                               // Pointer
        0x00U, 0xB0U, 0x0DU,                 // Table 0, syntax 1, length 13
        0x00U, 0x01U, 0xC1U, 0x00U, 0x00U,   // Transport stream ID, version, section
        0x00U, 0x01U, 0xE1U, 0x00U,         // Program 1 -> PMT PID 0x0100
        0x00U, 0x00U, 0x00U, 0x00U          // Dummy CRC
    };
    auto patPkt = makeTsPacket(0x0000U, true, patPayload);

    // 2. Construct PMT packet (PID 0x0100): Video PID 0x0101 (H.264 = 0x1B), KLV PID 0x0102 (Metadata = 0x15)
    std::vector<std::uint8_t> pmtPayload = {
        0x00U,                               // Pointer
        0x02U, 0xB0U, 0x17U,                 // Table 2, length 23
        0x00U, 0x01U, 0xC1U, 0x00U, 0x00U,   // Program 1
        0xE1U, 0x00U,                        // PCR PID 0x0100
        0xF0U, 0x00U,                        // Program info length = 0
        0x1BU, 0xE1U, 0x01U, 0xF0U, 0x00U,   // Stream 0x1B (H.264) -> PID 0x0101
        0x15U, 0xE1U, 0x02U, 0xF0U, 0x00U,   // Stream 0x15 (Metadata) -> PID 0x0102
        0x00U, 0x00U, 0x00U, 0x00U          // Dummy CRC
    };
    auto pmtPkt = makeTsPacket(0x0100U, true, pmtPayload);

    // 3. Construct Video PES packet on PID 0x0101 (PTS = 90000, IDR slice NAL 0x0000000165)
    std::uint8_t vidPtsBytes[5] {};
    encodePts(90000U, vidPtsBytes);

    std::vector<std::uint8_t> vidPayload = {
        0x00U, 0x00U, 0x01U, 0xE0U,          // PES start, stream 0xE0
        0x00U, 0x20U,                        // Length
        0x80U, 0x80U, 0x05U,                 // Flags: PTS present, header len = 5
        vidPtsBytes[0], vidPtsBytes[1], vidPtsBytes[2], vidPtsBytes[3], vidPtsBytes[4],
        0x00U, 0x00U, 0x00U, 0x01U, 0x65U,   // H.264 IDR Slice (0x65 -> NAL type 5)
        0x88U, 0x88U, 0x88U
    };
    auto vidPkt = makeTsPacket(0x0101U, true, vidPayload);

    // 4. Construct KLV PES packet on PID 0x0102 (PTS = 90000, MISB ST 0601 UL + Tag 2)
    std::uint8_t klvPtsBytes[5] {};
    encodePts(90000U, klvPtsBytes);

    std::vector<std::uint8_t> klvPayload = {
        0x00U, 0x00U, 0x01U, 0xBDU,          // PES start, stream 0xBD
        0x00U, 0x30U,                        // Length
        0x80U, 0x80U, 0x05U,                 // Flags: PTS present, header len = 5
        klvPtsBytes[0], klvPtsBytes[1], klvPtsBytes[2], klvPtsBytes[3], klvPtsBytes[4],
        // MISB ST 0601 Universal Label:
        0x06U, 0x0EU, 0x2BU, 0x34U, 0x02U, 0x0BU, 0x01U, 0x01U,
        0x0EU, 0x01U, 0x03U, 0x01U, 0x01U, 0x00U, 0x00U, 0x00U,
        0x0AU,                               // Length = 10 bytes
        0x02U, 0x08U,                        // Tag 2, len 8
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
    indexer.timeIndex().addKlvEntry(k);
    indexer.timeIndex().finalize();

    const std::string testFile = "test_index_temp.sidx";
    EXPECT_TRUE(indexer.saveIndex(testFile));
    EXPECT_TRUE(std::filesystem::exists(testFile));

    StanagStreamIndexer loader {};
    EXPECT_TRUE(loader.loadIndex(testFile));

    std::filesystem::remove(testFile);
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
