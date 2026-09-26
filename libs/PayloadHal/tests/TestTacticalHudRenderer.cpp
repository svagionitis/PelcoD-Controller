#include <gtest/gtest.h>

#include "PayloadHal.h"
#include "TacticalHudRenderer.h"
#include "sim/SimulatedPayload.h"

#include <cmath>
#include <memory>
#include <vector>

using namespace PayloadHal;

TEST(TestTacticalHudRenderer, DefaultConfigurationAndPalette)
{
    TacticalHudRenderer renderer;
    auto cfg = renderer.config();

    EXPECT_EQ(cfg.reticle, ReticleType::MilDotLadder);
    EXPECT_EQ(cfg.declutterLevel, HudDeclutterLevel::Full);
    EXPECT_EQ(cfg.colorPalette, HudColorPalette::TacticalGreen);

    auto cGreen = renderer.currentColor();
    EXPECT_NEAR(cGreen.r, 0.0f, 1e-3);
    EXPECT_NEAR(cGreen.g, 1.0f, 1e-3);
    EXPECT_NEAR(cGreen.b, 0.25f, 1e-3);

    // Test palette switching
    renderer.setColorPalette(HudColorPalette::AviationWhite);
    EXPECT_EQ(renderer.colorPalette(), HudColorPalette::AviationWhite);
    auto cWhite = renderer.currentColor();
    EXPECT_NEAR(cWhite.r, 1.0f, 1e-3);
    EXPECT_NEAR(cWhite.g, 1.0f, 1e-3);
    EXPECT_NEAR(cWhite.b, 1.0f, 1e-3);

    renderer.setColorPalette(HudColorPalette::HighContrastAmber);
    EXPECT_NEAR(renderer.currentColor().r, 1.0f, 1e-3);
    EXPECT_NEAR(renderer.currentColor().g, 0.69f, 0.05f);

    renderer.setColorPalette(HudColorPalette::ThermalRed);
    EXPECT_NEAR(renderer.currentColor().r, 1.0f, 1e-3);
    EXPECT_NEAR(renderer.currentColor().g, 0.2f, 1e-3);

    renderer.setColorPalette(HudColorPalette::Cyan);
    EXPECT_NEAR(renderer.currentColor().r, 0.0f, 1e-3);
    EXPECT_NEAR(renderer.currentColor().g, 0.94f, 1e-3);

    // Custom color
    renderer.setCustomColor(HudColor { 0.5f, 0.5f, 0.5f, 0.8f });
    EXPECT_NEAR(renderer.currentColor().r, 0.5f, 1e-3);
    EXPECT_NEAR(renderer.currentColor().a, 0.8f, 1e-3);
}

TEST(TestTacticalHudRenderer, ReticleGeometryGeneration)
{
    TacticalHudRenderer renderer;

    // Crosshair
    renderer.setReticleType(ReticleType::Crosshair);
    EXPECT_EQ(renderer.reticleType(), ReticleType::Crosshair);
    auto dlCross = renderer.generateDrawList();
    EXPECT_GE(dlCross.lines.size(), 4U);

    // MilDotLadder
    renderer.setReticleType(ReticleType::MilDotLadder);
    EXPECT_EQ(renderer.reticleType(), ReticleType::MilDotLadder);
    auto dlLadder = renderer.generateDrawList();
    // 4 arm lines + at least 8 ladder ticks
    EXPECT_GE(dlLadder.lines.size(), 12U);

    // CircleDot
    renderer.setReticleType(ReticleType::CircleDot);
    auto dlCircle = renderer.generateDrawList();
    EXPECT_GE(dlCircle.circles.size(), 2U);

    // BoxReticle
    renderer.setReticleType(ReticleType::BoxReticle);
    auto dlBox = renderer.generateDrawList();
    EXPECT_GE(dlBox.rectangles.size(), 1U);
    EXPECT_GE(dlBox.lines.size(), 2U);

    // BoresightPlus
    renderer.setReticleType(ReticleType::BoresightPlus);
    auto dlPlus = renderer.generateDrawList();
    EXPECT_GE(dlPlus.lines.size(), 2U);

    // None
    renderer.setReticleType(ReticleType::None);
    renderer.setDeclutterLevel(HudDeclutterLevel::DeCluttered);
    auto dlNone = renderer.generateDrawList();
    EXPECT_EQ(dlNone.lines.size(), 0U);
    EXPECT_EQ(dlNone.circles.size(), 0U);
    EXPECT_EQ(dlNone.rectangles.size(), 0U);
}

