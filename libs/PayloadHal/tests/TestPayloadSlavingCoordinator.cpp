#include <gtest/gtest.h>

#include "PayloadHal.h"
#include "PayloadSlavingCoordinator.h"
#include "GimbalSectorBlanking.h"
#include "sim/SimulatedPayload.h"

#include <cmath>
#include <memory>

using namespace PayloadHal;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kRadToDeg = 180.0 / kPi;

} // namespace

TEST(TestPayloadSlavingCoordinator, StationRegistrationAndQuery)
{
    PayloadSlavingCoordinator coord;

    PayloadStationRecord recA;
    recA.stationId = "StationA";
    recA.platformOffsetM = { 0.0, 0.0, 0.0 };
    recA.capability.canBeMaster = true;
    recA.capability.canBeSlave = true;
    recA.isMaster = true;

    EXPECT_TRUE(coord.registerStation(recA));
    EXPECT_FALSE(coord.registerStation(recA)); // Duplicate ID

    PayloadStationRecord emptyRec;
    EXPECT_FALSE(coord.registerStation(emptyRec)); // Empty ID

    PayloadStationRecord recB;
    recB.stationId = "StationB";
    recB.platformOffsetM = { 0.0, 10.0, 0.0 };
    recB.capability.canBeMaster = true;
    recB.capability.canBeSlave = true;
    recB.isMaster = false;
    recB.isSlaved = true;

    EXPECT_TRUE(coord.registerStation(recB));

    EXPECT_EQ(coord.masterStationId(), "StationA");
    EXPECT_TRUE(coord.isStationSlaved("StationB"));
    EXPECT_FALSE(coord.isStationSlaved("StationA"));

    const auto allStations = coord.stations();
    EXPECT_EQ(allStations.size(), 2U);

    const auto queryA = coord.station("StationA");
    ASSERT_TRUE(queryA.has_value());
    EXPECT_EQ(queryA->stationId, "StationA");

    EXPECT_TRUE(coord.unregisterStation("StationA"));
    EXPECT_FALSE(coord.station("StationA").has_value());
    EXPECT_EQ(coord.stations().size(), 1U);
}

TEST(TestPayloadSlavingCoordinator, CollimatedInfiniteRangeSlaving)
{
    PayloadSlavingCoordinator coord;
    coord.setSlavingMode(SlavingMode::CollimatedLOS);

    PayloadStationRecord master;
    master.stationId = "Master";
    master.platformOffsetM = { 0.0, 0.0, 0.0 };
    master.mountOrientation = { 0.0, 0.0, 0.0 }; // Upright forward
    master.isMaster = true;

    PayloadStationRecord slave;
    slave.stationId = "Slave";
    slave.platformOffsetM = { 0.0, 15.0, 0.0 }; // 15m Starboard offset
    slave.mountOrientation = { 0.0, 0.0, 0.0 }; // Upright forward
    slave.isSlaved = true;

    ASSERT_TRUE(coord.registerStation(master));
    ASSERT_TRUE(coord.registerStation(slave));

    // Master points Pan 45 deg, Tilt -10 deg. At optical infinity (or CollimatedLOS),
    // Slave must point parallel: Pan 45 deg, Tilt -10 deg.
    const auto lookOpt = coord.computeSlaveLookAngles("Master", "Slave", 45.0, -10.0, 0.0);
    ASSERT_TRUE(lookOpt.has_value());
    EXPECT_TRUE(lookOpt->isCollimated);
    EXPECT_NEAR(lookOpt->panDeg, 45.0, 1e-6);
    EXPECT_NEAR(lookOpt->tiltDeg, -10.0, 1e-6);
}

TEST(TestPayloadSlavingCoordinator, FiniteRangeParallaxCompensation)
{
    PayloadSlavingCoordinator coord;
    coord.setSlavingMode(SlavingMode::MasterSlave3D);

    // Master located at origin (0, 0, 0)
    PayloadStationRecord master;
    master.stationId = "Master";
    master.platformOffsetM = { 0.0, 0.0, 0.0 };
    master.mountOrientation = { 0.0, 0.0, 0.0 };
    master.isMaster = true;

    // Slave located 10m to starboard (Y = +10m)
    PayloadStationRecord slave;
    slave.stationId = "Slave";
    slave.platformOffsetM = { 0.0, 10.0, 0.0 };
    slave.mountOrientation = { 0.0, 0.0, 0.0 };
    slave.isSlaved = true;

    ASSERT_TRUE(coord.registerStation(master));
    ASSERT_TRUE(coord.registerStation(slave));

    // Master looks straight forward (Pan = 0 deg, Tilt = 0 deg) at slant range R = 100 meters
    // Target position in platform body is (100, 0, 0).
    // Vector from slave (0, 10, 0) to target is (100, -10, 0).
    // Required slave pan = atan2(-10, 100) * 180 / pi = -5.710593 deg
    const double expectedPan = std::atan2(-10.0, 100.0) * kRadToDeg;
    const double expectedRange = std::sqrt(100.0 * 100.0 + 10.0 * 10.0);

    const auto lookOpt = coord.computeSlaveLookAngles("Master", "Slave", 0.0, 0.0, 100.0);
    ASSERT_TRUE(lookOpt.has_value());
    EXPECT_FALSE(lookOpt->isCollimated);
    EXPECT_NEAR(lookOpt->panDeg, expectedPan, 1e-4);
    EXPECT_NEAR(lookOpt->tiltDeg, 0.0, 1e-4);
    EXPECT_NEAR(lookOpt->slantRangeMeters, expectedRange, 1e-4);
}

