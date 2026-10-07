#pragma once

/// @file PasswordPolicy.h
/// @brief Password strength, complexity, and anti-default validation (CWE-798, ETSI EN 303 645 §5.1).

#include "OnvifServerTypes.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace Onvif {

/// @enum PasswordCheckResult
/// @brief Specific outcome of a password policy check.
enum class PasswordCheckResult {
    Valid, ///< Password satisfies all policy requirements.
    EmptyPassword, ///< Password is empty.
    TooShort, ///< Password length is less than configured minimum.
    TooLong, ///< Password length exceeds configured maximum.
    InsufficientClasses, ///< Password contains fewer than required character classes.
    MatchesUsername, ///< Password contains or equals username.
    CommonDefault ///< Password is a well-known trivial default.
};

/// @class PasswordPolicy
/// @brief Enforces password complexity guidelines conforming to ETSI EN 303 645 §5.1
///        and NIST SP 800-63B.
class PasswordPolicy {
public:
    /// @brief Validates a candidate password against the supplied policy.
    /// @param[in] password Candidate password secret.
    /// @param[in] username Candidate username.
    /// @param[in] config Policy settings.
    /// @return PasswordCheckResult indicating pass or failure reason.
    [[nodiscard]] static PasswordCheckResult check(
        std::string_view password, std::string_view username, const PasswordPolicyConfig& config) noexcept;

    /// @brief Checks whether the password is a well-known trivial default.
    /// @param[in] password Candidate password.
    /// @return True if in common default dictionary (e.g. "admin", "password", "123456").
    [[nodiscard]] static bool isCommonDefault(std::string_view password) noexcept;

    /// @brief Counts distinct character classes present (lower, upper, digit, symbol).
    /// @param[in] password Candidate password.
    /// @return Number of classes present (0..4).
    [[nodiscard]] static std::size_t countClasses(std::string_view password) noexcept;

    /// @brief Formats human-readable explanation of a failure result.
    /// @param[in] result The check result.
    /// @return Descriptive error string for SOAP faults.
    [[nodiscard]] static std::string describe(PasswordCheckResult result);
};

} // namespace Onvif
