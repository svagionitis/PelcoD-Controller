#include "Klv/KlvTypes.h"
#include "MockPelcoDDevice.h"
#include "MockSonyCamera.h"
#include "PayloadFactory.h"
#include "PelcoDCore/PelcoDDevice.h"
#include "SonyFCBDevice.h"
#include "adapters/PelcoDPtzAdapter.h"
#include "adapters/PelcoDViscaCompositePayload.h"
#include "adapters/ViscaSonyAdapter.h"
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

namespace PayloadHal {
namespace {

    using namespace Visca;
    using namespace Visca::Sony;

    TEST(TestPelcoDViscaAdapter, LifecycleAndStateAggregation)
    {
        auto mockPtzTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
        auto pelcoDevice = std::make_shared<PelcoD::PelcoDDevice>(mockPtzTransport, 1U);

        auto mockCameraTransport = std::make_shared<MockSonyCamera>(SonyCameraModelType::FCB_EV9520L, 1);
        mockCameraTransport->open();
        auto sonyDevice = std::make_shared<SonyFCBDevice>(mockCameraTransport, 1);

        auto composite = PayloadFactory::createPelcoDViscaPayload(pelcoDevice, sonyDevice);
        ASSERT_NE(composite, nullptr);

        EXPECT_NE(composite->panTilt(), nullptr);
        EXPECT_NE(composite->primaryCamera(), nullptr);
        EXPECT_EQ(composite->secondaryCamera(), nullptr);
        EXPECT_EQ(composite->lrf(), nullptr);

        const DeviceInfo info = composite->info();
        EXPECT_FALSE(info.manufacturer.empty());
        EXPECT_FALSE(info.model.empty());

        std::atomic<int> callbackCount { 0 };
        std::atomic<DeviceState> lastObservedState { DeviceState::Disconnected };
        composite->registerStateCallback([&](DeviceState newState, const std::string& /*reason*/) {
            lastObservedState = newState;
            callbackCount++;
        });

        EXPECT_FALSE(composite->isConnected());
        EXPECT_EQ(composite->state(), DeviceState::Disconnected);

        ASSERT_TRUE(composite->connect());
        EXPECT_TRUE(composite->isConnected());
        EXPECT_EQ(composite->state(), DeviceState::Ready);
        EXPECT_GT(callbackCount.load(), 0);

        composite->disconnect();
        EXPECT_FALSE(composite->isConnected());
        EXPECT_EQ(composite->state(), DeviceState::Disconnected);
    }

    TEST(TestPelcoDViscaAdapter, DirectAdapterInjectionAndAccessors)
    {
        auto mockPtzTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
        auto pelcoDevice = std::make_shared<PelcoD::PelcoDDevice>(mockPtzTransport, 1U);
        auto ptu = std::make_shared<PelcoDPtzAdapter>(pelcoDevice);

        auto mockCameraTransport = std::make_shared<MockSonyCamera>(SonyCameraModelType::FCB_EV9520L, 1);
        mockCameraTransport->open();
        auto sonyDevice = std::make_shared<SonyFCBDevice>(mockCameraTransport, 1);
        auto camera = std::make_shared<ViscaSonyAdapter>(sonyDevice);

        PelcoDViscaCompositePayload composite(ptu, camera);

        EXPECT_EQ(composite.panTilt(), ptu);
        EXPECT_EQ(composite.primaryCamera(), camera);
        EXPECT_EQ(composite.underlyingPelcoDevice(), nullptr);
        EXPECT_EQ(composite.underlyingSonyDevice(), nullptr);

        ASSERT_TRUE(composite.connect());
        EXPECT_TRUE(composite.isConnected());
        EXPECT_EQ(composite.state(), DeviceState::Ready);

        composite.disconnect();
        EXPECT_FALSE(composite.isConnected());
    }

    TEST(TestPelcoDViscaAdapter, MotionOpticsAndGeoreferencing)
    {
        auto mockPtzTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
        auto pelcoDevice = std::make_shared<PelcoD::PelcoDDevice>(mockPtzTransport, 1U);

        auto mockCameraTransport = std::make_shared<MockSonyCamera>(SonyCameraModelType::FCB_EV9520L, 1);
        mockCameraTransport->open();
        auto sonyDevice = std::make_shared<SonyFCBDevice>(mockCameraTransport, 1);

        PelcoDViscaCompositePayload composite(pelcoDevice, sonyDevice);
        ASSERT_TRUE(composite.connect());

        // Test optical zoom and autofocus coordination
        auto cam = composite.primaryCamera();
        ASSERT_NE(cam, nullptr);
        EXPECT_TRUE(cam->setZoomNormalized(0.5));
        EXPECT_TRUE(cam->zoomStop());
        EXPECT_TRUE(cam->setFocusAuto(true));
        const CameraTelemetry camTelem = cam->currentTelemetry();
        EXPECT_GE(camTelem.opticalZoomFactor, 1.0);

        // Aim south (pan 180°), down 30°
        auto ptu = composite.panTilt();
        ASSERT_NE(ptu, nullptr);
        EXPECT_TRUE(ptu->setAbsoluteAngles(180.0, -30.0));
        const GimbalTelemetry gimbalTelem = ptu->currentTelemetry();
        EXPECT_DOUBLE_EQ(gimbalTelem.panAngleDeg, 180.0);
        EXPECT_DOUBLE_EQ(gimbalTelem.tiltAngleDeg, -30.0);

        // Target georeferencing projection:
        // Platform at (38.0, 24.0, 500m MSL), heading 0°, aiming South (pan 180°), down 30°
        const Klv::GeoPoint2D platformGps { 38.0, 24.0 };
        auto target = composite.calculateTargetCoordinates(platformGps, 0.0, 500.0);
        ASSERT_TRUE(target.has_value());
        EXPECT_LT(target->latitudeDeg, 38.0);
        EXPECT_NEAR(target->longitudeDeg, 24.0, 1e-4);

        // Nadir / horizon test: aiming straight up (+10°) should fail intersection
        EXPECT_TRUE(ptu->setAbsoluteAngles(0.0, 10.0));
        auto invalidTarget = composite.calculateTargetCoordinates(platformGps, 0.0, 500.0);
        EXPECT_FALSE(invalidTarget.has_value());

        composite.disconnect();
    }

} // namespace
} // namespace PayloadHal
