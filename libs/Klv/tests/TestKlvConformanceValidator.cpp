/// @file TestKlvConformanceValidator.cpp
/// @brief Unit tests for KlvConformanceValidator (MISB ST 0902.11 MMS and MISP-2025.1).

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "KlvConformanceValidator.h"
#include "KlvEncoder.h"
#include "KlvTypes.h"
#include "KlvValidationTypes.h"
#include "MiisCoreId.h"

using namespace Klv;

namespace {

[[nodiscard]] UasDatalinkMessage createCompliantMessage(std::uint64_t ptsUs)
{
    UasDatalinkMessage msg {};
    msg.precisionTimeStampUs = ptsUs;
    msg.missionId = "OPERATION_HERMES_2026";
    msg.platformTailNumber = "UAV-NATO-01";
    msg.platformHeadingDeg = 92.5;
    msg.platformPitchDeg = 1.2;
    msg.platformRollDeg = -0.8;
    msg.platformDesignation = "MQ-9 Reaper";
    msg.imageSourceSensor = "MTS-B EO/IR";
    msg.imageCoordinateSystem = "WGS-84";
    msg.sensorLatitudeDeg = 37.7749;
    msg.sensorLongitudeDeg = -122.4194;
    msg.sensorTrueAltitudeM = 3500.0;
    msg.sensorHfovDeg = 12.5;
    msg.sensorVfovDeg = 8.2;
    msg.slantRangeM = 4500.0;

    SecurityMetadata sec {};
    sec.classification = SecurityClassification::Unclassified;
    sec.countryCodingMethod = 1U;
    sec.classifyingCountry = "US";
    sec.version = 13U;
    msg.security = sec;

    msg.uasLsVersion = 16U;

    // Valid MIIS Core Identifier
    const Uuid sensorUuid = Uuid::generateV4();
    const Uuid platformUuid = Uuid::generateV4();
    msg.miisCoreId = MiisCoreId(sensorUuid, platformUuid);

    return msg;
}

} // namespace

TEST(KlvConformanceValidatorTest, CompliantStreamPassesValidation)
{
    KlvConformanceValidator validator {};
    const std::uint64_t startPts = 1700000000000000ULL;
    const std::uint64_t startArrival = 1000000ULL;

    for (std::uint64_t i = 0U; i < 20U; ++i) {
        const std::uint64_t pts = startPts + (i * 100000ULL); // 10 Hz cadence (100 ms)
        const std::uint64_t arrival = startArrival + (i * 100000ULL);
        UasDatalinkMessage msg = createCompliantMessage(pts);
        // Small realistic aircraft movement (~25 m/s)
        msg.sensorLatitudeDeg = 37.7749 + (static_cast<double>(i) * 0.0001);

        validator.validateMessage(msg, arrival);
    }

    EXPECT_TRUE(validator.isCompliant());
    const ConformanceReport report = validator.report();
    EXPECT_EQ(report.totalMessagesEvaluated, 20U);
    EXPECT_EQ(report.fatalCount, 0U);
    EXPECT_EQ(report.errorCount, 0U);
    EXPECT_TRUE(report.isCompliant);
}

TEST(KlvConformanceValidatorTest, MissingTimestampTriggersFatalError)
{
    KlvConformanceValidator validator {};
    UasDatalinkMessage msg = createCompliantMessage(1700000000000000ULL);
    msg.precisionTimeStampUs = std::nullopt;

    validator.validateMessage(msg, 1000000ULL);

    EXPECT_FALSE(validator.isCompliant());
    const ConformanceReport report = validator.report();
    EXPECT_GT(report.fatalCount, 0U);
}