TEST(TestPayloadSlavingCoordinator, CantedMountingAngleSlaving)
{
    PayloadSlavingCoordinator coord;
    coord.setSlavingMode(SlavingMode::CollimatedLOS);

    // Master upright forward (yaw = 0 deg)
    PayloadStationRecord master;
    master.stationId = "Master";
    master.platformOffsetM = { 0.0, 0.0, 0.0 };
    master.mountOrientation = { 0.0, 0.0, 0.0 };
    master.isMaster = true;

    // Slave mounted with +90 deg yaw offset (facing Starboard)
    PayloadStationRecord slave;
    slave.stationId = "SlaveStarboard";
    slave.platformOffsetM = { 0.0, 5.0, 0.0 };
    slave.mountOrientation = { 90.0, 0.0, 0.0 }; // Yawed +90 deg
    slave.isSlaved = true;

    ASSERT_TRUE(coord.registerStation(master));
    ASSERT_TRUE(coord.registerStation(slave));

    // Master points straight forward (Pan = 0 deg, Tilt = 0 deg)
    // For slave facing Starboard (+90 deg), platform forward is 90 deg to its left (-90 deg)
    const auto lookOpt = coord.computeSlaveLookAngles("Master", "SlaveStarboard", 0.0, 0.0, 0.0);
    ASSERT_TRUE(lookOpt.has_value());
    EXPECT_NEAR(lookOpt->panDeg, -90.0, 1e-5);
    EXPECT_NEAR(lookOpt->tiltDeg, 0.0, 1e-5);
}

TEST(TestPayloadSlavingCoordinator, BlindZoneProximityDetection)
{
    PayloadSlavingCoordinator coord;
    coord.setSlavingMode(SlavingMode::AutonomousHandoff);
    coord.setHandoverThresholds(5.0, 3.0); // 5 deg warning margin

    auto masterPayload = std::make_shared<SimulatedPayload>();
    // Define a keep-out zone at [30 deg, 50 deg] azimuth
    BlankingZone zone;
    zone.id = "MastObstruction";
    zone.type = SectorZoneType::MechanicalKeepOut;
    zone.azMinDeg = 30.0;
    zone.azMaxDeg = 50.0;
    zone.elMinDeg = -30.0;
    zone.elMaxDeg = +30.0;
    zone.safetyMarginDeg = 2.0;
    masterPayload->sectorBlanking()->addZone(zone);

    PayloadStationRecord master;
    master.stationId = "Master";
    master.payload = masterPayload;
    master.isMaster = true;

    ASSERT_TRUE(coord.registerStation(master));

    // Move Master PTU to 0 deg (30 deg away from zone)
    masterPayload->panTilt()->setAbsoluteAngles(0.0, 0.0);
    coord.update(0.1);

    auto rep = coord.statusReport();
    EXPECT_GT(rep.masterDistanceToBlindZoneDeg, 5.0);
    EXPECT_EQ(rep.handoverState, HandoverState::Tracking);

    // Move Master PTU to 27 deg (3 deg from zone boundary 30 deg, within 5 deg warning margin)
    masterPayload->panTilt()->setAbsoluteAngles(27.0, 0.0);
    coord.update(0.1);

    rep = coord.statusReport();
    EXPECT_LE(rep.masterDistanceToBlindZoneDeg, 5.0);
    // Since no candidate slave is registered, handover state reports failed
    EXPECT_EQ(rep.handoverState, HandoverState::HandoffFailed);
}

