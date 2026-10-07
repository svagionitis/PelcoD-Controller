/// @file PasswordPolicy.cpp
/// @brief Implementation of password complexity and anti-default validation.

#include "PasswordPolicy.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace Onvif {

namespace {

    constexpr std::array<std::string_view, 12> kCommonDefaults { "admin", "password", "123456", "12345678", "pass",
        "default", "operator", "camera", "root", "guest", "service", "support" };

    [[nodiscard]] char toLowerAscii(char ch) noexcept
    {
        return ((ch >= 'A') && (ch <= 'Z')) ? static_cast<char>((ch - 'A') + 'a') : ch;
    }

    [[nodiscard]] bool equalsIgnoreCase(std::string_view s1, std::string_view s2) noexcept
    {
        if (s1.size() != s2.size()) {
            return false;
        }
        for (std::size_t i { 0U }; i < s1.size(); ++i) {
            if (toLowerAscii(s1[i]) != toLowerAscii(s2[i])) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] char normalizeLeet(char ch) noexcept
    {
        const char lower { toLowerAscii(ch) };
        switch (lower) {
        case '0':
            return 'o';
        case '1':
        case '!':
            return 'i';
        case '3':
            return 'e';
        case '4':
        case '@':
            return 'a';
        case '5':
        case '$':
            return 's';
        case '7':
            return 't';
        default:
            return lower;
        }
    }

    [[nodiscard]] bool containsNormalized(std::string_view haystack, std::string_view needle) noexcept
    {
        if (needle.empty()) {
            return false;
        }
        if (needle.size() > haystack.size()) {
            return false;
        }
        const std::size_t limit { haystack.size() - needle.size() };
        for (std::size_t i { 0U }; i <= limit; ++i) {
            bool match { true };
            for (std::size_t j { 0U }; j < needle.size(); ++j) {
                if (normalizeLeet(haystack[i + j]) != normalizeLeet(needle[j])) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return true;
            }
        }
        return false;
    }

} // namespace

bool PasswordPolicy::isCommonDefault(std::string_view password) noexcept
{
    return std::any_of(kCommonDefaults.begin(), kCommonDefaults.end(),
        [password](std::string_view def) { return equalsIgnoreCase(password, def); });
}

std::size_t PasswordPolicy::countClasses(std::string_view password) noexcept
{
    bool hasLower { false };
    bool hasUpper { false };
    bool hasDigit { false };
    bool hasSymbol { false };

    for (const char ch : password) {
        if ((ch >= 'a') && (ch <= 'z')) {
            hasLower = true;
        } else if ((ch >= 'A') && (ch <= 'Z')) {
            hasUpper = true;
        } else if ((ch >= '0') && (ch <= '9')) {
            hasDigit = true;
        } else if ((ch >= 0x20) && (ch <= 0x7E)) {
            hasSymbol = true;
        }
    }

    std::size_t classes { 0U };
    if (hasLower) {
        ++classes;
    }
    if (hasUpper) {
        ++classes;
    }
    if (hasDigit) {
        ++classes;
    }
    if (hasSymbol) {
        ++classes;
    }
    return classes;
}

PasswordCheckResult PasswordPolicy::check(
    std::string_view password, std::string_view username, const PasswordPolicyConfig& config) noexcept
{
    if (password.empty()) {
        return PasswordCheckResult::EmptyPassword;
    }
    if (config.rejectCommonDefaults && isCommonDefault(password)) {
        return PasswordCheckResult::CommonDefault;
    }
    if (config.rejectUsername && !username.empty() && containsNormalized(password, username)) {
        return PasswordCheckResult::MatchesUsername;
    }
    if (password.size() < config.minLength) {
        return PasswordCheckResult::TooShort;
    }
    if (password.size() > config.maxLength) {
        return PasswordCheckResult::TooLong;
    }
    if (countClasses(password) < config.minClasses) {
        return PasswordCheckResult::InsufficientClasses;
    }
    return PasswordCheckResult::Valid;
}

std::string PasswordPolicy::describe(PasswordCheckResult result)
{
    switch (result) {
    case PasswordCheckResult::Valid:
        return "Password is valid";
    case PasswordCheckResult::EmptyPassword:
        return "Password cannot be empty";
    case PasswordCheckResult::TooShort:
        return "Password does not meet minimum length requirement";
    case PasswordCheckResult::TooLong:
        return "Password exceeds maximum allowable length";
    case PasswordCheckResult::InsufficientClasses:
        return "Password requires mixed character classes (upper, lower, digits, symbols)";
    case PasswordCheckResult::MatchesUsername:
        return "Password must not contain or match username";
    case PasswordCheckResult::CommonDefault:
        return "Password is a known default or trivial dictionary word";
    }
    return "Password validation failed";
}

} // namespace Onvif
