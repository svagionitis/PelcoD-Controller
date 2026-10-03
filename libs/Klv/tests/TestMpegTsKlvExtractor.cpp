#include "KlvEncoder.h"
#include "MpegTsKlvExtractor.h"
#include <gtest/gtest.h>
#include <vector>

using namespace Klv;

namespace {

std::vector<std::uint8_t> createTsPacket(std::uint16_t pid, bool pusi, const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> ts(MpegTsKlvExtractor::kTsPacketSize, 0xFFU);
    ts[0] = MpegTsKlvExtractor::kTsSyncByte; // 0x47

    // Transport Error (0), PUSI, Transport Priority (0), PID upper 5 bits
    ts[1] = (pusi ? 0x40U : 0x00U) | static_cast<std::uint8_t>((pid >> 8U) & 0x1FU);
    ts[2] = static_cast<std::uint8_t>(pid & 0xFFU);

    // Scrambling (00), Adaptation Control (01 = payload only), Continuity Counter (0)
    ts[3] = 0x10U;

    // Copy payload
    const std::size_t copySize = std::min(payload.size(), MpegTsKlvExtractor::kTsPacketSize - 4U);
    std::copy_n(payload.begin(), copySize, ts.begin() + 4);
    return ts;
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

    extractor.processStream(tsPacket.data(), tsPacket.size());
    EXPECT_TRUE(extractor.metadataPid().has_value());
    EXPECT_EQ(*extractor.metadataPid(), kUnknownMetaPid);
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


