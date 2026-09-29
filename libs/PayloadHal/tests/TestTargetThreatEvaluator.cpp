#include "TargetThreatEvaluator.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <vector>

namespace PayloadHal {
namespace {

    constexpr double kKnotsToMps = 0.5144444444444444;

    TEST(TestTargetThreatEvaluator, HeadOnCollisionCpaCalculation)
    {
        // Own ship at (37.0, -122.0), moving North (0 deg) at 10 knots
        const Klv::GeoPoint3D ownPos { 37.0, -122.0, 0.0 };
        const double ownSog = 10.0;
        const double ownCog = 0.0;

        // Target located 2.0 NM (3704 meters) directly North, moving South (180 deg) at 10 knots
        // Direct head-on collision course
        const Klv::GeoPoint2D pTarget2D
            = Klv::KlvGeodesy::directGeodetic({ ownPos.latitudeDeg, ownPos.longitudeDeg }, 0.0, 3704.0);
        const Klv::GeoPoint3D targetPos { pTarget2D.latitudeDeg, pTarget2D.longitudeDeg, 0.0 };
        const double targetSog = 10.0;
        const double targetCog = 180.0;

        double cpaMeters { 0.0 };
        double tcpaSeconds { 0.0 };

        TargetThreatEvaluator::calculateCpa(
            ownPos, ownSog, ownCog, targetPos, targetSog, targetCog, cpaMeters, tcpaSeconds);

        // CPA distance should be approximately 0 meters (direct collision)
        EXPECT_NEAR(cpaMeters, 0.0, 10.0);

        // Relative closure speed = 20 knots = 10.2888 m/s
        // TCPA = 3704 m / 10.2888 m/s = 360 seconds (6 minutes)
        EXPECT_NEAR(tcpaSeconds, 360.0, 2.0);
    }

    TEST(TestTargetThreatEvaluator, PerpendicularCrossingCpaCalculation)
    {
        // Own ship at (37.0, -122.0), moving North (0 deg) at 10 knots
        const Klv::GeoPoint3D ownPos { 37.0, -122.0, 0.0 };
        const double ownSog = 10.0; // ~5.144 m/s
        const double ownCog = 0.0;

        // Target 2000 m East, moving West (270 deg) at 10 knots
        const Klv::GeoPoint2D pTarget2D
            = Klv::KlvGeodesy::directGeodetic({ ownPos.latitudeDeg, ownPos.longitudeDeg }, 90.0, 2000.0);
        const Klv::GeoPoint3D targetPos { pTarget2D.latitudeDeg, pTarget2D.longitudeDeg, 0.0 };
        const double targetSog = 10.0;
        const double targetCog = 270.0;

        double cpaMeters { 0.0 };
        double tcpaSeconds { 0.0 };

        TargetThreatEvaluator::calculateCpa(
            ownPos, ownSog, ownCog, targetPos, targetSog, targetCog, cpaMeters, tcpaSeconds);

        // Target crosses at origin in 2000 / (10 * 0.51444) ~ 388.7 seconds.
        // At that time, own ship is also at y = 10 * 0.51444 * 388.7 = 2000 m North.
        // The relative velocity is (-5.144, -5.144). Relative speed = 5.144 * sqrt(2) = 7.275 m/s.
        // Distance delta is (2000, 0).
        // tCPA = -(2000 * (-5.144) + 0) / (7.275^2) = (2000 * 5.144) / (2 * 5.144^2) = 2000 / (2 * 5.144) = 194.38 s.
        // CPA distance = sqrt((2000 - 5.144 * 194.38)^2 + (-5.144 * 194.38)^2) = sqrt(1000^2 + (-1000)^2) = 1414.2 m.
        EXPECT_NEAR(tcpaSeconds, 194.4, 2.0);
        EXPECT_NEAR(cpaMeters, 1414.2, 10.0);
    }

