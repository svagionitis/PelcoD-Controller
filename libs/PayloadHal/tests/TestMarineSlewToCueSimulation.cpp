#include "AutoFramingController.h"
#include "GeoLockController.h"
#include "LocalPresetManager.h"
#include "Nmea/AisDecoder.h"
#include "Nmea/NmeaChecksum.h"
#include "Nmea/NmeaDevice.h"
#include "Nmea/replay/NmeaReplayTransport.h"
#include "NmeaSlavingBridge.h"
#include "PayloadAutoTrackerBridge.h"
#include "SlewToCueDirector.h"
#include "TargetThreatEvaluator.h"
#include "TourEngine.h"
#include "VesselAttitudeCompensator.h"
#include "sim/SimulatedPayload.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace PayloadHal {
namespace {

    constexpr double kKnotsToMps { 0.5144444444444444 };

    // =========================================================================
    // Suite 1: CPA / TCPA Mathematical Edge Cases
    // =========================================================================

    TEST(TestCpaTcpaEdgeCases, ReciprocalCoursesCpa)
    {
        // Own ship at (37.0, -122.0), heading North (000 deg) at 15 knots
        const Klv::GeoPoint3D ownPos { 37.0, -122.0, 0.0 };
        const double ownSog { 15.0 };
        const double ownCog { 0.0 };

        // Target located 3704 m ahead (North), steering South (180 deg) at 15 knots,
        // laterally offset by 200 m East (parallel reciprocal track)
        const Klv::GeoPoint2D northPt
            = Klv::KlvGeodesy::directGeodetic({ ownPos.latitudeDeg, ownPos.longitudeDeg }, 0.0, 3704.0);
        const Klv::GeoPoint2D targetPt
            = Klv::KlvGeodesy::directGeodetic(northPt, 90.0, 200.0);
        const Klv::GeoPoint3D targetPos { targetPt.latitudeDeg, targetPt.longitudeDeg, 0.0 };
        const double targetSog { 15.0 };
        const double targetCog { 180.0 };

        double cpaMeters { 0.0 };
        double tcpaSeconds { 0.0 };

        TargetThreatEvaluator::calculateCpa(
            ownPos, ownSog, ownCog, targetPos, targetSog, targetCog, cpaMeters, tcpaSeconds);

        // Lateral separation at closest approach should be 200 meters (+-15m geodetic curvature)
        EXPECT_NEAR(cpaMeters, 200.0, 15.0);

        // Closure speed = 30 knots ~ 15.433 m/s. Longitudinal distance ~ 3704 m.
        // TCPA ~ 3704 / 15.433 ~ 240 seconds.
        EXPECT_GT(tcpaSeconds, 230.0);
        EXPECT_LT(tcpaSeconds, 250.0);
    }

    TEST(TestCpaTcpaEdgeCases, ParallelIdenticalTracks)
    {
        // Own ship heading East (090 deg) at 12 knots
        const Klv::GeoPoint3D ownPos { 37.0, -122.0, 0.0 };
        const double ownSog { 12.0 };
        const double ownCog { 090.0 };

        // Target parallel 500 m North, also cruising East at identical speed 12 knots (delta V = 0)
        const Klv::GeoPoint2D targetPt
            = Klv::KlvGeodesy::directGeodetic({ ownPos.latitudeDeg, ownPos.longitudeDeg }, 0.0, 500.0);
        const Klv::GeoPoint3D targetPos { targetPt.latitudeDeg, targetPt.longitudeDeg, 0.0 };
        const double targetSog { 12.0 };
        const double targetCog { 090.0 };

        double cpaMeters { 0.0 };
        double tcpaSeconds { 0.0 };

        TargetThreatEvaluator::calculateCpa(
            ownPos, ownSog, ownCog, targetPos, targetSog, targetCog, cpaMeters, tcpaSeconds);

        // With zero relative velocity, division by zero must be protected:
        // TCPA should be 0.0 and CPA should equal the constant separation distance
        EXPECT_DOUBLE_EQ(tcpaSeconds, 0.0);
        EXPECT_NEAR(cpaMeters, 500.0, 10.0);
        EXPECT_FALSE(std::isnan(cpaMeters));
        EXPECT_FALSE(std::isnan(tcpaSeconds));
    }

