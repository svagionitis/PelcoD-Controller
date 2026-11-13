/// @file TestPresentationEngine.cpp
/// @brief Unit tests for presentation calculations, coordinate transforms, and overlay rendering.

#include <cstddef>
#include <cstdint>

#include <array>
#include <vector>

#include <QColor>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPointF>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QRectF>
#include <QUrl>

#include <gtest/gtest.h>

#include "OverlayRenderer.h"
#include "PresentationTypes.h"
#include "PresetManager.h"
#include "StabilizerConfigModel.h"
#include "StabilizerFramePacket.h"
#include "StabilizerPlayerController.h"
#include "StabilizerQuickItem.h"
#include "TelemetryRingBuffer.h"
#include "VideoStabilizerEngine.h"

namespace StabilizerApp {
namespace {

    TEST(TestPresentationEngine, VerifyZoomFactorBounds)
    {
        const ZoomFactor defaultZoom;
        EXPECT_FLOAT_EQ(defaultZoom.value(), 1.0F);
        EXPECT_FALSE(defaultZoom.isZoomed());

        const ZoomFactor belowMin(0.5F);
        EXPECT_FLOAT_EQ(belowMin.value(), 1.0F);
        EXPECT_FALSE(belowMin.isZoomed());

        const ZoomFactor aboveMax(12.0F);
        EXPECT_FLOAT_EQ(aboveMax.value(), 8.0F);
        EXPECT_TRUE(aboveMax.isZoomed());

        const ZoomFactor validZoom(2.5F);
        EXPECT_FLOAT_EQ(validZoom.value(), 2.5F);
        EXPECT_TRUE(validZoom.isZoomed());
    }

    TEST(TestPresentationEngine, VerifyAspectFitLetterboxing)
    {
        // Test 1: Identical 16:9 aspect ratio (1920x1080 into 1280x720)
        const QRectF viewport1(0, 0, 1280, 720);
        const qreal scale1 = std::min(viewport1.width() / 1920.0, viewport1.height() / 1080.0);
        const qreal w1 = 1920.0 * scale1;
        const qreal h1 = 1080.0 * scale1;
        EXPECT_DOUBLE_EQ(w1, 1280.0);
        EXPECT_DOUBLE_EQ(h1, 720.0);

        // Test 2: 16:9 video in 4:3 window (800x600) -> letterboxing top and bottom
        const QRectF viewport2(0, 0, 800, 600);
        const qreal scale2 = std::min(viewport2.width() / 1920.0, viewport2.height() / 1080.0);
        const qreal w2 = 1920.0 * scale2;
        const qreal h2 = 1080.0 * scale2;
        EXPECT_DOUBLE_EQ(w2, 800.0);
        EXPECT_DOUBLE_EQ(h2, 450.0);
        const qreal topMargin = (600.0 - h2) * 0.5;
        EXPECT_DOUBLE_EQ(topMargin, 75.0);

        // Test 3: 4:3 video (640x480) in 16:9 window (1280x720) -> pillarboxing left and right
        const QRectF viewport3(0, 0, 1280, 720);
        const qreal scale3 = std::min(viewport3.width() / 640.0, viewport3.height() / 480.0);
        const qreal w3 = 640.0 * scale3;
        const qreal h3 = 480.0 * scale3;
        EXPECT_DOUBLE_EQ(w3, 960.0);
        EXPECT_DOUBLE_EQ(h3, 720.0);
        const qreal sideMargin = (1280.0 - w3) * 0.5;
        EXPECT_DOUBLE_EQ(sideMargin, 160.0);
    }

    TEST(TestPresentationEngine, VerifyDividerHitTestingMath)
    {
        const QRectF targetRect(100.0, 50.0, 800.0, 450.0);
        constexpr float splitPos = 0.50F;
        const qreal splitX = targetRect.left() + (static_cast<qreal>(splitPos) * targetRect.width()); // 500.0

        // Exactly on divider
        EXPECT_LE(std::abs(500.0 - splitX), 8.0);

        // Within +/- 8px margin
        EXPECT_LE(std::abs(505.0 - splitX), 8.0);
        EXPECT_LE(std::abs(493.0 - splitX), 8.0);

        // Beyond margin
        EXPECT_GT(std::abs(512.0 - splitX), 8.0);
        EXPECT_GT(std::abs(488.0 - splitX), 8.0);
    }

