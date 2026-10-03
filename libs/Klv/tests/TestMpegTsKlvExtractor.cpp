#include "KlvEncoder.h"
#include "MpegTsKlvExtractor.h"
#include <gtest/gtest.h>
#include <vector>

using namespace Klv;

namespace {

std::vector<std::uint8_t> createTsPacket(std::uint16_t pid,
                                         bool pusi,
                                         const std::vector<std::uint8_t>& payload,
                                         std::uint8_t cc = 0U) {
    std::vector<std::uint8_t> ts(MpegTsKlvExtractor::kTsPacketSize, 0xFFU);
    ts[0] = MpegTsKlvExtractor::kTsSyncByte; // 0x47

    // Transport Error (0), PUSI, Transport Priority (0), PID upper 5 bits
    ts[1] = (pusi ? 0x40U : 0x00U) | static_cast<std::uint8_t>((pid >> 8U) & 0x1FU);
    ts[2] = static_cast<std::uint8_t>(pid & 0xFFU);

    // Scrambling (00), Adaptation Control (01 = payload only), Continuity Counter
    ts[3] = 0x10U | (cc & 0x0FU);

    // Copy payload
    const std::size_t copySize = std::min(payload.size(), MpegTsKlvExtractor::kTsPacketSize - 4U);
    std::copy_n(payload.begin(), copySize, ts.begin() + 4);
    return ts;
}

std::vector<std::uint8_t> createPesPacket(std::uint8_t streamId,
                                          const std::vector<std::uint8_t>& klvData) {
    std::vector<std::uint8_t> pes;
    pes.push_back(0x00U);
    pes.push_back(0x00U);
    pes.push_back(0x01U);
    pes.push_back(streamId);

    const std::size_t pesPacketLen = 3U + klvData.size();
    pes.push_back(static_cast<std::uint8_t>((pesPacketLen >> 8U) & 0xFFU));
    pes.push_back(static_cast<std::uint8_t>(pesPacketLen & 0xFFU));

    // Optional PES header extension: flags (0x84, 0x00) and header data length (0x00)
    pes.push_back(0x84U);
    pes.push_back(0x00U);
    pes.push_back(0x00U);

    pes.insert(pes.end(), klvData.begin(), klvData.end());
    return pes;
}

} // namespace


TEST(MpegTsKlvExtractorTest, DirectPidPacketExtraction) {
    UasDatalinkMessage msg;
    msg.missionId = "MPEG_TS_MISSION";
    msg.platformHeadingDeg = 315.0;
    const auto klvPacket = KlvEncoder::encode(msg);

    constexpr std::uint16_t kMetaPid = 0x01E1U;
    const auto tsPacket = createTsPacket(kMetaPid, true, klvPacket);

    MpegTsKlvExtractor extractor;
    extractor.setMetadataPid(kMetaPid);

    int receivedCount = 0;
    std::string receivedMission;

    extractor.setMessageCallback([&](const UasDatalinkMessage& m) {
        receivedCount++;
        if (m.missionId) {
            receivedMission = *m.missionId;
        }
    });

    const std::size_t dispatched = extractor.processStream(tsPacket.data(), tsPacket.size());
    EXPECT_EQ(dispatched, 1U);
    EXPECT_EQ(receivedCount, 1);
    EXPECT_EQ(receivedMission, "MPEG_TS_MISSION");
}

TEST(MpegTsKlvExtractorTest, AutoDiscoversMetadataPidFromUniversalLabel) {
    UasDatalinkMessage msg;
    msg.platformHeadingDeg = 45.0;
    const auto klvPacket = KlvEncoder::encode(msg);

    constexpr std::uint16_t kUnknownMetaPid = 0x0450U;
    const auto tsPacket = createTsPacket(kUnknownMetaPid, true, klvPacket);

    MpegTsKlvExtractor extractor;
    EXPECT_FALSE(extractor.metadataPid().has_value());

    int count = 0;
    extractor.setMessageCallback([&](const UasDatalinkMessage&) {
        count++;
    });

    const std::size_t dispatched = extractor.processStream(tsPacket.data(), tsPacket.size());
    EXPECT_EQ(dispatched, 1U);
    EXPECT_TRUE(extractor.metadataPid().has_value());
    EXPECT_EQ(*extractor.metadataPid(), kUnknownMetaPid);
    EXPECT_EQ(count, 1);
}

