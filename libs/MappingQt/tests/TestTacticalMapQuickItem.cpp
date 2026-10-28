/// @file TestTacticalMapQuickItem.cpp
/// @brief Unit tests for MappingQt::TacticalMapQuickItem properties, rendering, concurrency, and P0 safety guards.

#include "TacticalMapQuickItem.h"

#include <atomic>
#include <cmath>
#include <limits>
#include <thread>
#include <type_traits>
#include <utility>

#include <QColor>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPointF>
#include <QSignalSpy>
#include <QtGlobal>
#include <gtest/gtest.h>

#include "KlvTypes.h"

using namespace MappingQt;

TEST(TacticalMapQuickItemTest, PropertyDefaultsAndSetters)
{
    TacticalMapQuickItem item { };

    // Center coordinates
    item.setCenter(37.9838, 23.7275);
    EXPECT_NEAR(item.centerLatitude(), 37.9838, 1e-4);
    EXPECT_NEAR(item.centerLongitude(), 23.7275, 1e-4);

    // Zoom level
    item.setZoom(14.5);
    EXPECT_NEAR(item.zoom(), 14.5, 1e-4);

    // Platform telemetry
    item.setPlatformPosition(37.98, 23.72, 180.0);
    EXPECT_NEAR(item.platformLatitude(), 37.98, 1e-4);
    EXPECT_NEAR(item.platformLongitude(), 23.72, 1e-4);
    EXPECT_NEAR(item.platformHeading(), 180.0, 1e-4);

    // Frustum
    item.setFrustum(38.0, 23.7, 38.0, 23.75, 37.95, 23.75, 37.95, 23.7);
    EXPECT_TRUE(item.showFrustum());

    item.clearFrustum();

    // Coordinate conversion
    const QPointF screenPt { item.geoToScreen(37.9838, 23.7275) };
    const QPointF recoveredGeo { item.screenToGeo(screenPt.x(), screenPt.y()) };
    EXPECT_NEAR(recoveredGeo.x(), 37.9838, 1e-4);
    EXPECT_NEAR(recoveredGeo.y(), 23.7275, 1e-4);
}

TEST(TacticalMapQuickItemTest, Err55CppNoexceptContractCompliance)
{
    // SEI CERT ERR55-CPP: Methods that lock QMutex, call update(), or emit Qt signals
    // must not be marked noexcept.
    EXPECT_FALSE(noexcept(std::declval<TacticalMapQuickItem&>().setCenter(0.0, 0.0)));
    EXPECT_FALSE(noexcept(std::declval<TacticalMapQuickItem&>().setCenterLatitude(0.0)));
    EXPECT_FALSE(noexcept(std::declval<TacticalMapQuickItem&>().setCenterLongitude(0.0)));
    EXPECT_FALSE(noexcept(std::declval<TacticalMapQuickItem&>().setZoom(10.0)));
    EXPECT_FALSE(noexcept(std::declval<TacticalMapQuickItem&>().setOfflineOnly(true)));
    EXPECT_FALSE(noexcept(std::declval<TacticalMapQuickItem&>().setShowFrustum(true)));
    EXPECT_FALSE(noexcept(std::declval<TacticalMapQuickItem&>().setShowHeading(true)));
    EXPECT_FALSE(noexcept(std::declval<TacticalMapQuickItem&>().setShowGrid(true)));
    EXPECT_FALSE(noexcept(std::declval<TacticalMapQuickItem&>().setPlatformLatitude(0.0)));
    EXPECT_FALSE(noexcept(std::declval<TacticalMapQuickItem&>().setPlatformLongitude(0.0)));
    EXPECT_FALSE(noexcept(std::declval<TacticalMapQuickItem&>().setPlatformHeading(0.0)));
    EXPECT_FALSE(noexcept(std::declval<TacticalMapQuickItem&>().setPlatformPosition(0.0, 0.0, 0.0)));
    EXPECT_FALSE(noexcept(std::declval<TacticalMapQuickItem&>().setThemeColor(QColor())));
}

