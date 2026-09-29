#include "bam/BridgeAlertManager.h"
#include "NmeaSentenceParser.h"

#include <gtest/gtest.h>
#include <chrono>
#include <string>
#include <vector>

namespace Nmea::Bam {
namespace {

    TEST(TestBridgeAlertManager, RegisterAndRaiseAlert)
    {
        BridgeAlertManager bam {};
        std::vector<std::string> broadcasts {};
        bam.setBroadcastCallback([&broadcasts](const std::string& sentence) {
            broadcasts.push_back(sentence);
        });

        // 1. Raise Alarm
        const auto record = bam.registerAlert(1001U, 1U, AlertPriority::Alarm, AlertCategory::CategoryB,
            "Motor Overtemp", "Azimuth drive temperature exceeded 85C");

        EXPECT_EQ(record.alertIdentifier, 1001U);
        EXPECT_EQ(record.alertInstance, 1U);
        EXPECT_EQ(record.priority, AlertPriority::Alarm);
        EXPECT_EQ(record.state, AlertState::ActiveUnacknowledged);
        EXPECT_EQ(record.revisionCounter, 1U);
        EXPECT_EQ(record.escalationCounter, 0U);

        // Should broadcast $--ALF and legacy $--ALR
        ASSERT_GE(broadcasts.size(), 2U);
        EXPECT_NE(broadcasts[0].find("ALF"), std::string::npos);
        EXPECT_NE(broadcasts[1].find("ALR"), std::string::npos);

        // Active alerts query
        const auto active = bam.activeAlerts();
        ASSERT_EQ(active.size(), 1U);
        EXPECT_EQ(active[0].alertIdentifier, 1001U);
    }

    TEST(TestBridgeAlertManager, CautionBypassesUnack)
    {
        BridgeAlertManager bam {};
        const auto record = bam.registerAlert(1002U, 1U, AlertPriority::Caution, AlertCategory::CategoryB,
            "Sensor Drift", "Compass deviation approaching limit");

        // Cautions have no audible annunciator and go directly to ActiveAcknowledged
        EXPECT_EQ(record.state, AlertState::ActiveAcknowledged);
    }

    TEST(TestBridgeAlertManager, SilenceAndTimeoutRevert)
    {
        BamConfig cfg {};
        cfg.silenceTimeoutSec = std::chrono::seconds(10);
        BridgeAlertManager bam { cfg };

        bam.registerAlert(1003U, 1U, AlertPriority::Alarm, AlertCategory::CategoryB, "Loss of Lock", "Tracker Lost");

        // Silence the alert
        EXPECT_TRUE(bam.silenceAlert(1003U, 1U));
        auto record = bam.alertById(1003U, 1U);
        ASSERT_TRUE(record.has_value());
        EXPECT_EQ(record->state, AlertState::ActiveSilenced);
        EXPECT_TRUE(record->isSilenced);

        const auto now = std::chrono::steady_clock::now();
        // Poll at 5 seconds (not timed out yet)
        bam.pollEscalations(now + std::chrono::seconds(5));
        record = bam.alertById(1003U, 1U);
        EXPECT_EQ(record->state, AlertState::ActiveSilenced);

        // Poll at 12 seconds (timeout expired -> reverts to ActiveUnacknowledged)
        bam.pollEscalations(now + std::chrono::seconds(12));
        record = bam.alertById(1003U, 1U);
        EXPECT_EQ(record->state, AlertState::ActiveUnacknowledged);
        EXPECT_FALSE(record->isSilenced);
    }

    TEST(TestBridgeAlertManager, EscalationCounterIncrement)
    {
        BamConfig cfg {};
        cfg.escalationTimeoutSec = std::chrono::seconds(30);
        BridgeAlertManager bam { cfg };

        bam.registerAlert(1004U, 1U, AlertPriority::Alarm, AlertCategory::CategoryB, "Power Degradation", "Volt Drop");

        const auto now = std::chrono::steady_clock::now();
        bam.pollEscalations(now + std::chrono::seconds(10));
        auto record = bam.alertById(1004U, 1U);
        EXPECT_EQ(record->escalationCounter, 0U);

        // After 35 seconds, escalation counter should increment
        bam.pollEscalations(now + std::chrono::seconds(35));
        record = bam.alertById(1004U, 1U);
        EXPECT_EQ(record->escalationCounter, 1U);
    }

