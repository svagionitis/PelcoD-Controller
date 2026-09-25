#include "PayloadFactory.h"
#include "adapters/OnvifPayloadAdapter.h"
#include <Onvif/OnvifClient.h>
#include <Onvif/OnvifServer.h>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace PayloadHal {
namespace {

    class MockPtzHandler : public Onvif::IPtzHandler {
    public:
        void handleContinuousMove(float panSpeed, float tiltSpeed, float zoomSpeed) override
        {
            lastPanSpeed = panSpeed;
            lastTiltSpeed = tiltSpeed;
            lastZoomSpeed = zoomSpeed;
            continuousMoveCount++;
        }

        void handleAbsoluteMove(float pan, float tilt, float zoom) override
        {
            lastAbsPan = pan;
            lastAbsTilt = tilt;
            lastAbsZoom = zoom;
            absoluteMoveCount++;
        }

        void handleStop(bool stopPanTilt, bool stopZoom) override
        {
            lastStopPanTilt = stopPanTilt;
            lastStopZoom = stopZoom;
            stopCount++;
        }

        std::string handleSetPreset(const std::string& name, const std::string& token) override
        {
            const std::string tok = token.empty() ? std::to_string(presets.size() + 1) : token;
            presets.push_back({ tok, name });
            return tok;
        }

        bool handleGotoPreset(const std::string& token) override
        {
            lastGotoToken = token;
            return true;
        }

        bool handleRemovePreset(const std::string& token) override
        {
            lastRemoveToken = token;
            return true;
        }

        std::vector<Onvif::PtzPreset> handleGetPresets() override
        {
            return presets;
        }

        Onvif::PtzStatus handleGetStatus() override
        {
            Onvif::PtzStatus st {};
            st.pan = currentPan;
            st.tilt = currentTilt;
            st.zoom = currentZoom;
            st.isMoving = isMoving;
            return st;
        }

        float lastPanSpeed { 0.0f };
        float lastTiltSpeed { 0.0f };
        float lastZoomSpeed { 0.0f };
        int continuousMoveCount { 0 };

        float lastAbsPan { 0.0f };
        float lastAbsTilt { 0.0f };
        float lastAbsZoom { 0.0f };
        int absoluteMoveCount { 0 };

        bool lastStopPanTilt { false };
        bool lastStopZoom { false };
        int stopCount { 0 };

        std::string lastGotoToken {};
        std::string lastRemoveToken {};
        std::vector<Onvif::PtzPreset> presets {};

        float currentPan { 120.0f };
        float currentTilt { -30.0f };
        float currentZoom { 0.4f };
        bool isMoving { false };
    };

    class MockImagingHandler : public Onvif::IImagingHandler {
    public:
        Onvif::ImagingSettings handleGetImagingSettings(const std::string& /*videoSourceToken*/) override
        {
            return settings;
        }

        bool handleSetImagingSettings(
            const std::string& /*videoSourceToken*/, const Onvif::ImagingSettings& newSettings) override
        {
            settings = newSettings;
            setSettingsCount++;
            return true;
        }

        void handleMoveFocus(const std::string& /*videoSourceToken*/, float speed) override
        {
            lastFocusSpeed = speed;
            moveFocusCount++;
        }

        void handleStopFocus(const std::string& /*videoSourceToken*/) override
        {
            stopFocusCount++;
        }

        Onvif::ImagingSettings settings {};
        int setSettingsCount { 0 };
        float lastFocusSpeed { 0.0f };
        int moveFocusCount { 0 };
        int stopFocusCount { 0 };
    };

    class OnvifAdapterFixture : public ::testing::Test {
    protected:
        static constexpr int kTestPort = 18965;

        void SetUp() override
        {
            Onvif::OnvifServerConfig config {};
            config.bindAddress = "127.0.0.1";
            config.port = kTestPort;
            config.deviceName = "HAL Test Camera";
            config.manufacturer = "PelcoDHAL";
            config.model = "ONVIF-PTZ-PRO";
            config.firmwareVersion = "2.4.0";
            config.serialNumber = "SN-ONVIF-12345";
            config.rtspStreamUri = "rtsp://127.0.0.1:8554/live.sdp";

            m_ptzHandler = std::make_shared<MockPtzHandler>();
            m_imagingHandler = std::make_shared<MockImagingHandler>();

            m_server = std::make_unique<Onvif::OnvifServer>(config, m_ptzHandler, m_imagingHandler);
            ASSERT_TRUE(m_server->start());
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            const std::string endpoint = "http://127.0.0.1:" + std::to_string(kTestPort) + "/onvif/device_service";
            Onvif::SecurityCredentials creds {};
            creds.username = "admin";
            creds.password = "secretPass";
            m_client = std::make_shared<Onvif::OnvifClient>(endpoint, creds);
            m_client->setTimeout(std::chrono::milliseconds(1000));
        }

        void TearDown() override
        {
            if (m_server) {
                m_server->stop();
            }
        }

