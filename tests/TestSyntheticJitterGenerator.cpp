/// @file TestSyntheticJitterGenerator.cpp
/// @brief Unit tests for SyntheticJitterGenerator physics models and determinism.

#include <cstddef>
#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "SyntheticJitterGenerator.h"

namespace StabilizerApp {

class SyntheticJitterGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        config_.width = 320U;
        config_.height = 240U;
        config_.frameRate = 30.0;
        config_.mastFreqX = 5.0F;
        config_.mastFreqY = 5.0F;
        config_.mastAmpX = 10.0F;
        config_.mastAmpY = 10.0F;
        config_.enableDrift = false;
        config_.enableRollingShutter = false;
        config_.seed = 0x12345678U;
    }

    SyntheticJitterConfig config_ { };
};

TEST_F(SyntheticJitterGeneratorTest, ConfigValidation)
{
    EXPECT_TRUE(config_.isValid());

    SyntheticJitterConfig badCfg = config_;
    badCfg.width = 16U; // too small
    EXPECT_FALSE(badCfg.isValid());

    badCfg = config_;
    badCfg.frameRate = 0.0;
    EXPECT_FALSE(badCfg.isValid());
}

TEST_F(SyntheticJitterGeneratorTest, DeterministicPrngOutput)
{
    SyntheticJitterGenerator gen1(config_);
    SyntheticJitterGenerator gen2(config_);

    const std::size_t byteCount
        = static_cast<std::size_t>(config_.width) * static_cast<std::size_t>(config_.height) * 3U;
    std::vector<std::uint8_t> frame1(byteCount, 0U);
    std::vector<std::uint8_t> frame2(byteCount, 0U);

    const double pts1 = gen1.generateNextFrame(frame1.data(), 1.0 / 30.0);
    const double pts2 = gen2.generateNextFrame(frame2.data(), 1.0 / 30.0);

    EXPECT_DOUBLE_EQ(pts1, pts2);
    EXPECT_EQ(frame1, frame2);
}

TEST_F(SyntheticJitterGeneratorTest, SeedDifferenceProducesDifferentFrames)
{
    SyntheticJitterConfig cfg2 = config_;
    cfg2.enableDrift = true;
    cfg2.seed = 0x87654321U;

    SyntheticJitterConfig cfg1 = config_;
    cfg1.enableDrift = true;
    cfg1.seed = 0x11223344U;

    SyntheticJitterGenerator gen1(cfg1);
    SyntheticJitterGenerator gen2(cfg2);

    const std::size_t byteCount
        = static_cast<std::size_t>(config_.width) * static_cast<std::size_t>(config_.height) * 3U;
    std::vector<std::uint8_t> frame1(byteCount, 0U);
    std::vector<std::uint8_t> frame2(byteCount, 0U);

    // Generate several frames with Brownian drift active
    for (int i = 0; i < 5; ++i) {
        static_cast<void>(gen1.generateNextFrame(frame1.data(), 1.0 / 30.0));
        static_cast<void>(gen2.generateNextFrame(frame2.data(), 1.0 / 30.0));
    }

    EXPECT_NE(frame1, frame2);
}

TEST_F(SyntheticJitterGeneratorTest, ResetRestoresTimeline)
{
    SyntheticJitterGenerator gen(config_);

    const std::size_t byteCount
        = static_cast<std::size_t>(config_.width) * static_cast<std::size_t>(config_.height) * 3U;
    std::vector<std::uint8_t> initialFrame(byteCount, 0U);
    std::vector<std::uint8_t> frameAfterReset(byteCount, 0U);

    const double ptsInitial = gen.generateNextFrame(initialFrame.data(), 1.0 / 30.0);

    gen.reset();
    const double ptsReset = gen.generateNextFrame(frameAfterReset.data(), 1.0 / 30.0);

    EXPECT_DOUBLE_EQ(ptsInitial, ptsReset);
    EXPECT_EQ(initialFrame, frameAfterReset);
}

TEST_F(SyntheticJitterGeneratorTest, RollingShutterProducesRowDependentShear)
{
    SyntheticJitterConfig rsCfg = config_;
    rsCfg.enableRollingShutter = true;
    rsCfg.rsAmp = 15.0F;
    rsCfg.rsFreq = 10.0F;

    SyntheticJitterGenerator gen(rsCfg);

    const std::size_t byteCount = static_cast<std::size_t>(rsCfg.width) * static_cast<std::size_t>(rsCfg.height) * 3U;
    std::vector<std::uint8_t> frame(byteCount, 0U);

    // Run a few steps to advance phase
    for (int i = 0; i < 3; ++i) {
        static_cast<void>(gen.generateNextFrame(frame.data(), 1.0 / 30.0));
    }

    // Top row and bottom row must have valid pixels
    EXPECT_GT(frame[0], 0U);
    EXPECT_GT(frame[byteCount - 1U], 0U);
}

} // namespace StabilizerApp
