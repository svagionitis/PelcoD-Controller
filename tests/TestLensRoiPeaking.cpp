/// @file TestLensRoiPeaking.cpp
/// @brief Automated unit verification for interactive ROI manipulation, presets, and focus peaking.

#include <cstdint>

#include <QGuiApplication>
#include <QRectF>
#include <QSignalSpy>
#include <QTest>
#include <gtest/gtest.h>

#include "LensPlayerController.h"
#include "LensVideoQuickItem.h"

namespace LensApp {

class LensRoiPeakingTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (qGuiApp == nullptr) {
            static int argc = 1;
            static char appName[] = "LensRoiPeakingTest";
            static char* argv[] = { appName, nullptr };
            new QGuiApplication(argc, argv);
        }
    }
};

TEST_F(LensRoiPeakingTest, DefaultRoiAndClamping)
{
    LensVideoQuickItem item;
    QSignalSpy roiSpy(&item, &LensVideoQuickItem::roiRectChanged);

    // Initial default: 25% offset, 50% dimension
    const QRectF initial = item.roiRect();
    EXPECT_DOUBLE_EQ(initial.x(), 0.25);
    EXPECT_DOUBLE_EQ(initial.y(), 0.25);
    EXPECT_DOUBLE_EQ(initial.width(), 0.50);
    EXPECT_DOUBLE_EQ(initial.height(), 0.50);

    // Out-of-bounds coordinate clamping
    item.setRoiRect(QRectF(-0.5, -0.5, 2.0, 2.0));
    const QRectF clamped = item.roiRect();
    EXPECT_GE(clamped.x(), 0.0);
    EXPECT_GE(clamped.y(), 0.0);
    EXPECT_LE(clamped.right(), 1.0);
    EXPECT_LE(clamped.bottom(), 1.0);
    EXPECT_GE(roiSpy.count(), 1);
}

TEST_F(LensRoiPeakingTest, PresetApplication)
{
    LensVideoQuickItem item;

    // Preset 0: Full Frame
    item.applyRoiPreset(0);
    EXPECT_DOUBLE_EQ(item.roiRect().x(), 0.0);
    EXPECT_DOUBLE_EQ(item.roiRect().y(), 0.0);
    EXPECT_DOUBLE_EQ(item.roiRect().width(), 1.0);
    EXPECT_DOUBLE_EQ(item.roiRect().height(), 1.0);

    // Preset 1: Center 50%
    item.applyRoiPreset(1);
    EXPECT_DOUBLE_EQ(item.roiRect().x(), 0.25);
    EXPECT_DOUBLE_EQ(item.roiRect().y(), 0.25);
    EXPECT_DOUBLE_EQ(item.roiRect().width(), 0.50);
    EXPECT_DOUBLE_EQ(item.roiRect().height(), 0.50);

    // Preset 2: Center Spot 25%
    item.applyRoiPreset(2);
    EXPECT_DOUBLE_EQ(item.roiRect().x(), 0.375);
    EXPECT_DOUBLE_EQ(item.roiRect().y(), 0.375);
    EXPECT_DOUBLE_EQ(item.roiRect().width(), 0.25);
    EXPECT_DOUBLE_EQ(item.roiRect().height(), 0.25);

    // Preset 3: Lower Third
    item.applyRoiPreset(3);
    EXPECT_DOUBLE_EQ(item.roiRect().x(), 0.10);
    EXPECT_DOUBLE_EQ(item.roiRect().y(), 0.60);
    EXPECT_DOUBLE_EQ(item.roiRect().width(), 0.80);
    EXPECT_DOUBLE_EQ(item.roiRect().height(), 0.30);

    // Preset 4: Upper Third
    item.applyRoiPreset(4);
    EXPECT_DOUBLE_EQ(item.roiRect().x(), 0.10);
    EXPECT_DOUBLE_EQ(item.roiRect().y(), 0.10);
    EXPECT_DOUBLE_EQ(item.roiRect().width(), 0.80);
    EXPECT_DOUBLE_EQ(item.roiRect().height(), 0.30);
}