    TEST(TestTargetThreatEvaluator, DivergingVesselsPastCpa)
    {
        // Own ship at (37.0, -122.0) moving North (0 deg) at 10 knots
        const Klv::GeoPoint3D ownPos { 37.0, -122.0, 0.0 };
        const double ownSog = 10.0;
        const double ownCog = 0.0;

        // Target 2000 m North, but moving North at 20 knots (pulling away)
        const Klv::GeoPoint2D pTarget2D
            = Klv::KlvGeodesy::directGeodetic({ ownPos.latitudeDeg, ownPos.longitudeDeg }, 0.0, 2000.0);
        const Klv::GeoPoint3D targetPos { pTarget2D.latitudeDeg, pTarget2D.longitudeDeg, 0.0 };
        const double targetSog = 20.0;
        const double targetCog = 0.0;

        double cpaMeters { 0.0 };
        double tcpaSeconds { 0.0 };

        TargetThreatEvaluator::calculateCpa(
            ownPos, ownSog, ownCog, targetPos, targetSog, targetCog, cpaMeters, tcpaSeconds);

        // Vessels are opening/diverging; TCPA must be negative
        EXPECT_LT(tcpaSeconds, 0.0);
        // Minimum distance was in the past; current distance is 2000 m
        EXPECT_LE(cpaMeters, 2005.0);
    }

    TEST(TestTargetThreatEvaluator, MultiTargetPriorityRankingAndDarkVessel)
    {
        TargetThreatEvaluator evaluator;

        Nmea::NmeaNavSnapshot ownNav;
        ownNav.hasPosition = true;
        ownNav.position.latitudeDeg = 37.0;
        ownNav.position.longitudeDeg = -122.0;
        ownNav.hasHeading = true;
        ownNav.trueHeadingDegrees = 0.0;
        ownNav.sogKnots = 10.0;
        ownNav.cogDegrees = 0.0;

        // 1. Distant benign AIS cargo vessel at 5 NM moving away
        Nmea::AisVesselTarget distantAis;
        distantAis.mmsi = 111222333;
        distantAis.vesselName = "PACIFIC_TRADER";
        const auto pDistant = Klv::KlvGeodesy::directGeodetic(
            { ownNav.position.latitudeDeg, ownNav.position.longitudeDeg }, 45.0, 5.0 * 1852.0);
        distantAis.coordinates.latitudeDeg = pDistant.latitudeDeg;
        distantAis.coordinates.longitudeDeg = pDistant.longitudeDeg;
        distantAis.speedOverGroundKnots = 12.0;
        distantAis.courseOverGroundDegrees = 45.0;

        // 2. Unidentified fast radar contact (Dark Vessel) closing at 25 knots from 1.5 NM
        Nmea::TtmData darkRadar;
        darkRadar.targetNumber = 12;
        darkRadar.targetName = "UNIDENTIFIED_SKIF";
        darkRadar.targetDistanceNmi = 1.5;
        darkRadar.bearingDegrees = 10.0;
        darkRadar.bearingReference = Nmea::TtmReference::True;
        darkRadar.targetSpeedKnots = 25.0;
        darkRadar.targetCourseDegrees = 190.0; // Heading back towards us
        darkRadar.status = Nmea::TtmTargetStatus::Tracking;

        // 3. Fused Radar & AIS contact at 1.0 NM closing slowly
        const auto pAis2 = Klv::KlvGeodesy::directGeodetic(
            { ownNav.position.latitudeDeg, ownNav.position.longitudeDeg }, 340.0, 1.0 * 1852.0);
        Nmea::AisVesselTarget fusedAis;
        fusedAis.mmsi = 999888777;
        fusedAis.vesselName = "COASTAL_FERRY";
        fusedAis.coordinates.latitudeDeg = pAis2.latitudeDeg;
        fusedAis.coordinates.longitudeDeg = pAis2.longitudeDeg;
        fusedAis.speedOverGroundKnots = 8.0;
        fusedAis.courseOverGroundDegrees = 160.0;

        Nmea::TtmData fusedRadar;
        fusedRadar.targetNumber = 5;
        fusedRadar.targetDistanceNmi = 1.0;
        fusedRadar.bearingDegrees = 340.0;
        fusedRadar.bearingReference = Nmea::TtmReference::True;
        fusedRadar.targetSpeedKnots = 8.0;
        fusedRadar.targetCourseDegrees = 160.0;
        fusedRadar.status = Nmea::TtmTargetStatus::Tracking;

        const std::size_t count = evaluator.evaluate(ownNav, { darkRadar, fusedRadar }, { distantAis, fusedAis });

        // Total evaluated targets: 3 (dark radar, fused radar/ais, distant ais)
        EXPECT_EQ(count, 3U);

        const auto prioritized = evaluator.getPrioritizedTargets();
        ASSERT_EQ(prioritized.size(), 3U);

        // Top threat should be the high-speed closing Dark Vessel
        const auto& top = prioritized.front();
        EXPECT_EQ(top.targetId, 12U);
        EXPECT_TRUE(top.isDarkVessel);
        EXPECT_TRUE(top.isClosing);
        EXPECT_GT(top.sogKnots, 20.0);
        EXPECT_GE(top.threatScore, 40.0);

        // Fused contact should have FusedRadarAis source and both IDs preserved
        const auto fusedTargetOpt = evaluator.getTarget(5U, TargetTrackSource::FusedRadarAis);
        ASSERT_TRUE(fusedTargetOpt.has_value());
        EXPECT_EQ(fusedTargetOpt->associatedAisMmsi.value_or(0U), 999888777U);
        EXPECT_FALSE(fusedTargetOpt->isDarkVessel);

        // Distant vessel should have lowest threat score
        EXPECT_LT(prioritized.back().threatScore, top.threatScore);
    }

