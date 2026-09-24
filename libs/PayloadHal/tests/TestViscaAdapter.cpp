#include "adapters/ViscaSonyAdapter.h"
#include "MockSonyCamera.h"
#include "SonyFCBDevice.h"
#include <gtest/gtest.h>

namespace PayloadHal {
namespace {

using namespace Visca;
using namespace Visca::Sony;

TEST(TestViscaAdapter, LifecycleAndOpticsMapping) {
    auto mockCamera = std::make_shared<MockSonyCamera>(SonyCameraModelType::FCB_EV9520L, 1);
    mockCamera->open();

    auto fcbDevice = std::make_shared<SonyFCBDevice>(mockCamera, 1);
    ViscaSonyAdapter adapter(fcbDevice);

    ASSERT_TRUE(adapter.connect());
    EXPECT_TRUE(adapter.isConnected());
    EXPECT_EQ(adapter.state(), DeviceState::Ready);
    EXPECT_EQ(adapter.spectrum(), CameraSpectrum::DaylightVisible);

    DeviceInfo d = adapter.info();
    EXPECT_EQ(d.manufacturer, "Sony");
    EXPECT_EQ(d.model, "FCB-EV9520L");

    // Direct normalized zoom [0.0, 1.0] -> 0x0000 to 0x4000
    EXPECT_TRUE(adapter.setZoomNormalized(0.5));
    // Continuous zoom
    EXPECT_TRUE(adapter.zoomContinuous(0.7f));
    EXPECT_TRUE(adapter.zoomContinuous(-0.7f));
    EXPECT_TRUE(adapter.zoomStop());

    // Autofocus & manual focus
    EXPECT_TRUE(adapter.setFocusAuto(true));
    EXPECT_TRUE(adapter.setFocusAuto(false));
    EXPECT_TRUE(adapter.setFocusNormalized(0.8));
    EXPECT_TRUE(adapter.triggerOnePushFocus());

    // ICR Day/Night, Defog, Image Stabilizer
    EXPECT_TRUE(adapter.setDayNightIcr(true));
    EXPECT_TRUE(adapter.setDefog(true));
    EXPECT_TRUE(adapter.setStabilizer(true));

    adapter.disconnect();
    EXPECT_FALSE(adapter.isConnected());
}

} // namespace
} // namespace PayloadHal
