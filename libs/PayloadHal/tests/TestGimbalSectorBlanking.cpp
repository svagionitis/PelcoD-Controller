#include "PayloadHal.h"
#include "GimbalSectorBlanking.h"
#include "sim/SimulatedPayload.h"
#include <gtest/gtest.h>

namespace PayloadHal {
namespace {

TEST(TestGimbalSectorBlanking, ZoneRegistrationAndQuery)
{
    GimbalSectorBlanking blanking;
    EXPECT_TRUE(blanking.zones().empty());

    BlankingZone z1 {};
    z1.id = "MastKeepOut";
    z1.description = "Main Ship Mast Obstruction";
    z1.type = SectorZoneType::MechanicalKeepOut;
    z1.azMinDeg = 170.0;
    z1.azMaxDeg = 190.0;
    z1.elMinDeg = -10.0;
    z1.elMaxDeg = 50.0;

    EXPECT_TRUE(blanking.addZone(z1));
    // Duplicate ID must be rejected
    EXPECT_FALSE(blanking.addZone(z1));
    EXPECT_EQ(blanking.zones().size(), 1U);

    const auto retrieved = blanking.zone("MastKeepOut");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->description, "Main Ship Mast Obstruction");

    // Update zone
    z1.description = "Updated Mast Description";
    EXPECT_TRUE(blanking.updateZone(z1));
    EXPECT_EQ(blanking.zone("MastKeepOut")->description, "Updated Mast Description");

    // Enable / disable toggle
    EXPECT_TRUE(blanking.setZoneEnabled("MastKeepOut", false));
    EXPECT_FALSE(blanking.zone("MastKeepOut")->enabled);

