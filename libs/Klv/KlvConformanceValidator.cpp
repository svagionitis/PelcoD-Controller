/// @file KlvConformanceValidator.cpp
/// @brief Automated real-time and offline stream conformance verification engine.

#include "KlvConformanceValidator.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "KlvCrc.h"
#include "KlvGeodesy.h"
#include "KlvParser.h"
#include "KlvTypes.h"
#include "KlvValidationTypes.h"

namespace Klv {

namespace {

    [[nodiscard]] std::string severityToString(ValidationSeverity severity)
    {
        switch (severity) {
        case ValidationSeverity::Info:
            return "Info";
        case ValidationSeverity::Warning:
            return "Warning";
        case ValidationSeverity::Error:
            return "Error";
        case ValidationSeverity::Fatal:
            return "Fatal";
        }
        return "Unknown";
    }

    [[nodiscard]] std::string categoryToString(ViolationCategory category)
    {
        switch (category) {
        case ViolationCategory::FramingAndCrc:
            return "FramingAndCrc";
        case ViolationCategory::MandatoryTag:
            return "MandatoryTag";
        case ViolationCategory::CadenceRate:
            return "CadenceRate";
        case ViolationCategory::TimingJitter:
            return "TimingJitter";
        case ViolationCategory::ValueRange:
            return "ValueRange";
        case ViolationCategory::Kinematics:
            return "Kinematics";
        case ViolationCategory::SecurityMetadata:
            return "SecurityMetadata";
        }
        return "Unknown";
    }

    [[nodiscard]] std::string profileToString(ValidationProfile profile)
    {
        switch (profile) {
        case ValidationProfile::Misb0902_11:
            return "MISB-ST-0902.11";
        case ValidationProfile::Misp2025_1:
            return "MISP-2025.1";
        }
        return "Unknown";
    }

