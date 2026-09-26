#include "PayloadHal.h"
#include "TacticalSearchEngine.h"
#include "sim/SimulatedPayload.h"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>

namespace PayloadHal {
namespace {

TEST(TestTacticalSearchEngine, SectorScanExecutionAndLifecycle)
{
    auto simPayload = std::dynamic_pointer_cast<SimulatedPayload>(PayloadFactory::createSimulatedPayload());
    ASSERT_NE(simPayload, nullptr);

    auto ptu = simPayload->panTilt();
    auto cam = simPayload->primaryCamera();
    ASSERT_NE(ptu, nullptr);
    ASSERT_NE(cam, nullptr);

    TacticalSearchEngine engine(ptu, cam);

    EXPECT_EQ(engine.status().state, TacticalEngineState::Idle);
    EXPECT_EQ(engine.status().activePattern, SearchPatternType::None);

    // Invalid config rejected
    SectorScanConfig invalidCfg {};
    invalidCfg.minAzimuthDeg = 50.0;
    invalidCfg.maxAzimuthDeg = 20.0; // min > max
    EXPECT_FALSE(engine.startSectorScan(invalidCfg));

    // Valid Sector Scan
    SectorScanConfig cfg {};
    cfg.minAzimuthDeg = -30.0;
    cfg.maxAzimuthDeg = 30.0;
    cfg.minElevationDeg = -10.0;
    cfg.maxElevationDeg = 10.0;
    cfg.scanSpeedDegPerSec = 50.0;
    cfg.elevationStepDeg = 2.0;

    EXPECT_TRUE(engine.startSectorScan(cfg));
    auto st = engine.status();
    EXPECT_EQ(st.state, TacticalEngineState::ExecutingPattern);
    EXPECT_EQ(st.activePattern, SearchPatternType::SectorScan);

    // Allow engine loop to execute a few steps
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    st = engine.status();
    EXPECT_EQ(st.state, TacticalEngineState::ExecutingPattern);

    // Stop pattern
    engine.stopPattern();
    st = engine.status();
    EXPECT_EQ(st.state, TacticalEngineState::Idle);
    EXPECT_EQ(st.activePattern, SearchPatternType::None);
}

TEST(TestTacticalSearchEngine, ExpandingSquareAndSpiralScan)
{
    auto simPayload = std::dynamic_pointer_cast<SimulatedPayload>(PayloadFactory::createSimulatedPayload());
    ASSERT_NE(simPayload, nullptr);

    TacticalSearchEngine engine(simPayload->panTilt(), simPayload->primaryCamera());

    // Expanding Square
    ExpandingSquareConfig sqCfg {};
    sqCfg.centerAzimuthDeg = 0.0;
    sqCfg.centerElevationDeg = 0.0;
    sqCfg.maxRadiusDeg = 20.0;
    sqCfg.stepSizeDeg = 5.0;
    sqCfg.scanSpeedDegPerSec = 30.0;

    EXPECT_TRUE(engine.startExpandingSquare(sqCfg));
    EXPECT_EQ(engine.status().activePattern, SearchPatternType::ExpandingSquare);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Switch to Spiral Scan
    SpiralScanConfig spCfg {};
    spCfg.centerAzimuthDeg = 0.0;
    spCfg.centerElevationDeg = 0.0;
    spCfg.maxRadiusDeg = 15.0;
    spCfg.expansionRateDeg = 5.0;
    spCfg.angularVelocityDegPerSec = 40.0;

    EXPECT_TRUE(engine.startSpiralScan(spCfg));
    EXPECT_EQ(engine.status().activePattern, SearchPatternType::SpiralScan);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Switch to Creeping Line
    CreepingLineConfig clCfg {};
    clCfg.baselineHeadingDeg = 0.0;
    clCfg.sweepWidthDeg = 30.0;
    clCfg.scanSpeedDegPerSec = 40.0;
    EXPECT_TRUE(engine.startCreepingLine(clCfg));
    EXPECT_EQ(engine.status().activePattern, SearchPatternType::CreepingLine);

    engine.stopPattern();
    EXPECT_EQ(engine.status().state, TacticalEngineState::Idle);
}

TEST(TestTacticalSearchEngine, PrioritizedCuePreemption)
{
    auto simPayload = std::dynamic_pointer_cast<SimulatedPayload>(PayloadFactory::createSimulatedPayload());
    ASSERT_NE(simPayload, nullptr);

    TacticalSearchEngine engine(simPayload->panTilt(), simPayload->primaryCamera());

    // Start background sector scan
    SectorScanConfig cfg {};
    cfg.minAzimuthDeg = -40.0;
    cfg.maxAzimuthDeg = 40.0;
    cfg.scanSpeedDegPerSec = 20.0;
    ASSERT_TRUE(engine.startSectorScan(cfg));
    EXPECT_EQ(engine.status().state, TacticalEngineState::ExecutingPattern);

    // Enqueue a Routine cue (priority 3)
    TargetCue routineCue {};
    routineCue.cueId = "AIS_VESSEL_101";
    routineCue.source = CueSource::Ais;
    routineCue.priority = CuePriority::Routine;
    routineCue.panAngleDeg = 80.0;
    routineCue.tiltAngleDeg = -5.0;
    routineCue.timestamp = std::chrono::system_clock::now();

    EXPECT_TRUE(engine.enqueueCue(routineCue));

    // Enqueue a Flash cue (priority 0) - should preempt immediately!
    TargetCue flashCue {};
    flashCue.cueId = "GUNSHOT_ALERT_911";
    flashCue.source = CueSource::Acoustic;
    flashCue.priority = CuePriority::Flash;
    flashCue.panAngleDeg = 25.0;
    flashCue.tiltAngleDeg = 0.0;
    flashCue.dwellTime = std::chrono::milliseconds(500);
    flashCue.timestamp = std::chrono::system_clock::now();

    EXPECT_TRUE(engine.enqueueCue(flashCue));

    // Status should immediately reflect SlewingToCue on flashCue
    auto st = engine.status();
    EXPECT_EQ(st.state, TacticalEngineState::SlewingToCue);
    EXPECT_EQ(st.activeCueId, "GUNSHOT_ALERT_911");

    engine.clearCues();
    engine.stopPattern();
}

TEST(TestTacticalSearchEngine, CueArrivalDwellAndPatternAutoResume)
{
    auto simPayload = std::dynamic_pointer_cast<SimulatedPayload>(PayloadFactory::createSimulatedPayload());
    ASSERT_NE(simPayload, nullptr);

    auto ptu = simPayload->panTilt();
    ASSERT_NE(ptu, nullptr);

    TacticalSearchEngine engine(ptu, simPayload->primaryCamera(), simPayload->lrf());
    engine.setArrivalThresholdDeg(1.0);

    bool cueAcquiredFired = false;
    std::string acquiredCueId;
    engine.registerCueAcquiredCallback([&](const TargetCue& cue) {
        cueAcquiredFired = true;
        acquiredCueId = cue.cueId;
    });

    // Start background pattern
    SectorScanConfig cfg {};
    cfg.minAzimuthDeg = -20.0;
    cfg.maxAzimuthDeg = 20.0;
    cfg.scanSpeedDegPerSec = 30.0;
    ASSERT_TRUE(engine.startSectorScan(cfg));

    // Enqueue target cue with short dwell time (100ms)
    TargetCue cue {};
    cue.cueId = "RADAR_TRACK_42";
    cue.source = CueSource::Radar;
    cue.priority = CuePriority::Immediate;
    cue.panAngleDeg = 15.0;
    cue.tiltAngleDeg = -5.0;
    cue.dwellTime = std::chrono::milliseconds(100);
    cue.timestamp = std::chrono::system_clock::now();

    ASSERT_TRUE(engine.enqueueCue(cue));

    // Wait for arrival, dwell, and resumption of search pattern
    const auto startWait = std::chrono::steady_clock::now();
    while ((!cueAcquiredFired || engine.status().state != TacticalEngineState::ExecutingPattern) &&
           std::chrono::steady_clock::now() - startWait < std::chrono::seconds(3)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }

    EXPECT_TRUE(cueAcquiredFired);
    EXPECT_EQ(acquiredCueId, "RADAR_TRACK_42");
    EXPECT_EQ(engine.status().state, TacticalEngineState::ExecutingPattern);
    EXPECT_EQ(engine.status().activePattern, SearchPatternType::SectorScan);

    engine.stopPattern();
}

TEST(TestTacticalSearchEngine, ManualInterventionPauseAndResume)
{
    auto simPayload = std::dynamic_pointer_cast<SimulatedPayload>(PayloadFactory::createSimulatedPayload());
    ASSERT_NE(simPayload, nullptr);

    TacticalSearchEngine engine(simPayload->panTilt(), simPayload->primaryCamera());

    SectorScanConfig cfg {};
    cfg.minAzimuthDeg = -30.0;
    cfg.maxAzimuthDeg = 30.0;
    ASSERT_TRUE(engine.startSectorScan(cfg));

    // Operator intervenes
    engine.notifyManualIntervention();
    EXPECT_EQ(engine.status().state, TacticalEngineState::Paused);

    // Resume
    engine.resumePattern();
    EXPECT_EQ(engine.status().state, TacticalEngineState::ExecutingPattern);

    // Pause manually
    engine.pausePattern();
    EXPECT_EQ(engine.status().state, TacticalEngineState::Paused);

    engine.stopPattern();
    EXPECT_EQ(engine.status().state, TacticalEngineState::Idle);
}

TEST(TestTacticalSearchEngine, SimulatedPayloadIntegration)
{
    auto payload = PayloadFactory::createSimulatedPayload();
    ASSERT_NE(payload, nullptr);

    auto tactical = payload->tacticalSearch();
    ASSERT_NE(tactical, nullptr);

    EXPECT_EQ(tactical->status().state, TacticalEngineState::Idle);

    SectorScanConfig cfg {};
    cfg.minAzimuthDeg = -15.0;
    cfg.maxAzimuthDeg = 15.0;
    EXPECT_TRUE(tactical->startSectorScan(cfg));
    EXPECT_EQ(tactical->status().state, TacticalEngineState::ExecutingPattern);

    tactical->stopPattern();
    EXPECT_EQ(tactical->status().state, TacticalEngineState::Idle);
}

} // namespace
} // namespace PayloadHal
