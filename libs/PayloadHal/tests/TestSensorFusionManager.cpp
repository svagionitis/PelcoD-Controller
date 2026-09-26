#include <gtest/gtest.h>

#include "PayloadHal.h"
#include "SensorFusionManager.h"
#include "sim/SimulatedPayload.h"

#include <cmath>
#include <memory>

using namespace PayloadHal;

TEST(TestSensorFusionManager, ChannelSelectionAndManualSwitch)
{
    auto sim = std::make_shared<SimulatedPayload>();
    SensorFusionManager mgr(sim->primaryCamera(), sim->secondaryCamera());

    EXPECT_EQ(mgr.activeChannel(), OpticalChannel::Primary);
    EXPECT_EQ(mgr.activeCamera(), sim->primaryCamera());

    bool callbackFired = false;
    OpticalChannel oldCh = OpticalChannel::Primary;
    OpticalChannel newCh = OpticalChannel::Primary;
    std::string switchReason;

    mgr.setChannelSwitchCallback([&](OpticalChannel prev, OpticalChannel curr, const std::string& reason) {
        callbackFired = true;
        oldCh = prev;
        newCh = curr;
        switchReason = reason;
    });

    EXPECT_TRUE(mgr.setActiveChannel(OpticalChannel::Secondary));
    EXPECT_TRUE(callbackFired);
    EXPECT_EQ(oldCh, OpticalChannel::Primary);
    EXPECT_EQ(newCh, OpticalChannel::Secondary);
    EXPECT_EQ(mgr.activeChannel(), OpticalChannel::Secondary);
    EXPECT_EQ(mgr.activeCamera(), sim->secondaryCamera());

    // Switching to same channel is a no-op returning true
    callbackFired = false;
    EXPECT_TRUE(mgr.setActiveChannel(OpticalChannel::Secondary));
    EXPECT_FALSE(callbackFired);
}

TEST(TestSensorFusionManager, MatchZoomDaylightToThermal)
{
    auto sim = std::make_shared<SimulatedPayload>();
    SensorFusionManager mgr(sim->primaryCamera(), sim->secondaryCamera());
    mgr.setMatchZoomEnabled(true);

    // Zoom daylight camera to 0.4 normalized
    sim->primaryCamera()->setZoomNormalized(0.4);
    const double daylightHfov = sim->primaryCamera()->currentTelemetry().horizontalFovDeg;

    // Compute match zoom
    const auto matchRes = mgr.computeMatchZoom(OpticalChannel::Primary, OpticalChannel::Secondary);
    EXPECT_NEAR(matchRes.achievedHfovDeg, daylightHfov, 0.1);
    EXPECT_NEAR(matchRes.digitalCropFactor, 1.0, 1e-3);
    EXPECT_FALSE(matchRes.isClamped);

    // Apply switch with match-zoom
    EXPECT_TRUE(mgr.setActiveChannel(OpticalChannel::Secondary, true));

    const double thermalHfov = sim->secondaryCamera()->currentTelemetry().horizontalFovDeg;
    EXPECT_NEAR(thermalHfov, daylightHfov, 0.1);
    EXPECT_NEAR(mgr.currentDigitalCropFactor(), 1.0, 1e-3);
}

TEST(TestSensorFusionManager, MatchZoomThermalToDaylight)
{
    auto sim = std::make_shared<SimulatedPayload>();
    SensorFusionManager mgr(sim->primaryCamera(), sim->secondaryCamera());

    // Thermal at wide view (zoom = 0.0)
    sim->secondaryCamera()->setZoomNormalized(0.0);
    const double thermalHfov = sim->secondaryCamera()->currentTelemetry().horizontalFovDeg;

    // Daylight currently zoomed in to 0.8
    sim->primaryCamera()->setZoomNormalized(0.8);
    EXPECT_LT(sim->primaryCamera()->currentTelemetry().horizontalFovDeg, thermalHfov);

    // Switch from Thermal back to Daylight with match zoom
    mgr.setActiveChannel(OpticalChannel::Secondary, false);
    EXPECT_TRUE(mgr.setActiveChannel(OpticalChannel::Primary, true));

    const double daylightHfov = sim->primaryCamera()->currentTelemetry().horizontalFovDeg;
    EXPECT_NEAR(daylightHfov, thermalHfov, 0.1);
}

TEST(TestSensorFusionManager, MatchZoomDigitalCropBeyondOpticalLimit)
{
    auto sim = std::make_shared<SimulatedPayload>();
    SensorFusionManager mgr(sim->primaryCamera(), sim->secondaryCamera());

    // Daylight at maximum optical tele (zoom = 1.0, HFOV ~ 2.2 deg)
    sim->primaryCamera()->setZoomNormalized(1.0);

    // If source HFOV is artificially narrow (e.g. 1.0 deg), optical tele cannot reach it alone
    // computeMatchZoom will set target optical zoom to 1.0 and calculate digitalCropFactor > 1.0
    const auto res = mgr.computeMatchZoom(OpticalChannel::Primary, OpticalChannel::Secondary);
    EXPECT_EQ(res.targetOpticalZoom01, 1.0);
    EXPECT_GE(res.digitalCropFactor, 1.0);
    EXPECT_FALSE(res.isClamped);
}