TEST(KlvConformanceValidatorTest, MissingMandatoryMmsTagsTriggerErrors)
{
    KlvConformanceValidator validator {};
    UasDatalinkMessage msg = createCompliantMessage(1700000000000000ULL);
    msg.missionId = std::nullopt; // Tag 3 missing
    msg.platformHeadingDeg = std::nullopt; // Tag 5 missing
    msg.sensorLatitudeDeg = std::nullopt; // Tag 13 missing
    msg.security = std::nullopt; // Tag 48 missing

    validator.validateMessage(msg, 1000000ULL);

    EXPECT_FALSE(validator.isCompliant());
    const ConformanceReport report = validator.report();
    EXPECT_GE(report.errorCount, 4U);
}

TEST(KlvConformanceValidatorTest, NonMonotonicPtsDetected)
{
    KlvConformanceValidator validator {};
    const std::uint64_t pts1 = 1700000000100000ULL;
    const std::uint64_t pts2 = 1700000000050000ULL; // Time travel backwards!

    UasDatalinkMessage msg1 = createCompliantMessage(pts1);
    validator.validateMessage(msg1, 1000000ULL);
    EXPECT_TRUE(validator.isCompliant());

    UasDatalinkMessage msg2 = createCompliantMessage(pts2);
    validator.validateMessage(msg2, 1100000ULL);
    EXPECT_FALSE(validator.isCompliant());

    const ConformanceReport report = validator.report();
    EXPECT_GT(report.fatalCount, 0U);
}

TEST(KlvConformanceValidatorTest, TimingJitterThresholdExceeded)
{
    ValidationConfig config {};
    config.maxTimingJitterUs = 50000U; // 50 ms
    KlvConformanceValidator validator { config };

    const std::uint64_t pts1 = 1700000000000000ULL;
    const std::uint64_t arrival1 = 1000000ULL;
    UasDatalinkMessage msg1 = createCompliantMessage(pts1);
    validator.validateMessage(msg1, arrival1);

    // Delta PTS is 100 ms (100,000 us), but arrival delta is only 20 ms (20,000 us)
    // Jitter = |100000 - 20000| = 80000 us > 50000 us threshold!
    const std::uint64_t pts2 = pts1 + 100000ULL;
    const std::uint64_t arrival2 = arrival1 + 20000ULL;
    UasDatalinkMessage msg2 = createCompliantMessage(pts2);
    validator.validateMessage(msg2, arrival2);

    const ConformanceReport report = validator.report();
    EXPECT_GT(report.warningCount, 0U);
}

TEST(KlvConformanceValidatorTest, KinematicAnomaliesDetected)
{
    ValidationConfig config {};
    config.maxVelocityMps = 1029.0; // Mach 3
    KlvConformanceValidator validator { config };

    const std::uint64_t pts1 = 1700000000000000ULL;
    UasDatalinkMessage msg1 = createCompliantMessage(pts1);
    msg1.sensorLatitudeDeg = 37.0;
    msg1.sensorLongitudeDeg = -122.0;
    validator.validateMessage(msg1, 1000000ULL);

    // In 0.1s, jump 2 degrees latitude (~222 km) => speed ~ 2,220,000 m/s >> Mach 3!
    const std::uint64_t pts2 = pts1 + 100000ULL;
    UasDatalinkMessage msg2 = createCompliantMessage(pts2);
    msg2.sensorLatitudeDeg = 39.0;
    msg2.sensorLongitudeDeg = -122.0;
    validator.validateMessage(msg2, 1100000ULL);

    const ConformanceReport report = validator.report();
    EXPECT_GT(report.warningCount, 0U);

    bool foundVelocityAnomaly = false;
    for (const auto& issue : report.issues) {
        if (issue.category == ViolationCategory::Kinematics && issue.ruleId == "MISB-ST0902-KINEMATICS-VELOCITY") {
            foundVelocityAnomaly = true;
            break;
        }
    }
    EXPECT_TRUE(foundVelocityAnomaly);
}

