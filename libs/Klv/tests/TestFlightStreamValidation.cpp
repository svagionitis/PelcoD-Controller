#include "KlvCrc.h"
#include "KlvGeodesy.h"
#include "MpegTsKlvExtractor.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace Klv;

namespace {

std::string findSampleVideo(const std::string& filename) {
    const std::vector<std::string> prefixes = {
        "",
        "sample-videos/",
        "../sample-videos/",
        "../../sample-videos/",
        "../../../sample-videos/",
        "../../../../sample-videos/"
    };

    for (const auto& prefix : prefixes) {
        const std::string candidate = prefix + filename;
        FILE* fp = std::fopen(candidate.c_str(), "rb");
        if (fp != nullptr) {
            std::fclose(fp);
            return candidate;
        }
    }
    return {};
}

std::vector<std::uint8_t> loadFile(const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (fp == nullptr) {
        return {};
    }

    std::fseek(fp, 0, SEEK_END);
    const auto fileSize = static_cast<std::size_t>(std::ftell(fp));
    std::fseek(fp, 0, SEEK_SET);

    std::vector<std::uint8_t> buffer(fileSize);
    const std::size_t readBytes = std::fread(buffer.data(), 1U, fileSize, fp);
    std::fclose(fp);
    buffer.resize(readBytes);
    return buffer;
}

} // namespace

TEST(FlightStreamValidationTest, FullDayFlightStreamEndToEnd) {
    const std::string videoPath = findSampleVideo("mpegts-klv-day-flight.ts");
    if (videoPath.empty()) {
        GTEST_SKIP() << "sample-videos/mpegts-klv-day-flight.ts not found.";
    }

    const auto streamData = loadFile(videoPath);
    ASSERT_GT(streamData.size(), 30000000U); // ~30.8 MB

    MpegTsKlvExtractor extractor;
    std::vector<UasDatalinkMessage> messages;
    std::vector<std::vector<std::uint8_t>> rawPayloads;

    extractor.setMessageCallback([&](const UasDatalinkMessage& msg) {
        messages.push_back(msg);
    });

    extractor.setKlvPayloadCallback([&](const std::uint8_t* data, std::size_t size) {
        rawPayloads.emplace_back(data, data + size);
    });

    const auto startTime = std::chrono::steady_clock::now();
    const std::size_t dispatched = extractor.processStream(streamData.data(), streamData.size());
    const std::size_t flushed = extractor.flush();
    const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();

    // High throughput performance check (< 500 ms for 30.8 MB)
    EXPECT_LT(durationMs, 500);

    // Verify all packets in flight video were demuxed
    EXPECT_EQ(dispatched + flushed, 6U);
    EXPECT_EQ(messages.size(), 6U);
    EXPECT_EQ(rawPayloads.size(), 6U);

    // Verify discovered PID
    EXPECT_TRUE(extractor.metadataPid().has_value());
    EXPECT_EQ(*extractor.metadataPid(), 0x0101U);

    // Verify 100% BCC-16 verification on raw KLV packets
    for (const auto& raw : rawPayloads) {
        EXPECT_TRUE(KlvCrc::verifyPacket(raw.data(), raw.size()));
    }

    // Verify telemetry fields and monotonic timestamps
    std::uint64_t lastTimestamp = 0U;
    for (const auto& msg : messages) {
        ASSERT_TRUE(msg.precisionTimeStampUs.has_value());
        EXPECT_GT(*msg.precisionTimeStampUs, lastTimestamp);
        lastTimestamp = *msg.precisionTimeStampUs;

        ASSERT_TRUE(msg.imageSourceSensor.has_value());
        EXPECT_TRUE(*msg.imageSourceSensor == "EON" || *msg.imageSourceSensor == "IR");

        ASSERT_TRUE(msg.platformHeadingDeg.has_value());
        EXPECT_GE(*msg.platformHeadingDeg, 0.0);
        EXPECT_LT(*msg.platformHeadingDeg, 360.0);

        if (msg.platformPitchDeg) {
            EXPECT_GE(*msg.platformPitchDeg, -20.0);
            EXPECT_LE(*msg.platformPitchDeg, 20.0);
        }

        if (msg.platformRollDeg) {
            EXPECT_GE(*msg.platformRollDeg, -50.0);
            EXPECT_LE(*msg.platformRollDeg, 50.0);
        }

        ASSERT_TRUE(msg.sensorLatitudeDeg.has_value());
        EXPECT_GE(*msg.sensorLatitudeDeg, -90.0);
        EXPECT_LE(*msg.sensorLatitudeDeg, 90.0);

        ASSERT_TRUE(msg.sensorLongitudeDeg.has_value());
        EXPECT_GE(*msg.sensorLongitudeDeg, -180.0);
        EXPECT_LE(*msg.sensorLongitudeDeg, 180.0);

        ASSERT_TRUE(msg.sensorTrueAltitudeM.has_value());
        EXPECT_GT(*msg.sensorTrueAltitudeM, 0.0);
        EXPECT_LT(*msg.sensorTrueAltitudeM, 15000.0);
    }
}