TEST(TestSensorFusionManager, OpticalPaletteSelection)
{
    auto sim = std::make_shared<SimulatedPayload>();
    SensorFusionManager mgr(sim->primaryCamera(), sim->secondaryCamera());

    EXPECT_EQ(mgr.palette(), OpticalPalette::DaylightColor);

    EXPECT_TRUE(mgr.setPalette(OpticalPalette::WhiteHot));
    EXPECT_EQ(mgr.palette(), OpticalPalette::WhiteHot);

    EXPECT_TRUE(mgr.setPalette(OpticalPalette::BlackHot));
    EXPECT_EQ(mgr.palette(), OpticalPalette::BlackHot);

    EXPECT_TRUE(mgr.setPalette(OpticalPalette::Ironbow));
    EXPECT_EQ(mgr.palette(), OpticalPalette::Ironbow);

    EXPECT_TRUE(mgr.setPalette(OpticalPalette::Rainbow));
    EXPECT_EQ(mgr.palette(), OpticalPalette::Rainbow);

    EXPECT_TRUE(mgr.setPalette(OpticalPalette::HazePenetration));
    EXPECT_EQ(mgr.palette(), OpticalPalette::HazePenetration);

    EXPECT_TRUE(mgr.setPalette(OpticalPalette::DaylightMonochrome));
    EXPECT_EQ(mgr.palette(), OpticalPalette::DaylightMonochrome);

    EXPECT_TRUE(mgr.setPalette(OpticalPalette::DaylightColor));
    EXPECT_EQ(mgr.palette(), OpticalPalette::DaylightColor);
}

TEST(TestSensorFusionManager, IsothermHighlightConfiguration)
{
    SensorFusionManager mgr;

    EXPECT_FALSE(mgr.isIsothermEnabled());

    mgr.setIsothermHighlight(true, 36.5, 41.0);
    EXPECT_TRUE(mgr.isIsothermEnabled());

    double minT = 0.0, maxT = 0.0;
    mgr.getIsothermRange(minT, maxT);
    EXPECT_NEAR(minT, 36.5, 1e-5);
    EXPECT_NEAR(maxT, 41.0, 1e-5);

    mgr.setIsothermHighlight(false);
    EXPECT_FALSE(mgr.isIsothermEnabled());
}

TEST(TestSensorFusionManager, EnvironmentalAutoSwitchDayToNight)
{
    auto sim = std::make_shared<SimulatedPayload>();
    SensorFusionManager mgr(sim->primaryCamera(), sim->secondaryCamera());

    AutoSwitchConfig cfg;
    cfg.enabled = true;
    cfg.lowLightThresholdLux = 1.5;
    cfg.daylightReturnThresholdLux = 5.0;
    cfg.hysteresisTimeSec = 2.0;
    cfg.cooldownTimeSec = 0.0;
    mgr.setAutoSwitchConfig(cfg);

    bool switchFired = false;
    mgr.setChannelSwitchCallback([&](OpticalChannel, OpticalChannel curr, const std::string&) {
        if (curr == OpticalChannel::Secondary) {
            switchFired = true;
        }
    });

    // Initial daytime lighting: 500 Lux
    EnvironmentalSceneMetrics metrics;
    metrics.ambientIlluminanceLux = 500.0;
    mgr.updateSceneMetrics(metrics);
    mgr.update(1.0);

    EXPECT_EQ(mgr.activeChannel(), OpticalChannel::Primary);
    EXPECT_FALSE(switchFired);

    // Nightfall occurs: ambient light drops to 0.5 Lux
    metrics.ambientIlluminanceLux = 0.5;
    mgr.updateSceneMetrics(metrics);

    // Time elapsed = 1.0s (less than 2.0s hysteresis)
    mgr.update(1.0);
    EXPECT_EQ(mgr.activeChannel(), OpticalChannel::Primary);
    EXPECT_FALSE(switchFired);

    // Another 1.1s elapsed (total 2.1s >= 2.0s hysteresis) -> auto-switch to Thermal!
    mgr.update(1.1);
    EXPECT_EQ(mgr.activeChannel(), OpticalChannel::Secondary);
    EXPECT_TRUE(switchFired);
}