    [[nodiscard]] std::string escapeJson(const std::string& input)
    {
        std::string output {};
        output.reserve(input.size() + 8U);
        for (const char ch : input) {
            if (ch == '\"') {
                output += "\\\"";
            } else if (ch == '\\') {
                output += "\\\\";
            } else if (ch == '\b') {
                output += "\\b";
            } else if (ch == '\f') {
                output += "\\f";
            } else if (ch == '\n') {
                output += "\\n";
            } else if (ch == '\r') {
                output += "\\r";
            } else if (ch == '\t') {
                output += "\\t";
            } else {
                output += ch;
            }
        }
        return output;
    }

} // namespace

std::string ConformanceReport::toJson() const
{
    std::ostringstream ss {};
    ss << "{\n";
    ss << "  \"isCompliant\": " << (isCompliant ? "true" : "false") << ",\n";
    ss << "  \"profile\": \"" << profileToString(profile) << "\",\n";
    ss << "  \"totalMessagesEvaluated\": " << totalMessagesEvaluated << ",\n";
    ss << "  \"totalViolations\": " << totalViolations << ",\n";
    ss << "  \"fatalCount\": " << fatalCount << ",\n";
    ss << "  \"errorCount\": " << errorCount << ",\n";
    ss << "  \"warningCount\": " << warningCount << ",\n";
    ss << "  \"issues\": [\n";

    for (std::size_t i = 0U; i < issues.size(); ++i) {
        const auto& issue = issues[i];
        ss << "    {\n";
        ss << "      \"severity\": \"" << severityToString(issue.severity) << "\",\n";
        ss << "      \"category\": \"" << categoryToString(issue.category) << "\",\n";
        ss << "      \"tag\": " << issue.tag << ",\n";
        ss << "      \"ruleId\": \"" << escapeJson(issue.ruleId) << "\",\n";
        ss << "      \"message\": \"" << escapeJson(issue.message) << "\",\n";
        ss << "      \"streamTimestampUs\": " << issue.streamTimestampUs << ",\n";
        ss << "      \"arrivalTimestampUs\": " << issue.arrivalTimestampUs << "\n";
        ss << "    }" << (i + 1U < issues.size() ? "," : "") << "\n";
    }

    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

std::string ConformanceReport::toSummaryText() const
{
    std::ostringstream ss {};
    ss << "================================================================================\n";
    ss << "                    KLV CONFORMANCE AUDIT SUMMARY REPORT\n";
    ss << "================================================================================\n";
    ss << "Profile                 : " << profileToString(profile) << "\n";
    ss << "Overall Compliance      : " << (isCompliant ? "COMPLIANT" : "NON-COMPLIANT") << "\n";
    ss << "Total Messages Tested   : " << totalMessagesEvaluated << "\n";
    ss << "Total Violations Logged : " << totalViolations << "\n";
    ss << "  - Fatal Issues        : " << fatalCount << "\n";
    ss << "  - Error Violations    : " << errorCount << "\n";
    ss << "  - Warning Notices     : " << warningCount << "\n";
    ss << "Retained Issues In Log  : " << issues.size() << "\n";
    ss << "--------------------------------------------------------------------------------\n";

    if (issues.empty()) {
        ss << "No conformance violations detected in evaluated stream.\n";
    } else {
        ss << "Leading Findings:\n";
        const std::size_t printLimit = std::min(issues.size(), static_cast<std::size_t>(20U));
        for (std::size_t i = 0U; i < printLimit; ++i) {
            const auto& issue = issues[i];
            ss << " [" << std::setw(7) << severityToString(issue.severity) << "] "
               << "Tag " << std::setw(3) << issue.tag << " | " << std::setw(28) << issue.ruleId << " : "
               << issue.message << "\n";
        }
        if (issues.size() > printLimit) {
            ss << " ... and " << (issues.size() - printLimit) << " additional issues truncated.\n";
        }
    }
    ss << "================================================================================\n";
    return ss.str();
}

KlvConformanceValidator::KlvConformanceValidator(const ValidationConfig& config)
    : config_(config)
{
    reset();
}

void KlvConformanceValidator::reset() noexcept
{
    report_ = ConformanceReport {};
    report_.profile = config_.profile;
    lastPtsUs_ = 0U;
    lastArrivalUs_ = 0U;
    lastPlatformStatePtsUs_ = 0U;
    lastAttitudePtsUs_ = 0U;
    lastFrameCenterPtsUs_ = 0U;
    lastSecurityPtsUs_ = 0U;
    hasPreviousState_ = false;
    lastLatDeg_ = 0.0;
    lastLonDeg_ = 0.0;
    lastAltM_ = 0.0;
    lastHeadingDeg_ = 0.0;
    lastPitchDeg_ = 0.0;
    lastRollDeg_ = 0.0;
}

void KlvConformanceValidator::setCallback(OnIssueCallback callback) noexcept
{
    callback_ = std::move(callback);
}

void KlvConformanceValidator::setConfig(const ValidationConfig& config) noexcept
{
    config_ = config;
    report_.profile = config_.profile;
}

ConformanceReport KlvConformanceValidator::report() const
{
    return report_;
}

bool KlvConformanceValidator::isCompliant() const noexcept
{
    return report_.isCompliant;
}

void KlvConformanceValidator::emitIssue(const ValidationIssue& issue)
{
    ++report_.totalViolations;
    if (issue.severity == ValidationSeverity::Fatal) {
        ++report_.fatalCount;
        report_.isCompliant = false;
    } else if (issue.severity == ValidationSeverity::Error) {
        ++report_.errorCount;
        report_.isCompliant = false;
    } else if (issue.severity == ValidationSeverity::Warning) {
        ++report_.warningCount;
    }

    if (report_.issues.size() < config_.maxRetainedIssues) {
        report_.issues.push_back(issue);
    }

    if (callback_) {
        callback_(issue);
    }
}

KlvStatus KlvConformanceValidator::validatePacket(
    const std::uint8_t* data, std::size_t size, std::uint64_t arrivalTimeUs)
{
    if (data == nullptr || size < kUniversalLabelSize) {
        emitIssue(
            ValidationIssue { ValidationSeverity::Fatal, ViolationCategory::FramingAndCrc, 0U, "MISB-ST0601-UL-INVALID",
                "Packet buffer size smaller than Universal Label (16 bytes)", 0U, arrivalTimeUs });
        return KlvStatus::BufferTooSmall;
    }

    if (!KlvParser::isMisb0601(data, size)) {
        emitIssue(ValidationIssue { ValidationSeverity::Fatal, ViolationCategory::FramingAndCrc, 0U,
            "MISB-ST0601-UL-MISMATCH", "Packet header Universal Label does not match MISB ST 0601 key", 0U,
            arrivalTimeUs });
        return KlvStatus::InvalidUniversalLabel;
    }

    if (!KlvCrc::verifyPacket(data, size)) {
        emitIssue(ValidationIssue { ValidationSeverity::Fatal, ViolationCategory::FramingAndCrc, 1U,
            "MISB-ST0601-CRC-MISMATCH", "Tag 1 CRC-16-CCITT packet checksum verification failed", 0U, arrivalTimeUs });
        return KlvStatus::CrcMismatch;
    }

    UasDatalinkMessage message {};
    const KlvStatus parseStatus = KlvParser::parse(data, size, message, false);
    if (parseStatus != KlvStatus::Success) {
        emitIssue(ValidationIssue { ValidationSeverity::Fatal, ViolationCategory::FramingAndCrc, 0U,
            "MISB-ST0601-PARSE-FAILED", "Failed to parse packet payload into UasDatalinkMessage structure", 0U,
            arrivalTimeUs });
        return parseStatus;
    }

    validateMessage(message, arrivalTimeUs);
    return KlvStatus::Success;
}

void KlvConformanceValidator::validateMessage(const UasDatalinkMessage& message, std::uint64_t arrivalTimeUs)
{
    ++report_.totalMessagesEvaluated;

    std::uint64_t ptsUs = 0U;
    if (message.precisionTimeStampUs.has_value()) {
        ptsUs = *message.precisionTimeStampUs;
    } else {
        emitIssue(ValidationIssue { ValidationSeverity::Fatal, ViolationCategory::MandatoryTag,
            static_cast<std::uint32_t>(Tag::PrecisionTimeStamp), "MISB-ST0902-MMS-TAG2",
            "Precision Time Stamp (Tag 2) is missing from packet", 0U, arrivalTimeUs });
        ptsUs = arrivalTimeUs;
    }

    checkMandatoryTags(message, ptsUs, arrivalTimeUs);
    checkSecurity(message, ptsUs, arrivalTimeUs);
    checkCadenceAndJitter(message, ptsUs, arrivalTimeUs);
    checkNumericRanges(message, ptsUs, arrivalTimeUs);
    checkKinematics(message, ptsUs, arrivalTimeUs);

    lastPtsUs_ = ptsUs;
    lastArrivalUs_ = arrivalTimeUs;
}

void KlvConformanceValidator::checkMandatoryTags(
    const UasDatalinkMessage& msg, std::uint64_t pts, std::uint64_t arrival)
{
    const bool isFirstEvaluation = (report_.totalMessagesEvaluated == 1U);

    // Tag 3: Mission ID (MMS mandatory >= 1 Hz)
    if (msg.missionId.has_value()) {
        lastPlatformStatePtsUs_ = pts;
    } else if (isFirstEvaluation
        || (lastPlatformStatePtsUs_ > 0U && pts > lastPlatformStatePtsUs_ + config_.lowRateMaxIntervalUs)) {
        emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::MandatoryTag,
            static_cast<std::uint32_t>(Tag::MissionId), "MISB-ST0902-MMS-TAG3",
            "Mission ID (Tag 3) missing or exceeded 1.0 Hz cadence bound", pts, arrival });
    }

