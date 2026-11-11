/// @file TestAutoFocusEngine.cpp
/// @brief GoogleTest suite for closed-loop AutofocusEngine algorithms and peak convergence.

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "AutoFocusEngine.h"
#include "AutoFocusTypes.h"
#include "FocusEvaluator.h"
#include "MockLensDriver.h"

using namespace LensControl;

namespace {

/// @brief Generates an 8-bit synthetic image whose sharpness follows a Lorentzian profile centered at peakPos.
std::vector<std::uint8_t> generateSyntheticFrame(
    std::size_t width, std::size_t height, MotorPosition curPos, std::uint16_t peakPos)
{
    std::vector<std::uint8_t> frame(width * height, 128U);

    const auto z = static_cast<float>(curPos.value());
    const auto zPeak = static_cast<float>(peakPos);
    const float delta = (z - zPeak) / 60.0F;
    const float sharpness = 1.0F / (1.0F + (delta * delta));

    // Modulate 4-pixel vertical stripe pattern with sharpness contrast
    for (std::size_t y = 0U; y < height; ++y) {
        for (std::size_t x = 0U; x < width; ++x) {
            const float pattern = ((x / 4U) % 2U == 0U) ? 1.0F : -1.0F;
            const float contrast = sharpness * 100.0F;
            const float val = 128.0F + (pattern * contrast);
            const auto byteVal = static_cast<std::uint8_t>(std::clamp(val, 0.0F, 255.0F));
            frame[(y * width) + x] = byteVal;
        }
    }

    return frame;
}

} // namespace

class AutoFocusEngineTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        LensRanges ranges { };
        ranges.focusMin = 0U;
        ranges.focusMax = 10000U;
        ranges.zoomMin = 0U;
        ranges.zoomMax = 10000U;
        m_driver.setRanges(ranges);
    }

    MockLensDriver m_driver { };
    FocusEvaluator m_evaluator { FocusMetricType::SobelGradientSquared };
};

TEST_F(AutoFocusEngineTest, ScanThreePhaseConvergence)
{
    AutoFocusEngine engine { m_driver, m_evaluator };

    constexpr std::uint16_t kTruePeak = 2500U;
    m_driver.setFocusPosition(MotorPosition { 1000U }, MotorSpeed { 127U });

    AutoFocusConfig config { };
    config.searchMin = 1000U;
    config.searchMax = 4000U;
    config.coarseStep = 150U;
    config.midStep = 50U;
    config.fineStep = 15U;
    config.cdownMax = 3U;
    config.frameOffsetMult8 = 0U;
    config.backlashSteps = 0U;

    engine.trigger(AutoFocusMethod::ScanThreePhase, config);
    EXPECT_TRUE(engine.isRunning());

    constexpr std::size_t kWidth = 64U;
    constexpr std::size_t kHeight = 64U;

    std::uint64_t fakeTimeNs = 1000000000ULL;
    std::uint32_t frameId = 1U;

    for (std::size_t iter = 0U; iter < 150U && engine.isRunning(); ++iter) {
        const MotorPosition curPos = m_driver.currentFocus();
        const auto frame = generateSyntheticFrame(kWidth, kHeight, curPos, kTruePeak);

        engine.processFrame(frame.data(), kWidth, kHeight, kWidth, fakeTimeNs, frameId);
        engine.step(curPos, fakeTimeNs);

        fakeTimeNs += 33333333ULL;
        ++frameId;
    }

    EXPECT_FALSE(engine.isRunning());
    EXPECT_EQ(engine.state(), AutoFocusState::PeakConfirmed);
    EXPECT_NEAR(static_cast<float>(engine.convergedPosition().value()), static_cast<float>(kTruePeak), 30.0F);
}

