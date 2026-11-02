#pragma once

/// @file KlvConformanceValidator.h
/// @brief Automated real-time and offline stream conformance verification engine.
/// @details Validates STANAG 4609 / MISB KLV streams against MISB ST 0902.11 MMS and MISP-2025.1.

#include <cstddef>
#include <cstdint>

#include <functional>
#include <string>
#include <vector>

#include "KlvTypes.h"
#include "KlvValidationTypes.h"

namespace Klv {

/// @class KlvConformanceValidator
/// @brief Evaluates KLV streams against MISB ST 0902.11 MMS and MISP-2025.1 criteria.
class KlvConformanceValidator {
public:
    /// @brief Callback signature for streaming issue notifications.
    using OnIssueCallback = std::function<void(const ValidationIssue&)>;

    /// @brief Constructs a validator instance with optional configuration.
    /// @param[in] config Conformance tolerances and active profile settings.
    explicit KlvConformanceValidator(const ValidationConfig& config = ValidationConfig {});

    /// @brief Resets all internal tracking state, rolling timers, and cumulative reports.
    void reset() noexcept;

    /// @brief Registers a real-time issue callback.
    /// @param[in] callback Callable invoked whenever an issue is flagged.
    void setCallback(OnIssueCallback callback) noexcept;

    /// @brief Updates validator configuration.
    /// @param[in] config Conformance tolerances and active profile settings.
    void setConfig(const ValidationConfig& config) noexcept;

    /// @brief Unpacks and validates a raw KLV packet starting with Universal Label.
    /// @param[in] data Pointer to raw byte packet buffer.
    /// @param[in] size Size of byte buffer in bytes.
    /// @param[in] arrivalTimeUs Monotonic local system arrival timestamp in microseconds.
    /// @return Status indicating framing, BER, and CRC validity.
    [[nodiscard]] KlvStatus validatePacket(const std::uint8_t* data, std::size_t size, std::uint64_t arrivalTimeUs);

    /// @brief Evaluates a deserialized telemetry message against the active profile.
    /// @param[in] message Parsed UAS Datalink Local Set message.
    /// @param[in] arrivalTimeUs Monotonic local system arrival timestamp in microseconds.
    void validateMessage(const UasDatalinkMessage& message, std::uint64_t arrivalTimeUs);

    /// @brief Retrieves the cumulative conformance report.
    /// @return Aggregated report containing metrics and violation history.
    [[nodiscard]] ConformanceReport report() const;

    /// @brief Checks whether the stream evaluated so far has zero Fatal or Error issues.
    /// @return True if compliant; false if any error or fatal violation occurred.
    [[nodiscard]] bool isCompliant() const noexcept;

private:
    void emitIssue(const ValidationIssue& issue);
    void checkMandatoryTags(const UasDatalinkMessage& msg, std::uint64_t pts, std::uint64_t arrival);
    void checkSecurity(const UasDatalinkMessage& msg, std::uint64_t pts, std::uint64_t arrival);
    void checkCadenceAndJitter(const UasDatalinkMessage& msg, std::uint64_t pts, std::uint64_t arrival);
    void checkNumericRanges(const UasDatalinkMessage& msg, std::uint64_t pts, std::uint64_t arrival);
    void checkKinematics(const UasDatalinkMessage& msg, std::uint64_t pts, std::uint64_t arrival);

    ValidationConfig config_ {};
    OnIssueCallback callback_ { nullptr };
    ConformanceReport report_ {};

    // Stateful cadence tracking (timestamps in microseconds)
    std::uint64_t lastPtsUs_ { 0U };
    std::uint64_t lastArrivalUs_ { 0U };
    std::uint64_t lastPlatformStatePtsUs_ { 0U };
    std::uint64_t lastAttitudePtsUs_ { 0U };
    std::uint64_t lastFrameCenterPtsUs_ { 0U };
    std::uint64_t lastSecurityPtsUs_ { 0U };

    // Stateful kinematic tracking
    bool hasPreviousState_ { false };
    double lastLatDeg_ { 0.0 };
    double lastLonDeg_ { 0.0 };
    double lastAltM_ { 0.0 };
    double lastHeadingDeg_ { 0.0 };
    double lastPitchDeg_ { 0.0 };
    double lastRollDeg_ { 0.0 };
};

} // namespace Klv
