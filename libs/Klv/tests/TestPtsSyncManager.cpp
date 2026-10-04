#include "MpegTsKlvMuxer.h"
#include "PtsSyncManager.h"
#include <gtest/gtest.h>
#include <vector>

using namespace Klv;

TEST(TestPtsSyncManager, NominalInSync) {
    PtsSyncConfig config;
    config.maxSyncDeltaMs = 50U;
    config.jitterBufferDepthMs = 40U;
    PtsSyncManager manager(config);

    std::vector<SynchronizedAccessUnit> outputs;
    manager.setSyncCallback([&outputs](const SynchronizedAccessUnit& item) {
        outputs.push_back(item);
    });

    constexpr std::uint64_t kBaseTimeUs = 1000000ULL;
    // Push 3 frames at 30 fps (33,333 us) with matching telemetry
    for (std::size_t i = 0U; i < 3U; ++i) {
        const std::uint64_t t = kBaseTimeUs + static_cast<std::uint64_t>(i) * 33333ULL;
        UasDatalinkMessage msg;
        msg.missionId = "SYNC_FRAME_" + std::to_string(i);
        msg.precisionTimeStampUs = t;
        manager.pushTelemetryMessage(msg);

        const std::vector<std::uint8_t> frameData = { 0x00U, 0x00U, 0x00U, 0x01U, 0x41U };
        manager.pushVideoFrame(frameData.data(), frameData.size(), t);
    }

    manager.flush();

    ASSERT_EQ(outputs.size(), 3U);
    for (std::size_t i = 0U; i < 3U; ++i) {
        EXPECT_FALSE(outputs[i].wasClamped);
        EXPECT_NEAR(static_cast<double>(outputs[i].skewDeltaUs) / 1000.0, 0.0, 1.0);
        ASSERT_TRUE(outputs[i].metadataMessage.has_value());
        EXPECT_EQ(outputs[i].metadataMessage->missionId.value_or(""), "SYNC_FRAME_" + std::to_string(i));
    }

    const auto stats = manager.stats();
    EXPECT_TRUE(stats.isCompliant);
    EXPECT_EQ(stats.clampedPackets, 0U);
    EXPECT_EQ(stats.droppedPackets, 0U);
}

TEST(TestPtsSyncManager, TelemetrySkewWithinCompliantLimit) {
    PtsSyncConfig config;
    config.maxSyncDeltaMs = 50U;
    config.jitterBufferDepthMs = 40U;
    PtsSyncManager manager(config);

    std::vector<SynchronizedAccessUnit> outputs;
    manager.setSyncCallback([&outputs](const SynchronizedAccessUnit& item) {
        outputs.push_back(item);
    });

    constexpr std::uint64_t kVideoPts = 2000000ULL;
    constexpr std::uint64_t kTelemetryPts = 2025000ULL; // +25 ms skew (compliant since <= 50 ms)

    UasDatalinkMessage msg;
    msg.missionId = "COMPLIANT_SKEW";
    msg.precisionTimeStampUs = kTelemetryPts;
    manager.pushTelemetryMessage(msg);

    const std::vector<std::uint8_t> frameData = { 0x00U, 0x00U, 0x00U, 0x01U, 0x41U };
    manager.pushVideoFrame(frameData.data(), frameData.size(), kVideoPts);

    manager.flush();

    ASSERT_EQ(outputs.size(), 1U);
    EXPECT_FALSE(outputs[0].wasClamped);
    EXPECT_EQ(outputs[0].skewDeltaUs, 25000LL);

    const auto stats = manager.stats();
    EXPECT_TRUE(stats.isCompliant);
    EXPECT_EQ(stats.clampedPackets, 0U);
}

TEST(TestPtsSyncManager, StrictClampPolicy) {
    PtsSyncConfig config;
    config.syncPolicy = PtsSyncPolicy::StrictClamp;
    config.maxSyncDeltaMs = 50U;
    config.preserveSensorTimestamp = false;
    PtsSyncManager manager(config);

    std::vector<SynchronizedAccessUnit> outputs;
    manager.setSyncCallback([&outputs](const SynchronizedAccessUnit& item) {
        outputs.push_back(item);
    });

    constexpr std::uint64_t kVideoPts = 3000000ULL;
    constexpr std::uint64_t kLaggingTelemetryPts = 3080000ULL; // +80 ms skew (> 50 ms)

    UasDatalinkMessage msg;
    msg.missionId = "CLAMP_TEST";
    msg.precisionTimeStampUs = kLaggingTelemetryPts;
    manager.pushTelemetryMessage(msg);

    const std::vector<std::uint8_t> frameData = { 0x00U, 0x00U, 0x00U, 0x01U, 0x65U };
    manager.pushVideoFrame(frameData.data(), frameData.size(), kVideoPts, std::nullopt, /*isKeyframe=*/true);

    manager.flush();

    ASSERT_EQ(outputs.size(), 1U);
    EXPECT_TRUE(outputs[0].wasClamped);
    ASSERT_TRUE(outputs[0].metadataMessage.has_value());

    // Timestamp should be clamped to kVideoPts + 50 ms (3,050,000 us)
    EXPECT_EQ(outputs[0].metadataMessage->precisionTimeStampUs.value_or(0U), 3050000ULL);

    const auto stats = manager.stats();
    EXPECT_EQ(stats.clampedPackets, 1U);
}

