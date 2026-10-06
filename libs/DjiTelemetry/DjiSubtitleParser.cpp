/// @file DjiSubtitleParser.cpp
/// @brief Implementation of the DJI telemetry text parser.

#include "DjiSubtitleParser.h"

#include <array>
#include <charconv>
#include <cmath>
#include <system_error>
#include <vector>

namespace Dji {

namespace {

    constexpr std::int64_t kUsPerSecond { 1000000 };
    constexpr std::int64_t kSecondsPerDay { 86400 };
    constexpr std::int64_t kMinYear { 1900 };
    constexpr std::int64_t kMaxYear { 2999 };
    constexpr std::size_t kDateLen { 10U }; // YYYY-MM-DD
    constexpr std::size_t kTimeMinLen { 8U }; // HH:MM:SS
    constexpr std::size_t kMaxFracDigits { 6U };

    /// @brief Returns true for token separators (whitespace and commas).
    /// @param[in] c Character to test.
    /// @return True if @p c separates tokens.
    [[nodiscard]] constexpr bool isSeparator(char c) noexcept
    {
        return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n') || (c == ',');
    }

    /// @brief Returns true if @p c is an ASCII decimal digit.
    /// @param[in] c Character to test.
    /// @return True for '0'..'9'.
    [[nodiscard]] constexpr bool isDigit(char c) noexcept
    {
        return (c >= '0') && (c <= '9');
    }

    /// @brief Splits text into tokens on whitespace and commas.
    /// @param[in] text Input text.
    /// @return Non-empty token views into @p text.
    [[nodiscard]] std::vector<std::string_view> splitTokens(std::string_view text)
    {
        std::vector<std::string_view> tokens {};
        std::size_t pos { 0U };
        while (pos < text.size()) {
            while ((pos < text.size()) && isSeparator(text[pos])) {
                ++pos;
            }
            const std::size_t start { pos };
            while ((pos < text.size()) && !isSeparator(text[pos])) {
                ++pos;
            }
            if (pos > start) {
                tokens.push_back(text.substr(start, pos - start));
            }
        }
        return tokens;
    }

    /// @brief Parses a complete token as a finite double.
    /// @param[in] token Token text.
    /// @return Parsed value, or std::nullopt if the whole token is not a finite number.
    [[nodiscard]] std::optional<double> toDouble(std::string_view token)
    {
        if (token.empty()) {
            return std::nullopt;
        }
        double value { 0.0 };
        const char* const end { token.data() + token.size() };
        const auto result { std::from_chars(token.data(), end, value) };
        if ((result.ec != std::errc {}) || (result.ptr != end) || !std::isfinite(value)) {
            return std::nullopt;
        }
        return value;
    }

    /// @brief Parses a complete token as an unsigned 64-bit integer.
    /// @param[in] token Token text.
    /// @return Parsed value, or std::nullopt on any error.
    [[nodiscard]] std::optional<std::uint64_t> toU64(std::string_view token)
    {
        if (token.empty()) {
            return std::nullopt;
        }
        std::uint64_t value { 0U };
        const char* const end { token.data() + token.size() };
        const auto result { std::from_chars(token.data(), end, value) };
        if ((result.ec != std::errc {}) || (result.ptr != end)) {
            return std::nullopt;
        }
        return value;
    }

    /// @brief Parses a fixed-width run of digits.
    /// @param[in] s Source text.
    /// @param[in] pos Start index.
    /// @param[in] len Number of digits.
    /// @return Parsed value, or std::nullopt if out of bounds or non-digit.
    [[nodiscard]] std::optional<std::int64_t> digitsAt(std::string_view s, std::size_t pos, std::size_t len)
    {
        if ((pos > s.size()) || (len > (s.size() - pos))) {
            return std::nullopt;
        }
        std::int64_t value { 0 };
        for (std::size_t i { pos }; i < (pos + len); ++i) {
            if (!isDigit(s[i])) {
                return std::nullopt;
            }
            value = (value * 10) + static_cast<std::int64_t>(s[i] - '0');
        }
        return value;
    }

    /// @brief Returns true for Gregorian leap years.
    /// @param[in] y Year.
    /// @return True if @p y is a leap year.
    [[nodiscard]] constexpr bool isLeapYear(std::int64_t y) noexcept
    {
        return ((y % 4) == 0) && (((y % 100) != 0) || ((y % 400) == 0));
    }