    TEST(TestCpaTcpaEdgeCases, OvertakingScenarios)
    {
        const Klv::GeoPoint3D ownPos { 37.0, -122.0, 0.0 };

        // Case A: Own ship at 20 knots overtaking slower vessel at 8 knots on same track 1000m ahead
        const Klv::GeoPoint2D tgtPtA
            = Klv::KlvGeodesy::directGeodetic({ ownPos.latitudeDeg, ownPos.longitudeDeg }, 0.0, 1000.0);
        const Klv::GeoPoint3D tgtPosA { tgtPtA.latitudeDeg, tgtPtA.longitudeDeg, 0.0 };

        double cpaA { 0.0 };
        double tcpaA { 0.0 };
        TargetThreatEvaluator::calculateCpa(ownPos, 20.0, 0.0, tgtPosA, 8.0, 0.0, cpaA, tcpaA);

        // Relative speed = 12 knots ~ 6.173 m/s. Range = 1000m -> TCPA ~ 162 s
        EXPECT_NEAR(cpaA, 0.0, 10.0);
        EXPECT_NEAR(tcpaA, 162.0, 3.0);

        // Case B: Faster vessel (25 knots) overtaking own ship (10 knots) from 1000m behind (180 deg)
        const Klv::GeoPoint2D tgtPtB
            = Klv::KlvGeodesy::directGeodetic({ ownPos.latitudeDeg, ownPos.longitudeDeg }, 180.0, 1000.0);
        const Klv::GeoPoint3D tgtPosB { tgtPtB.latitudeDeg, tgtPtB.longitudeDeg, 0.0 };

        double cpaB { 0.0 };
        double tcpaB { 0.0 };
        TargetThreatEvaluator::calculateCpa(ownPos, 10.0, 0.0, tgtPosB, 25.0, 0.0, cpaB, tcpaB);

        // Relative speed = 15 knots ~ 7.716 m/s. Range = 1000m -> TCPA ~ 130 s
        EXPECT_NEAR(cpaB, 0.0, 10.0);
        EXPECT_NEAR(tcpaB, 130.0, 3.0);
    }

    TEST(TestCpaTcpaEdgeCases, DivergingTracksPastCpa)
    {
        const Klv::GeoPoint3D ownPos { 37.0, -122.0, 0.0 };

        // Target 2000m North, but pulling away at 25 knots while own ship is at 10 knots North
        const Klv::GeoPoint2D targetPt
            = Klv::KlvGeodesy::directGeodetic({ ownPos.latitudeDeg, ownPos.longitudeDeg }, 0.0, 2000.0);
        const Klv::GeoPoint3D targetPos { targetPt.latitudeDeg, targetPt.longitudeDeg, 0.0 };

        double cpaMeters { 0.0 };
        double tcpaSeconds { 0.0 };

        TargetThreatEvaluator::calculateCpa(
            ownPos, 10.0, 0.0, targetPos, 25.0, 0.0, cpaMeters, tcpaSeconds);

        // Past CPA: TCPA is negative (diverging)
        EXPECT_LT(tcpaSeconds, 0.0);
        EXPECT_LE(cpaMeters, 2005.0);
    }

    TEST(TestCpaTcpaEdgeCases, StationaryEntities)
    {
        const Klv::GeoPoint3D ownPos { 37.0, -122.0, 0.0 };
        const Klv::GeoPoint2D targetPt
            = Klv::KlvGeodesy::directGeodetic({ ownPos.latitudeDeg, ownPos.longitudeDeg }, 0.0, 1500.0);
        const Klv::GeoPoint3D targetPos { targetPt.latitudeDeg, targetPt.longitudeDeg, 0.0 };

        // 1. Stationary target (anchored vessel / navigation mark)
        double cpa1 { 0.0 };
        double tcpa1 { 0.0 };
        TargetThreatEvaluator::calculateCpa(ownPos, 10.0, 0.0, targetPos, 0.0, 0.0, cpa1, tcpa1);
        EXPECT_NEAR(cpa1, 0.0, 10.0);
        EXPECT_NEAR(tcpa1, 1500.0 / (10.0 * kKnotsToMps), 2.0);

        // 2. Stationary own ship
        double cpa2 { 0.0 };
        double tcpa2 { 0.0 };
        TargetThreatEvaluator::calculateCpa(ownPos, 0.0, 0.0, targetPos, 20.0, 180.0, cpa2, tcpa2);
        EXPECT_NEAR(cpa2, 0.0, 10.0);
        EXPECT_NEAR(tcpa2, 1500.0 / (20.0 * kKnotsToMps), 2.0);

        // 3. Both vessels stationary
        double cpa3 { 0.0 };
        double tcpa3 { 0.0 };
        TargetThreatEvaluator::calculateCpa(ownPos, 0.0, 0.0, targetPos, 0.0, 0.0, cpa3, tcpa3);
        EXPECT_DOUBLE_EQ(tcpa3, 0.0);
        EXPECT_NEAR(cpa3, 1500.0, 10.0);
    }

