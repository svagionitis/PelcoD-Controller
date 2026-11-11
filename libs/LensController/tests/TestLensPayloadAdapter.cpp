/// @file TestLensPayloadAdapter.cpp
/// @brief GoogleTest suite for LensPayloadAdapter bridging LensController to ICameraPayload.

#include <cstddef>
#include <cstdint>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "PayloadHal/ICameraPayload.h"
#include "PayloadHal/IDevice.h"
#include "PayloadHal/PayloadTypes.h"
#include "PayloadHal/VideoStreamTypes.h"

#include "LensController.h"
#include "LensPayloadAdapter.h"
#include "LensTypes.h"
#include "MockLensDriver.h"

using namespace LensControl;

class LensPayloadAdapterTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_controller = std::make_shared<LensController>();

        LensRanges ranges { };
        ranges.focusMin = 0U;
        ranges.focusMax = 1000U;
        ranges.zoomMin = 0U;
        ranges.zoomMax = 2000U;

        auto mock = std::make_unique<MockLensDriver>(ranges);
        m_mockDriver = mock.get();
        m_controller->setDriver(CameraIndex { 0U }, std::move(mock));

        OpticalSensorConfig cfg { };
        cfg.minFocalLengthMm = 4.3;
        cfg.maxFocalLengthMm = 129.0;
        cfg.sensorWidthMm = 5.568;
        cfg.sensorHeightMm = 3.132;
        cfg.maxOpticalZoom = 30.0;
        cfg.spectrum = PayloadHal::CameraSpectrum::DaylightVisible;

        m_adapter = std::make_unique<LensPayloadAdapter>(m_controller, CameraIndex { 0U }, cfg);
    }

    std::shared_ptr<LensController> m_controller;
    MockLensDriver* m_mockDriver { nullptr };
    std::unique_ptr<LensPayloadAdapter> m_adapter;
};

TEST_F(LensPayloadAdapterTest, DeviceLifecycle)
{
    EXPECT_FALSE(m_adapter->isConnected());
    EXPECT_EQ(m_adapter->state(), PayloadHal::DeviceState::Disconnected);

    bool stateCallbackCalled = false;
    PayloadHal::DeviceState reportedState = PayloadHal::DeviceState::Disconnected;

    m_adapter->registerStateCallback([&](PayloadHal::DeviceState newState, const std::string& reason) {
        (void)reason;
        stateCallbackCalled = true;
        reportedState = newState;
    });

    EXPECT_TRUE(m_adapter->connect());
    EXPECT_TRUE(m_adapter->isConnected());
    EXPECT_EQ(m_adapter->state(), PayloadHal::DeviceState::Ready);
    EXPECT_TRUE(stateCallbackCalled);
    EXPECT_EQ(reportedState, PayloadHal::DeviceState::Ready);

    const auto info = m_adapter->info();
    EXPECT_EQ(info.manufacturer, "LensController");
    EXPECT_EQ(m_adapter->spectrum(), PayloadHal::CameraSpectrum::DaylightVisible);

    m_adapter->disconnect();
    EXPECT_FALSE(m_adapter->isConnected());
    EXPECT_EQ(m_adapter->state(), PayloadHal::DeviceState::Disconnected);
}

TEST_F(LensPayloadAdapterTest, NormalizedZoomDirectPositioning)
{
    EXPECT_TRUE(m_adapter->connect());

    // Zoom range: [0..2000]
    EXPECT_TRUE(m_adapter->setZoomNormalized(0.0));
    EXPECT_EQ(m_mockDriver->currentZoom().value(), 0U);

    EXPECT_TRUE(m_adapter->setZoomNormalized(0.5));
    EXPECT_EQ(m_mockDriver->currentZoom().value(), 1000U);

    EXPECT_TRUE(m_adapter->setZoomNormalized(1.0));
    EXPECT_EQ(m_mockDriver->currentZoom().value(), 2000U);

    // Below 0.0 or above 1.0 clamped
    EXPECT_TRUE(m_adapter->setZoomNormalized(-0.5));
    EXPECT_EQ(m_mockDriver->currentZoom().value(), 0U);

    EXPECT_TRUE(m_adapter->setZoomNormalized(1.5));
    EXPECT_EQ(m_mockDriver->currentZoom().value(), 2000U);
}

TEST_F(LensPayloadAdapterTest, NormalizedFocusDirectPositioning)
{
    EXPECT_TRUE(m_adapter->connect());

    // Focus range: [0..1000]
    EXPECT_TRUE(m_adapter->setFocusNormalized(0.0));
    EXPECT_EQ(m_mockDriver->currentFocus().value(), 0U);

    EXPECT_TRUE(m_adapter->setFocusNormalized(0.25));
    EXPECT_EQ(m_mockDriver->currentFocus().value(), 250U);

    EXPECT_TRUE(m_adapter->setFocusNormalized(1.0));
    EXPECT_EQ(m_mockDriver->currentFocus().value(), 1000U);
}