    // Remove zone
    EXPECT_TRUE(blanking.removeZone("MastKeepOut"));
    EXPECT_TRUE(blanking.zones().empty());
}

TEST(TestGimbalSectorBlanking, AzimuthWrapAround360)
{
    GimbalSectorBlanking blanking;

    // Sector spanning 345° to 15° across zero-meridian
    BlankingZone z {};
    z.id = "NoseLaserInhibit";
    z.type = SectorZoneType::LaserInhibit;
    z.azMinDeg = 345.0;
    z.azMaxDeg = 15.0;
    z.elMinDeg = -20.0;
    z.elMaxDeg = 20.0;
    blanking.addZone(z);

    // Dead center forward (0°, 0°)
    const auto eval0 = blanking.evaluate(0.0, 0.0);
    EXPECT_FALSE(eval0.laserAllowed);
    EXPECT_TRUE(eval0.mechanicalAllowed);

    // In sector at 5°
    EXPECT_FALSE(blanking.isLaserAllowed(5.0, 0.0));

    // In sector at 350°
    EXPECT_FALSE(blanking.isLaserAllowed(350.0, 0.0));

    // Outside sector at 90° (East)
    EXPECT_TRUE(blanking.isLaserAllowed(90.0, 0.0));

    // Outside sector in elevation (0°, 45°)
    EXPECT_TRUE(blanking.isLaserAllowed(0.0, 45.0));
}

TEST(TestGimbalSectorBlanking, PolygonBoundaryContainment)
{
    GimbalSectorBlanking blanking;

    BlankingZone polyZone {};
    polyZone.id = "TriangularAntenna";
    polyZone.type = SectorZoneType::TotalExclusion;
    polyZone.polygonVertices = {
        { 10.0, 10.0 },
        { 30.0, 10.0 },
        { 20.0, 30.0 }
    };
    blanking.addZone(polyZone);

    // Center of triangle (20°, 15°)
    const auto evalInside = blanking.evaluate(20.0, 15.0);
    EXPECT_FALSE(evalInside.mechanicalAllowed);
    EXPECT_FALSE(evalInside.laserAllowed);
    EXPECT_TRUE(evalInside.videoBlanked);

    // Outside triangle (0°, 0°)
    const auto evalOutside = blanking.evaluate(0.0, 0.0);
    EXPECT_TRUE(evalOutside.mechanicalAllowed);
    EXPECT_TRUE(evalOutside.laserAllowed);
    EXPECT_FALSE(evalOutside.videoBlanked);

    // Outside above apex (20°, 35°)
    EXPECT_TRUE(blanking.isMotionAllowed(20.0, 35.0));
}

TEST(TestGimbalSectorBlanking, SafetyMarginBuffer)
{
    GimbalSectorBlanking blanking;

    BlankingZone z {};
    z.id = "CabinRoof";
    z.type = SectorZoneType::MechanicalKeepOut;
    z.azMinDeg = 80.0;
    z.azMaxDeg = 100.0;
    z.elMinDeg = -10.0;
    z.elMaxDeg = 10.0;
    z.safetyMarginDeg = 3.0; // 3° warning margin
    blanking.addZone(z);

    // Inside zone (90°, 0°)
    const auto inside = blanking.evaluate(90.0, 0.0);
    EXPECT_FALSE(inside.mechanicalAllowed);

    // Just outside boundary at 101.5° (1.5° away <= 3.0°)
    const auto nearBoundary = blanking.evaluate(101.5, 0.0);
    EXPECT_TRUE(nearBoundary.mechanicalAllowed); // technically still outside
    EXPECT_TRUE(nearBoundary.inWarningMargin);    // but in safety buffer!
    EXPECT_NEAR(nearBoundary.distanceToNearestZoneDeg, 1.5, 0.1);

    // Far away at 130°
    const auto farAway = blanking.evaluate(130.0, 0.0);
    EXPECT_TRUE(farAway.mechanicalAllowed);
    EXPECT_FALSE(farAway.inWarningMargin);
    EXPECT_GT(farAway.distanceToNearestZoneDeg, 25.0);
}

TEST(TestGimbalSectorBlanking, MechanicalKeepOutAndPathValidation)
{
    GimbalSectorBlanking blanking;

    BlankingZone z {};
    z.id = "PortMast";
    z.type = SectorZoneType::MechanicalKeepOut;
    z.azMinDeg = 90.0;
    z.azMaxDeg = 110.0;
    z.elMinDeg = -20.0;
    z.elMaxDeg = 20.0;
    z.safetyMarginDeg = 2.0;
    blanking.addZone(z);

    // 1. Slewing directly into the zone from (0, 0) to (100, 0)
    const auto resInto = blanking.validatePath(0.0, 0.0, 100.0, 0.0);
    EXPECT_FALSE(resInto.pathClear);
    EXPECT_TRUE(resInto.clamped);
    EXPECT_EQ(resInto.blockingZoneId, "PortMast");
    // Target should be clamped before 90° minus safety margin (<= 88°)
    EXPECT_LE(resInto.safeTargetAzDeg, 89.0);

    // 2. Trajectory crossing through zone: from (80, 0) to (120, 0)
    const auto resTraverse = blanking.validatePath(80.0, 0.0, 120.0, 0.0);
    EXPECT_FALSE(resTraverse.pathClear);
    EXPECT_TRUE(resTraverse.clamped);

    // 3. Clear trajectory: from (0, 0) to (0, 45)
    const auto resClear = blanking.validatePath(0.0, 0.0, 0.0, 45.0);
    EXPECT_TRUE(resClear.pathClear);
    EXPECT_FALSE(resClear.clamped);
    EXPECT_DOUBLE_EQ(resClear.safeTargetAzDeg, 0.0);
    EXPECT_DOUBLE_EQ(resClear.safeTargetElDeg, 45.0);
}

TEST(TestGimbalSectorBlanking, LaserInterlockCallback)
{
    GimbalSectorBlanking blanking;

    BlankingZone z {};
    z.id = "PersonnelDeck";
    z.type = SectorZoneType::LaserInhibit;
    z.azMinDeg = 200.0;
    z.azMaxDeg = 220.0;
    z.elMinDeg = -30.0;
    z.elMaxDeg = 0.0;
    blanking.addZone(z);

    bool callbackTriggered = false;
    bool lastLaserAllowed = true;
    std::string triggeredZoneId {};

    blanking.registerInterlockCallback([&](bool allowed, const std::string& zoneId) {
        callbackTriggered = true;
        lastLaserAllowed = allowed;
        triggeredZoneId = zoneId;
    });

    // Move into laser keep-out zone
    const auto evalIn = blanking.evaluate(210.0, -10.0);
    EXPECT_FALSE(evalIn.laserAllowed);
    EXPECT_TRUE(callbackTriggered);
    EXPECT_FALSE(lastLaserAllowed);
    EXPECT_EQ(triggeredZoneId, "PersonnelDeck");

    // Move out of laser keep-out zone
    callbackTriggered = false;
    const auto evalOut = blanking.evaluate(0.0, 0.0);
    EXPECT_TRUE(evalOut.laserAllowed);
    EXPECT_TRUE(callbackTriggered);
    EXPECT_TRUE(lastLaserAllowed);
}

TEST(TestGimbalSectorBlanking, SimulatedPayloadIntegration)
{
    auto payload = PayloadFactory::createSimulatedPayload();
    ASSERT_NE(payload, nullptr);

    auto blanking = payload->sectorBlanking();
    ASSERT_NE(blanking, nullptr);

    // Configure Laser Inhibit Zone (45° to 75°, -10° to 10°)
    BlankingZone laserZone {};
    laserZone.id = "WingtipSensorProtection";
    laserZone.type = SectorZoneType::LaserInhibit;
    laserZone.azMinDeg = 45.0;
    laserZone.azMaxDeg = 75.0;
    laserZone.elMinDeg = -10.0;
    laserZone.elMaxDeg = 10.0;
    blanking->addZone(laserZone);

    // Configure Mechanical Keep-Out Zone (170° to 190°, -20° to 20°)
    BlankingZone mechZone {};
    mechZone.id = "TailRotorKeepOut";
    mechZone.type = SectorZoneType::MechanicalKeepOut;
    mechZone.azMinDeg = 170.0;
    mechZone.azMaxDeg = 190.0;
    mechZone.elMinDeg = -20.0;
    mechZone.elMaxDeg = 20.0;
    mechZone.safetyMarginDeg = 2.0;
    blanking->addZone(mechZone);

    auto ptu = payload->panTilt();
    auto lrf = payload->lrf();
    auto illuminator = payload->illuminator();

    ASSERT_NE(ptu, nullptr);
    ASSERT_NE(lrf, nullptr);
    ASSERT_NE(illuminator, nullptr);

    // 1. Aim into safe zone (0°, 0°)
    EXPECT_TRUE(ptu->setAbsoluteAngles(0.0, 0.0));
    EXPECT_TRUE(lrf->armLaser());
    EXPECT_TRUE(lrf->triggerSingleMeasurement()); // Should succeed!

    EXPECT_TRUE(illuminator->armLaser());
    EXPECT_TRUE(illuminator->startEmission()); // Should succeed!
    EXPECT_TRUE(illuminator->isEmitting());
    EXPECT_TRUE(illuminator->stopEmission());

    // 2. Aim into Laser Inhibit Zone (60°, 0°)
    EXPECT_TRUE(ptu->setAbsoluteAngles(60.0, 0.0));

    // LRF pulse attempt must be blocked by safety interlock!
    EXPECT_FALSE(lrf->triggerSingleMeasurement());
    // Continuous mode attempt must be blocked!
    EXPECT_FALSE(lrf->setContinuousMode(LrfMode::Continuous10Hz));

    // Illuminator emission attempt must be blocked!
    EXPECT_FALSE(illuminator->startEmission());
    EXPECT_FALSE(illuminator->isEmitting());

    // 3. Move gimbal back to safe orientation (0°, 0°)
    EXPECT_TRUE(ptu->setAbsoluteAngles(0.0, 0.0));
    EXPECT_TRUE(lrf->triggerSingleMeasurement()); // Clear to fire again

    // 4. Slew into Mechanical Keep-Out Zone (180°, 0°)
    // Gimbal must clamp target outside the zone (< 170° minus safety margin)
    EXPECT_TRUE(ptu->setAbsoluteAngles(180.0, 0.0));
    const auto telem = ptu->currentTelemetry();
    EXPECT_NE(telem.panAngleDeg, 180.0);
    EXPECT_LE(telem.panAngleDeg, 169.0);
}

} // namespace
} // namespace PayloadHal