    TEST(TestCpaTcpaEdgeCases, ExtremeHighSpeedClosure)
    {
        const Klv::GeoPoint3D ownPos { 37.0, -122.0, 0.0 };
        const Klv::GeoPoint2D targetPt
            = Klv::KlvGeodesy::directGeodetic({ ownPos.latitudeDeg, ownPos.longitudeDeg }, 045.0, 2778.0); // 1.5 NM
        const Klv::GeoPoint3D targetPos { targetPt.latitudeDeg, targetPt.longitudeDeg, 0.0 };

        // Fast intercept craft closing at 55 knots
        double cpaMeters { 0.0 };
        double tcpaSeconds { 0.0 };
        TargetThreatEvaluator::calculateCpa(ownPos, 15.0, 045.0, targetPos, 55.0, 225.0, cpaMeters, tcpaSeconds);

        // Closure speed = 70 knots ~ 36.01 m/s. Range = 2778m -> TCPA ~ 77.1 seconds
        EXPECT_NEAR(cpaMeters, 0.0, 15.0);
        EXPECT_NEAR(tcpaSeconds, 77.1, 2.0);
    }

    // =========================================================================
    // Suite 2: Multi-Target Fusion & Priority Ranking
    // =========================================================================

    TEST(TestMultiTargetFusionRanking, RadarToAisFusionCorrelation)
    {
        TargetThreatEvaluator evaluator {};

        Nmea::NmeaNavSnapshot ownNav {};
        ownNav.hasPosition = true;
        ownNav.position.latitudeDeg = 37.0;
        ownNav.position.longitudeDeg = -122.0;
        ownNav.hasHeading = true;
        ownNav.trueHeadingDegrees = 0.0;
        ownNav.sogKnots = 10.0;
        ownNav.cogDegrees = 0.0;

        // Target at 1.0 NM (1852m) bearing 045 deg
        const Klv::GeoPoint2D tgtPt
            = Klv::KlvGeodesy::directGeodetic({ ownNav.position.latitudeDeg, ownNav.position.longitudeDeg }, 45.0, 1852.0);

        // 1. Radar ARPA target track 10
        Nmea::TtmData radarTgt {};
        radarTgt.targetNumber = 10U;
        radarTgt.targetDistanceNmi = 1.0;
        radarTgt.bearingDegrees = 45.0;
        radarTgt.bearingReference = Nmea::TtmReference::True;
        radarTgt.targetSpeedKnots = 14.0;
        radarTgt.targetCourseDegrees = 225.0;
        radarTgt.courseReference = Nmea::TtmReference::True;
        radarTgt.status = Nmea::TtmTargetStatus::Tracking;
        radarTgt.targetName = "RADAR_CONTACT";

        // 2. Matching AIS vessel within 50m and 0.5 kts
        Nmea::AisVesselTarget aisTgt {};
        aisTgt.mmsi = 239999999U;
        aisTgt.vesselName = "MERCHANT_VESSEL";
        aisTgt.coordinates.latitudeDeg = tgtPt.latitudeDeg;
        aisTgt.coordinates.longitudeDeg = tgtPt.longitudeDeg;
        aisTgt.positionValid = true;
        aisTgt.speedOverGroundKnots = 14.2;
        aisTgt.courseOverGroundDegrees = 224.5;
        aisTgt.shipType = 70U; // Cargo

        const std::size_t evaluatedCount = evaluator.evaluate(ownNav, { radarTgt }, { aisTgt });
        // Fused into a single target
        EXPECT_EQ(evaluatedCount, 1U);

        const auto targets = evaluator.getPrioritizedTargets();
        ASSERT_EQ(targets.size(), 1U);
        EXPECT_EQ(targets[0].source, TargetTrackSource::FusedRadarAis);
        EXPECT_EQ(targets[0].associatedRadarId.value_or(0U), 10U);
        EXPECT_EQ(targets[0].associatedAisMmsi.value_or(0U), 239999999U);
        EXPECT_FALSE(targets[0].isDarkVessel);
    }