TEST(TacticalMapQuickItemTest, NaNAndInfiniteInputsRejectedWithoutSignalLoop)
{
    TacticalMapQuickItem item { };
    item.setCenter(37.9838, 23.7275);
    item.setZoom(12.0);
    item.setPlatformPosition(37.9838, 23.7275, 90.0);

    QSignalSpy centerSpy(&item, &TacticalMapQuickItem::centerChanged);
    QSignalSpy zoomSpy(&item, &TacticalMapQuickItem::zoomChanged);
    QSignalSpy platformSpy(&item, &TacticalMapQuickItem::platformChanged);

    const double qnan { std::numeric_limits<double>::quiet_NaN() };
    const double pinf { std::numeric_limits<double>::infinity() };
    const double ninf { -std::numeric_limits<double>::infinity() };

    item.setCenterLatitude(qnan);
    item.setCenterLongitude(pinf);
    item.setCenter(qnan, ninf);
    item.setZoom(qnan);
    item.setZoom(pinf);

    item.setPlatformLatitude(qnan);
    item.setPlatformLatitude(pinf);
    item.setPlatformLongitude(qnan);
    item.setPlatformLongitude(ninf);
    item.setPlatformHeading(qnan);
    item.setPlatformHeading(pinf);
    item.setPlatformPosition(qnan, 23.0, 45.0);
    item.setPlatformPosition(37.0, pinf, 45.0);
    item.setPlatformPosition(37.0, 23.0, ninf);

    item.setFrustum(qnan, 23.7, 38.0, 23.75, 37.95, 23.75, 37.95, 23.7);

    EXPECT_EQ(centerSpy.count(), 0);
    EXPECT_EQ(zoomSpy.count(), 0);
    EXPECT_EQ(platformSpy.count(), 0);

    EXPECT_TRUE(std::isfinite(item.centerLatitude()));
    EXPECT_TRUE(std::isfinite(item.centerLongitude()));
    EXPECT_TRUE(std::isfinite(item.zoom()));
    EXPECT_TRUE(std::isfinite(item.platformLatitude()));
    EXPECT_TRUE(std::isfinite(item.platformLongitude()));
    EXPECT_TRUE(std::isfinite(item.platformHeading()));

    EXPECT_NEAR(item.platformLatitude(), 37.9838, 1e-4);
    EXPECT_NEAR(item.platformLongitude(), 23.7275, 1e-4);
    EXPECT_NEAR(item.platformHeading(), 90.0, 1e-4);

    // Out-of-bounds finite coordinates must be clamped/normalized
    item.setPlatformPosition(95.0, 540.0, -90.0);
    EXPECT_LE(item.platformLatitude(), 85.0512);
    EXPECT_NEAR(item.platformLongitude(), -180.0, 1e-4);
    EXPECT_NEAR(item.platformHeading(), 270.0, 1e-4);
}

TEST(TacticalMapQuickItemTest, NullPointerSafetyInPaintAndEvents)
{
    TacticalMapQuickItem item { };
    item.setWidth(320.0);
    item.setHeight(240.0);

    // SEI CERT EXP34-C: paint(nullptr) must safely return without dereferencing nullptr
    EXPECT_NO_FATAL_FAILURE(item.paint(nullptr));

    // Valid offscreen paint with frustum and heading enabled
    item.setPlatformPosition(37.9838, 23.7275, 45.0);
    item.setFrustum(37.99, 23.72, 37.99, 23.73, 37.97, 23.73, 37.97, 23.72);

    QImage canvas(320, 240, QImage::Format_ARGB32);
    canvas.fill(Qt::black);
    {
        QPainter painter(&canvas);
        item.paint(&painter);
    }
    // Verify background/tiles/overlays modified the canvas
    EXPECT_NE(canvas.pixelColor(160, 120), QColor(Qt::black));
}

TEST(TacticalMapQuickItemTest, ReentrantSlotInSignalDoesNotDeadlock)
{
    TacticalMapQuickItem item { };
    int callbackCount { 0 };

    QObject::connect(&item, &TacticalMapQuickItem::centerChanged, &item, [&]() {
        EXPECT_TRUE(std::isfinite(item.centerLatitude()));
        EXPECT_TRUE(std::isfinite(item.centerLongitude()));
        ++callbackCount;
    });
    QObject::connect(&item, &TacticalMapQuickItem::platformChanged, &item, [&]() {
        EXPECT_TRUE(std::isfinite(item.platformLatitude()));
        EXPECT_TRUE(std::isfinite(item.platformLongitude()));
        EXPECT_TRUE(std::isfinite(item.platformHeading()));
        ++callbackCount;
    });
    QObject::connect(&item, &TacticalMapQuickItem::themeColorChanged, &item, [&]() {
        EXPECT_TRUE(item.themeColor().isValid());
        ++callbackCount;
    });

    item.setCenter(38.5, 24.0);
    item.setPlatformPosition(38.5, 24.0, 120.0);
    item.setThemeColor(QColor(255, 128, 0));

    EXPECT_EQ(callbackCount, 3);
}

TEST(TacticalMapQuickItemTest, ConcurrentGuiMutationAndRenderThreadPaint)
{
    TacticalMapQuickItem item { };
    item.setWidth(256.0);
    item.setHeight(256.0);

    std::atomic<bool> running { true };

    std::thread mutator([&]() {
        for (int i { 0 }; i < 150; ++i) {
            const double offset { static_cast<double>(i) * 0.001 };
            item.setPlatformPosition(37.98 + offset, 23.72 + offset, static_cast<double>((i * 15) % 360));
            item.setFrustum(37.99 + offset, 23.71, 37.99 + offset, 23.73, 37.97 + offset, 23.73, 37.97 + offset, 23.71);
            item.setShowFrustum((i % 2) == 0);
            item.setShowHeading((i % 3) != 0);
            item.setThemeColor(QColor((i * 7) % 255, 200, 255));
        }
        running.store(false, std::memory_order_release);
    });

    QImage canvas(256, 256, QImage::Format_ARGB32);
    while (running.load(std::memory_order_acquire)) {
        QPainter painter(&canvas);
        item.paint(&painter);
        (void)item.platformLatitude();
        (void)item.platformLongitude();
        (void)item.platformHeading();
        (void)item.showFrustum();
        (void)item.themeColor();
    }

    mutator.join();
    EXPECT_TRUE(std::isfinite(item.platformLatitude()));
}

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