    TEST(TestBridgeAlertManager, AcknowledgeAndRectifyLifecycle)
    {
        BridgeAlertManager bam {};
        bam.registerAlert(1005U, 1U, AlertPriority::Alarm, AlertCategory::CategoryB, "Fault 1005", "Details");

        // 1. Acknowledge while still active
        EXPECT_TRUE(bam.acknowledgeAlert(1005U, 1U));
        auto record = bam.alertById(1005U, 1U);
        ASSERT_TRUE(record.has_value());
        EXPECT_EQ(record->state, AlertState::ActiveAcknowledged);

        // 2. Rectify -> condition cleared, transitions to Normal
        EXPECT_TRUE(bam.rectifyAlert(1005U, 1U));
        record = bam.alertById(1005U, 1U);
        ASSERT_TRUE(record.has_value());
        EXPECT_EQ(record->state, AlertState::Normal);
        EXPECT_TRUE(bam.activeAlerts().empty());
    }

    TEST(TestBridgeAlertManager, RectifyBeforeAcknowledge)
    {
        BridgeAlertManager bam {};
        bam.registerAlert(1006U, 1U, AlertPriority::Alarm, AlertCategory::CategoryB, "Fault 1006", "Details");

        // Rectify before crew acknowledged -> state becomes RectifiedUnacknowledged
        EXPECT_TRUE(bam.rectifyAlert(1006U, 1U));
        auto record = bam.alertById(1006U, 1U);
        ASSERT_TRUE(record.has_value());
        EXPECT_EQ(record->state, AlertState::RectifiedUnacknowledged);
        EXPECT_EQ(bam.activeAlerts().size(), 1U);

        // Now acknowledge -> transitions to Normal
        EXPECT_TRUE(bam.acknowledgeAlert(1006U, 1U));
        record = bam.alertById(1006U, 1U);
        EXPECT_EQ(record->state, AlertState::Normal);
        EXPECT_TRUE(bam.activeAlerts().empty());
    }

    TEST(TestBridgeAlertManager, ProcessArcAndAckCommands)
    {
        BridgeAlertManager bam {};
        bam.registerAlert(1007U, 1U, AlertPriority::Alarm, AlertCategory::CategoryB, "Fault 1007", "Details");

        // ARC Silence 'Q'
        ArcData arcSilence {};
        arcSilence.alertIdentifier = 1007U;
        arcSilence.alertInstance = 1U;
        arcSilence.command = 'Q';
        EXPECT_TRUE(bam.processArcCommand(arcSilence));
        auto record = bam.alertById(1007U, 1U);
        EXPECT_EQ(record->state, AlertState::ActiveSilenced);

        // ARC Acknowledge 'A'
        ArcData arcAck {};
        arcAck.alertIdentifier = 1007U;
        arcAck.alertInstance = 1U;
        arcAck.command = 'A';
        EXPECT_TRUE(bam.processArcCommand(arcAck));
        record = bam.alertById(1007U, 1U);
        EXPECT_EQ(record->state, AlertState::ActiveAcknowledged);

        // Legacy ACK command
        bam.registerAlert(1008U, 1U, AlertPriority::Alarm, AlertCategory::CategoryB, "Fault 1008", "Details");
        AckData legacyAck {};
        legacyAck.alertIdentifier = 1008U;
        EXPECT_TRUE(bam.processAckCommand(legacyAck));
        record = bam.alertById(1008U, 1U);
        EXPECT_EQ(record->state, AlertState::ActiveAcknowledged);
    }

    TEST(TestBridgeAlertManager, GenerateHbtAndAlcBroadcasts)
    {
        BamConfig cfg {};
        cfg.heartbeatIntervalSec = std::chrono::seconds(10);
        cfg.cyclicAlertListIntervalSec = std::chrono::seconds(15);
        BridgeAlertManager bam { cfg };

        bam.registerAlert(1009U, 1U, AlertPriority::Alarm, AlertCategory::CategoryB, "Fault 1009", "Details");

        std::vector<std::string> broadcasts {};
        bam.setBroadcastCallback([&broadcasts](const std::string& sentence) {
            broadcasts.push_back(sentence);
        });

        const auto now = std::chrono::steady_clock::now();
        bam.pollEscalations(now + std::chrono::seconds(20));

        bool foundHbt { false };
        bool foundAlc { false };
        for (const auto& s : broadcasts) {
            if (s.find("HBT") != std::string::npos) {
                foundHbt = true;
            }
            if (s.find("ALC") != std::string::npos) {
                foundAlc = true;
            }
        }
        EXPECT_TRUE(foundHbt);
        EXPECT_TRUE(foundAlc);
    }

} // namespace
} // namespace Nmea::Bam