    // Tag 4: Platform Tail Number (MMS mandatory >= 1 Hz)
    if (!msg.platformTailNumber.has_value() && isFirstEvaluation) {
        emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::MandatoryTag,
            static_cast<std::uint32_t>(Tag::PlatformTailNumber), "MISB-ST0902-MMS-TAG4",
            "Platform Tail Number (Tag 4) missing from initial telemetry stream", pts, arrival });
    }

    // Tag 5: Platform Heading (MMS mandatory >= 10 Hz)
    if (msg.platformHeadingDeg.has_value()) {
        lastAttitudePtsUs_ = pts;
    } else if (isFirstEvaluation
        || (lastAttitudePtsUs_ > 0U && pts > lastAttitudePtsUs_ + config_.highRateMaxIntervalUs)) {
        emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::MandatoryTag,
            static_cast<std::uint32_t>(Tag::PlatformHeading), "MISB-ST0902-MMS-TAG5",
            "Platform Heading Angle (Tag 5) missing or exceeded 10 Hz cadence bound", pts, arrival });
    }

    // Tag 6 / 90: Platform Pitch (MMS mandatory >= 10 Hz)
    if (!msg.platformPitchDeg.has_value() && isFirstEvaluation) {
        emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::MandatoryTag,
            static_cast<std::uint32_t>(Tag::PlatformPitch), "MISB-ST0902-MMS-TAG6",
            "Platform Pitch Angle (Tag 6) missing from telemetry", pts, arrival });
    }

    // Tag 7 / 91: Platform Roll (MMS mandatory >= 10 Hz)
    if (!msg.platformRollDeg.has_value() && isFirstEvaluation) {
        emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::MandatoryTag,
            static_cast<std::uint32_t>(Tag::PlatformRoll), "MISB-ST0902-MMS-TAG7",
            "Platform Roll Angle (Tag 7) missing from telemetry", pts, arrival });
    }

    // Tag 10: Platform Designation (MMS mandatory >= 1 Hz)
    if (!msg.platformDesignation.has_value() && isFirstEvaluation) {
        emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::MandatoryTag,
            static_cast<std::uint32_t>(Tag::PlatformDesignation), "MISB-ST0902-MMS-TAG10",
            "Platform Designation (Tag 10) missing from initial telemetry stream", pts, arrival });
    }

    // Tag 11: Image Source Sensor (MMS mandatory >= 1 Hz)
    if (!msg.imageSourceSensor.has_value() && isFirstEvaluation) {
        emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::MandatoryTag,
            static_cast<std::uint32_t>(Tag::ImageSourceSensor), "MISB-ST0902-MMS-TAG11",
            "Image Source Sensor (Tag 11) missing from initial telemetry stream", pts, arrival });
    }

    // Tag 12: Image Coordinate System (MMS mandatory >= 1 Hz)
    if (!msg.imageCoordinateSystem.has_value() && isFirstEvaluation) {
        emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::MandatoryTag,
            static_cast<std::uint32_t>(Tag::ImageCoordinateSystem), "MISB-ST0902-MMS-TAG12",
            "Image Coordinate System (Tag 12) missing from initial telemetry stream", pts, arrival });
    }

    // Tag 13 & 14: Sensor Latitude and Longitude (MMS mandatory >= 1 Hz)
    if (!msg.sensorLatitudeDeg.has_value() || !msg.sensorLongitudeDeg.has_value()) {
        if (isFirstEvaluation) {
            emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::MandatoryTag,
                static_cast<std::uint32_t>(Tag::SensorLatitude), "MISB-ST0902-MMS-TAG13-14",
                "Sensor Latitude (Tag 13) or Longitude (Tag 14) missing", pts, arrival });
        }
    }

    // Tag 15 / 75: Sensor Altitude (MMS mandatory >= 1 Hz)
    if (!msg.sensorTrueAltitudeM.has_value() && !msg.sensorAltitudeHaeM.has_value()) {
        if (isFirstEvaluation) {
            emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::MandatoryTag,
                static_cast<std::uint32_t>(Tag::SensorTrueAltitude), "MISB-ST0902-MMS-TAG15",
                "Sensor Altitude MSL (Tag 15) or HAE (Tag 75) missing", pts, arrival });
        }
    }

    // Tag 16 & 17: Sensor HFOV and VFOV (MMS mandatory >= 10 Hz)
    if (!msg.sensorHfovDeg.has_value() || !msg.sensorVfovDeg.has_value()) {
        if (isFirstEvaluation) {
            emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::MandatoryTag,
                static_cast<std::uint32_t>(Tag::SensorHFOV), "MISB-ST0902-MMS-TAG16-17",
                "Sensor Horizontal (Tag 16) or Vertical (Tag 17) FOV missing", pts, arrival });
        }
    }

    // Tag 48: Security Local Set (MMS mandatory >= 1 Hz)
    if (!msg.security.has_value()) {
        if (isFirstEvaluation) {
            emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::MandatoryTag,
                static_cast<std::uint32_t>(Tag::SecurityLocalSet), "MISB-ST0902-MMS-TAG48",
                "Security Classification Local Set (Tag 48) missing", pts, arrival });
        }
    }

    // Tag 65: UAS Datalink LS Version Number (MMS mandatory)
    if (msg.uasLsVersion.has_value()) {
        const std::uint8_t ver = *msg.uasLsVersion;
        if (config_.profile == ValidationProfile::Misp2025_1 && ver < 16U) {
            emitIssue(ValidationIssue { ValidationSeverity::Warning, ViolationCategory::MandatoryTag,
                static_cast<std::uint32_t>(Tag::UasLsVersion), "MISP-2025.1-TAG65-VERSION",
                "MISP-2025.1 recommends UAS Datalink LS Version >= 16 (found: " + std::to_string(ver) + ")", pts,
                arrival });
        } else if (ver < 12U) {
            emitIssue(ValidationIssue { ValidationSeverity::Warning, ViolationCategory::MandatoryTag,
                static_cast<std::uint32_t>(Tag::UasLsVersion), "MISB-ST0902-TAG65-LEGACY",
                "UAS Datalink LS Version is older than ST 0601.12 (found: " + std::to_string(ver) + ")", pts,
                arrival });
        }
    } else if (isFirstEvaluation) {
        emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::MandatoryTag,
            static_cast<std::uint32_t>(Tag::UasLsVersion), "MISB-ST0902-MMS-TAG65",
            "UAS Datalink LS Version (Tag 65) missing from stream", pts, arrival });
    }

    // Tag 94: MIIS Core Identifier (ST 1204 / MISP-2025.1 mandatory)
    if (config_.profile == ValidationProfile::Misp2025_1 && config_.requireMiisCoreId) {
        if (!msg.miisCoreId.has_value() || !msg.miisCoreId->validate()) {
            if (isFirstEvaluation) {
                emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::MandatoryTag,
                    static_cast<std::uint32_t>(Tag::MiisCoreIdentifier), "MISP-2025.1-TAG94-MIIS",
                    "MISB ST 1204 MIIS Core Identifier (Tag 94) missing or invalid under MISP-2025.1", pts, arrival });
            }
        }
    }
}