TEST_F(AutoFocusEngineTest, GoldenSectionConvergence)
{
    AutoFocusEngine engine { m_driver, m_evaluator };

    constexpr std::uint16_t kTruePeak = 3200U;
    m_driver.setFocusPosition(MotorPosition { 2000U }, MotorSpeed { 127U });

    AutoFocusConfig config { };
    config.searchMin = 2000U;
    config.searchMax = 5000U;
    config.gssTolerance = 25U;
    config.frameOffsetMult8 = 0U;
    config.backlashSteps = 0U;

    engine.trigger(AutoFocusMethod::GoldenSection, config);
    EXPECT_TRUE(engine.isRunning());

    constexpr std::size_t kWidth = 64U;
    constexpr std::size_t kHeight = 64U;

    std::uint64_t fakeTimeNs = 1000000000ULL;
    std::uint32_t frameId = 1U;

    for (std::size_t iter = 0U; iter < 60U && engine.isRunning(); ++iter) {
        const MotorPosition curPos = m_driver.currentFocus();
        const auto frame = generateSyntheticFrame(kWidth, kHeight, curPos, kTruePeak);

        engine.processFrame(frame.data(), kWidth, kHeight, kWidth, fakeTimeNs, frameId);
        engine.step(curPos, fakeTimeNs);

        fakeTimeNs += 33333333ULL;
        ++frameId;
    }

    EXPECT_FALSE(engine.isRunning());
    EXPECT_EQ(engine.state(), AutoFocusState::PeakConfirmed);
    EXPECT_NEAR(static_cast<float>(engine.convergedPosition().value()), static_cast<float>(kTruePeak), 35.0F);
}

TEST_F(AutoFocusEngineTest, SeekAdaptiveConvergence)
{
    AutoFocusEngine engine { m_driver, m_evaluator };

    constexpr std::uint16_t kTruePeak = 2100U;
    m_driver.setFocusPosition(MotorPosition { 1900U }, MotorSpeed { 127U });

    AutoFocusConfig config { };
    config.searchMin = 1500U;
    config.searchMax = 3000U;
    config.midStep = 25U;
    config.cdownMax = 3U;
    config.frameOffsetMult8 = 0U;
    config.backlashSteps = 0U;

    engine.trigger(AutoFocusMethod::SeekAdaptive, config);
    EXPECT_TRUE(engine.isRunning());

    constexpr std::size_t kWidth = 64U;
    constexpr std::size_t kHeight = 64U;

    std::uint64_t fakeTimeNs = 1000000000ULL;
    std::uint32_t frameId = 1U;

    for (std::size_t iter = 0U; iter < 60U && engine.isRunning(); ++iter) {
        const MotorPosition curPos = m_driver.currentFocus();
        const auto frame = generateSyntheticFrame(kWidth, kHeight, curPos, kTruePeak);

        engine.processFrame(frame.data(), kWidth, kHeight, kWidth, fakeTimeNs, frameId);
        engine.step(curPos, fakeTimeNs);

        fakeTimeNs += 33333333ULL;
        ++frameId;
    }

    EXPECT_FALSE(engine.isRunning());
    EXPECT_EQ(engine.state(), AutoFocusState::PeakConfirmed);
    EXPECT_NEAR(static_cast<float>(engine.convergedPosition().value()), static_cast<float>(kTruePeak), 30.0F);
}

TEST_F(AutoFocusEngineTest, AdaptiveMomentumConvergence)
{
    AutoFocusEngine engine { m_driver, m_evaluator };

    constexpr std::uint16_t kTruePeak = 4000U;
    m_driver.setFocusPosition(MotorPosition { 3500U }, MotorSpeed { 127U });

    AutoFocusConfig config { };
    config.searchMin = 3000U;
    config.searchMax = 5000U;
    config.coarseStep = 100U;
    config.midStep = 40U;
    config.cdownMax = 3U;
    config.frameOffsetMult8 = 0U;
    config.backlashSteps = 0U;

    engine.trigger(AutoFocusMethod::AdaptiveMomentum, config);
    EXPECT_TRUE(engine.isRunning());

    constexpr std::size_t kWidth = 64U;
    constexpr std::size_t kHeight = 64U;

    std::uint64_t fakeTimeNs = 1000000000ULL;
    std::uint32_t frameId = 1U;

    for (std::size_t iter = 0U; iter < 80U && engine.isRunning(); ++iter) {
        const MotorPosition curPos = m_driver.currentFocus();
        const auto frame = generateSyntheticFrame(kWidth, kHeight, curPos, kTruePeak);

        engine.processFrame(frame.data(), kWidth, kHeight, kWidth, fakeTimeNs, frameId);
        engine.step(curPos, fakeTimeNs);

        fakeTimeNs += 33333333ULL;
        ++frameId;
    }

    EXPECT_FALSE(engine.isRunning());
    EXPECT_EQ(engine.state(), AutoFocusState::PeakConfirmed);
    EXPECT_NEAR(static_cast<float>(engine.convergedPosition().value()), static_cast<float>(kTruePeak), 45.0F);
}