TEST(KlvConformanceValidatorTest, NullIslandDetection)
{
    KlvConformanceValidator validator {};
    UasDatalinkMessage msg = createCompliantMessage(1700000000000000ULL);
    msg.sensorLatitudeDeg = 0.0;
    msg.sensorLongitudeDeg = 0.0;

    validator.validateMessage(msg, 1000000ULL);

    const ConformanceReport report = validator.report();
    bool foundNullIsland = false;
    for (const auto& issue : report.issues) {
        if (issue.ruleId == "MISB-ST0902-NULL-ISLAND") {
            foundNullIsland = true;
            break;
        }
    }
    EXPECT_TRUE(foundNullIsland);
}

TEST(KlvConformanceValidatorTest, ValueRangesEnforced)
{
    KlvConformanceValidator validator {};
    UasDatalinkMessage msg = createCompliantMessage(1700000000000000ULL);
    msg.sensorLatitudeDeg = 95.0; // Out of [-90, +90]
    msg.sensorHfovDeg = 200.0; // Out of [0, 180]
    msg.slantRangeM = 6000000.0; // Out of [0, 5,000,000]

    validator.validateMessage(msg, 1000000ULL);

    EXPECT_FALSE(validator.isCompliant());
    const ConformanceReport report = validator.report();
    EXPECT_GE(report.errorCount, 3U);
}

TEST(KlvConformanceValidatorTest, SecuritySubtagsEnforced)
{
    KlvConformanceValidator validator {};
    UasDatalinkMessage msg = createCompliantMessage(1700000000000000ULL);
    SecurityMetadata sec {};
    sec.classifyingCountry = ""; // Empty country code is invalid!
    msg.security = sec;

    validator.validateMessage(msg, 1000000ULL);

    EXPECT_FALSE(validator.isCompliant());
    const ConformanceReport report = validator.report();
    EXPECT_GT(report.errorCount, 0U);
}

TEST(KlvConformanceValidatorTest, RawPacketValidationAndCrcIntegrity)
{
    KlvConformanceValidator validator {};
    const UasDatalinkMessage msg = createCompliantMessage(1700000000000000ULL);

    std::vector<std::uint8_t> validPacket = KlvEncoder::encode(msg);
    ASSERT_FALSE(validPacket.empty());

    // Valid packet verification
    const KlvStatus statusValid = validator.validatePacket(validPacket.data(), validPacket.size(), 1000000ULL);
    EXPECT_EQ(statusValid, KlvStatus::Success);
    EXPECT_TRUE(validator.isCompliant());

    // Corrupt the CRC (last 2 bytes)
    validator.reset();
    std::vector<std::uint8_t> corruptPacket = validPacket;
    corruptPacket.back() ^= 0xFF; // Invert last byte of CRC-16

    const KlvStatus statusCorrupt = validator.validatePacket(corruptPacket.data(), corruptPacket.size(), 1100000ULL);
    EXPECT_EQ(statusCorrupt, KlvStatus::CrcMismatch);
    EXPECT_FALSE(validator.isCompliant());
    EXPECT_GT(validator.report().fatalCount, 0U);
}

TEST(KlvConformanceValidatorTest, JsonAndSummaryReportExport)
{
    KlvConformanceValidator validator {};
    UasDatalinkMessage msg = createCompliantMessage(1700000000000000ULL);
    msg.sensorLatitudeDeg = 92.0; // Deliberate range error
    validator.validateMessage(msg, 1000000ULL);

    const ConformanceReport report = validator.report();
    const std::string json = report.toJson();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("\"isCompliant\": false"), std::string::npos);
    EXPECT_NE(json.find("\"totalMessagesEvaluated\": 1"), std::string::npos);
    EXPECT_NE(json.find("\"ruleId\": \"MISB-ST0601-LAT-BOUND\""), std::string::npos);

    const std::string text = report.toSummaryText();
    EXPECT_FALSE(text.empty());
    EXPECT_NE(text.find("NON-COMPLIANT"), std::string::npos);
    EXPECT_NE(text.find("MISB-ST0601-LAT-BOUND"), std::string::npos);
}