TEST(MpegTsKlvExtractorTest, SinglePacketPesImmediateDispatch) {
    UasDatalinkMessage msg;
    msg.missionId = "SINGLE_PES";
    msg.platformHeadingDeg = 120.0;
    const auto klv = KlvEncoder::encode(msg);
    const auto pes = createPesPacket(0xBDU, klv);

    constexpr std::uint16_t kPid = 0x01A0U;
    const auto ts = createTsPacket(kPid, true, pes, 0U);

    MpegTsKlvExtractor extractor;
    extractor.setMetadataPid(kPid);

    std::size_t msgCount = 0U;
    extractor.setMessageCallback([&](const UasDatalinkMessage& m) {
        msgCount++;
        EXPECT_EQ(m.missionId.value_or(""), "SINGLE_PES");
    });

    // A single TS packet containing a complete PES must dispatch immediately
    const std::size_t dispatched = extractor.processTsPacket(ts.data());
    EXPECT_EQ(dispatched, 1U);
    EXPECT_EQ(msgCount, 1U);
}

TEST(MpegTsKlvExtractorTest, FragmentedPesMultiPacketReassembly) {
    UasDatalinkMessage msg;
    msg.missionId = "LARGE_FRAGMENTED_PAYLOAD_MISSION_ID_TESTING_123456789";
    msg.platformTailNumber = "DRONE_FRAGMENT_TEST_ALPHA_BRAVO";
    msg.platformHeadingDeg = 270.0;
    msg.platformPitchDeg = 5.0;
    msg.platformRollDeg = -2.0;
    msg.sensorLatitudeDeg = 34.05;
    msg.sensorLongitudeDeg = -118.25;
    msg.sensorTrueAltitudeM = 1500.0;
    msg.imageCoordinateSystem = "Geodetic WGS84 Universal Transverse Mercator (UTM) Zone 11 North";
    msg.imageSourceSensor = "EO_LONG_RANGE_OPTICAL_CAMERA_HIGH_DEF";
    msg.sensorRelAzimuthDeg = 180.0;
    msg.sensorRelElevationDeg = -15.0;
    const auto klv = KlvEncoder::encode(msg);
    const auto pes = createPesPacket(0xBDU, klv);


    ASSERT_GT(pes.size(), 184U);

    constexpr std::uint16_t kPid = 0x0200U;

    // Split PES across 2 TS packets
    const std::vector<std::uint8_t> payload1(pes.begin(), pes.begin() + 184);
    const std::vector<std::uint8_t> payload2(pes.begin() + 184, pes.end());

    const auto ts1 = createTsPacket(kPid, true, payload1, 0U);
    const auto ts2 = createTsPacket(kPid, false, payload2, 1U);

    MpegTsKlvExtractor extractor;
    extractor.setMetadataPid(kPid);

    std::size_t msgCount = 0U;
    extractor.setMessageCallback([&](const UasDatalinkMessage& m) {
        msgCount++;
        EXPECT_EQ(m.missionId.value_or(""), "LARGE_FRAGMENTED_PAYLOAD_MISSION_ID_TESTING_123456789");
    });

    // Packet 1: incomplete PES -> 0 dispatched
    EXPECT_EQ(extractor.processTsPacket(ts1.data()), 0U);
    EXPECT_EQ(msgCount, 0U);

    // Packet 2: completes PES -> 1 dispatched
    EXPECT_EQ(extractor.processTsPacket(ts2.data()), 1U);
    EXPECT_EQ(msgCount, 1U);
}