    TEST(TestPresentationEngine, VerifyOverlayRendererExecution)
    {
        if (qApp == nullptr) {
            static int argc = 1;
            static char name[] = "TestPresentationEngine";
            static char* argv[] = { name, nullptr };
            static QGuiApplication app(argc, argv);
        }

        QImage canvas(640, 480, QImage::Format_RGB888);
        canvas.fill(QColor(0x0e, 0x10, 0x14));

        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF targetRect(40.0, 30.0, 560.0, 420.0);

        // Reticle
        OverlayRenderer::renderReticle(&painter, targetRect);

        // Crop boundary
        OverlayRenderer::renderCropBoundary(&painter, targetRect, 0.08F, 0.80F);

        // Features & Quiver
        std::array<VisualFeaturePoint, 3U> features { { { 100.0F, 100.0F, 5.0F, -2.0F, true },
            { 200.0F, 200.0F, -3.0F, 4.0F, false }, { 300.0F, 300.0F, 0.0F, 0.0F, true } } };

        OverlayRenderer::renderLandmarks(&painter, targetRect, features.data(), features.size(), 1.0F, 1.0F);
        OverlayRenderer::renderQuiverPlot(&painter, targetRect, features.data(), features.size(), 1.0F, 1.0F, 2.5F);

        // Badge
        OverlayRenderer::renderBadge(&painter, QRectF(50.0, 40.0, 120.0, 24.0), QStringLiteral("TEST BADGE"),
            QColor(0x00, 0xe6, 0x76), QColor(0, 0, 0, 160));

        painter.end();

        // Verify drawing modified pixels on the canvas
        bool hasNonBgPixel = false;
        const auto* scan0 = canvas.constBits();
        for (int i = 0; i < canvas.sizeInBytes(); ++i) {
            if ((scan0[i] != 0x0e) && (scan0[i] != 0x10) && (scan0[i] != 0x14)) {
                hasNonBgPixel = true;
                break;
            }
        }
        EXPECT_TRUE(hasNonBgPixel);
    }

    TEST(TestPresentationEngine, VerifyEngineCopyLandmarks)
    {
        Stabilizer::VideoStabilizerEngine engine;
        std::array<Stabilizer::VisualLandmark, 10U> outLandmarks {};

        // Fresh unconfigured engine has 0 landmarks
        const std::size_t copied = engine.copyLandmarks(outLandmarks.data(), outLandmarks.size());
        EXPECT_EQ(copied, 0U);

        // Null safety
        EXPECT_EQ(engine.copyLandmarks(nullptr, 10U), 0U);
        EXPECT_EQ(engine.copyLandmarks(outLandmarks.data(), 0U), 0U);
    }

    TEST(TestPresentationEngine, VerifyQmlMainComponentLoads)
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));

        qmlRegisterType<StabilizerPlayerController>("StabilizerApp", 1, 0, "StabilizerPlayerController");
        qmlRegisterType<StabilizerQuickItem>("StabilizerApp", 1, 0, "StabilizerQuickItem");
        qmlRegisterUncreatableType<StabilizerConfigModel>(
            "StabilizerApp", 1, 0, "StabilizerConfigModel", QStringLiteral("Access via StabilizerPlayerController"));
        qmlRegisterUncreatableType<TelemetryRingBuffer>(
            "StabilizerApp", 1, 0, "TelemetryRingBuffer", QStringLiteral("Access via StabilizerPlayerController"));
        qmlRegisterUncreatableType<PresetManager>(
            "StabilizerApp", 1, 0, "PresetManager", QStringLiteral("Access via StabilizerPlayerController"));

        QQmlApplicationEngine engine;
        engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
        ASSERT_FALSE(engine.rootObjects().isEmpty());
    }

} // namespace
} // namespace StabilizerApp