        std::shared_ptr<MockPtzHandler> m_ptzHandler;
        std::shared_ptr<MockImagingHandler> m_imagingHandler;
        std::unique_ptr<Onvif::OnvifServer> m_server;
        std::shared_ptr<Onvif::OnvifClient> m_client;
    };

    TEST_F(OnvifAdapterFixture, PtuAdapterControlsAndTelemetry)
    {
        OnvifPtuAdapter ptu(m_client);

        EXPECT_FALSE(ptu.isConnected());
        EXPECT_EQ(ptu.state(), DeviceState::Disconnected);

        ASSERT_TRUE(ptu.connect());
        EXPECT_TRUE(ptu.isConnected());
        EXPECT_EQ(ptu.state(), DeviceState::Ready);

        const DeviceInfo d = ptu.info();
        EXPECT_EQ(d.manufacturer, "PelcoDHAL");
        EXPECT_EQ(d.model, "ONVIF-PTZ-PRO");

        // Continuous velocity motion
        EXPECT_TRUE(ptu.setNormalizedVelocity(0.5f, -0.25f));
        EXPECT_NEAR(m_ptzHandler->lastPanSpeed, 0.5f, 0.01f);
        EXPECT_NEAR(m_ptzHandler->lastTiltSpeed, -0.25f, 0.01f);

        // Stop motion
        EXPECT_TRUE(ptu.stopMotion());
        EXPECT_TRUE(m_ptzHandler->lastStopPanTilt);

        // Absolute spherical positioning
        EXPECT_TRUE(ptu.setAbsoluteAngles(90.0, 15.0));

        // Preset management
        EXPECT_TRUE(ptu.savePreset(1, "Gate1"));
        EXPECT_TRUE(ptu.recallPreset(1));
        EXPECT_EQ(m_ptzHandler->lastGotoToken, "1");

        // Telemetry interrogation
        ptu.updateTelemetry();
        const GimbalTelemetry telem = ptu.currentTelemetry();
        EXPECT_NEAR(telem.panAngleDeg, 120.0, 0.1);
        EXPECT_NEAR(telem.tiltAngleDeg, -30.0, 0.1);
        EXPECT_FALSE(telem.isMoving);

        // PTU capabilities
        EXPECT_FALSE(ptu.supportsStabilization());
        EXPECT_FALSE(ptu.setStabilizationMode(StabilizationMode::RateStabilized));
        EXPECT_EQ(ptu.stabilizationMode(), StabilizationMode::Disabled);

        ptu.disconnect();
        EXPECT_FALSE(ptu.isConnected());
        EXPECT_EQ(ptu.state(), DeviceState::Disconnected);
    }

    TEST_F(OnvifAdapterFixture, CameraAdapterOpticsAndImaging)
    {
        OnvifCameraAdapter camera(m_client);

        ASSERT_TRUE(camera.connect());
        EXPECT_TRUE(camera.isConnected());
        EXPECT_EQ(camera.spectrum(), CameraSpectrum::DaylightVisible);

        // Video stream URI resolution
        const std::string uri = camera.videoStreamUri();
        EXPECT_EQ(uri, "rtsp://admin:secretPass@127.0.0.1:8554/live.sdp");

        // Continuous zoom
        EXPECT_TRUE(camera.zoomContinuous(0.7f));
        EXPECT_NEAR(m_ptzHandler->lastZoomSpeed, 0.7f, 0.01f);

        EXPECT_TRUE(camera.zoomContinuous(-0.5f));
        EXPECT_NEAR(m_ptzHandler->lastZoomSpeed, -0.5f, 0.01f);

        EXPECT_TRUE(camera.zoomStop());
        EXPECT_TRUE(m_ptzHandler->lastStopZoom);

        // Normalized zoom
        EXPECT_TRUE(camera.setZoomNormalized(0.5));
        const CameraTelemetry telemZoom = camera.currentTelemetry();
        EXPECT_NEAR(telemZoom.normalizedZoom, 0.5, 0.01);
        EXPECT_GT(telemZoom.opticalZoomFactor, 1.0);

        // Autofocus controls
        EXPECT_TRUE(camera.setFocusAuto(true));
        EXPECT_TRUE(camera.currentTelemetry().autoFocusActive);
        EXPECT_EQ(m_imagingHandler->settings.autoFocusMode, "AUTO");

        EXPECT_TRUE(camera.setFocusAuto(false));
        EXPECT_FALSE(camera.currentTelemetry().autoFocusActive);
        EXPECT_EQ(m_imagingHandler->settings.autoFocusMode, "MANUAL");

        // Continuous focus
        EXPECT_TRUE(camera.focusContinuous(0.8f));
        EXPECT_TRUE(camera.focusContinuous(-0.8f));
        EXPECT_TRUE(camera.focusStop());

        // Iris controls
        EXPECT_TRUE(camera.setIrisAuto(false));
        EXPECT_FALSE(camera.currentTelemetry().autoIrisActive);
        EXPECT_EQ(m_imagingHandler->settings.exposure.mode, "MANUAL");

        EXPECT_TRUE(camera.setIrisNormalized(0.7));
        EXPECT_NEAR(camera.currentTelemetry().irisNormalized, 0.7, 0.01);
        EXPECT_NEAR(m_imagingHandler->settings.exposure.iris, 70.0f, 0.1f);

        EXPECT_TRUE(camera.irisContinuous(0.5f));
        EXPECT_TRUE(camera.irisStop());

        EXPECT_TRUE(camera.setIrisAuto(true));
        EXPECT_TRUE(camera.currentTelemetry().autoIrisActive);
        EXPECT_EQ(m_imagingHandler->settings.exposure.mode, "AUTO");

        // Day/Night ICR filter
        EXPECT_TRUE(camera.setDayNightIcr(true));
        EXPECT_TRUE(camera.currentTelemetry().dayNightIcrActive);
        EXPECT_EQ(m_imagingHandler->settings.irCutFilter, "OFF");

        EXPECT_TRUE(camera.setDayNightIcr(false));
        EXPECT_FALSE(camera.currentTelemetry().dayNightIcrActive);
        EXPECT_EQ(m_imagingHandler->settings.irCutFilter, "ON");

        // Defog & Stabilizer
        EXPECT_TRUE(camera.setDefog(true));
        EXPECT_FALSE(camera.setStabilizer(true));

        camera.disconnect();
        EXPECT_FALSE(camera.isConnected());
    }