TEST(FlightStreamValidationTest, FullNightFlightStreamEndToEnd) {
    const std::string videoPath = findSampleVideo("mpegts-klv-night-flight-IR.ts");
    if (videoPath.empty()) {
        GTEST_SKIP() << "sample-videos/mpegts-klv-night-flight-IR.ts not found.";
    }

    const auto streamData = loadFile(videoPath);
    ASSERT_GT(streamData.size(), 35000000U); // ~38.3 MB

    MpegTsKlvExtractor extractor;
    std::vector<UasDatalinkMessage> messages;
    std::vector<std::vector<std::uint8_t>> rawPayloads;

    extractor.setMessageCallback([&](const UasDatalinkMessage& msg) {
        messages.push_back(msg);
    });

    extractor.setKlvPayloadCallback([&](const std::uint8_t* data, std::size_t size) {
        rawPayloads.emplace_back(data, data + size);
    });

    const auto startTime = std::chrono::steady_clock::now();
    const std::size_t dispatched = extractor.processStream(streamData.data(), streamData.size());
    const std::size_t flushed = extractor.flush();
    const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();

    EXPECT_LT(durationMs, 500);

    // Verify all 18 packets were demuxed
    EXPECT_EQ(dispatched + flushed, 18U);
    EXPECT_EQ(messages.size(), 18U);
    EXPECT_EQ(rawPayloads.size(), 18U);

    EXPECT_TRUE(extractor.metadataPid().has_value());
    EXPECT_EQ(*extractor.metadataPid(), 0x0101U);

    // Verify 100% BCC-16 verification on raw KLV packets
    for (const auto& raw : rawPayloads) {
        EXPECT_TRUE(KlvCrc::verifyPacket(raw.data(), raw.size()));
    }

    std::uint64_t lastTimestamp = 0U;
    for (const auto& msg : messages) {
        ASSERT_TRUE(msg.precisionTimeStampUs.has_value());
        EXPECT_GT(*msg.precisionTimeStampUs, lastTimestamp);
        lastTimestamp = *msg.precisionTimeStampUs;

        ASSERT_TRUE(msg.imageSourceSensor.has_value());
        EXPECT_EQ(*msg.imageSourceSensor, "IR");

        ASSERT_TRUE(msg.platformHeadingDeg.has_value());
        EXPECT_GE(*msg.platformHeadingDeg, 0.0);
        EXPECT_LT(*msg.platformHeadingDeg, 360.0);

        ASSERT_TRUE(msg.sensorLatitudeDeg.has_value());
        EXPECT_GE(*msg.sensorLatitudeDeg, -90.0);
        EXPECT_LE(*msg.sensorLatitudeDeg, 90.0);

        ASSERT_TRUE(msg.sensorLongitudeDeg.has_value());
        EXPECT_GE(*msg.sensorLongitudeDeg, -180.0);
        EXPECT_LE(*msg.sensorLongitudeDeg, 180.0);

        ASSERT_TRUE(msg.sensorTrueAltitudeM.has_value());
        EXPECT_GT(*msg.sensorTrueAltitudeM, 0.0);
    }
}

