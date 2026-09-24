#include "PayloadHal.h"
#include <gtest/gtest.h>

namespace PayloadHal {
namespace {

TEST(TestPayloadHalUnits, DeviceInfoInitialization) {
    DeviceInfo info {};
    EXPECT_TRUE(info.manufacturer.empty());
    EXPECT_TRUE(info.model.empty());
    EXPECT_TRUE(info.serialNumber.empty());
    EXPECT_TRUE(info.firmwareVersion.empty());

    info.manufacturer = "Fujinon";
    info.model = "SX800";
    info.serialNumber = "SN-2026-9912";
    info.firmwareVersion = "v2.4.1";

    EXPECT_EQ(info.manufacturer, "Fujinon");
    EXPECT_EQ(info.model, "SX800");
    EXPECT_EQ(info.serialNumber, "SN-2026-9912");
    EXPECT_EQ(info.firmwareVersion, "v2.4.1");
}

TEST(TestPayloadHalUnits, DeviceStateEnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(DeviceState::Disconnected), 0U);
    EXPECT_EQ(static_cast<uint8_t>(DeviceState::Connecting), 1U);
    EXPECT_EQ(static_cast<uint8_t>(DeviceState::Standby), 2U);
    EXPECT_EQ(static_cast<uint8_t>(DeviceState::Ready), 3U);
    EXPECT_EQ(static_cast<uint8_t>(DeviceState::Degraded), 4U);
    EXPECT_EQ(static_cast<uint8_t>(DeviceState::Fault), 5U);
}

TEST(TestPayloadHalUnits, PanTiltModesAndStabilization) {
    EXPECT_EQ(static_cast<uint8_t>(PanTiltMode::Rate), 0U);
    EXPECT_EQ(static_cast<uint8_t>(PanTiltMode::Angle), 1U);
    EXPECT_EQ(static_cast<uint8_t>(PanTiltMode::Relative), 2U);
    EXPECT_EQ(static_cast<uint8_t>(PanTiltMode::Stow), 3U);
    EXPECT_EQ(static_cast<uint8_t>(PanTiltMode::Park), 4U);

    EXPECT_EQ(static_cast<uint8_t>(StabilizationMode::Disabled), 0U);
    EXPECT_EQ(static_cast<uint8_t>(StabilizationMode::RateStabilized), 1U);
    EXPECT_EQ(static_cast<uint8_t>(StabilizationMode::GeoHold), 2U);
    EXPECT_EQ(static_cast<uint8_t>(StabilizationMode::FollowPlatform), 3U);
}

TEST(TestPayloadHalUnits, GimbalTelemetryFields) {
    GimbalTelemetry telem {};
    EXPECT_DOUBLE_EQ(telem.panAngleDeg, 0.0);
    EXPECT_DOUBLE_EQ(telem.tiltAngleDeg, 0.0);
    EXPECT_DOUBLE_EQ(telem.panRateDegPerSec, 0.0);
    EXPECT_DOUBLE_EQ(telem.tiltRateDegPerSec, 0.0);
    EXPECT_FALSE(telem.isStabilized);
    EXPECT_FALSE(telem.isMoving);
    EXPECT_FALSE(telem.limitReached);

    telem.panAngleDeg = 145.5;
    telem.tiltAngleDeg = -22.3;
    telem.isStabilized = true;
    telem.isMoving = true;

    EXPECT_DOUBLE_EQ(telem.panAngleDeg, 145.5);
    EXPECT_DOUBLE_EQ(telem.tiltAngleDeg, -22.3);
    EXPECT_TRUE(telem.isStabilized);
    EXPECT_TRUE(telem.isMoving);
}

TEST(TestPayloadHalUnits, LrfMeasurementFields) {
    LrfTargetMeasurement m {};
    EXPECT_FALSE(m.valid);
    EXPECT_DOUBLE_EQ(m.slantRangeMeters, 0.0);
    EXPECT_DOUBLE_EQ(m.signalQualityRatio, 0.0);
    EXPECT_DOUBLE_EQ(m.diodeTemperatureC, 0.0);
    EXPECT_EQ(m.pulseCounter, 0U);

    m.valid = true;
    m.slantRangeMeters = 2450.5;
    m.signalQualityRatio = 0.95;
    m.diodeTemperatureC = 38.2;
    m.pulseCounter = 1204U;

    EXPECT_TRUE(m.valid);
    EXPECT_DOUBLE_EQ(m.slantRangeMeters, 2450.5);
    EXPECT_DOUBLE_EQ(m.signalQualityRatio, 0.95);
    EXPECT_DOUBLE_EQ(m.diodeTemperatureC, 38.2);
    EXPECT_EQ(m.pulseCounter, 1204U);
}

TEST(TestPayloadHalUnits, CameraTelemetryFields) {
    CameraTelemetry cam {};
    EXPECT_DOUBLE_EQ(cam.opticalZoomFactor, 1.0);
    EXPECT_DOUBLE_EQ(cam.normalizedZoom, 0.0);
    EXPECT_DOUBLE_EQ(cam.focusDistanceNormalized, 0.0);
    EXPECT_TRUE(cam.autoFocusActive);
    EXPECT_FALSE(cam.dayNightIcrActive);
    EXPECT_DOUBLE_EQ(cam.horizontalFovDeg, 60.0);

    cam.opticalZoomFactor = 40.0;
    cam.normalizedZoom = 1.0;
    cam.horizontalFovDeg = 1.4;
    cam.dayNightIcrActive = true;

    EXPECT_DOUBLE_EQ(cam.opticalZoomFactor, 40.0);
    EXPECT_DOUBLE_EQ(cam.normalizedZoom, 1.0);
    EXPECT_DOUBLE_EQ(cam.horizontalFovDeg, 1.4);
    EXPECT_TRUE(cam.dayNightIcrActive);
}

} // namespace
} // namespace PayloadHal