    TEST(TestTargetThreatEvaluator, EmergencyBeaconPreemption)
    {
        TargetThreatEvaluator evaluator;

        Nmea::NmeaNavSnapshot ownNav;
        ownNav.hasPosition = true;
        ownNav.position.latitudeDeg = 37.0;
        ownNav.position.longitudeDeg = -122.0;

        // Emergency AIS-SART beacon
        Nmea::AisVesselTarget sartTarget;
        sartTarget.mmsi = 970012345;
        sartTarget.vesselName = "AIS-SART 970012345";
        sartTarget.coordinates.latitudeDeg = 37.02;
        sartTarget.coordinates.longitudeDeg = -122.01;
        sartTarget.beaconType = Nmea::AisBeaconType::AisSart;
        sartTarget.isEmergencyBeacon = true;

        // Standard radar target
        Nmea::TtmData radarTarget;
        radarTarget.targetNumber = 1;
        radarTarget.targetDistanceNmi = 0.5;
        radarTarget.bearingDegrees = 180.0;
        radarTarget.bearingReference = Nmea::TtmReference::True;
        radarTarget.targetSpeedKnots = 10.0;
        radarTarget.targetCourseDegrees = 0.0;
        radarTarget.status = Nmea::TtmTargetStatus::Tracking;

        bool alertFired { false };
        evaluator.setThreatAlertCallback([&](const EvaluatedTarget& target) {
            if (target.isEmergency) {
                alertFired = true;
            }
        });

        evaluator.evaluate(ownNav, { radarTarget }, { sartTarget });

        const auto topOpt = evaluator.getHighestThreatTarget();
        ASSERT_TRUE(topOpt.has_value());

        // Emergency beacon must be top priority with ThreatLevel::Emergency
        EXPECT_EQ(topOpt->targetId, 970012345U);
        EXPECT_TRUE(topOpt->isEmergency);
        EXPECT_EQ(topOpt->threatLevel, ThreatLevel::Emergency);
        EXPECT_GE(topOpt->threatScore, 1000.0);
        EXPECT_TRUE(alertFired);
    }

