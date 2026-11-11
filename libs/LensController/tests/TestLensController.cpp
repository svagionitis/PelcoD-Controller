/// @file TestLensController.cpp
/// @brief GoogleTest suite for LensController multi-channel coordinator and boundary guards.

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "AutoFocusTypes.h"
#include "ILensDriver.h"
#include "LensController.h"
#include "LensTypes.h"
#include "MockLensDriver.h"

using namespace LensControl;

namespace {

std::vector<std::uint8_t> makeSyntheticFrame(
    std::size_t width, std::size_t height, MotorPosition curPos, std::uint16_t peakPos)
{
    std::vector<std::uint8_t> frame(width * height, 128U);
    const auto z = static_cast<float>(curPos.value());
    const auto zPeak = static_cast<float>(peakPos);
    const float delta = (z - zPeak) / 50.0F;
    const float sharpness = 1.0F / (1.0F + (delta * delta));

    for (std::size_t y = 0U; y < height; ++y) {
        for (std::size_t x = 0U; x < width; ++x) {
            const float pattern = ((x / 4U) % 2U == 0U) ? 1.0F : -1.0F;
            const float contrast = sharpness * 100.0F;
            const float val = 128.0F + (pattern * contrast);
            frame[(y * width) + x] = static_cast<std::uint8_t>(std::clamp(val, 0.0F, 255.0F));
        }
    }
    return frame;
}

} // namespace

class LensControllerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_controller = std::make_unique<LensController>();

        LensRanges ranges { };
        ranges.focusMin = 100U;
        ranges.focusMax = 5000U;
        ranges.zoomMin = 200U;
        ranges.zoomMax = 4000U;

        auto mock0 = std::make_unique<MockLensDriver>(ranges);
        m_mockDriver0 = mock0.get();
        m_controller->setDriver(CameraIndex { 0U }, std::move(mock0));

        auto mock1 = std::make_unique<MockLensDriver>(ranges);
        m_mockDriver1 = mock1.get();
        m_controller->setDriver(CameraIndex { 1U }, std::move(mock1));
    }

    std::unique_ptr<LensController> m_controller;
    MockLensDriver* m_mockDriver0 { nullptr };
    MockLensDriver* m_mockDriver1 { nullptr };
};

TEST_F(LensControllerTest, ChannelBoundsValidation)
{
    const CameraIndex validCam { 0U };
    const CameraIndex outOfBoundsCam { 5U };

    m_controller->moveFocusStep(validCam, 50);
    EXPECT_EQ(m_mockDriver0->currentFocus().value(), 150U);

    // Out of bounds channel should safely no-op without crashes
    m_controller->moveFocusStep(outOfBoundsCam, 50);
    m_controller->setZoomTarget(outOfBoundsCam, MotorPosition { 1000U });
    m_controller->stopMotion(outOfBoundsCam);

    const LensStatus st = m_controller->getLensStatus(outOfBoundsCam);
    EXPECT_EQ(st.focusPos.value(), 0U);
}

TEST_F(LensControllerTest, SoftLimitClamping)
{
    const CameraIndex cam0 { 0U };

    // Range is [100..5000] for focus and [200..4000] for zoom
    m_controller->setFocusTarget(cam0, MotorPosition { 50U }); // Below min
    EXPECT_EQ(m_mockDriver0->currentFocus().value(), 100U);

    m_controller->setFocusTarget(cam0, MotorPosition { 60000U }); // Above max
    EXPECT_EQ(m_mockDriver0->currentFocus().value(), 5000U);

    m_controller->setZoomTarget(cam0, MotorPosition { 10U }); // Below min
    EXPECT_EQ(m_mockDriver0->currentZoom().value(), 200U);

    m_controller->setZoomTarget(cam0, MotorPosition { 10000U }); // Above max
    EXPECT_EQ(m_mockDriver0->currentZoom().value(), 4000U);
}

TEST_F(LensControllerTest, LimitSwitchInhibition)
{
    const CameraIndex cam0 { 0U };

    // Move focus to focusMin (100) so NearLimit flag is automatically active
    m_controller->setFocusTarget(cam0, MotorPosition { 100U });
    EXPECT_EQ(m_mockDriver0->currentFocus().value(), 100U);
    EXPECT_TRUE(hasFlag(m_mockDriver0->limitFlags(), LensLimitFlags::NearLimit));

    // Negative steps (Near) should be inhibited by NearLimit
    m_controller->moveFocusStep(cam0, -50);
    EXPECT_EQ(m_mockDriver0->currentFocus().value(), 100U);

    // Positive steps (Far) should be allowed
    m_controller->moveFocusStep(cam0, +50);
    EXPECT_EQ(m_mockDriver0->currentFocus().value(), 150U);

    // Move focus to focusMax (5000) so FarLimit flag is automatically active
    m_controller->setFocusTarget(cam0, MotorPosition { 5000U });
    EXPECT_EQ(m_mockDriver0->currentFocus().value(), 5000U);
    EXPECT_TRUE(hasFlag(m_mockDriver0->limitFlags(), LensLimitFlags::FarLimit));

    // Positive steps (Far) should be inhibited by FarLimit
    m_controller->moveFocusStep(cam0, +50);
    EXPECT_EQ(m_mockDriver0->currentFocus().value(), 5000U);

    // Negative steps (Near) should be allowed
    m_controller->moveFocusStep(cam0, -50);
    EXPECT_EQ(m_mockDriver0->currentFocus().value(), 4950U);
}

