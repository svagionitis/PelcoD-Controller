#include "TacticalMapQuickItem.h"
#include <QGuiApplication>
#include <gtest/gtest.h>

using namespace MappingQt;

TEST(TacticalMapQuickItemTest, PropertyDefaultsAndSetters) {
    TacticalMapQuickItem item;

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
    const QPointF screenPt = item.geoToScreen(37.9838, 23.7275);
    const QPointF recoveredGeo = item.screenToGeo(screenPt.x(), screenPt.y());
    EXPECT_NEAR(recoveredGeo.x(), 37.9838, 1e-4);
    EXPECT_NEAR(recoveredGeo.y(), 23.7275, 1e-4);
}

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