    TEST(TestTargetThreatEvaluator, TargetInspectionCooldownAndQueueAdvancement)
    {
        TargetThreatEvaluator evaluator;

        Nmea::NmeaNavSnapshot ownNav;
        ownNav.hasPosition = true;
        ownNav.position.latitudeDeg = 37.0;
        ownNav.position.longitudeDeg = -122.0;

        // Radar Target #1 (imminent collision, higher threat)
        Nmea::TtmData radarTarget1;
        radarTarget1.targetNumber = 10;
        radarTarget1.targetDistanceNmi = 0.5;
        radarTarget1.bearingDegrees = 0.0;
        radarTarget1.bearingReference = Nmea::TtmReference::True;
        radarTarget1.targetSpeedKnots = 20.0;
        radarTarget1.targetCourseDegrees = 180.0;
        radarTarget1.status = Nmea::TtmTargetStatus::Tracking;

        // Radar Target #2 (crossing, lower threat)
        Nmea::TtmData radarTarget2;
        radarTarget2.targetNumber = 20;
        radarTarget2.targetDistanceNmi = 1.5;
        radarTarget2.bearingDegrees = 45.0;
        radarTarget2.bearingReference = Nmea::TtmReference::True;
        radarTarget2.targetSpeedKnots = 10.0;
        radarTarget2.targetCourseDegrees = 270.0;
        radarTarget2.status = Nmea::TtmTargetStatus::Tracking;

        evaluator.evaluate(ownNav, { radarTarget1, radarTarget2 }, {});

        const auto now = std::chrono::steady_clock::now();

        // Initially Target #10 is highest candidate
        auto cand1 = evaluator.getNextUninspectedCandidate(30.0, now);
        ASSERT_TRUE(cand1.has_value());
        EXPECT_EQ(cand1->targetId, 10U);

        // Mark Target #10 as inspected with 60-second cooldown
        evaluator.markTargetInspected(10U, TargetTrackSource::RadarArpa, std::chrono::milliseconds(60000));
        EXPECT_TRUE(evaluator.isTargetInCooldown(10U, TargetTrackSource::RadarArpa, now));
        EXPECT_FALSE(evaluator.isTargetInCooldown(20U, TargetTrackSource::RadarArpa, now));

        // Next candidate must advance to Target #20
        auto cand2 = evaluator.getNextUninspectedCandidate(30.0, now);
        ASSERT_TRUE(cand2.has_value());
        EXPECT_EQ(cand2->targetId, 20U);

        // Mark Target #20 as inspected
        evaluator.markTargetInspected(20U, TargetTrackSource::RadarArpa, std::chrono::milliseconds(60000));

        // Queue exhausted for uninspected candidates
        auto candNone = evaluator.getNextUninspectedCandidate(30.0, now);
        EXPECT_FALSE(candNone.has_value());

        // Fast forward 65 seconds into future
        const auto futureTime = now + std::chrono::seconds(65);
        EXPECT_FALSE(evaluator.isTargetInCooldown(10U, TargetTrackSource::RadarArpa, futureTime));
        EXPECT_FALSE(evaluator.isTargetInCooldown(20U, TargetTrackSource::RadarArpa, futureTime));

        // Target #10 becomes available again after cooldown
        auto candReactivated = evaluator.getNextUninspectedCandidate(30.0, futureTime);
        ASSERT_TRUE(candReactivated.has_value());
        EXPECT_EQ(candReactivated->targetId, 10U);

        // Cleanup expired cooldowns
        evaluator.cleanupExpiredCooldowns(futureTime);
        evaluator.clearCooldownHistory();
        EXPECT_FALSE(evaluator.isTargetInCooldown(10U, TargetTrackSource::RadarArpa, now));
    }

} // namespace
} // namespace PayloadHal