TEST(FlightStreamValidationTest, ChunkBoundaryInvarianceStressTest) {
    const std::string videoPath = findSampleVideo("mpegts-klv-day-flight.ts");
    if (videoPath.empty()) {
        GTEST_SKIP() << "sample-videos/mpegts-klv-day-flight.ts not found.";
    }

    const auto streamData = loadFile(videoPath);
    ASSERT_GT(streamData.size(), 30000000U);

    // Test different feeding chunk sizes
    const std::vector<std::size_t> chunkSizes = {
        188U,          // Single TS packet
        512U,          // Arbitrary non-188 boundary
        4096U,         // Page boundary
        65536U,        // Socket buffer boundary
        streamData.size() // Full monolithic buffer
    };

    std::vector<std::uint64_t> referenceTimestamps;

    for (std::size_t i = 0; i < chunkSizes.size(); ++i) {
        const std::size_t chunkSize = chunkSizes[i];
        MpegTsKlvExtractor extractor;
        std::vector<std::uint64_t> timestamps;

        extractor.setMessageCallback([&](const UasDatalinkMessage& msg) {
            if (msg.precisionTimeStampUs) {
                timestamps.push_back(*msg.precisionTimeStampUs);
            }
        });

        std::size_t offset = 0U;
        std::size_t totalDispatched = 0U;
        while (offset < streamData.size()) {
            const std::size_t bytesToFeed = std::min(chunkSize, streamData.size() - offset);
            totalDispatched += extractor.processStream(streamData.data() + offset, bytesToFeed);
            offset += bytesToFeed;
        }
        totalDispatched += extractor.flush();

        EXPECT_EQ(totalDispatched, 6U);
        EXPECT_EQ(timestamps.size(), 6U);

        if (i == 0) {
            referenceTimestamps = timestamps;
        } else {
            // Assert bit-for-bit exact timestamp sequence across all chunk feeding strategies
            EXPECT_EQ(timestamps, referenceTimestamps);
        }
    }
}

TEST(FlightStreamValidationTest, MidStreamSeekAndRecovery) {
    const std::string videoPath = findSampleVideo("mpegts-klv-day-flight.ts");
    if (videoPath.empty()) {
        GTEST_SKIP() << "sample-videos/mpegts-klv-day-flight.ts not found.";
    }

    const auto streamData = loadFile(videoPath);
    ASSERT_GT(streamData.size(), 30000000U);

    MpegTsKlvExtractor extractor;
    std::size_t msgCount = 0U;
    extractor.setMessageCallback([&](const UasDatalinkMessage&) {
        msgCount++;
    });

    // Ingest first 2 MB
    constexpr std::size_t kFirstChunk = 2U * 1024U * 1024U;
    const std::size_t initialDispatched = extractor.processStream(streamData.data(), kFirstChunk);
    EXPECT_GT(initialDispatched, 0U);
    EXPECT_GT(msgCount, 0U);
    const std::size_t countBeforeSeek = msgCount;

    // Simulate seeking: skip ahead 10 MB and reset extractor
    constexpr std::size_t kSeekOffset = 12U * 1024U * 1024U;
    extractor.reset();

    // Ingest remaining bytes from seek position
    const std::size_t seekDispatched = extractor.processStream(
        streamData.data() + kSeekOffset, streamData.size() - kSeekOffset);
    const std::size_t seekFlushed = extractor.flush();
    EXPECT_GT(seekDispatched + seekFlushed, 0U);

    // Demuxer should recover and extract remaining metadata packets
    EXPECT_GT(msgCount, countBeforeSeek);
    EXPECT_TRUE(extractor.metadataPid().has_value());
    EXPECT_EQ(*extractor.metadataPid(), 0x0101U);
}

TEST(FlightStreamValidationTest, GeodesyTargetConsistency) {
    const std::string videoPath = findSampleVideo("mpegts-klv-day-flight.ts");
    if (videoPath.empty()) {
        GTEST_SKIP() << "sample-videos/mpegts-klv-day-flight.ts not found.";
    }

    const auto streamData = loadFile(videoPath);
    ASSERT_GT(streamData.size(), 30000000U);

    MpegTsKlvExtractor extractor;
    std::vector<UasDatalinkMessage> messages;
    extractor.setMessageCallback([&](const UasDatalinkMessage& msg) {
        messages.push_back(msg);
    });

    const std::size_t dispatched = extractor.processStream(streamData.data(), streamData.size());
    const std::size_t flushed = extractor.flush();
    EXPECT_EQ(dispatched + flushed, 6U);

    ASSERT_FALSE(messages.empty());

    for (const auto& msg : messages) {
        if (msg.sensorLatitudeDeg && msg.sensorLongitudeDeg && msg.sensorTrueAltitudeM &&
            msg.platformHeadingDeg) {
            // Verify WGS84 distance calculation from sensor to adjacent point
            const GeoPoint2D p1 { *msg.sensorLatitudeDeg, *msg.sensorLongitudeDeg };
            const GeoPoint2D p2 { *msg.sensorLatitudeDeg + 0.01, *msg.sensorLongitudeDeg };
            const double distanceM = KlvGeodesy::distanceMeters(p1, p2);
            EXPECT_GT(distanceM, 1000.0);
            EXPECT_LT(distanceM, 1200.0); // ~1.11 km for 0.01 deg latitude
        }
    }
}