TEST(TestSensorFusionManager, EnvironmentalAutoSwitchHysteresisRejection)
{
    auto sim = std::make_shared<SimulatedPayload>();
    SensorFusionManager mgr(sim->primaryCamera(), sim->secondaryCamera());

    AutoSwitchConfig cfg;
    cfg.enabled = true;
    cfg.lowLightThresholdLux = 1.5;
    cfg.hysteresisTimeSec = 2.0;
    mgr.setAutoSwitchConfig(cfg);

    EnvironmentalSceneMetrics metrics;
    metrics.ambientIlluminanceLux = 500.0;
    mgr.updateSceneMetrics(metrics);
    mgr.update(0.5);

    // Transient shadow / tunnel: light drops to 0.2 Lux for only 0.8s
    metrics.ambientIlluminanceLux = 0.2;
    mgr.updateSceneMetrics(metrics);
    mgr.update(0.8);
    EXPECT_EQ(mgr.activeChannel(), OpticalChannel::Primary);

    // Light restored before hysteresis elapsed
    metrics.ambientIlluminanceLux = 500.0;
    mgr.updateSceneMetrics(metrics);
    mgr.update(2.0);

    // Channel must remain Primary
    EXPECT_EQ(mgr.activeChannel(), OpticalChannel::Primary);
}

TEST(TestSensorFusionManager, EnvironmentalAutoSwitchNightToDayReturn)
{
    auto sim = std::make_shared<SimulatedPayload>();
    SensorFusionManager mgr(sim->primaryCamera(), sim->secondaryCamera());

    AutoSwitchConfig cfg;
    cfg.enabled = true;
    cfg.lowLightThresholdLux = 1.5;
    cfg.daylightReturnThresholdLux = 5.0;
    cfg.hysteresisTimeSec = 1.5;
    cfg.cooldownTimeSec = 0.0;
    mgr.setAutoSwitchConfig(cfg);

    // Start in Thermal night mode
    mgr.setActiveChannel(OpticalChannel::Secondary, false);
    EXPECT_EQ(mgr.activeChannel(), OpticalChannel::Secondary);

    EnvironmentalSceneMetrics metrics;
    metrics.ambientIlluminanceLux = 0.2; // Night
    mgr.updateSceneMetrics(metrics);
    mgr.update(1.0);
    EXPECT_EQ(mgr.activeChannel(), OpticalChannel::Secondary);

    // Dawn arrives: ambient light increases to 10.0 Lux (> 5.0 daylight return threshold)
    metrics.ambientIlluminanceLux = 10.0;
    mgr.updateSceneMetrics(metrics);

    mgr.update(1.0); // 1.0s < 1.5s
    EXPECT_EQ(mgr.activeChannel(), OpticalChannel::Secondary);

    mgr.update(0.6); // 1.6s >= 1.5s -> return to Daylight!
    EXPECT_EQ(mgr.activeChannel(), OpticalChannel::Primary);
}

TEST(TestSensorFusionManager, FusionLayoutAndPipConfiguration)
{
    SensorFusionManager mgr;

    EXPECT_EQ(mgr.fusionLayoutMode(), FusionLayoutMode::SingleChannel);

    mgr.setFusionLayoutMode(FusionLayoutMode::PictureInPicture);
    EXPECT_EQ(mgr.fusionLayoutMode(), FusionLayoutMode::PictureInPicture);

    mgr.setFusionLayoutMode(FusionLayoutMode::AlphaBlend);
    EXPECT_EQ(mgr.fusionLayoutMode(), FusionLayoutMode::AlphaBlend);

    mgr.setAlphaBlendWeight(0.75);
    EXPECT_NEAR(mgr.alphaBlendWeight(), 0.75, 1e-5);

    mgr.setAlphaBlendWeight(1.5); // Should clamp to 1.0
    EXPECT_NEAR(mgr.alphaBlendWeight(), 1.0, 1e-5);

    PipWindowConfig pip;
    pip.normalizedX = 0.65;
    pip.normalizedY = 0.10;
    pip.normalizedWidth = 0.30;
    pip.normalizedHeight = 0.20;
    pip.borderEnabled = false;

    mgr.setPipConfig(pip);
    const auto queryPip = mgr.pipConfig();
    EXPECT_NEAR(queryPip.normalizedX, 0.65, 1e-5);
    EXPECT_NEAR(queryPip.normalizedWidth, 0.30, 1e-5);
    EXPECT_FALSE(queryPip.borderEnabled);
}

TEST(TestSensorFusionManager, SimulatedPayloadIntegration)
{
    auto sim = std::make_shared<SimulatedPayload>();
    ASSERT_NE(sim->sensorFusion(), nullptr);

    auto fusion = sim->sensorFusion();
    EXPECT_EQ(fusion->activeChannel(), OpticalChannel::Primary);
    EXPECT_EQ(fusion->primaryCamera(), sim->primaryCamera());
    EXPECT_EQ(fusion->secondaryCamera(), sim->secondaryCamera());

    // Switch channels and verify integration
    EXPECT_TRUE(fusion->setActiveChannel(OpticalChannel::Secondary));
    EXPECT_EQ(fusion->activeChannel(), OpticalChannel::Secondary);
}