TEST_F(AutoFocusEngineTest, DynamicTrackerRoiMapping)
{
    AutoFocusEngine engine { m_driver, m_evaluator };

    // Center target (x=0.5, y=0.5, w=0.2, h=0.2) with 20% margin
    engine.setTrackerRoi(0.5F, 0.5F, 0.2F, 0.2F, 0.2F);
    NormalizedRoi roi = m_evaluator.roi();

    EXPECT_TRUE(roi.isValid());
    EXPECT_NEAR(static_cast<float>(roi.colFrac), 97.0F, 5.0F);
    EXPECT_NEAR(static_cast<float>(roi.rowFrac), 97.0F, 5.0F);

    engine.clearTrackerRoi();
    roi = m_evaluator.roi();
    EXPECT_EQ(roi.colFrac, 64U); // Reverted to default center 25%
}

TEST_F(AutoFocusEngineTest, AbortOperation)
{
    AutoFocusEngine engine { m_driver, m_evaluator };
    engine.trigger(AutoFocusMethod::ScanThreePhase);
    EXPECT_TRUE(engine.isRunning());

    engine.abort();
    EXPECT_FALSE(engine.isRunning());
    EXPECT_EQ(engine.state(), AutoFocusState::Idle);
    EXPECT_TRUE(m_driver.isMotionStopped());
}

TEST_F(AutoFocusEngineTest, TimeoutFailure)
{
    AutoFocusEngine engine { m_driver, m_evaluator };
    AutoFocusConfig config { };
    config.timeoutMs = 100U;

    engine.trigger(AutoFocusMethod::ScanThreePhase, config);
    EXPECT_TRUE(engine.isRunning());

    std::uint64_t tStart = 1000000000ULL;
    engine.step(MotorPosition { 1000U }, tStart);

    // Step after 150 ms (150,000,000 ns)
    engine.step(MotorPosition { 1000U }, tStart + 150000000ULL);

    EXPECT_FALSE(engine.isRunning());
    EXPECT_EQ(engine.state(), AutoFocusState::Failed);
}

TEST_F(AutoFocusEngineTest, ContinuousAutofocusRefocusTrigger)
{
    AutoFocusEngine engine { m_driver, m_evaluator };
    engine.setCafEnabled(true);
    EXPECT_TRUE(engine.isCafEnabled());

    constexpr std::uint16_t kTruePeak = 2000U;
    m_driver.setFocusPosition(MotorPosition { 1800U }, MotorSpeed { 127U });

    AutoFocusConfig config { };
    config.searchMin = 1500U;
    config.searchMax = 2500U;
    config.coarseStep = 100U;
    config.midStep = 40U;
    config.fineStep = 10U;
    config.frameOffsetMult8 = 0U;
    config.backlashSteps = 0U;
    config.cafDefocusFrac = 0.3F;

    engine.trigger(AutoFocusMethod::ScanThreePhase, config);

    constexpr std::size_t kWidth = 64U;
    constexpr std::size_t kHeight = 64U;
    std::uint64_t fakeTime = 1000000000ULL;
    std::uint32_t frameId = 1U;

    for (std::size_t i = 0U; i < 40U && engine.isRunning(); ++i) {
        const MotorPosition curPos = m_driver.currentFocus();
        const auto frame = generateSyntheticFrame(kWidth, kHeight, curPos, kTruePeak);
        engine.processFrame(frame.data(), kWidth, kHeight, kWidth, fakeTime, frameId);
        engine.step(curPos, fakeTime);
        fakeTime += 33333333ULL;
        ++frameId;
    }

    EXPECT_FALSE(engine.isRunning());

    // In converged state, feed severely blurred frame (position far away)
    const auto blurredFrame = generateSyntheticFrame(kWidth, kHeight, MotorPosition { 500U }, kTruePeak);
    engine.processFrame(blurredFrame.data(), kWidth, kHeight, kWidth, fakeTime, frameId);

    // Defocus triggers automatic refocus
    EXPECT_TRUE(engine.isRunning());
}