TEST_F(LensControllerTest, DiscreteJogStepsAndSpeed)
{
    const CameraIndex cam0 { 0U };
    m_controller->setFocusSpeed(cam0, MotorSpeed { 180U });
    m_controller->setZoomSpeed(cam0, MotorSpeed { 210U });

    m_controller->moveFocusStep(cam0, 25);
    EXPECT_EQ(m_mockDriver0->currentFocus().value(), 125U);
    EXPECT_EQ(m_mockDriver0->lastFocusSpeed().value(), 180U);

    m_controller->moveZoomStep(cam0, 30);
    EXPECT_EQ(m_mockDriver0->currentZoom().value(), 230U);
    EXPECT_EQ(m_mockDriver0->lastZoomSpeed().value(), 210U);
}

TEST_F(LensControllerTest, EmergencyStopHalt)
{
    const CameraIndex cam0 { 0U };
    m_controller->moveFocusStep(cam0, 100);
    EXPECT_FALSE(m_mockDriver0->isMotionStopped());

    m_controller->stopMotion(cam0);
    EXPECT_TRUE(m_mockDriver0->isMotionStopped());
}

TEST_F(LensControllerTest, AutoFocusTriggerAndCycle)
{
    const CameraIndex cam0 { 0U };
    const std::uint16_t kPeakFocus = 1500U;

    m_controller->setFocusTarget(cam0, MotorPosition { 500U });
    m_controller->triggerAutoFocus(cam0, AutoFocusMethod::ScanThreePhase);

    EXPECT_TRUE(m_controller->isAutoFocusRunning(cam0));
    const LensStatus stInit = m_controller->getLensStatus(cam0);
    EXPECT_EQ(stInit.mode, LensMode::AutoFocusOnce);

    // Simulate video frame pipeline and control tick loop
    constexpr std::size_t kWidth = 64U;
    constexpr std::size_t kHeight = 64U;

    for (std::uint32_t f = 0U; f < 60U && m_controller->isAutoFocusRunning(cam0); ++f) {
        const MotorPosition curPos = m_mockDriver0->currentFocus();
        const auto frame = makeSyntheticFrame(kWidth, kHeight, curPos, kPeakFocus);
        const auto nowNs = static_cast<std::uint64_t>(f * 33333333ULL);

        m_controller->processFrame(cam0, frame.data(), kWidth, kHeight, nowNs, f);
        m_controller->update(cam0);
    }

    // State machine should advance and mode revert to ManualJog upon completion
    const LensStatus stFinal = m_controller->getLensStatus(cam0);
    EXPECT_FALSE(m_controller->isAutoFocusRunning(cam0));
    EXPECT_EQ(stFinal.mode, LensMode::ManualJog);
}

TEST_F(LensControllerTest, ZoomTrackFocusSlew)
{
    const CameraIndex cam0 { 0U };

    // Calibration table: Zoom 500 -> Focus 1000, Zoom 2000 -> Focus 3000
    const std::string curveData = "2:\n"
                                  "z:500, 2000\n"
                                  "f:1000, 3000\n";

    // Write temporary calibration file
    const std::string tmpPath = "test_zoom_curve.txt";
    FILE* fp = fopen(tmpPath.c_str(), "w");
    ASSERT_NE(fp, nullptr);
    fputs(curveData.c_str(), fp);
    fclose(fp);

    EXPECT_TRUE(m_controller->loadTrackCurves(cam0, tmpPath));
    m_controller->setZoomTrackFocus(cam0, true);

    // Initial state: Zoom at 200, update baseline
    m_controller->update(cam0);

    // Change zoom to 500
    m_mockDriver0->setZoomPosition(MotorPosition { 500U }, MotorSpeed { 128U });
    m_controller->update(cam0);

    // Parfocal slew should update focus to ~1000
    EXPECT_EQ(m_mockDriver0->currentFocus().value(), 1000U);

    // Change zoom to 2000
    m_mockDriver0->setZoomPosition(MotorPosition { 2000U }, MotorSpeed { 128U });
    m_controller->update(cam0);

    // Parfocal slew should update focus to ~3000
    EXPECT_EQ(m_mockDriver0->currentFocus().value(), 3000U);

    remove(tmpPath.c_str());
}

TEST_F(LensControllerTest, DynamicDllLoaderErrorHandling)
{
    const CameraIndex cam0 { 0U };
    // Non-existent library should return FileNotFound
    const LensLoadResult res = m_controller->loadLensDll(cam0, "non_existent_lens_driver.dll", 1U, 9600U);
    EXPECT_EQ(res, LensLoadResult::FileNotFound);
}

TEST_F(LensControllerTest, TrackerRoiConfiguration)
{
    const CameraIndex cam0 { 0U };
    m_controller->setTrackerRoi(cam0, 0.5F, 0.5F, 0.2F, 0.2F, 0.1F);
    m_controller->clearTrackerRoi(cam0);
    SUCCEED();
}
