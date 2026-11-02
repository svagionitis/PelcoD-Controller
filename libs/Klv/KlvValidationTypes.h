#pragma once

/// @file KlvValidationTypes.h
/// @brief Strongly-typed domain models, enumerations, and reports for KLV conformance verification.

#include <cstddef>
#include <cstdint>

#include <string>
#include <vector>

namespace Klv {

/// @enum ValidationSeverity
/// @brief Severity level of a conformance check violation.
enum class ValidationSeverity : std::uint8_t {
    Info = 0U, ///< Informational note (e.g. optional feature present)
    Warning = 1U, ///< Minor deviation (e.g. slight timing jitter or high-rate cadence drop)
    Error = 2U, ///< Standards violation (e.g. missing mandatory tag or out-of-range coordinate)
    Fatal = 3U ///< Stream corruption (e.g. CRC mismatch, unparseable BER length)
};

/// @enum ValidationProfile
/// @brief Profile specification standard applied during conformance checking.
enum class ValidationProfile : std::uint8_t {
    Misb0902_11 = 0U, ///< Baseline MISB ST 0902.11 Minimum Metadata Set (MMS)
    Misp2025_1 = 1U ///< Strict DoD / NATO MISP-2025.1 Tactical Profile
};

/// @enum ViolationCategory
/// @brief Categorical classification of the conformance issue.
enum class ViolationCategory : std::uint8_t {
    FramingAndCrc = 0U, ///< Universal label, BER encoding, or CRC-16 checksum failure
    MandatoryTag = 1U, ///< Missing mandatory tag per active profile
    CadenceRate = 2U, ///< Cadence frequency below profile specification (e.g. < 1 Hz or < 10 Hz)
    TimingJitter = 3U, ///< PTS non-monotonicity, timestamp jump, or arrival jitter > 50 ms
    ValueRange = 4U, ///< Value outside physical or mathematical bounds (lat/lon/altitude/FOV)
    Kinematics = 5U, ///< Unrealistic motion (excessive velocity, climb rate, or angular rate)
    SecurityMetadata = 6U ///< ST 0102 Security Local Set missing mandatory classification elements
};

/// @struct ValidationIssue
/// @brief Details of an individual conformance violation or diagnostic notice.
struct ValidationIssue {
    ValidationSeverity severity { ValidationSeverity::Warning };
    ViolationCategory category { ViolationCategory::MandatoryTag };
    std::uint32_t tag { 0U }; ///< MISB ST 0601 tag identifier (or 0 if stream-level)
    std::string ruleId {}; ///< Standards rule identifier (e.g. "MISP-2025.1-MMS-TAG2")
    std::string message {}; ///< Human-readable diagnostic description
    std::uint64_t streamTimestampUs { 0U }; ///< PTS from Tag 2 (or 0 if unavailable)
    std::uint64_t arrivalTimestampUs { 0U }; ///< System monotonic arrival timestamp in microseconds
};

/// @struct ValidationConfig
/// @brief Configurable tolerances and parameters for conformance evaluation.
struct ValidationConfig {
    ValidationProfile profile { ValidationProfile::Misp2025_1 };
    std::uint64_t maxTimingJitterUs { 50000U }; ///< Maximum PTS vs arrival jitter (50 ms)
    std::uint64_t highRateMaxIntervalUs { 150000U }; ///< 10 Hz nominal + tolerance (150 ms)
    std::uint64_t lowRateMaxIntervalUs { 1200000U }; ///< 1 Hz nominal + tolerance (1200 ms)
    std::uint64_t maxTimestampGapUs { 5000000U }; ///< Max acceptable gap before drop alert (5 s)
    double maxVelocityMps { 1029.0 }; ///< Maximum kinematic speed (Mach 3 = 1029 m/s)
    double maxClimbRateMps { 200.0 }; ///< Maximum vertical rate (200 m/s)
    double maxAngularRateDegPerSec { 360.0 }; ///< Maximum attitude rate of change (360 deg/s)
    bool rejectNullIsland { true }; ///< Flag lat=0.0, lon=0.0 as violation
    bool requireMiisCoreId { true }; ///< Enforce Tag 94 presence for MISP-2025.1
    std::size_t maxRetainedIssues { 1000U }; ///< Maximum issues stored in memory
};

/// @struct ConformanceReport
/// @brief Aggregated compliance assessment summary.
struct ConformanceReport {
    bool isCompliant { true };
    ValidationProfile profile { ValidationProfile::Misp2025_1 };
    std::size_t totalMessagesEvaluated { 0U };
    std::size_t totalViolations { 0U };
    std::size_t errorCount { 0U };
    std::size_t warningCount { 0U };
    std::size_t fatalCount { 0U };
    std::vector<ValidationIssue> issues {};

    /// @brief Serializes the conformance report to a valid JSON string.
    /// @return Formatted JSON document representing evaluation statistics and issues.
    [[nodiscard]] std::string toJson() const;

    /// @brief Generates a formatted ASCII/CLI table summarizing compliance findings.
    /// @return Multi-line plain text summary.
    [[nodiscard]] std::string toSummaryText() const;
};

} // namespace Klv