    TEST(TestMultiTargetFusionRanking, DarkVesselDetectionHeuristic)
    {
        TargetThreatEvaluator evaluator {};

        Nmea::NmeaNavSnapshot ownNav {};
        ownNav.hasPosition = true;
        ownNav.position.latitudeDeg = 37.0;
        ownNav.position.longitudeDeg = -122.0;
        ownNav.hasHeading = true;
        ownNav.trueHeadingDegrees = 0.0;
        ownNav.sogKnots = 10.0;
        ownNav.cogDegrees = 0.0;

        // Radar target with NO matching AIS in vicinity
        Nmea::TtmData radarTgt {};
        radarTgt.targetNumber = 77U;
        radarTgt.targetDistanceNmi = 1.2;
        radarTgt.bearingDegrees = 300.0;
        radarTgt.bearingReference = Nmea::TtmReference::True;
        radarTgt.targetSpeedKnots = 28.0;
        radarTgt.targetCourseDegrees = 120.0;
        radarTgt.courseReference = Nmea::TtmReference::True;
        radarTgt.status = Nmea::TtmTargetStatus::Tracking;

        // AIS target far away (5 NM away)
        const Klv::GeoPoint2D farPt
            = Klv::KlvGeodesy::directGeodetic({ ownNav.position.latitudeDeg, ownNav.position.longitudeDeg }, 090.0, 9260.0);
        Nmea::AisVesselTarget distantAis {};
        distantAis.mmsi = 311000111U;
        distantAis.coordinates.latitudeDeg = farPt.latitudeDeg;
        distantAis.coordinates.longitudeDeg = farPt.longitudeDeg;
        distantAis.positionValid = true;
        distantAis.speedOverGroundKnots = 8.0;

        evaluator.evaluate(ownNav, { radarTgt }, { distantAis });

        const auto targets = evaluator.getPrioritizedTargets();
        ASSERT_EQ(targets.size(), 2U);

        // Radar target 77 should be classified as Dark Vessel and ranked #1
        EXPECT_EQ(targets[0].targetId, 77U);
        EXPECT_TRUE(targets[0].isDarkVessel);
        EXPECT_GT(targets[0].threatScore, targets[1].threatScore);
    }

    TEST(TestMultiTargetFusionRanking, MultiTargetQueueAndCooldown)
    {
        TargetThreatEvaluator evaluator {};

        Nmea::NmeaNavSnapshot ownNav {};
        ownNav.hasPosition = true;
        ownNav.position.latitudeDeg = 37.0;
        ownNav.position.longitudeDeg = -122.0;
        ownNav.hasHeading = true;
        ownNav.trueHeadingDegrees = 0.0;

        // Target A: Low threat distant cargo
        Nmea::TtmData tgtA {};
        tgtA.targetNumber = 1U;
        tgtA.targetDistanceNmi = 4.0;
        tgtA.bearingDegrees = 090.0;
        tgtA.bearingReference = Nmea::TtmReference::True;
        tgtA.targetSpeedKnots = 8.0;
        tgtA.targetCourseDegrees = 090.0;
        tgtA.courseReference = Nmea::TtmReference::True;
        tgtA.status = Nmea::TtmTargetStatus::Tracking;

        // Target B: Moderate threat crossing ferry
        Nmea::TtmData tgtB {};
        tgtB.targetNumber = 2U;
        tgtB.targetDistanceNmi = 2.0;
        tgtB.bearingDegrees = 045.0;
        tgtB.bearingReference = Nmea::TtmReference::True;
        tgtB.targetSpeedKnots = 18.0;
        tgtB.targetCourseDegrees = 225.0;
        tgtB.courseReference = Nmea::TtmReference::True;
        tgtB.status = Nmea::TtmTargetStatus::Tracking;

        // Target C: Critical threat fast closing dark craft
        Nmea::TtmData tgtC {};
        tgtC.targetNumber = 3U;
        tgtC.targetDistanceNmi = 0.8;
        tgtC.bearingDegrees = 000.0;
        tgtC.bearingReference = Nmea::TtmReference::True;
        tgtC.targetSpeedKnots = 32.0;
        tgtC.targetCourseDegrees = 180.0;
        tgtC.courseReference = Nmea::TtmReference::True;
        tgtC.status = Nmea::TtmTargetStatus::Tracking;

        evaluator.evaluate(ownNav, { tgtA, tgtB, tgtC }, {});

        const auto ranked = evaluator.getPrioritizedTargets();
        ASSERT_EQ(ranked.size(), 3U);
        EXPECT_EQ(ranked[0].targetId, 3U); // Highest threat
        EXPECT_EQ(ranked[1].targetId, 2U); // Medium threat
        EXPECT_EQ(ranked[2].targetId, 1U); // Low threat

        // Mark target 3 as inspected
        evaluator.markTargetInspected(3U, TargetTrackSource::RadarArpa);
        EXPECT_TRUE(evaluator.isTargetInCooldown(3U, TargetTrackSource::RadarArpa));

        // Next candidate should be target 2
        const auto nextCandidate = evaluator.getNextUninspectedCandidate(10.0);
        ASSERT_TRUE(nextCandidate.has_value());
        EXPECT_EQ(nextCandidate->targetId, 2U);
    }