    /// @brief Returns the number of days in a month.
    /// @param[in] y Year.
    /// @param[in] m Month 1..12.
    /// @return Days in month.
    [[nodiscard]] constexpr std::int64_t daysInMonth(std::int64_t y, std::int64_t m) noexcept
    {
        constexpr std::array<std::int64_t, 12U> kDays { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        if ((m == 2) && isLeapYear(y)) {
            return 29;
        }
        return kDays[static_cast<std::size_t>(m - 1)];
    }

    /// @brief Converts a proleptic Gregorian date to days since 1970-01-01.
    /// @details H. Hinnant's days_from_civil algorithm.
    /// @param[in] y Year.
    /// @param[in] m Month 1..12.
    /// @param[in] d Day 1..31.
    /// @return Days since the Unix epoch.
    [[nodiscard]] constexpr std::int64_t daysFromCivil(std::int64_t y, std::int64_t m, std::int64_t d) noexcept
    {
        const std::int64_t yy { (m <= 2) ? (y - 1) : y };
        const std::int64_t era { ((yy >= 0) ? yy : (yy - 399)) / 400 };
        const std::int64_t yoe { yy - (era * 400) };
        const std::int64_t mp { (m > 2) ? (m - 3) : (m + 9) };
        const std::int64_t doy { (((153 * mp) + 2) / 5) + (d - 1) };
        const std::int64_t doe { (yoe * 365) + (yoe / 4) - (yoe / 100) + doy };
        return (era * 146097) + doe - 719468;
    }

    /// @brief Parses "YYYY-MM-DD" + "HH:MM:SS[.fff…]" into µs since 1970 (no TZ applied).
    /// @param[in] date Date token.
    /// @param[in] time Time token.
    /// @return Microseconds, or std::nullopt if either token is malformed or out of range.
    [[nodiscard]] std::optional<std::int64_t> parseDateTime(std::string_view date, std::string_view time)
    {
        if ((date.size() != kDateLen) || (date[4] != '-') || (date[7] != '-')) {
            return std::nullopt;
        }
        if ((time.size() < kTimeMinLen) || (time[2] != ':') || (time[5] != ':')) {
            return std::nullopt;
        }
        const auto y { digitsAt(date, 0U, 4U) };
        const auto mo { digitsAt(date, 5U, 2U) };
        const auto d { digitsAt(date, 8U, 2U) };
        const auto h { digitsAt(time, 0U, 2U) };
        const auto mi { digitsAt(time, 3U, 2U) };
        const auto s { digitsAt(time, 6U, 2U) };
        if (!y || !mo || !d || !h || !mi || !s) {
            return std::nullopt;
        }
        if ((*y < kMinYear) || (*y > kMaxYear) || (*mo < 1) || (*mo > 12) || (*d < 1) || (*d > daysInMonth(*y, *mo))
            || (*h > 23) || (*mi > 59) || (*s > 59)) {
            return std::nullopt;
        }

        std::int64_t fracUs { 0 };
        if (time.size() > kTimeMinLen) {
            if (time[kTimeMinLen] != '.') {
                return std::nullopt;
            }
            const std::string_view frac { time.substr(kTimeMinLen + 1U) };
            if (frac.empty()) {
                return std::nullopt;
            }
            std::int64_t scale { kUsPerSecond };
            for (std::size_t i { 0U }; i < frac.size(); ++i) {
                if (!isDigit(frac[i])) {
                    return std::nullopt;
                }
                if (i < kMaxFracDigits) {
                    scale /= 10;
                    fracUs += static_cast<std::int64_t>(frac[i] - '0') * scale;
                }
            }
        }

        const std::int64_t days { daysFromCivil(*y, *mo, *d) };
        const std::int64_t secs { (days * kSecondsPerDay) + (*h * 3600) + (*mi * 60) + *s };
        return (secs * kUsPerSecond) + fracUs;
    }

    /// @brief Stores a numeric field if the value parses.
    /// @param[out] field Destination optional.
    /// @param[in] value Raw value token.
    void setNumber(std::optional<double>& field, std::string_view value)
    {
        const auto parsed { toDouble(value) };
        if (parsed.has_value()) {
            field = parsed;
        }
    }

    /// @brief Assigns a key/value pair to the matching sample field.
    /// @param[in,out] sample Destination sample.
    /// @param[in] key Key text (without colon).
    /// @param[in] value Raw value token.
    void assignField(DjiTelemetrySample& sample, std::string_view key, std::string_view value)
    {
        if (key == "latitude") {
            setNumber(sample.latitudeDeg, value);
        } else if ((key == "longitude") || (key == "longtitude")) {
            setNumber(sample.longitudeDeg, value);
        } else if (key == "abs_alt") {
            setNumber(sample.absAltM, value);
        } else if (key == "rel_alt") {
            setNumber(sample.relAltM, value);
        } else if (key == "gb_yaw") {
            setNumber(sample.gimbalYawDeg, value);
        } else if (key == "gb_pitch") {
            setNumber(sample.gimbalPitchDeg, value);
        } else if (key == "gb_roll") {
            setNumber(sample.gimbalRollDeg, value);
        } else if (key == "focal_len") {
            setNumber(sample.focalLenMm, value);
        } else if ((key == "dzoom_ratio") || (key == "dzoom")) {
            setNumber(sample.digitalZoom, value);
        } else {
            sample.extra[std::string(key)] = std::string(value);
        }
    }

    /// @brief Returns true if @p token looks like a key terminator ("key:" or ":").
    /// @param[in] token Token text.
    /// @return True if the token ends with a colon.
    [[nodiscard]] bool endsWithColon(std::string_view token) noexcept
    {
        return !token.empty() && (token.back() == ':');
    }

    /// @brief Parses the key/value pairs inside one [...] group.
    /// @param[in] inner Text between the brackets.
    /// @param[in,out] sample Destination sample.
    void parseBracket(std::string_view inner, DjiTelemetrySample& sample)
    {
        const std::vector<std::string_view> toks { splitTokens(inner) };
        std::size_t i { 0U };
        while (i < toks.size()) {
            const std::string_view t { toks[i] };
            std::string_view key {};
            if ((t.size() > 1U) && endsWithColon(t)) {
                key = t.substr(0U, t.size() - 1U);
                i += 1U;
            } else if (((i + 1U) < toks.size()) && (toks[i + 1U] == ":")) {
                key = t;
                i += 2U;
            } else {
                const std::size_t colon { t.find(':') };
                if ((colon != std::string_view::npos) && (colon > 0U)) {
                    assignField(sample, t.substr(0U, colon), t.substr(colon + 1U));
                }
                i += 1U;
                continue;
            }
            if ((i < toks.size()) && !endsWithColon(toks[i])) {
                assignField(sample, key, toks[i]);
                i += 1U;
            }
        }
    }

    /// @brief Scans the pre-bracket header for FrameCnt and the date/time stamp.
    /// @param[in] header Text before the first '['.
    /// @param[in,out] sample Destination sample.
    void parseHeader(std::string_view header, DjiTelemetrySample& sample)
    {
        const std::vector<std::string_view> toks { splitTokens(header) };
        for (std::size_t i { 0U }; i < toks.size(); ++i) {
            const bool hasNext { (i + 1U) < toks.size() };
            if ((toks[i] == "FrameCnt:") && hasNext) {
                sample.frameCount = toU64(toks[i + 1U]);
            } else if (hasNext && !sample.localTimeUs.has_value() && (toks[i].size() == kDateLen)) {
                sample.localTimeUs = parseDateTime(toks[i], toks[i + 1U]);
            } else {
                // Other header tokens (SrtCnt, DiffTime, HTML font tags) are ignored.
            }
        }
    }

} // namespace

std::optional<DjiTelemetrySample> parseDjiText(std::string_view text)
{
    DjiTelemetrySample sample {};

    const std::size_t firstBracket { text.find('[') };
    parseHeader(text.substr(0U, firstBracket), sample);

    std::size_t pos { firstBracket };
    while (pos != std::string_view::npos) {
        const std::size_t close { text.find(']', pos + 1U) };
        if (close == std::string_view::npos) {
            break; // unterminated group: ignore the remainder
        }
        parseBracket(text.substr(pos + 1U, close - pos - 1U), sample);
        pos = text.find('[', close + 1U);
    }

    if (!sample.latitudeDeg.has_value() && !sample.longitudeDeg.has_value()) {
        return std::nullopt;
    }
    return sample;
}

} // namespace Dji