void KlvConformanceValidator::checkSecurity(const UasDatalinkMessage& msg, std::uint64_t pts, std::uint64_t arrival)
{
    if (!msg.security.has_value()) {
        return;
    }

    const auto& sec = *msg.security;
    lastSecurityPtsUs_ = pts;

    // Sub-tag 1: Classification check
    const auto classificationVal = static_cast<std::uint8_t>(sec.classification);
    if (classificationVal < 1U || classificationVal > 5U) {
        emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::SecurityMetadata,
            static_cast<std::uint32_t>(Tag::SecurityLocalSet), "MISB-ST0102-SUBTAG1-INVALID",
            "Security Classification Sub-tag 1 out of allowable range (1..5)", pts, arrival });
    }

    // Sub-tag 3: Classifying Country code non-empty
    if (sec.classifyingCountry.empty()) {
        emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::SecurityMetadata,
            static_cast<std::uint32_t>(Tag::SecurityLocalSet), "MISB-ST0102-SUBTAG3-EMPTY",
            "Security Classifying Country (Sub-tag 3) cannot be empty", pts, arrival });
    }

    // Sub-tag 22: Version check
    if (sec.version == 0U) {
        emitIssue(ValidationIssue { ValidationSeverity::Warning, ViolationCategory::SecurityMetadata,
            static_cast<std::uint32_t>(Tag::SecurityLocalSet), "MISB-ST0102-SUBTAG22-ZERO",
            "Security Local Set Version (Sub-tag 22) must be >= 1", pts, arrival });
    }
}