TEST(MpegTsKlvExtractorTest, ContinuityCounterDropHandling) {
    UasDatalinkMessage msg1;
    msg1.missionId = "FIRST_FRAGMENTED";
    const auto pes1 = createPesPacket(0xBDU, KlvEncoder::encode(msg1));

    UasDatalinkMessage msg2;
    msg2.missionId = "RECOVERED_STREAM";
    const auto pes2 = createPesPacket(0xBDU, KlvEncoder::encode(msg2));

    constexpr std::uint16_t kPid = 0x0300U;
    // Packet 1: Part 1 of msg1 (CC = 0) - incomplete fragment
    const auto splitLen = static_cast<std::ptrdiff_t>(pes1.size() / 2U);
    const std::vector<std::uint8_t> part1(pes1.begin(), pes1.begin() + splitLen);
    const auto ts1 = createTsPacket(kPid, true, part1, 0U);


    // Packet 2: CC jump/gap! Expected CC = 1, but receives CC = 5 (dropped packets)
    const std::vector<std::uint8_t> badPart(20, 0xAAU);
    const auto ts2 = createTsPacket(kPid, false, badPart, 5U);

    // Packet 3: Fresh new packet with PUSI=1 (CC = 6)
    const auto ts3 = createTsPacket(kPid, true, pes2, 6U);

    MpegTsKlvExtractor extractor;
    extractor.setMetadataPid(kPid);

    std::vector<std::string> received;
    extractor.setMessageCallback([&](const UasDatalinkMessage& m) {
        if (m.missionId) received.push_back(*m.missionId);
    });

    EXPECT_EQ(extractor.processTsPacket(ts1.data()), 0U);
    EXPECT_EQ(extractor.processTsPacket(ts2.data()), 0U); // Dropped due to CC discontinuity
    EXPECT_EQ(extractor.processTsPacket(ts3.data()), 1U); // Clean recovery

    ASSERT_EQ(received.size(), 1U);
    EXPECT_EQ(received.front(), "RECOVERED_STREAM");
}

TEST(MpegTsKlvExtractorTest, FlushDispatchesBufferedPes) {
    UasDatalinkMessage msg;
    msg.missionId = "FLUSHED_MISSION";
    const auto klv = KlvEncoder::encode(msg);

    // PES header with length = 0 (unbounded PES stream)
    std::vector<std::uint8_t> pes = { 0x00U, 0x00U, 0x01U, 0xBDU, 0x00U, 0x00U, 0x84U, 0x00U, 0x00U };
    pes.insert(pes.end(), klv.begin(), klv.end());

    constexpr std::uint16_t kPid = 0x0150U;
    const auto ts = createTsPacket(kPid, true, pes, 0U);

    MpegTsKlvExtractor extractor;
    extractor.setMetadataPid(kPid);

    int count = 0;
    extractor.setMessageCallback([&](const UasDatalinkMessage&) { count++; });

    EXPECT_EQ(extractor.processTsPacket(ts.data()), 0U);
    // Since PES length was 0, it is held in the reassembly buffer
    EXPECT_EQ(count, 0);

    // Flush should dispatch the remaining payload
    const std::size_t flushed = extractor.flush();
    EXPECT_EQ(flushed, 1U);
    EXPECT_EQ(count, 1);
}

