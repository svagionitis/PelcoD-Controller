#include "IPtzPresetManager.h"
#include "LocalPresetManager.h"
#include "PayloadHal.h"
#include "sim/SimulatedPayload.h"
#include <gtest/gtest.h>

namespace PayloadHal {
namespace {

TEST(TestPtzPresetManager, BasicPresetSaveAndQuery)
{
    auto simPayload = std::dynamic_pointer_cast<SimulatedPayload>(PayloadFactory::createSimulatedPayload());
    ASSERT_NE(simPayload, nullptr);

    auto ptu = simPayload->panTilt();
    auto cam = simPayload->primaryCamera();
    ASSERT_NE(ptu, nullptr);
    ASSERT_NE(cam, nullptr);

    LocalPresetManager mgr(ptu, cam);

    EXPECT_TRUE(mgr.listPresets().empty());
    EXPECT_FALSE(mgr.getPreset(1).has_value());

    // Save Preset 1
    PtzPreset p1 {};
    p1.id = 1;
    p1.name = "North Gate";
    p1.panAngleDeg = 45.0;
    p1.tiltAngleDeg = -10.0;
    p1.opticalZoomFactor = 10.0;
    p1.focusDistanceNormalized = 0.5;

    EXPECT_TRUE(mgr.savePreset(p1));

    auto fetched = mgr.getPreset(1);
    ASSERT_TRUE(fetched.has_value());
    EXPECT_EQ(fetched->id, 1U);
    EXPECT_EQ(fetched->name, "North Gate");
    EXPECT_DOUBLE_EQ(fetched->panAngleDeg, 45.0);
    EXPECT_DOUBLE_EQ(fetched->tiltAngleDeg, -10.0);
    EXPECT_DOUBLE_EQ(fetched->opticalZoomFactor, 10.0);
    EXPECT_DOUBLE_EQ(fetched->focusDistanceNormalized, 0.5);

    // Reject ID 0
    PtzPreset p0 = p1;
    p0.id = 0;
    EXPECT_FALSE(mgr.savePreset(p0));
}

TEST(TestPtzPresetManager, SaveCurrentPositionCapture)
{
    auto simPayload = std::dynamic_pointer_cast<SimulatedPayload>(PayloadFactory::createSimulatedPayload());
    ASSERT_NE(simPayload, nullptr);

    auto ptu = simPayload->panTilt();
    auto cam = simPayload->primaryCamera();
    ASSERT_NE(ptu, nullptr);
    ASSERT_NE(cam, nullptr);

    LocalPresetManager mgr(ptu, cam);

    // Position PTU and camera
    EXPECT_TRUE(ptu->setAbsoluteAngles(120.0, -15.0));
    EXPECT_TRUE(cam->setZoomNormalized(0.5));
    EXPECT_TRUE(cam->setFocusNormalized(0.75));

    // Capture position into Preset 5
    EXPECT_TRUE(mgr.saveCurrentPosition(5, "Guard Tower"));

    auto p5 = mgr.getPreset(5);
    ASSERT_TRUE(p5.has_value());
    EXPECT_EQ(p5->id, 5U);
    EXPECT_EQ(p5->name, "Guard Tower");
    EXPECT_NEAR(p5->panAngleDeg, 120.0, 0.01);
    EXPECT_NEAR(p5->tiltAngleDeg, -15.0, 0.01);
    EXPECT_NEAR(p5->focusDistanceNormalized, 0.75, 0.01);

    // Reject capture with ID 0
    EXPECT_FALSE(mgr.saveCurrentPosition(0));
}

TEST(TestPtzPresetManager, RecallPresetDispatchesToHardware)
{
    auto simPayload = std::dynamic_pointer_cast<SimulatedPayload>(PayloadFactory::createSimulatedPayload());
    ASSERT_NE(simPayload, nullptr);

    auto ptu = simPayload->panTilt();
    auto cam = simPayload->primaryCamera();
    ASSERT_NE(ptu, nullptr);
    ASSERT_NE(cam, nullptr);

    LocalPresetManager mgr(ptu, cam);

    // Setup Preset 2
    PtzPreset p2 {};
    p2.id = 2;
    p2.name = "Perimeter West";
    p2.panAngleDeg = 270.0;
    p2.tiltAngleDeg = -20.0;
    p2.opticalZoomFactor = 20.0; // zoom ~ (20-1)/39 ~ 0.487
    p2.focusDistanceNormalized = 0.6;
    EXPECT_TRUE(mgr.savePreset(p2));

    // Set hardware to different coordinates first
    EXPECT_TRUE(ptu->setAbsoluteAngles(0.0, 0.0));
    EXPECT_TRUE(cam->setZoomNormalized(0.0));

    // Recall preset 2
    EXPECT_TRUE(mgr.recallPreset(2));

    const auto ptuTelem = ptu->currentTelemetry();
    EXPECT_NEAR(ptuTelem.panAngleDeg, 270.0, 0.01);
    EXPECT_NEAR(ptuTelem.tiltAngleDeg, -20.0, 0.01);

    const auto camTelem = cam->currentTelemetry();
    EXPECT_NEAR(camTelem.focusDistanceNormalized, 0.6, 0.01);

    // Recall non-existent preset returns false
    EXPECT_FALSE(mgr.recallPreset(999));
}

TEST(TestPtzPresetManager, ClearPresetAndHomeManagement)
{
    LocalPresetManager mgr(nullptr, nullptr);

    PtzPreset p1 {};
    p1.id = 1;
    p1.name = "Preset 1";
    EXPECT_TRUE(mgr.savePreset(p1));

    PtzPreset p2 {};
    p2.id = 2;
    p2.name = "Preset 2";
    EXPECT_TRUE(mgr.savePreset(p2));

    EXPECT_EQ(mgr.listPresets().size(), 2U);

    // Home preset management
    EXPECT_FALSE(mgr.getHomePresetId().has_value());
    mgr.setHomePresetId(1);
    ASSERT_TRUE(mgr.getHomePresetId().has_value());
    EXPECT_EQ(*mgr.getHomePresetId(), 1U);

    // Clearing preset 1 should also reset homePresetId
    EXPECT_TRUE(mgr.clearPreset(1));
    EXPECT_FALSE(mgr.getPreset(1).has_value());
    EXPECT_FALSE(mgr.getHomePresetId().has_value());
    EXPECT_EQ(mgr.listPresets().size(), 1U);

    // Clear non-existent preset returns false
    EXPECT_FALSE(mgr.clearPreset(999));
}

TEST(TestPtzPresetManager, GoHome)
{
    auto simPayload = std::dynamic_pointer_cast<SimulatedPayload>(PayloadFactory::createSimulatedPayload());
    ASSERT_NE(simPayload, nullptr);

    auto ptu = simPayload->panTilt();
    ASSERT_NE(ptu, nullptr);

    LocalPresetManager mgr(ptu, nullptr);

    // goHome fails if no home configured
    EXPECT_FALSE(mgr.goHome());

    PtzPreset home {};
    home.id = 10;
    home.name = "Home Base";
    home.panAngleDeg = 0.0;
    home.tiltAngleDeg = 0.0;
    EXPECT_TRUE(mgr.savePreset(home));
    mgr.setHomePresetId(10);

    // Move away
    EXPECT_TRUE(ptu->setAbsoluteAngles(50.0, 30.0));

    // Go Home
    EXPECT_TRUE(mgr.goHome());
    const auto telem = ptu->currentTelemetry();
    EXPECT_NEAR(telem.panAngleDeg, 0.0, 0.01);
    EXPECT_NEAR(telem.tiltAngleDeg, 0.0, 0.01);
}

TEST(TestPtzPresetManager, SimulatedPayloadIntegration)
{
    auto payload = PayloadFactory::createSimulatedPayload();
    ASSERT_NE(payload, nullptr);

    auto presetMgr = payload->presetManager();
    ASSERT_NE(presetMgr, nullptr);

    auto tourEngine = payload->tourEngine();
    ASSERT_NE(tourEngine, nullptr);

    // Save and recall through payload-integrated manager
    PtzPreset p {};
    p.id = 1;
    p.panAngleDeg = 30.0;
    p.tiltAngleDeg = -5.0;
    EXPECT_TRUE(presetMgr->savePreset(p));
    EXPECT_TRUE(presetMgr->recallPreset(1));

    const auto ptuTelem = payload->panTilt()->currentTelemetry();
    EXPECT_NEAR(ptuTelem.panAngleDeg, 30.0, 0.01);
    EXPECT_NEAR(ptuTelem.tiltAngleDeg, -5.0, 0.01);
}

} // namespace
} // namespace PayloadHal