void KlvConformanceValidator::checkCadenceAndJitter(
    const UasDatalinkMessage& msg, std::uint64_t pts, std::uint64_t arrival)
{
    (void)msg;

    if (lastPtsUs_ > 0U) {
        // Monotonicity Check
        if (pts <= lastPtsUs_) {
            emitIssue(ValidationIssue { ValidationSeverity::Fatal, ViolationCategory::TimingJitter,
                static_cast<std::uint32_t>(Tag::PrecisionTimeStamp), "MISB-ST0902-PTS-NON-MONOTONIC",
                "Non-monotonic timestamp detected: current PTS (" + std::to_string(pts) + ") <= previous PTS ("
                    + std::to_string(lastPtsUs_) + ")",
                pts, arrival });
        } else if (pts > lastPtsUs_ + config_.maxTimestampGapUs) {
            emitIssue(ValidationIssue { ValidationSeverity::Warning, ViolationCategory::TimingJitter,
                static_cast<std::uint32_t>(Tag::PrecisionTimeStamp), "MISB-ST0902-PTS-GAP",
                "Large timestamp gap detected between consecutive frames (> 5 seconds)", pts, arrival });
        }

        // Timing Jitter Check
        if (lastArrivalUs_ > 0U && arrival >= lastArrivalUs_ && pts > lastPtsUs_) {
            const std::uint64_t deltaPts = pts - lastPtsUs_;
            const std::uint64_t deltaArrival = arrival - lastArrivalUs_;
            const std::uint64_t jitter
                = (deltaPts >= deltaArrival) ? (deltaPts - deltaArrival) : (deltaArrival - deltaPts);

            if (jitter > config_.maxTimingJitterUs) {
                emitIssue(ValidationIssue { ValidationSeverity::Warning, ViolationCategory::TimingJitter,
                    static_cast<std::uint32_t>(Tag::PrecisionTimeStamp), "MISP-2025.1-JITTER-BOUND",
                    "Arrival vs PTS timing skew jitter (" + std::to_string(jitter) + " us) exceeded profile bound ("
                        + std::to_string(config_.maxTimingJitterUs) + " us)",
                    pts, arrival });
            }
        }
    }
}

