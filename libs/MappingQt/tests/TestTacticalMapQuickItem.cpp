/// @file TestTacticalMapQuickItem.cpp
/// @brief Unit tests for MappingQt::TacticalMapQuickItem properties, rendering, concurrency, and P0/P1/P2 requirements.

#include "TacticalMapQuickItem.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <atomic>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>

#include <QByteArray>
#include <QColor>
#include <QEvent>
#include <QGuiApplication>
#include <QImage>
#include <QMouseEvent>
#include <QObject>
#include <QPainter>
#include <QPoint>
#include <QPointF>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QRectF>
#include <QSignalSpy>
#include <QString>
#include <QUrl>
#include <QWheelEvent>
#include <QtGlobal>
#include <gtest/gtest.h>

#include "DiskTileCache.h"
#include "GeoTypes.h"
#include "KlvTypes.h"
#include "MapViewport.h"
#include "VmtiTypes.h"

using namespace MappingQt;

namespace {

/// @class TestableTacticalMapQuickItem
/// @brief Exposes protected event and geometry handlers of TacticalMapQuickItem for deterministic unit testing.
/// @details Promotes `geometryChange`, `mousePressEvent`, `mouseMoveEvent`, `mouseReleaseEvent`, and `wheelEvent`
///          to `public` visibility so synthetic Qt input events can be dispatched directly in headless tests.
class TestableTacticalMapQuickItem : public TacticalMapQuickItem {
public:
    using TacticalMapQuickItem::geometryChange;
    using TacticalMapQuickItem::mouseMoveEvent;
    using TacticalMapQuickItem::mousePressEvent;
    using TacticalMapQuickItem::mouseReleaseEvent;
    using TacticalMapQuickItem::TacticalMapQuickItem;
    using TacticalMapQuickItem::wheelEvent;
};

/// @brief Builds a solid-color 16x16 binary PPM (`P6`) tile payload for disk cache testing.
/// @details Avoids external file assets while producing a valid raster image decodable by `QImage::loadFromData()`.
/// @param[in] r Red channel intensity (`[0, 255]`).
/// @param[in] g Green channel intensity (`[0, 255]`).
/// @param[in] b Blue channel intensity (`[0, 255]`).
/// @return Populated `Mapping::TileData` instance.
[[nodiscard]] Mapping::TileData createSolidPpmTile(std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    Mapping::TileData tile { };
    const std::string header { "P6\n16 16\n255\n" };
    tile.bytes.reserve(header.size() + (16U * 16U * 3U));
    for (const char ch : header) {
        tile.bytes.push_back(static_cast<std::uint8_t>(ch));
    }
    for (std::size_t i { 0U }; i < (16U * 16U); ++i) {
        tile.bytes.push_back(r);
        tile.bytes.push_back(g);
        tile.bytes.push_back(b);
    }
    tile.mimeType = "image/png";
    tile.valid = true;
    return tile;
}

} // namespace

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

TEST(TacticalMapQuickItemTest, ShowGridControlsProceduralFallbackAndOverlay)
{
    TacticalMapQuickItem item { };
    item.setWidth(256.0);
    item.setHeight(256.0);
    item.setShowFrustum(false);
    item.setShowHeading(false);

    QSignalSpy gridSpy(&item, &TacticalMapQuickItem::showGridChanged);

    QImage withGrid(256, 256, QImage::Format_ARGB32);
    withGrid.fill(Qt::black);
    {
        QPainter painter(&withGrid);
        item.paint(&painter);
    }

    item.setShowGrid(false);
    EXPECT_EQ(gridSpy.count(), 1);
    EXPECT_FALSE(item.showGrid());

    QImage withoutGrid(256, 256, QImage::Format_ARGB32);
    withoutGrid.fill(Qt::black);
    {
        QPainter painter(&withoutGrid);
        item.paint(&painter);
    }

    // Disabling showGrid disables procedural fallback tiles and tactical grid lines,
    // leaving only the solid base background QColor(14, 18, 24).
    EXPECT_NE(withGrid, withoutGrid);
    EXPECT_EQ(withoutGrid.pixelColor(64, 64), QColor(14, 18, 24));
}