TEST(TestPtsSyncManager, NearestFramePolicy) {
    PtsSyncConfig config;
    config.syncPolicy = PtsSyncPolicy::NearestFrame;
    config.maxSyncDeltaMs = 50U;
    config.preserveSensorTimestamp = false;
    PtsSyncManager manager(config);

    std::vector<SynchronizedAccessUnit> outputs;
    manager.setSyncCallback([&outputs](const SynchronizedAccessUnit& item) {
        outputs.push_back(item);
    });

    constexpr std::uint64_t kVideoPts = 4000000ULL;
    constexpr std::uint64_t kTelemetryPts = 4070000ULL; // +70 ms skew

    UasDatalinkMessage msg;
    msg.missionId = "NEAREST_FRAME";
    msg.precisionTimeStampUs = kTelemetryPts;
    manager.pushTelemetryMessage(msg);

    const std::vector<std::uint8_t> frameData = { 0x00U, 0x00U, 0x00U, 0x01U, 0x41U };
    manager.pushVideoFrame(frameData.data(), frameData.size(), kVideoPts);

    manager.flush();

    ASSERT_EQ(outputs.size(), 1U);
    EXPECT_TRUE(outputs[0].wasClamped);
    ASSERT_TRUE(outputs[0].metadataMessage.has_value());

    // Timestamp should be snapped directly to video PTS (4,000,000 us)
    EXPECT_EQ(outputs[0].metadataMessage->precisionTimeStampUs.value_or(0U), kVideoPts);

    const auto stats = manager.stats();
    EXPECT_EQ(stats.clampedPackets, 1U);
}

TEST(TestPtsSyncManager, DropStalePolicy) {
    PtsSyncConfig config;
    config.syncPolicy = PtsSyncPolicy::DropStale;
    config.maxSyncDeltaMs = 50U;
    PtsSyncManager manager(config);

    std::vector<SynchronizedAccessUnit> outputs;
    manager.setSyncCallback([&outputs](const SynchronizedAccessUnit& item) {
        outputs.push_back(item);
    });

    constexpr std::uint64_t kVideoPts = 5000000ULL;
    constexpr std::uint64_t kStaleTelemetryPts = 5090000ULL; // +90 ms skew

    UasDatalinkMessage msg;
    msg.missionId = "STALE_DATA";
    msg.precisionTimeStampUs = kStaleTelemetryPts;
    manager.pushTelemetryMessage(msg);

    const std::vector<std::uint8_t> frameData = { 0x00U, 0x00U, 0x00U, 0x01U, 0x41U };
    manager.pushVideoFrame(frameData.data(), frameData.size(), kVideoPts);

    manager.flush();

    ASSERT_EQ(outputs.size(), 1U);
    // Metadata should have been dropped due to stale policy
    EXPECT_FALSE(outputs[0].metadataMessage.has_value());

    const auto stats = manager.stats();
    EXPECT_EQ(stats.droppedPackets, 1U);
}

TEST(TestPtsSyncManager, MuxerBindingRoundTrip) {
    MpegTsMuxerConfig muxConfig;
    muxConfig.videoCodec = VideoCodec::H264;
    muxConfig.videoPid = 0x0101U;
    muxConfig.metadataPid = 0x01E0U;
    muxConfig.pcrOnVideo = true;
    MpegTsKlvMuxer muxer(muxConfig);

    std::vector<std::vector<std::uint8_t>> tsPackets;
    muxer.setPacketCallback([&tsPackets](const std::uint8_t* p, std::size_t sz) {
        tsPackets.emplace_back(p, p + sz);
    });

    PtsSyncConfig syncConfig;
    PtsSyncManager syncManager(syncConfig);
    syncManager.bindMuxer(&muxer);

    constexpr std::uint64_t kTimestamp = 6000000ULL;
    UasDatalinkMessage msg;
    msg.missionId = "BOUND_MUX_TEST";
    msg.precisionTimeStampUs = kTimestamp;
    syncManager.pushTelemetryMessage(msg);

    const std::vector<std::uint8_t> frameData = { 0x00U, 0x00U, 0x00U, 0x01U, 0x65U, 0x88U };
    syncManager.pushVideoFrame(frameData.data(), frameData.size(), kTimestamp, std::nullopt, /*isKeyframe=*/true);

    syncManager.flush();

    ASSERT_FALSE(tsPackets.empty());

    bool foundVideo = false;
    bool foundMeta = false;
    for (const auto& pkt : tsPackets) {
        const std::uint16_t pid = static_cast<std::uint16_t>(((pkt[1] & 0x1FU) << 8U) | pkt[2]);
        if (pid == 0x0101U) foundVideo = true;
        if (pid == 0x01E0U) foundMeta = true;
    }

    EXPECT_TRUE(foundVideo);
    EXPECT_TRUE(foundMeta);
}