    // =========================================================================
    // Suite 3: End-to-End Maritime Simulation with Replay Transport
    // =========================================================================

    TEST(TestEndToEndSlewToCueLifecycle, FullStackReplayPreemptionAndPatrolResumption)
    {
        // 1. Physical / Simulated Payload & Gimbal Setup
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());
        auto ptu = payload->panTilt();
        auto camera = payload->primaryCamera();

        // 2. Preset Manager & Tour Engine (Cyclical Patrol)
        auto presetMgr = std::make_shared<LocalPresetManager>(ptu, camera);
        PtzPreset p1 {};
        p1.id = 1;
        p1.panAngleDeg = 10.0;
        p1.tiltAngleDeg = -5.0;
        presetMgr->savePreset(p1);

        PtzPreset p2 {};
        p2.id = 2;
        p2.panAngleDeg = 20.0;
        p2.tiltAngleDeg = -5.0;
        presetMgr->savePreset(p2);

        auto tourEngine = std::make_shared<TourEngine>(ptu, camera, presetMgr);
        TourDefinition tourDef {};
        tourDef.tourId = "horizon_patrol";
        tourDef.name = "Horizon Guard Patrol";
        tourDef.loop = true;
        tourDef.waypoints = {
            { 1, std::chrono::milliseconds(200), 1.0f },
            { 2, std::chrono::milliseconds(200), 1.0f }
        };
        ASSERT_TRUE(tourEngine->registerTour(tourDef));
        ASSERT_TRUE(tourEngine->startTour("horizon_patrol"));
        EXPECT_NE(tourEngine->status().state, TourState::Idle);

        // 3. Navigation & Slew-to-Cue Infrastructure
        auto geoLock = std::make_shared<GeoLockController>(payload);
        auto attitudeComp = std::make_shared<VesselAttitudeCompensator>();
        geoLock->setAttitudeCompensator(attitudeComp);

        auto replayTransport = std::make_shared<Nmea::NmeaReplayTransport>();
        auto nmeaDevice = std::make_shared<Nmea::NmeaDevice>(replayTransport);
        ASSERT_TRUE(nmeaDevice->start());

        auto slavingBridge = std::make_shared<NmeaSlavingBridge>(nmeaDevice, geoLock);
        slavingBridge->setAttitudeCompensator(attitudeComp);
        slavingBridge->setLockToleranceDeg(1.0);

        auto threatEvaluator = std::make_shared<TargetThreatEvaluator>();
        AutoFramingConfig frameCfg {};
        frameCfg.maxZoomVelocityPerSec = 0.5;
        auto framing = std::make_shared<AutoFramingController>(frameCfg);

        auto autoTracker = std::make_shared<PayloadAutoTrackerBridge>(payload);

        SlewToCueConfig cueCfg {};
        cueCfg.autonomousEngagement = true;
        cueCfg.minThreatScoreToCue = 30.0;
        cueCfg.opticalAcquisitionTimeout = std::chrono::milliseconds(50);
        cueCfg.inspectionDwellDuration = std::chrono::milliseconds(50);
        cueCfg.waitForZoomConvergence = true;

        auto director = std::make_shared<SlewToCueDirector>(
            threatEvaluator, slavingBridge, framing, autoTracker, camera, geoLock, tourEngine, cueCfg);

        // 4. Construct Synthetic Voyage Log
        // - Own ship at (37.0, -122.0), heading 090, speed 12 knots, rolling/pitching in swell
        // - Fast unidentified radar contact at 1.0 NM closing at 35 knots
        std::string voyageLog {};
        voyageLog += Nmea::NmeaChecksum::frameSentence("GPGGA,120000.00,3700.000,N,12200.000,W,1,08,0.9,10.0,M,0.0,M,,");
        voyageLog += Nmea::NmeaChecksum::frameSentence("HEHDT,090.0,T");
        voyageLog += Nmea::NmeaChecksum::frameSentence("GPRMC,120000.00,A,3700.000,N,12200.000,W,12.0,090.0,290926,,,A");
        voyageLog += Nmea::NmeaChecksum::frameSentence("PASHR,120000.00,090.0,T,-03.5,+01.8,0.05,0.05,0.08,1.0,1");
        voyageLog += Nmea::NmeaChecksum::frameSentence("RATTM,101,1.0,090.0,T,35.0,270.0,T,0.0,1.7,K,FAST_INTERCEPT,T,,120000,A");