TEST_F(LensPayloadAdapterTest, ContinuousVelocityJog)
{
    EXPECT_TRUE(m_adapter->connect());

    // Continuous zoom tele at 50% velocity
    EXPECT_TRUE(m_adapter->zoomContinuous(+0.5F));
    EXPECT_EQ(m_mockDriver->lastZoomSpeed().value(), 128U);

    // Continuous zoom stop
    EXPECT_TRUE(m_adapter->zoomStop());
    EXPECT_TRUE(m_mockDriver->isMotionStopped());

    // Position focus at midpoint (500) away from NearLimit
    EXPECT_TRUE(m_adapter->setFocusNormalized(0.5));
    EXPECT_EQ(m_mockDriver->currentFocus().value(), 500U);

    // Continuous focus near at 100% velocity
    EXPECT_TRUE(m_adapter->focusContinuous(-1.0F));
    EXPECT_EQ(m_mockDriver->lastFocusSpeed().value(), 255U);

    // Continuous focus stop
    EXPECT_TRUE(m_adapter->focusStop());
    EXPECT_TRUE(m_mockDriver->isMotionStopped());
}

TEST_F(LensPayloadAdapterTest, OnePushFocusTrigger)
{
    EXPECT_TRUE(m_adapter->connect());
    EXPECT_TRUE(m_adapter->triggerOnePushFocus());
    EXPECT_TRUE(m_controller->isAutoFocusRunning(CameraIndex { 0U }));
}

TEST_F(LensPayloadAdapterTest, OpticalTelemetryAndFovCalculation)
{
    EXPECT_TRUE(m_adapter->connect());

    // Wide position: z = 0.0 -> f = 4.3 mm
    EXPECT_TRUE(m_adapter->setZoomNormalized(0.0));
    m_adapter->updateTelemetry();
    PayloadHal::CameraTelemetry telem = m_adapter->currentTelemetry();

    EXPECT_DOUBLE_EQ(telem.normalizedZoom, 0.0);
    EXPECT_DOUBLE_EQ(telem.opticalZoomFactor, 1.0);

    // Expected HFOV = 2 * arctan(5.568 / (2 * 4.3)) * 180 / pi ≈ 65.8 deg
    constexpr double kPi = 3.14159265358979323846;
    const double expectedWideHfov = 2.0 * std::atan(5.568 / (2.0 * 4.3)) * (180.0 / kPi);
    EXPECT_NEAR(telem.horizontalFovDeg, expectedWideHfov, 0.2);

    // Tele position: z = 1.0 -> f = 129.0 mm
    EXPECT_TRUE(m_adapter->setZoomNormalized(1.0));
    m_adapter->updateTelemetry();
    telem = m_adapter->currentTelemetry();

    EXPECT_DOUBLE_EQ(telem.normalizedZoom, 1.0);
    EXPECT_DOUBLE_EQ(telem.opticalZoomFactor, 30.0);

    // Expected HFOV = 2 * arctan(5.568 / (2 * 129.0)) * 180 / pi ≈ 2.47 deg
    const double expectedTeleHfov = 2.0 * std::atan(5.568 / (2.0 * 129.0)) * (180.0 / kPi);
    EXPECT_NEAR(telem.horizontalFovDeg, expectedTeleHfov, 0.1);
}

TEST_F(LensPayloadAdapterTest, TelemetryObserverCallback)
{
    EXPECT_TRUE(m_adapter->connect());

    bool callbackFired = false;
    double receivedNormZoom = -1.0;

    m_adapter->registerTelemetryCallback([&](const PayloadHal::CameraTelemetry& t) {
        callbackFired = true;
        receivedNormZoom = t.normalizedZoom;
    });

    EXPECT_TRUE(m_adapter->setZoomNormalized(0.75));
    m_adapter->updateTelemetry();

    EXPECT_TRUE(callbackFired);
    EXPECT_NEAR(receivedNormZoom, 0.75, 0.01);
}

TEST_F(LensPayloadAdapterTest, StreamUriManagement)
{
    EXPECT_TRUE(m_adapter->setVideoStreamUri("rtsp://192.168.1.100:554/ch0", PayloadHal::VideoStreamProfile::Primary));
    EXPECT_EQ(m_adapter->videoStreamUri(PayloadHal::VideoStreamProfile::Primary), "rtsp://192.168.1.100:554/ch0");
    EXPECT_TRUE(m_adapter->videoStreamUri(PayloadHal::VideoStreamProfile::Secondary).empty());

    const auto streams = m_adapter->availableStreams();
    ASSERT_EQ(streams.size(), 1U);
    EXPECT_EQ(streams[0].uri, "rtsp://192.168.1.100:554/ch0");
    EXPECT_EQ(streams[0].profile, PayloadHal::VideoStreamProfile::Primary);
}

TEST_F(LensPayloadAdapterTest, IrisAndEnhancementControls)
{
    EXPECT_TRUE(m_adapter->connect());

    EXPECT_TRUE(m_adapter->setIrisAuto(true));
    EXPECT_TRUE(m_adapter->setIrisNormalized(0.8));
    EXPECT_TRUE(m_adapter->setDayNightIcr(true));
    EXPECT_TRUE(m_adapter->setDefog(true));
    EXPECT_TRUE(m_adapter->setStabilizer(true));

    const auto telem = m_adapter->currentTelemetry();
    EXPECT_TRUE(telem.autoIrisActive);
    EXPECT_DOUBLE_EQ(telem.irisNormalized, 0.8);
    EXPECT_TRUE(telem.dayNightIcrActive);
}