TEST(MpegTsKlvExtractorTest, ExtractsFromSampleFlightVideo) {

    const std::vector<std::string> candidatePaths = {
        "sample-videos/mpegts-klv-day-flight.ts",
        "../../../../sample-videos/mpegts-klv-day-flight.ts",
        "../../../sample-videos/mpegts-klv-day-flight.ts"
    };

    std::string videoPath;
    for (const auto& p : candidatePaths) {
        FILE* fp = std::fopen(p.c_str(), "rb");
        if (fp != nullptr) {
            std::fclose(fp);
            videoPath = p;
            break;
        }
    }

    if (videoPath.empty()) {
        GTEST_SKIP() << "Sample flight video not found in search paths.";
    }

    FILE* fp = std::fopen(videoPath.c_str(), "rb");
    ASSERT_NE(fp, nullptr);

    constexpr std::size_t kChunkSize = 2U * 1024U * 1024U;
    std::vector<std::uint8_t> buffer(kChunkSize);
    const std::size_t bytesRead = std::fread(buffer.data(), 1U, kChunkSize, fp);
    std::fclose(fp);

    ASSERT_GT(bytesRead, 0U);

    MpegTsKlvExtractor extractor;
    std::size_t messageCount = 0U;
    std::vector<UasDatalinkMessage> receivedMessages;

    extractor.setMessageCallback([&](const UasDatalinkMessage& m) {
        messageCount++;
        receivedMessages.push_back(m);
    });

    const std::size_t dispatched = extractor.processStream(buffer.data(), bytesRead);
    EXPECT_GT(dispatched, 0U);
    EXPECT_GT(messageCount, 0U);
    EXPECT_TRUE(extractor.metadataPid().has_value());
    EXPECT_EQ(*extractor.metadataPid(), 0x0101U);

    if (!receivedMessages.empty()) {
        const auto& firstMsg = receivedMessages.front();
        EXPECT_TRUE(firstMsg.precisionTimeStampUs.has_value());
        EXPECT_TRUE(firstMsg.imageSourceSensor.has_value());
        EXPECT_EQ(*firstMsg.imageSourceSensor, "EON");
        EXPECT_TRUE(firstMsg.platformHeadingDeg.has_value());
    }
}

TEST(MpegTsKlvExtractorTest, ExtractsFromNightFlightVideo) {
    const std::vector<std::string> candidatePaths = {
        "sample-videos/mpegts-klv-night-flight-IR.ts",
        "../../../../sample-videos/mpegts-klv-night-flight-IR.ts",
        "../../../sample-videos/mpegts-klv-night-flight-IR.ts"
    };

    std::string videoPath;
    for (const auto& p : candidatePaths) {
        FILE* fp = std::fopen(p.c_str(), "rb");
        if (fp != nullptr) {
            std::fclose(fp);
            videoPath = p;
            break;
        }
    }

    if (videoPath.empty()) {
        GTEST_SKIP() << "Sample night flight video not found in search paths.";
    }

    FILE* fp = std::fopen(videoPath.c_str(), "rb");
    ASSERT_NE(fp, nullptr);

    constexpr std::size_t kChunkSize = 2U * 1024U * 1024U;
    std::vector<std::uint8_t> buffer(kChunkSize);
    const std::size_t bytesRead = std::fread(buffer.data(), 1U, kChunkSize, fp);
    std::fclose(fp);

    ASSERT_GT(bytesRead, 0U);

    MpegTsKlvExtractor extractor;
    std::size_t messageCount = 0U;
    std::vector<UasDatalinkMessage> receivedMessages;

    extractor.setMessageCallback([&](const UasDatalinkMessage& m) {
        messageCount++;
        receivedMessages.push_back(m);
    });

    const std::size_t dispatched = extractor.processStream(buffer.data(), bytesRead);
    EXPECT_GT(dispatched, 0U);
    EXPECT_GT(messageCount, 0U);
    EXPECT_TRUE(extractor.metadataPid().has_value());
    EXPECT_EQ(*extractor.metadataPid(), 0x0101U);

    if (!receivedMessages.empty()) {
        const auto& firstMsg = receivedMessages.front();
        EXPECT_TRUE(firstMsg.precisionTimeStampUs.has_value());
        EXPECT_TRUE(firstMsg.imageSourceSensor.has_value());
        EXPECT_EQ(*firstMsg.imageSourceSensor, "IR");
        EXPECT_TRUE(firstMsg.platformHeadingDeg.has_value());
    }
}