void KlvConformanceValidator::checkNumericRanges(
    const UasDatalinkMessage& msg, std::uint64_t pts, std::uint64_t arrival)
{
    // Latitude [-90.0, +90.0]
    if (msg.sensorLatitudeDeg.has_value()) {
        const double lat = *msg.sensorLatitudeDeg;
        if (lat < -90.0 || lat > 90.0) {
            emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::ValueRange,
                static_cast<std::uint32_t>(Tag::SensorLatitude), "MISB-ST0601-LAT-BOUND",
                "Sensor Latitude out of range [-90.0, +90.0]: " + std::to_string(lat), pts, arrival });
        }
    }

    // Longitude [-180.0, +180.0]
    if (msg.sensorLongitudeDeg.has_value()) {
        const double lon = *msg.sensorLongitudeDeg;
        if (lon < -180.0 || lon > 180.0) {
            emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::ValueRange,
                static_cast<std::uint32_t>(Tag::SensorLongitude), "MISB-ST0601-LON-BOUND",
                "Sensor Longitude out of range [-180.0, +180.0]: " + std::to_string(lon), pts, arrival });
        }
    }

    // Null Island Check (0.0, 0.0)
    if (config_.rejectNullIsland && msg.sensorLatitudeDeg.has_value() && msg.sensorLongitudeDeg.has_value()) {
        if (std::abs(*msg.sensorLatitudeDeg) < 1e-6 && std::abs(*msg.sensorLongitudeDeg) < 1e-6) {
            emitIssue(ValidationIssue { ValidationSeverity::Warning, ViolationCategory::ValueRange,
                static_cast<std::uint32_t>(Tag::SensorLatitude), "MISB-ST0902-NULL-ISLAND",
                "Sensor coordinate located at (0.0, 0.0) Null Island (potential uninitialized GPS)", pts, arrival });
        }
    }

    // Sensor Altitude [-900.0, 19000.0]
    if (msg.sensorTrueAltitudeM.has_value()) {
        const double alt = *msg.sensorTrueAltitudeM;
        if (alt < -900.0 || alt > 19000.0) {
            emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::ValueRange,
                static_cast<std::uint32_t>(Tag::SensorTrueAltitude), "MISB-ST0601-ALT-BOUND",
                "Sensor Altitude MSL out of range [-900, 19000] m: " + std::to_string(alt), pts, arrival });
        }
    }

    // Platform Heading [0.0, 360.0)
    if (msg.platformHeadingDeg.has_value()) {
        const double heading = *msg.platformHeadingDeg;
        if (heading < 0.0 || heading >= 360.0) {
            emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::ValueRange,
                static_cast<std::uint32_t>(Tag::PlatformHeading), "MISB-ST0601-HEADING-BOUND",
                "Platform Heading Angle out of range [0.0, 360.0): " + std::to_string(heading), pts, arrival });
        }
    }

    // Platform Pitch [-90.0, +90.0]
    if (msg.platformPitchDeg.has_value()) {
        const double pitch = *msg.platformPitchDeg;
        if (pitch < -90.0 || pitch > 90.0) {
            emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::ValueRange,
                static_cast<std::uint32_t>(Tag::PlatformPitch), "MISB-ST0601-PITCH-BOUND",
                "Platform Pitch Angle out of range [-90.0, +90.0]: " + std::to_string(pitch), pts, arrival });
        }
    }

    // Platform Roll [-180.0, +180.0]
    if (msg.platformRollDeg.has_value()) {
        const double roll = *msg.platformRollDeg;
        if (roll < -180.0 || roll > 180.0) {
            emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::ValueRange,
                static_cast<std::uint32_t>(Tag::PlatformRoll), "MISB-ST0601-ROLL-BOUND",
                "Platform Roll Angle out of range [-180.0, +180.0]: " + std::to_string(roll), pts, arrival });
        }
    }

    // HFOV [0.0, 180.0]
    if (msg.sensorHfovDeg.has_value()) {
        const double hfov = *msg.sensorHfovDeg;
        if (hfov < 0.0 || hfov > 180.0) {
            emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::ValueRange,
                static_cast<std::uint32_t>(Tag::SensorHFOV), "MISB-ST0601-HFOV-BOUND",
                "Sensor HFOV out of range [0.0, 180.0]: " + std::to_string(hfov), pts, arrival });
        }
    }

    // VFOV [0.0, 180.0]
    if (msg.sensorVfovDeg.has_value()) {
        const double vfov = *msg.sensorVfovDeg;
        if (vfov < 0.0 || vfov > 180.0) {
            emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::ValueRange,
                static_cast<std::uint32_t>(Tag::SensorVFOV), "MISB-ST0601-VFOV-BOUND",
                "Sensor VFOV out of range [0.0, 180.0]: " + std::to_string(vfov), pts, arrival });
        }
    }

    // Slant Range [0.0, 5000000.0]
    if (msg.slantRangeM.has_value()) {
        const double slant = *msg.slantRangeM;
        if (slant < 0.0 || slant > 5000000.0) {
            emitIssue(ValidationIssue { ValidationSeverity::Error, ViolationCategory::ValueRange,
                static_cast<std::uint32_t>(Tag::SlantRange), "MISB-ST0601-SLANT-BOUND",
                "Slant Range out of range [0.0, 5000000.0] m: " + std::to_string(slant), pts, arrival });
        }
    }
}

