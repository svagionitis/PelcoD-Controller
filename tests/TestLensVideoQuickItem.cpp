/// @file TestLensVideoQuickItem.cpp
/// @brief Unit tests for LensVideoQuickItem property bindings, ROI normalization, and letterbox geometry.

#include <cstdint>

#include <QGuiApplication>
#include <QRectF>

#include <gtest/gtest.h>

#include "LensVideoQuickItem.h"

namespace LensApp {
namespace {

    class TestLensVideoQuickItem : public ::testing::Test {
    protected:
        static void SetUpTestSuite()
        {
            if (qApp == nullptr) {
                static int argc = 1;
                static char appName[] = "TestLensVideoQuickItem";
                static char* argv[] = { appName, nullptr };
                s_app = new QGuiApplication(argc, argv);
            }
        }

        static void TearDownTestSuite()
        {
            delete s_app;
            s_app = nullptr;
        }

    private:
        static QGuiApplication* s_app;
    };

    QGuiApplication* TestLensVideoQuickItem::s_app = nullptr;

    TEST_F(TestLensVideoQuickItem, DefaultProperties)
    {
        LensVideoQuickItem item;
        EXPECT_TRUE(item.showGrid());
        EXPECT_TRUE(item.showReticle());
        EXPECT_TRUE(item.showRoi());
        EXPECT_FALSE(item.showPeaking());

        const QRectF defaultRoi = item.roiRect();
        EXPECT_DOUBLE_EQ(defaultRoi.x(), 0.25);
        EXPECT_DOUBLE_EQ(defaultRoi.y(), 0.25);
        EXPECT_DOUBLE_EQ(defaultRoi.width(), 0.50);
        EXPECT_DOUBLE_EQ(defaultRoi.height(), 0.50);
    }

    TEST_F(TestLensVideoQuickItem, RoiClampingWithinUnitBounds)
    {
        LensVideoQuickItem item;

        // Test out of bounds top-left
        item.setRoiRect(QRectF(-0.5, -0.5, 0.5, 0.5));
        EXPECT_GE(item.roiRect().x(), 0.0);
        EXPECT_GE(item.roiRect().y(), 0.0);

        // Test out of bounds bottom-right
        item.setRoiRect(QRectF(0.8, 0.8, 0.5, 0.5));
        EXPECT_LE(item.roiRect().right(), 1.0);
        EXPECT_LE(item.roiRect().bottom(), 1.0);

        // Test minimum size enforcement
        item.setRoiRect(QRectF(0.5, 0.5, 0.01, 0.01));
        EXPECT_GE(item.roiRect().width(), 0.05);
        EXPECT_GE(item.roiRect().height(), 0.05);
    }

    TEST_F(TestLensVideoQuickItem, PropertyToggles)
    {
        LensVideoQuickItem item;

        item.setShowPeaking(true);
        EXPECT_TRUE(item.showPeaking());

        item.setShowGrid(false);
        EXPECT_FALSE(item.showGrid());

        item.setShowReticle(false);
        EXPECT_FALSE(item.showReticle());

        item.setShowRoi(false);
        EXPECT_FALSE(item.showRoi());
    }

} // namespace
} // namespace LensApp