TEST_F(LensRoiPeakingTest, TrackerTargetCoupling)
{
    LensVideoQuickItem item;
    QSignalSpy lockSpy(&item, &LensVideoQuickItem::trackerLockChanged);

    EXPECT_FALSE(item.isRoiLockedToTrack());

    // When not locked, target boxes are ignored
    const QRectF initialRoi = item.roiRect();
    item.setTrackerTargetBox(0.40, 0.40, 0.20, 0.20);
    EXPECT_EQ(item.roiRect(), initialRoi);

    // Lock to tracker
    item.setIsRoiLockedToTrack(true);
    EXPECT_TRUE(item.isRoiLockedToTrack());
    EXPECT_EQ(lockSpy.count(), 1);

    // Now feeding tracker target box should update ROI with 20% margin
    item.setTrackerTargetBox(0.40, 0.40, 0.20, 0.20);
    const QRectF lockedRoi = item.roiRect();

    // 0.40 - (0.20 * 0.20) = 0.36
    EXPECT_NEAR(lockedRoi.x(), 0.36, 0.001);
    EXPECT_NEAR(lockedRoi.y(), 0.36, 0.001);
    // 0.20 * 1.4 = 0.28
    EXPECT_NEAR(lockedRoi.width(), 0.28, 0.001);
    EXPECT_NEAR(lockedRoi.height(), 0.28, 0.001);
}

TEST_F(LensRoiPeakingTest, FocusPeakingConfiguration)
{
    LensVideoQuickItem item;
    QSignalSpy threshSpy(&item, &LensVideoQuickItem::peakingThresholdChanged);
    QSignalSpy colorSpy(&item, &LensVideoQuickItem::peakingColorChanged);

    EXPECT_FALSE(item.showPeaking());
    EXPECT_EQ(item.peakingThreshold(), 80);
    EXPECT_EQ(item.peakingColorIndex(), 0); // Emerald

    item.setShowPeaking(true);
    EXPECT_TRUE(item.showPeaking());

    item.setPeakingThreshold(120);
    EXPECT_EQ(item.peakingThreshold(), 120);
    EXPECT_EQ(threshSpy.count(), 1);

    // Clamping boundaries
    item.setPeakingThreshold(5); // Below 10
    EXPECT_EQ(item.peakingThreshold(), 10);

    item.setPeakingThreshold(500); // Above 300
    EXPECT_EQ(item.peakingThreshold(), 300);

    // Palette indexes
    item.setPeakingColorIndex(1); // Cyan
    EXPECT_EQ(item.peakingColorIndex(), 1);
    EXPECT_EQ(colorSpy.count(), 1);

    item.setPeakingColorIndex(2); // Coral
    EXPECT_EQ(item.peakingColorIndex(), 2);

    item.setPeakingColorIndex(99); // Clamped to 2
    EXPECT_EQ(item.peakingColorIndex(), 2);
}

TEST_F(LensRoiPeakingTest, ControllerRoiSynchronization)
{
    LensPlayerController controller;
    LensVideoQuickItem item;
    item.setController(&controller);

    controller.applyRoiPreset(2); // Spot 25%
    item.setRoiRect(controller.roiRect());

    EXPECT_DOUBLE_EQ(item.roiRect().width(), 0.25);
    EXPECT_DOUBLE_EQ(item.roiRect().height(), 0.25);

    controller.setPeakingThreshold(140);
    EXPECT_EQ(controller.peakingThreshold(), 140);

    controller.setPeakingColorIndex(1);
    EXPECT_EQ(controller.peakingColorIndex(), 1);

    controller.setIsRoiLockedToTrack(true);
    EXPECT_TRUE(controller.isRoiLockedToTrack());
}

} // namespace LensApp