        ASSERT_TRUE(replayTransport->loadFromMemory(voyageLog, std::chrono::milliseconds(10)));
        ASSERT_TRUE(replayTransport->open());

        // Step through replay transport to inject all sentences into NmeaDevice
        while (replayTransport->state() != Nmea::ReplayState::Finished) {
            replayTransport->stepForward();
        }

        // Evaluate threats from ingested telemetry
        const auto snap = nmeaDevice->navSnapshot();
        ASSERT_TRUE(snap.hasPosition);
        const auto radarList = nmeaDevice->activeRadarTargets();
        ASSERT_EQ(radarList.size(), 1U);
        threatEvaluator->evaluate(snap, radarList, nmeaDevice->activeAisTargets());

        const auto highest = threatEvaluator->getHighestThreatTarget();
        ASSERT_TRUE(highest.has_value());
        EXPECT_EQ(highest->targetId, 101U);
        EXPECT_TRUE(highest->isDarkVessel);

        // 5. Execute SlewToCueDirector Autonomous Lifecycle

        // Update 1: Autonomous preemption detected -> Suspends TourEngine, starts Slewing
        director->update();
        EXPECT_TRUE(director->isCueingActive());
        EXPECT_EQ(director->status().state, CueingState::SlewingToTarget);
        EXPECT_EQ(director->status().activeTargetId, 101U);
        EXPECT_TRUE(director->isTourSuspended());
        EXPECT_EQ(tourEngine->status().state, TourState::Paused);

        // Update 2: Gimbal reaches boresight lock -> Transitions to FramingTarget
        director->update();
        EXPECT_EQ(director->status().state, CueingState::FramingTarget);

        // Settle zoom convergence
        framing->update(*camera, std::chrono::milliseconds(3000));
        EXPECT_TRUE(framing->isZoomConverged());

        // Update 3: Transitions to AcquiringOpticalLock and engages tracker
        director->update();
        EXPECT_EQ(director->status().state, CueingState::AcquiringOpticalLock);
        EXPECT_TRUE(autoTracker->isEngaged());

        // Wait for optical acquisition timeout (50ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(60));

        // Update 4: Acquires / dwells via geodetic coasting or tracker
        director->update();
        EXPECT_EQ(director->status().state, CueingState::DwellInspection);

        // Elapse inspection dwell timeout (50ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(60));