TEST(TestPayloadSlavingCoordinator, CandidateSlaveSelectionAndAutonomousHandover)
{
    PayloadSlavingCoordinator coord;
    coord.setSlavingMode(SlavingMode::AutonomousHandoff);
    coord.setHandoverThresholds(5.0, 3.0);

    // Station 1: Master (has keep-out zone at [40, 60] deg)
    auto p1 = std::make_shared<SimulatedPayload>();
    BlankingZone z1;
    z1.id = "Superstructure";
    z1.type = SectorZoneType::MechanicalKeepOut;
    z1.azMinDeg = 40.0;
    z1.azMaxDeg = 60.0;
    z1.elMinDeg = -45.0;
    z1.elMaxDeg = +45.0;
    p1->sectorBlanking()->addZone(z1);

    PayloadStationRecord rec1;
    rec1.stationId = "Turret1";
    rec1.payload = p1;
    rec1.isMaster = true;
    rec1.capability.canBeMaster = true;
    rec1.capability.canBeSlave = true;
    ASSERT_TRUE(coord.registerStation(rec1));

    // Station 2: Candidate slave (clear line of sight)
    auto p2 = std::make_shared<SimulatedPayload>();
    PayloadStationRecord rec2;
    rec2.stationId = "Turret2";
    rec2.payload = p2;
    rec2.isMaster = false;
    rec2.isSlaved = true;
    rec2.capability.canBeMaster = true;
    rec2.capability.canBeSlave = true;
    rec2.capability.lockToleranceDeg = 1.0;
    ASSERT_TRUE(coord.registerStation(rec2));

    bool callbackFired = false;
    std::string cbOldMaster, cbNewMaster;
    HandoverState cbState = HandoverState::Idle;
    coord.setHandoverCallback([&](const std::string& oldM, const std::string& newM, HandoverState st) {
        callbackFired = true;
        cbOldMaster = oldM;
        cbNewMaster = newM;
        cbState = st;
    });

    // Initial state: Turret 1 at Pan 0 deg
    p1->panTilt()->setAbsoluteAngles(0.0, 0.0);
    p2->panTilt()->setAbsoluteAngles(0.0, 0.0);
    coord.update(0.1);
    EXPECT_EQ(coord.handoverState(), HandoverState::Tracking);

    // Turret 1 slews towards keep-out zone, arriving at 38 deg (within 5 deg of zone at 40 deg)
    p1->panTilt()->setAbsoluteAngles(38.0, 0.0);
    coord.update(0.1);

    // Coordinator should cue Turret 2
    EXPECT_TRUE(callbackFired);
    EXPECT_EQ(coord.statusReport().candidateSlaveId, "Turret2");
    EXPECT_TRUE(coord.handoverState() == HandoverState::CueingSlave ||
                coord.handoverState() == HandoverState::SlaveConverging);

    // Simulate Turret 2 PTU arriving at the slaved angle (38 deg)
    p2->panTilt()->setAbsoluteAngles(38.0, 0.0);
    coord.update(0.1);

    // Handover should complete: Turret 2 promoted to Master!
    EXPECT_EQ(coord.masterStationId(), "Turret2");
    EXPECT_EQ(coord.handoverState(), HandoverState::HandoverComplete);

    coord.update(0.1);
    EXPECT_EQ(coord.handoverState(), HandoverState::Tracking);
    EXPECT_EQ(coord.masterStationId(), "Turret2");
}

TEST(TestPayloadSlavingCoordinator, ManualHandoverRequestAndAbort)
{
    PayloadSlavingCoordinator coord;
    coord.setSlavingMode(SlavingMode::MasterSlave3D);

    auto pA = std::make_shared<SimulatedPayload>();
    auto pB = std::make_shared<SimulatedPayload>();

    PayloadStationRecord recA;
    recA.stationId = "A";
    recA.payload = pA;
    recA.isMaster = true;
    recA.capability.canBeMaster = true;
    ASSERT_TRUE(coord.registerStation(recA));

    PayloadStationRecord recB;
    recB.stationId = "B";
    recB.payload = pB;
    recB.isMaster = false;
    recB.capability.canBeMaster = true;
    ASSERT_TRUE(coord.registerStation(recB));

    EXPECT_TRUE(coord.requestHandover("B"));
    EXPECT_EQ(coord.handoverState(), HandoverState::CueingSlave);
    EXPECT_EQ(coord.statusReport().candidateSlaveId, "B");

    // Operator aborts handover
    coord.cancelHandover();
    EXPECT_EQ(coord.handoverState(), HandoverState::Tracking);
    EXPECT_TRUE(coord.statusReport().candidateSlaveId.empty());
    EXPECT_EQ(coord.masterStationId(), "A");
}

TEST(TestPayloadSlavingCoordinator, SimulatedPayloadIntegration)
{
    auto sim = std::make_shared<SimulatedPayload>();
    ASSERT_NE(sim->slavingCoordinator(), nullptr);

    auto coord = sim->slavingCoordinator();
    EXPECT_EQ(coord->slavingMode(), SlavingMode::Disabled);

    coord->setSlavingMode(SlavingMode::MasterSlave3D);
    EXPECT_EQ(coord->slavingMode(), SlavingMode::MasterSlave3D);

    // Verify registration and telemetry queries
    PayloadStationRecord rec;
    rec.stationId = "SimStation";
    rec.payload = sim;
    rec.isMaster = true;
    EXPECT_TRUE(coord->registerStation(rec));
    EXPECT_EQ(coord->masterStationId(), "SimStation");
}
