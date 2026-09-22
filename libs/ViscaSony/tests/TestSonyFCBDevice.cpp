/// @file TestSonyFCBDevice.cpp
/// @brief Unit tests for SonyFCBDevice camera abstraction and model capability gating.

#include "MockSonyCamera.h"
#include "SonyFCBDevice.h"
#include <gtest/gtest.h>

using namespace Visca;
using namespace Visca::Sony;

/// @brief Tests automatic model detection and capability initialization for FCB-EV9520L.
/// @details Verifies that initialize() queries CAM_VersionInq and sets EV9520L traits.
TEST(TestSonyFCBDevice, InitializationEV9520L)
{
    auto mockCamera = std::make_shared<MockSonyCamera>(SonyCameraModelType::FCB_EV9520L, 1);
    mockCamera->open();

    SonyFCBDevice camera(mockCamera, 1);
    EXPECT_TRUE(camera.initialize());
    EXPECT_EQ(camera.modelType(), SonyCameraModelType::FCB_EV9520L);
    EXPECT_TRUE(camera.capabilities().supportsDistortionCompensation);
    EXPECT_FALSE(camera.capabilities().supports4K);
}

/// @brief Tests automatic model detection and capability initialization for FCB-EW9500H.
/// @details Verifies that initialize() queries CAM_VersionInq and sets EW9500H traits.
TEST(TestSonyFCBDevice, InitializationEW9500H)
{
    auto mockCamera = std::make_shared<MockSonyCamera>(SonyCameraModelType::FCB_EW9500H, 1);
    mockCamera->open();

    SonyFCBDevice camera(mockCamera, 1);
    EXPECT_TRUE(camera.initialize());
    EXPECT_EQ(camera.modelType(), SonyCameraModelType::FCB_EW9500H);
    EXPECT_FALSE(camera.capabilities().supportsDistortionCompensation);
    EXPECT_TRUE(camera.capabilities().supports4K);
}

/// @brief Tests capability gating for hardware-specific commands.
/// @details Verifies that Distortion Compensation is accepted by EV9520L and rejected by EW9500H,
/// and 4K Operating Mode is accepted by EW9500H and rejected by EV9520L.
TEST(TestSonyFCBDevice, CapabilityGating)
{
    auto mockEV = std::make_shared<MockSonyCamera>(SonyCameraModelType::FCB_EV9520L, 1);
    mockEV->open();
    SonyFCBDevice evCamera(mockEV, 1);
    ASSERT_TRUE(evCamera.initialize());

    auto mockEW = std::make_shared<MockSonyCamera>(SonyCameraModelType::FCB_EW9500H, 1);
    mockEW->open();
    SonyFCBDevice ewCamera(mockEW, 1);
    ASSERT_TRUE(ewCamera.initialize());

    // Distortion compensation: Supported on EV9520L, forbidden on EW9500H
    EXPECT_TRUE(evCamera.setDistortionCompensation(true));
    EXPECT_FALSE(ewCamera.setDistortionCompensation(true));

    // Optical Axis Gap compensation: Supported on EV9520L, forbidden on EW9500H
    EXPECT_TRUE(evCamera.setOpticalAxisGapCompensation(true));
    EXPECT_FALSE(ewCamera.setOpticalAxisGapCompensation(true));

    // 4K Video Mode (0x25 = 2160p30): Supported on EW9500H, forbidden on EV9520L
    EXPECT_FALSE(evCamera.setOperatingMode(0x25));
    EXPECT_TRUE(ewCamera.setOperatingMode(0x25));

    // TMDS Digital Output Mode: Supported on EW9500H, forbidden on EV9520L
    EXPECT_FALSE(evCamera.setDigitalOutputMode(0x01));
    EXPECT_TRUE(ewCamera.setDigitalOutputMode(0x01));

    // LVDS Serial Output Mode: Supported on EV9520L, forbidden on EW9500H
    EXPECT_TRUE(evCamera.setLvdsMode(0x01));
    EXPECT_FALSE(ewCamera.setLvdsMode(0x01));
}

/// @brief Tests lens, exposure, and enhancement controls.
/// @details Verifies setting zoom, focus, shutter, iris, and stabilizer.
TEST(TestSonyFCBDevice, HighLevelControls)
{
    auto mockCamera = std::make_shared<MockSonyCamera>(SonyCameraModelType::FCB_EV9520L, 1);
    mockCamera->open();

    SonyFCBDevice camera(mockCamera, 1);
    ASSERT_TRUE(camera.initialize());

    EXPECT_TRUE(camera.setZoomDirect(0x3000));
    EXPECT_EQ(mockCamera->zoomPosition(), 0x3000);

    EXPECT_TRUE(camera.setFocusDirect(0x6000));
    EXPECT_EQ(mockCamera->focusPosition(), 0x6000);

    EXPECT_TRUE(camera.setExposureMode(SonyExposureMode::Manual));
    EXPECT_EQ(mockCamera->exposureMode(), SonyExposureMode::Manual);

    EXPECT_TRUE(camera.setStabilizer(SonyStabilizerMode::SuperPlus));
    EXPECT_EQ(mockCamera->stabilizerMode(), SonyStabilizerMode::SuperPlus);

    EXPECT_TRUE(camera.setDefog(SonyDefogMode::High));
    EXPECT_EQ(mockCamera->defogMode(), SonyDefogMode::High);
}

/// @brief Tests Block Inquiry polling to update camera telemetry status.
/// @details Verifies that pollStatus() queries Block Inquiries 00..04 and populates SonyFCBStatus.
TEST(TestSonyFCBDevice, BlockInquiryStatusPolling)
{
    auto mockCamera = std::make_shared<MockSonyCamera>(SonyCameraModelType::FCB_EV9520L, 1);
    mockCamera->open();

    mockCamera->setZoomPosition(0x2500);
    mockCamera->setFocusPosition(0x4500);

    SonyFCBDevice camera(mockCamera, 1);
    ASSERT_TRUE(camera.initialize());

    EXPECT_TRUE(camera.pollStatus());

    const SonyFCBStatus status = camera.status();
    EXPECT_EQ(status.zoomPosition, 0x2500);
    EXPECT_EQ(status.focusPosition, 0x4500);
    EXPECT_TRUE(status.focusAuto);
    EXPECT_TRUE(status.powerOn);
}