        // Update 5: Dwell expires -> Marks target inspected, returns to Idle, resumes TourEngine!
        director->update(); // TargetCompleted
        director->update(); // Idle
        EXPECT_EQ(director->status().state, CueingState::Idle);
        EXPECT_FALSE(director->isCueingActive());
        EXPECT_FALSE(director->isTourSuspended());
        EXPECT_NE(tourEngine->status().state, TourState::Paused);
    }

    // =========================================================================
    // Suite 4: Dynamic Attitude Stabilization & Horizon Leveling
    // =========================================================================

    TEST(TestDynamicAttitudeStabilizationScenario, PitchRollCompensatedKinematicsAndCounterRoll)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        auto geoLock = std::make_shared<GeoLockController>(payload);
        auto attitudeComp = std::make_shared<VesselAttitudeCompensator>();
        geoLock->setAttitudeCompensator(attitudeComp);

        // Target directly North on the horizon (Az = 0.0, El = 0.0)
        const Klv::GeoPoint3D ownPos { 37.0, -122.0, 0.0 };
        const Klv::GeoPoint2D tgtPt
            = Klv::KlvGeodesy::directGeodetic({ ownPos.latitudeDeg, ownPos.longitudeDeg }, 0.0, 2000.0);
        const Klv::GeoPoint3D targetPos { tgtPt.latitudeDeg, tgtPt.longitudeDeg, 0.0 };
        geoLock->engage(targetPos);

        // 1. Vessel Pitch = +5.0 deg (bow up), Heading = 000 deg, Roll = 0.0 deg
        attitudeComp->updateAttitude(0.0, 5.0, 0.0, 0.0);
        geoLock->updatePlatform(ownPos, 0.0);

        // Looking North with bow up requires the camera to tilt down (-5 deg) relative to deck
        auto st1 = geoLock->status();
        EXPECT_NEAR(st1.commandedPanDeg, 0.0, 0.2);
        EXPECT_NEAR(st1.commandedTiltDeg, -5.0, 0.5);

        // 2. Vessel Roll = +10.0 deg (starboard down), Pitch = 0.0 deg, Looking Starboard (Az = 90 deg)
        const Klv::GeoPoint2D tgtEastPt
            = Klv::KlvGeodesy::directGeodetic({ ownPos.latitudeDeg, ownPos.longitudeDeg }, 90.0, 2000.0);
        geoLock->engage({ tgtEastPt.latitudeDeg, tgtEastPt.longitudeDeg, 0.0 });

        attitudeComp->updateAttitude(0.0, 0.0, 10.0, 0.0);
        geoLock->updatePlatform(ownPos, 0.0);

        // Looking starboard with starboard deck tilted down (+10 deg) requires tilting UP (+10 deg)
        auto st2 = geoLock->status();
        EXPECT_NEAR(st2.commandedPanDeg, 90.0, 0.5);
        EXPECT_NEAR(st2.commandedTiltDeg, 10.0, 0.5);

        // Active 3-axis horizon counter-roll: when looking along vessel centerline (pan=0),
        // counter-roll must oppose vessel roll: Phi_gimbal = -roll = -10 deg
        const double horizonRollCenter = attitudeComp->computeHorizonRoll(0.0, 0.0);
        EXPECT_NEAR(horizonRollCenter, -10.0, 0.1);

        // When looking beam-to (pan = 90 deg), vessel pitch becomes apparent roll
        attitudeComp->updateAttitude(0.0, 4.0, 0.0, 0.0);
        const double horizonRollBeam = attitudeComp->computeHorizonRoll(90.0, 0.0);
        EXPECT_NEAR(horizonRollBeam, -4.0, 0.1);
    }

    TEST(TestDynamicAttitudeStabilizationScenario, AttitudeTimeoutWatchdogDegradation)
    {
        auto attitudeComp = std::make_shared<VesselAttitudeCompensator>();
        attitudeComp->setAttitudeTimeout(std::chrono::milliseconds(50));

        attitudeComp->updateAttitude(0.0, 5.0, 10.0, 0.0);
        EXPECT_TRUE(attitudeComp->hasValidAttitude());

        // Wait for timeout
        std::this_thread::sleep_for(std::chrono::milliseconds(70));
        EXPECT_FALSE(attitudeComp->hasValidAttitude());

        // Calling compensateLookAngles falls back safely to 2D deck model
        PlatformLeverArmConfig armCfg {};
        const Klv::GeoPoint3D platformPos { 37.0, -122.0, 0.0 };
        const Klv::GeoPoint3D targetPos { 37.01, -122.0, 0.0 };
        const auto look = attitudeComp->compensateLookAngles(platformPos, targetPos, armCfg);

        // When timed out, output look angles are valid without NaN
        EXPECT_FALSE(std::isnan(look.panAngleDeg));
        EXPECT_FALSE(std::isnan(look.tiltAngleDeg));
        EXPECT_NEAR(look.tiltAngleDeg, 0.0, 0.2);
    }

    // =========================================================================
    // Suite 5: Emergency Distress Beacon Immediate Preemption
    // =========================================================================

    TEST(TestEmergencyDistressPreemption, ImmediateDwellInterruptionByDistressBeacon)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());
        auto threatEvaluator = std::make_shared<TargetThreatEvaluator>();

        // Own ship
        Nmea::NmeaNavSnapshot ownNav {};
        ownNav.hasPosition = true;
        ownNav.position.latitudeDeg = 37.0;
        ownNav.position.longitudeDeg = -122.0;
        ownNav.hasHeading = true;
        ownNav.trueHeadingDegrees = 0.0;

        // 1. Initial moderate threat (Radar target 5)
        Nmea::TtmData radarTgt {};
        radarTgt.targetNumber = 5U;
        radarTgt.targetDistanceNmi = 1.0;
        radarTgt.bearingDegrees = 45.0;
        radarTgt.bearingReference = Nmea::TtmReference::True;
        radarTgt.targetSpeedKnots = 10.0;
        radarTgt.targetCourseDegrees = 225.0;
        radarTgt.courseReference = Nmea::TtmReference::True;
        radarTgt.status = Nmea::TtmTargetStatus::Tracking;

        threatEvaluator->evaluate(ownNav, { radarTgt }, {});
        EXPECT_EQ(threatEvaluator->getHighestThreatTarget()->targetId, 5U);

        // 2. Emergency Distress Beacon arrives (AIS-SART MMSI 970123456)
        Nmea::AisVesselTarget sartBeacon {};
        sartBeacon.mmsi = 970123456U;
        sartBeacon.vesselName = "AIS-SART ACTIVE";
        sartBeacon.coordinates.latitudeDeg = 37.01;
        sartBeacon.coordinates.longitudeDeg = -122.01;
        sartBeacon.positionValid = true;
        sartBeacon.isEmergencyBeacon = true;
        sartBeacon.beaconType = Nmea::AisBeaconType::AisSart;

        threatEvaluator->evaluate(ownNav, { radarTgt }, { sartBeacon });

        // Emergency beacon immediately takes Priority #1 override
        const auto topThreat = threatEvaluator->getHighestThreatTarget();
        ASSERT_TRUE(topThreat.has_value());
        EXPECT_EQ(topThreat->targetId, 970123456U);
        EXPECT_TRUE(topThreat->isEmergency);
        EXPECT_EQ(topThreat->threatLevel, ThreatLevel::Emergency);
        EXPECT_GE(topThreat->threatScore, 1000.0);
    }

    // =========================================================================
    // Suite 6: Fault Tolerance & Geodetic Fallback Resilience
    // =========================================================================

    TEST(TestSimulationFaultTolerance, VideoTrackerDropGeodeticFallbackResilience)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());
        auto geoLock = std::make_shared<GeoLockController>(payload);

        auto transport = std::make_shared<Nmea::NmeaReplayTransport>();
        auto nmeaDevice = std::make_shared<Nmea::NmeaDevice>(transport);
        ASSERT_TRUE(nmeaDevice->start());

        auto slavingBridge = std::make_shared<NmeaSlavingBridge>(nmeaDevice, geoLock);
        auto threatEvaluator = std::make_shared<TargetThreatEvaluator>();
        auto framing = std::make_shared<AutoFramingController>();
        auto autoTracker = std::make_shared<PayloadAutoTrackerBridge>(payload);

        SlewToCueConfig cfg {};
        cfg.autonomousEngagement = false;
        cfg.autoOpticalHandover = true;
        cfg.opticalAcquisitionTimeout = std::chrono::milliseconds(20);
        cfg.inspectionDwellDuration = std::chrono::milliseconds(200);

        auto director = std::make_shared<SlewToCueDirector>(
            threatEvaluator, slavingBridge, framing, autoTracker, payload->primaryCamera(), geoLock, cfg);

        // Inject own ship and target
        std::string log {};
        log += Nmea::NmeaChecksum::frameSentence("GPGGA,120000,3700.000,N,12200.000,W,1,08,1.0,0.0,M,0.0,M,,");
        log += Nmea::NmeaChecksum::frameSentence("HEHDT,000.0,T");
        log += Nmea::NmeaChecksum::frameSentence("RATTM,42,1.0,000.0,T,20.0,180.0,T,0.0,3.0,K,FAST_TARGET,T,,120000,A");
        ASSERT_TRUE(transport->loadFromMemory(log));
        ASSERT_TRUE(transport->open());
        while (transport->state() != Nmea::ReplayState::Finished) {
            transport->stepForward();
        }

        threatEvaluator->evaluate(nmeaDevice->navSnapshot(), nmeaDevice->activeRadarTargets(), {});

        // Manually cue radar target 42
        ASSERT_TRUE(director->cueRadarTarget(42U));
        EXPECT_EQ(director->status().state, CueingState::SlewingToTarget);

        // Step to FramingTarget
        director->update();
        EXPECT_EQ(director->status().state, CueingState::FramingTarget);

        // Step to AcquiringOpticalLock
        director->update();
        EXPECT_EQ(director->status().state, CueingState::AcquiringOpticalLock);

        // Optical tracker does not acquire within timeout (20ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        // Director update must smoothly fall back to GeodeticTrackingFallback and dwell without aborting!
        director->update();
        EXPECT_EQ(director->status().state, CueingState::DwellInspection);
        EXPECT_TRUE(director->isGeodeticFallback());
        EXPECT_TRUE(director->status().isGeodeticFallback);
        EXPECT_TRUE(director->isCueingActive());
    }

} // namespace
} // namespace PayloadHal