void KlvConformanceValidator::checkKinematics(const UasDatalinkMessage& msg, std::uint64_t pts, std::uint64_t arrival)
{
    if (!hasPreviousState_) {
        if (msg.sensorLatitudeDeg.has_value() && msg.sensorLongitudeDeg.has_value()) {
            lastLatDeg_ = *msg.sensorLatitudeDeg;
            lastLonDeg_ = *msg.sensorLongitudeDeg;
            hasPreviousState_ = true;
        }
        if (msg.sensorTrueAltitudeM.has_value()) {
            lastAltM_ = *msg.sensorTrueAltitudeM;
        }
        if (msg.platformHeadingDeg.has_value()) {
            lastHeadingDeg_ = *msg.platformHeadingDeg;
        }
        if (msg.platformPitchDeg.has_value()) {
            lastPitchDeg_ = *msg.platformPitchDeg;
        }
        if (msg.platformRollDeg.has_value()) {
            lastRollDeg_ = *msg.platformRollDeg;
        }
        return;
    }

    if (lastPtsUs_ > 0U && pts > lastPtsUs_) {
        const double dtSec = static_cast<double>(pts - lastPtsUs_) / 1000000.0;
        if (dtSec > 0.001) {
            // Ground velocity check
            if (msg.sensorLatitudeDeg.has_value() && msg.sensorLongitudeDeg.has_value()) {
                const GeoPoint2D p1 { lastLatDeg_, lastLonDeg_ };
                const GeoPoint2D p2 { *msg.sensorLatitudeDeg, *msg.sensorLongitudeDeg };
                const double distMeters = KlvGeodesy::distanceMeters(p1, p2);
                const double velocityMps = distMeters / dtSec;

                if (velocityMps > config_.maxVelocityMps) {
                    emitIssue(ValidationIssue { ValidationSeverity::Warning, ViolationCategory::Kinematics,
                        static_cast<std::uint32_t>(Tag::SensorLatitude), "MISB-ST0902-KINEMATICS-VELOCITY",
                        "Platform ground velocity (" + std::to_string(velocityMps)
                            + " m/s) exceeds maximum kinematic envelope (" + std::to_string(config_.maxVelocityMps)
                            + " m/s)",
                        pts, arrival });
                }

                lastLatDeg_ = *msg.sensorLatitudeDeg;
                lastLonDeg_ = *msg.sensorLongitudeDeg;
            }

            // Vertical climb / descent rate check
            if (msg.sensorTrueAltitudeM.has_value()) {
                const double deltaAlt = std::abs(*msg.sensorTrueAltitudeM - lastAltM_);
                const double climbRateMps = deltaAlt / dtSec;

                if (climbRateMps > config_.maxClimbRateMps) {
                    emitIssue(ValidationIssue { ValidationSeverity::Warning, ViolationCategory::Kinematics,
                        static_cast<std::uint32_t>(Tag::SensorTrueAltitude), "MISB-ST0902-KINEMATICS-CLIMB",
                        "Platform vertical rate (" + std::to_string(climbRateMps)
                            + " m/s) exceeds maximum allowable threshold (" + std::to_string(config_.maxClimbRateMps)
                            + " m/s)",
                        pts, arrival });
                }
                lastAltM_ = *msg.sensorTrueAltitudeM;
            }

            // Angular heading rate check
            if (msg.platformHeadingDeg.has_value()) {
                double dHeading = std::abs(*msg.platformHeadingDeg - lastHeadingDeg_);
                if (dHeading > 180.0) {
                    dHeading = 360.0 - dHeading;
                }
                const double yawRate = dHeading / dtSec;
                if (yawRate > config_.maxAngularRateDegPerSec) {
                    emitIssue(ValidationIssue { ValidationSeverity::Warning, ViolationCategory::Kinematics,
                        static_cast<std::uint32_t>(Tag::PlatformHeading), "MISB-ST0902-KINEMATICS-TURN-RATE",
                        "Platform heading slew rate (" + std::to_string(yawRate) + " deg/s) exceeds threshold ("
                            + std::to_string(config_.maxAngularRateDegPerSec) + " deg/s)",
                        pts, arrival });
                }
                lastHeadingDeg_ = *msg.platformHeadingDeg;
            }

            if (msg.platformPitchDeg.has_value()) {
                lastPitchDeg_ = *msg.platformPitchDeg;
            }
            if (msg.platformRollDeg.has_value()) {
                lastRollDeg_ = *msg.platformRollDeg;
            }
        }
    }
}

} // namespace Klv