TEST(TestTacticalHudRenderer, HeadingTapeRendering)
{
    TacticalHudRenderer renderer;
    HudTelemetrySnapshot snap {};
    snap.panAngleDeg = 45.0; // Heading NE
    renderer.updateTelemetry(snap);

    auto dl = renderer.generateDrawList();

    bool foundCenterHdg = false;
    bool foundCardinalNe = false;

    for (const auto& txt : dl.textLabels) {
        if (txt.text.find("045") != std::string::npos) {
            foundCenterHdg = true;
        }
        if (txt.text == "NE") {
            foundCardinalNe = true;
        }
    }

    EXPECT_TRUE(foundCenterHdg);
    EXPECT_TRUE(foundCardinalNe);

    // Heading N at 0 degrees
    snap.panAngleDeg = 0.0;
    renderer.updateTelemetry(snap);
    dl = renderer.generateDrawList();

    bool foundCardinalN = false;
    for (const auto& txt : dl.textLabels) {
        if (txt.text == "N") {
            foundCardinalN = true;
            break;
        }
    }
    EXPECT_TRUE(foundCardinalN);
}

TEST(TestTacticalHudRenderer, PitchLadderAndArtificialHorizon)
{
    TacticalHudRenderer renderer;
    HudTelemetrySnapshot snap {};
    snap.tiltAngleDeg = 10.0; // +10 degrees pitch up
    snap.rollAngleDeg = 15.0; // 15 degrees roll
    renderer.updateTelemetry(snap);

    auto dl = renderer.generateDrawList();

    bool foundPitchBar = false;
    for (const auto& txt : dl.textLabels) {
        if (txt.text.find("+10") != std::string::npos || txt.text.find("+5") != std::string::npos || txt.text.find("+15") != std::string::npos) {
            foundPitchBar = true;
            break;
        }
    }
    EXPECT_TRUE(foundPitchBar);
    EXPECT_GE(dl.lines.size(), 6U); // Baseline, horizon line, pitch bars
}

TEST(TestTacticalHudRenderer, TargetTrackingGateAndLeadPip)
{
    TacticalHudRenderer renderer;
    HudTelemetrySnapshot snap {};
    snap.targetTrackActive = true;
    snap.trackerStatus = "TRACKING";
    snap.targetCenterNormalized = { 0.6f, 0.45f };
    snap.targetWidthNormalized = 0.08f;
    snap.targetHeightNormalized = 0.06f;
    snap.hasLeadVector = true;
    snap.leadPipNormalized = { 0.65f, 0.43f };
    renderer.updateTelemetry(snap);

    auto dl = renderer.generateDrawList();

    bool foundTrackingText = false;
    for (const auto& txt : dl.textLabels) {
        if (txt.text == "TRACKING") {
            foundTrackingText = true;
            break;
        }
    }
    EXPECT_TRUE(foundTrackingText);

    // Lead pip circle should exist
    EXPECT_GE(dl.circles.size(), 1U);
}

TEST(TestTacticalHudRenderer, LrfAndLaserStatusOverlay)
{
    TacticalHudRenderer renderer;
    HudTelemetrySnapshot snap {};
    snap.lrfValid = true;
    snap.slantRangeMeters = 3450.0;
    snap.laserArmed = true;
    snap.laserFiring = false;
    snap.laserInterlockActive = false;
    renderer.updateTelemetry(snap);

    auto dl = renderer.generateDrawList();

    bool foundRange = false;
    bool foundArmed = false;
    for (const auto& txt : dl.textLabels) {
        if (txt.text.find("3450") != std::string::npos) {
            foundRange = true;
        }
        if (txt.text.find("ARMED") != std::string::npos) {
            foundArmed = true;
        }
    }
    EXPECT_TRUE(foundRange);
    EXPECT_TRUE(foundArmed);

    // Laser Interlock Active
    snap.laserInterlockActive = true;
    renderer.updateTelemetry(snap);
    dl = renderer.generateDrawList();

    bool foundInhibited = false;
    for (const auto& txt : dl.textLabels) {
        if (txt.text.find("LASER INHIBITED") != std::string::npos) {
            foundInhibited = true;
            break;
        }
    }
    EXPECT_TRUE(foundInhibited);

    // Laser Firing
    snap.laserInterlockActive = false;
    snap.laserFiring = true;
    renderer.updateTelemetry(snap);
    dl = renderer.generateDrawList();

    bool foundFiring = false;
    for (const auto& txt : dl.textLabels) {
        if (txt.text.find("LASER FIRING") != std::string::npos) {
            foundFiring = true;
            break;
        }
    }
    EXPECT_TRUE(foundFiring);
}