    TEST_F(OnvifAdapterFixture, CompositePayloadStationAndTargeting)
    {
        OnvifPayloadAdapter payload(m_client);

        ASSERT_TRUE(payload.connect());
        EXPECT_TRUE(payload.isConnected());
        EXPECT_EQ(payload.state(), DeviceState::Ready);

        // Subsystem accessors
        auto ptu = payload.panTilt();
        auto primaryCam = payload.primaryCamera();
        auto secondaryCam = payload.secondaryCamera();
        auto lrf = payload.lrf();

        ASSERT_NE(ptu, nullptr);
        ASSERT_NE(primaryCam, nullptr);
        EXPECT_EQ(secondaryCam, nullptr);
        EXPECT_EQ(lrf, nullptr);

        EXPECT_EQ(payload.videoStreamUri(), "rtsp://admin:secretPass@127.0.0.1:8554/live.sdp");

        // Configure PTU pointing forward (pan 0 deg) and depressed 45 degrees towards flat earth
        m_ptzHandler->currentPan = 0.0f;
        m_ptzHandler->currentTilt = -45.0f;
        auto onvifPtu = std::dynamic_pointer_cast<OnvifPtuAdapter>(ptu);
        ASSERT_NE(onvifPtu, nullptr);
        onvifPtu->updateTelemetry();

        const Klv::GeoPoint2D platformGps { 37.7749, -122.4194 };
        constexpr double platformHeadingDeg { 0.0 }; // True North
        constexpr double platformAltMeters { 100.0 }; // 100 m AGL

        const auto target = payload.calculateTargetCoordinates(platformGps, platformHeadingDeg, platformAltMeters);
        ASSERT_TRUE(target.has_value());
        // Pointing North, the target latitude should be north of platform
        EXPECT_GT(target->latitudeDeg, platformGps.latitudeDeg);
        // Longitude should remain approximately the same
        EXPECT_NEAR(target->longitudeDeg, platformGps.longitudeDeg, 0.001);

        payload.disconnect();
        EXPECT_FALSE(payload.isConnected());
    }

    TEST_F(OnvifAdapterFixture, FactoryCreationAndUriParsing)
    {
        // 1. Factory createFromUri with credentials and port
        const std::string validUri
            = "onvif://admin:secretPass@127.0.0.1:" + std::to_string(kTestPort) + "/onvif/device_service";
        auto payload = PayloadFactory::createFromUri(validUri);
        ASSERT_NE(payload, nullptr);

        ASSERT_TRUE(payload->connect());
        EXPECT_TRUE(payload->isConnected());
        EXPECT_NE(payload->panTilt(), nullptr);
        EXPECT_NE(payload->primaryCamera(), nullptr);
        payload->disconnect();

        // 2. Factory createFromUri with default path
        const std::string minimalUri = "onvif://127.0.0.1:" + std::to_string(kTestPort);
        auto payloadMinimal = PayloadFactory::createFromUri(minimalUri);
        ASSERT_NE(payloadMinimal, nullptr);

        // 3. Factory createFromUri invalid scheme
        auto invalidPayload = PayloadFactory::createFromUri("unknown://127.0.0.1:8000");
        EXPECT_EQ(invalidPayload, nullptr);

        // 4. Direct factory overload
        Onvif::SecurityCredentials creds { "admin", "secretPass" };
        auto directPayload = PayloadFactory::createOnvifPayload(
            "http://127.0.0.1:" + std::to_string(kTestPort) + "/onvif/device_service", creds);
        ASSERT_NE(directPayload, nullptr);
    }

} // namespace
} // namespace PayloadHal