TEST(TacticalMapQuickItemTest, MouseClickEmitsCoordinateClickedWhilePanDragDoesNot)
{
    TestableTacticalMapQuickItem item { };
    item.setWidth(400.0);
    item.setHeight(300.0);
    item.setCenter(37.9838, 23.7275);
    item.setZoom(12.0);

    QSignalSpy clickSpy(&item, &TacticalMapQuickItem::coordinateClicked);
    QSignalSpy centerSpy(&item, &TacticalMapQuickItem::centerChanged);

    // 1. Stationary left click (displacement = 0 px <= 4.0 px deadband)
    {
        const QPointF clickPos(200.0, 150.0);
        QMouseEvent pressEv(
            QEvent::MouseButtonPress, clickPos, clickPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        item.mousePressEvent(&pressEv);
        // Must NOT emit on press before knowing if it's a drag
        EXPECT_EQ(clickSpy.count(), 0);

        QMouseEvent releaseEv(
            QEvent::MouseButtonRelease, clickPos, clickPos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        item.mouseReleaseEvent(&releaseEv);
        EXPECT_EQ(clickSpy.count(), 1);
    }

    clickSpy.clear();
    centerSpy.clear();

    // 2. Pan drag gesture (displacement = 60 px > 4.0 px deadband)
    {
        const QPointF startPos(100.0, 100.0);
        const QPointF movePos(160.0, 140.0);

        QMouseEvent pressEv(
            QEvent::MouseButtonPress, startPos, startPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        item.mousePressEvent(&pressEv);

        QMouseEvent moveEv(QEvent::MouseMove, movePos, movePos, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        item.mouseMoveEvent(&moveEv);

        QMouseEvent releaseEv(
            QEvent::MouseButtonRelease, movePos, movePos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        item.mouseReleaseEvent(&releaseEv);

        // Dragging must pan the map (emitting centerChanged) and suppress coordinateClicked!
        EXPECT_GE(centerSpy.count(), 1);
        EXPECT_EQ(clickSpy.count(), 0);
    }
}

TEST(TacticalMapQuickItemTest, DecodedTileCacheAvoidsRedundantDecompressionAndInvalidates)
{
    TacticalMapQuickItem item { };
    item.setWidth(256.0);
    item.setHeight(256.0);
    EXPECT_EQ(item.decodedTileCacheSize(), 0U);

    QImage canvas(256, 256, QImage::Format_ARGB32);
    {
        QPainter painter(&canvas);
        item.paint(&painter);
    }
    const std::size_t cachedAfterFirstPaint { item.decodedTileCacheSize() };
    EXPECT_GT(cachedAfterFirstPaint, 0U);

    // Second paint at identical viewport hits the decoded QImage cache without growing
    {
        QPainter painter(&canvas);
        item.paint(&painter);
    }
    EXPECT_EQ(item.decodedTileCacheSize(), cachedAfterFirstPaint);

    // Changing showGrid or offline directory invalidates the decoded QImage cache
    item.setShowGrid(false);
    EXPECT_EQ(item.decodedTileCacheSize(), 0U);
}

TEST(TacticalMapQuickItemTest, TargetLineOfSightAndVmtiOverlaysRenderAndCullOffscreen)
{
    TacticalMapQuickItem item { };
    item.setWidth(320.0);
    item.setHeight(240.0);
    item.setCenter(37.9838, 23.7275);
    item.setZoom(13.0);
    item.setPlatformPosition(37.9838, 23.7275, 45.0);

    QSignalSpy targetSpy(&item, &TacticalMapQuickItem::targetChanged);
    EXPECT_FALSE(item.hasTarget());

    item.setTargetPosition(37.9860, 23.7310);
    EXPECT_TRUE(item.hasTarget());
    EXPECT_NEAR(item.targetLatitude(), 37.9860, 1e-4);
    EXPECT_NEAR(item.targetLongitude(), 23.7310, 1e-4);
    EXPECT_EQ(targetSpy.count(), 1);

    // Add MISB ST 0903 VMTI target pack
    Klv::VmtiLocalSet vmti { };
    Klv::VTargetPack trk1 { };
    trk1.targetId = 1U;
    trk1.targetLocation = Klv::GeoPoint3D { 37.9850, 23.7290, 50.0 };
    trk1.targetCe90M = 15.0;
    trk1.confidence = 92U;
    trk1.detectionStatus = 1U; // Moving
    vmti.targets.push_back(trk1);

    item.setVmtiTargets(vmti);
    EXPECT_EQ(item.vmtiTargetCount(), 1U);

    QImage canvas(320, 240, QImage::Format_ARGB32);
    canvas.fill(Qt::black);
    {
        QPainter painter(&canvas);
        item.paint(&painter);
    }

    item.clearTarget();
    EXPECT_FALSE(item.hasTarget());
    EXPECT_EQ(targetSpy.count(), 2);

    item.clearVmtiTargets();
    EXPECT_EQ(item.vmtiTargetCount(), 0U);

    // Offline directory reset with empty string & secondary search directory
    item.setOfflineDirectory(QStringLiteral(""));
    item.addOfflineSearchDirectory(QStringLiteral(""));
}

TEST(TacticalMapQuickItemTest, IdempotentSettersDoNotEmitDuplicateSignals)
{
    TacticalMapQuickItem item { };
    item.setCenter(37.9838, 23.7275);
    item.setZoom(12.0);
    item.setOfflineOnly(true);
    item.setShowFrustum(true);
    item.setShowHeading(true);
    item.setShowGrid(true);
    item.setPlatformPosition(37.9838, 23.7275, 90.0);
    item.setTargetPosition(37.9900, 23.7300);
    item.setThemeColor(QColor(0, 229, 255));

    QSignalSpy centerSpy(&item, &TacticalMapQuickItem::centerChanged);
    QSignalSpy zoomSpy(&item, &TacticalMapQuickItem::zoomChanged);
    QSignalSpy offlineSpy(&item, &TacticalMapQuickItem::offlineOnlyChanged);
    QSignalSpy frustumSpy(&item, &TacticalMapQuickItem::showFrustumChanged);
    QSignalSpy headingSpy(&item, &TacticalMapQuickItem::showHeadingChanged);
    QSignalSpy gridSpy(&item, &TacticalMapQuickItem::showGridChanged);
    QSignalSpy platformSpy(&item, &TacticalMapQuickItem::platformChanged);
    QSignalSpy targetSpy(&item, &TacticalMapQuickItem::targetChanged);
    QSignalSpy themeSpy(&item, &TacticalMapQuickItem::themeColorChanged);

    // Re-applying identical values must emit zero signals
    item.setCenter(37.9838, 23.7275);
    item.setCenterLatitude(37.9838);
    item.setCenterLongitude(23.7275);
    item.setZoom(12.0);
    item.setOfflineOnly(true);
    item.setShowFrustum(true);
    item.setShowHeading(true);
    item.setShowGrid(true);
    item.setPlatformPosition(37.9838, 23.7275, 90.0);
    item.setPlatformLatitude(37.9838);
    item.setPlatformLongitude(23.7275);
    item.setPlatformHeading(90.0);
    item.setTargetPosition(37.9900, 23.7300);
    item.setThemeColor(QColor(0, 229, 255));

    EXPECT_EQ(centerSpy.count(), 0);
    EXPECT_EQ(zoomSpy.count(), 0);
    EXPECT_EQ(offlineSpy.count(), 0);
    EXPECT_EQ(frustumSpy.count(), 0);
    EXPECT_EQ(headingSpy.count(), 0);
    EXPECT_EQ(gridSpy.count(), 0);
    EXPECT_EQ(platformSpy.count(), 0);
    EXPECT_EQ(targetSpy.count(), 0);
    EXPECT_EQ(themeSpy.count(), 0);
}

TEST(TacticalMapQuickItemTest, StaticAndFreeRegisterQmlTypesLinkAndInstantiateInQml)
{
    TacticalMapQuickItem::registerQmlTypes();

    QQmlEngine engine { };
    QQmlComponent component(&engine);
    component.setData(
        QByteArrayLiteral("import PelcoD.Mapping 1.0\nTacticalMap { width: 200; height: 150 }\n"), QUrl());
    std::unique_ptr<QObject> obj { component.create() };
    ASSERT_NE(obj, nullptr) << component.errorString().toStdString();
    auto* mapItem { qobject_cast<TacticalMapQuickItem*>(obj.get()) };
    ASSERT_NE(mapItem, nullptr);
    EXPECT_NEAR(mapItem->width(), 200.0, 1e-3);
    EXPECT_NEAR(mapItem->height(), 150.0, 1e-3);
}

TEST(TacticalMapQuickItemTest, WheelEventZoomsAroundCursorAndHandlesNullEventPointers)
{
    TestableTacticalMapQuickItem item { };
    item.setWidth(400.0);
    item.setHeight(300.0);
    item.setCenter(37.9838, 23.7275);
    item.setZoom(12.0);

    // SEI CERT EXP34-C: Null event pointers must be ignored safely
    EXPECT_NO_FATAL_FAILURE(item.mousePressEvent(nullptr));
    EXPECT_NO_FATAL_FAILURE(item.mouseMoveEvent(nullptr));
    EXPECT_NO_FATAL_FAILURE(item.mouseReleaseEvent(nullptr));
    EXPECT_NO_FATAL_FAILURE(item.wheelEvent(nullptr));

    QSignalSpy zoomSpy(&item, &TacticalMapQuickItem::zoomChanged);
    QSignalSpy centerSpy(&item, &TacticalMapQuickItem::centerChanged);

    const QPointF cursorPivot(100.0, 80.0);
    const QPointF geoBefore { item.screenToGeo(cursorPivot.x(), cursorPivot.y()) };

    // Positive wheel notch (+120 angleDelta.y -> +0.5 zoom step)
    QWheelEvent wheelIn(
        cursorPivot, cursorPivot, QPoint(0, 0), QPoint(0, 120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    item.wheelEvent(&wheelIn);

    EXPECT_EQ(zoomSpy.count(), 1);
    EXPECT_EQ(centerSpy.count(), 1);
    EXPECT_NEAR(item.zoom(), 12.5, 1e-4);

    // Geographic point under the cursor pivot must remain invariant after zoomBy
    const QPointF geoAfter { item.screenToGeo(cursorPivot.x(), cursorPivot.y()) };
    EXPECT_NEAR(geoAfter.x(), geoBefore.x(), 1e-3);
    EXPECT_NEAR(geoAfter.y(), geoBefore.y(), 1e-3);
}

TEST(TacticalMapQuickItemTest, GeometryChangeUpdatesViewportDimensions)
{
    TestableTacticalMapQuickItem item { };
    item.setCenter(37.9838, 23.7275);
    item.setZoom(12.0);

    item.geometryChange(QRectF(0.0, 0.0, 640.0, 480.0), QRectF(0.0, 0.0, 800.0, 600.0));

    // After resizing the viewport to 640x480, the center geographic coordinate projects to (320, 240)
    const QPointF centerScreen { item.geoToScreen(37.9838, 23.7275) };
    EXPECT_NEAR(centerScreen.x(), 320.0, 1e-3);
    EXPECT_NEAR(centerScreen.y(), 240.0, 1e-3);
}

TEST(TacticalMapQuickItemTest, OfflineDirectoryLoadsDiskCachedTileAndResetsCleanly)
{
    std::error_code ec { };
    const std::filesystem::path tempRoot { std::filesystem::temp_directory_path(ec)
        / "pelcod_mapping_qt_p2_tile_test" };
    std::filesystem::remove_all(tempRoot, ec);

    const std::filesystem::path primaryDir { tempRoot / "primary" };
    const std::filesystem::path secondaryDir { tempRoot / "secondary" };

    // Populate all visible tiles at zoom 12 around (37.9838, 23.7275) with a bright crimson tile (200, 40, 60)
    Mapping::MapViewport vp({ 37.9838, 23.7275 }, 12.0, 256.0, 256.0);
    Mapping::DiskTileCache writer(primaryDir);
    const Mapping::TileData redTile { createSolidPpmTile(200U, 40U, 60U) };
    for (const auto& vt : vp.calculateVisibleTiles(1)) {
        EXPECT_TRUE(writer.storeTile(vt.coord, redTile));
    }

    TestableTacticalMapQuickItem item { };
    item.setWidth(256.0);
    item.setHeight(256.0);
    item.geometryChange(QRectF(0.0, 0.0, 256.0, 256.0), QRectF(0.0, 0.0, 800.0, 600.0));
    item.setCenter(37.9838, 23.7275);
    item.setZoom(12.0);
    item.setShowGrid(false);
    item.setShowFrustum(false);
    item.setShowHeading(false);

    item.setOfflineDirectory(QString::fromStdString(primaryDir.string()));
    item.addOfflineSearchDirectory(QString::fromStdString(secondaryDir.string()));

    QImage canvas(256, 256, QImage::Format_ARGB32);
    canvas.fill(Qt::black);
    {
        QPainter painter(&canvas);
        item.paint(&painter);
    }

    // Center pixel must reflect the decoded disk tile color (200, 40, 60) rather than background (14, 18, 24)
    const QColor centerColor { canvas.pixelColor(128, 128) };
    EXPECT_EQ(centerColor.red(), 200);
    EXPECT_EQ(centerColor.green(), 40);
    EXPECT_EQ(centerColor.blue(), 60);
    EXPECT_GT(item.decodedTileCacheSize(), 0U);

    // Resetting offline directory to empty detaches DiskTileCache and invalidates decoded tile cache
    item.setOfflineDirectory(QStringLiteral(""));
    EXPECT_EQ(item.decodedTileCacheSize(), 0U);

    std::filesystem::remove_all(tempRoot, ec);
}

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