TEST(TestTacticalHudRenderer, DeclutterModes)
{
    TacticalHudRenderer renderer;
    HudTelemetrySnapshot snap {};
    snap.panAngleDeg = 90.0;
    snap.tiltAngleDeg = 0.0;
    snap.lrfValid = true;
    snap.slantRangeMeters = 1500.0;
    snap.targetTrackActive = true;
    snap.trackerStatus = "TRACKING";
    renderer.updateTelemetry(snap);

    // Full declutter level
    renderer.setDeclutterLevel(HudDeclutterLevel::Full);
    auto dlFull = renderer.generateDrawList();
    EXPECT_GT(dlFull.lines.size(), 15U);
    EXPECT_GT(dlFull.textLabels.size(), 5U);

    // Minimal declutter level: heading tape & pitch ladder removed, tracking gate & range kept
    renderer.setDeclutterLevel(HudDeclutterLevel::Minimal);
    auto dlMinimal = renderer.generateDrawList();
    EXPECT_LT(dlMinimal.lines.size(), dlFull.lines.size());
    EXPECT_LT(dlMinimal.textLabels.size(), dlFull.textLabels.size());

    // DeCluttered declutter level: only reticle lines
    renderer.setDeclutterLevel(HudDeclutterLevel::DeCluttered);
    auto dlNone = renderer.generateDrawList();
    EXPECT_EQ(dlNone.textLabels.size(), 0U);
}

TEST(TestTacticalHudRenderer, DirectRgbaRasterization)
{
    TacticalHudRenderer renderer;
    const int w = 640;
    const int h = 480;
    std::vector<std::uint8_t> buffer(w * h * 4, 0);

    // Initially buffer is all black/transparent zeros
    EXPECT_EQ(buffer[100], 0);

    EXPECT_TRUE(renderer.renderRgba(buffer.data(), w, h));

    // Verify some pixels were written with non-zero color
    bool hasNonZeroPixel = false;
    for (std::size_t i = 0; i < buffer.size(); i += 4) {
        if (buffer[i + 0] > 0 || buffer[i + 1] > 0 || buffer[i + 2] > 0) {
            hasNonZeroPixel = true;
            break;
        }
    }
    EXPECT_TRUE(hasNonZeroPixel);

    // Invalid parameters check
    EXPECT_FALSE(renderer.renderRgba(nullptr, w, h));
    EXPECT_FALSE(renderer.renderRgba(buffer.data(), 0, h));
}

TEST(TestTacticalHudRenderer, TelemetryIngestFromIPayload)
{
    auto sim = std::make_shared<SimulatedPayload>();
    sim->panTilt()->setAbsoluteAngles(135.0, -15.0);
    sim->primaryCamera()->setZoomNormalized(0.5);

    TacticalHudRenderer renderer;
    renderer.updateFromPayload(*sim);

    auto telem = renderer.telemetry();
    EXPECT_NEAR(telem.panAngleDeg, 135.0, 0.1);
    EXPECT_NEAR(telem.tiltAngleDeg, -15.0, 0.1);
    EXPECT_GT(telem.opticalZoomFactor, 5.0);
    EXPECT_FALSE(telem.opticalChannelName.empty());
}

TEST(TestTacticalHudRenderer, SimulatedPayloadIntegration)
{
    auto sim = std::make_shared<SimulatedPayload>();
    ASSERT_NE(sim->hudRenderer(), nullptr);

    sim->panTilt()->setAbsoluteAngles(270.0, 5.0);
    sim->hudRenderer()->updateFromPayload(*sim);

    auto dl = sim->hudRenderer()->generateDrawList();
    EXPECT_FALSE(dl.lines.empty());
    EXPECT_FALSE(dl.textLabels.empty());
}
