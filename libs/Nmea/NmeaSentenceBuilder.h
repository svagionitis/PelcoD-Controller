#pragma once

/// @file NmeaSentenceBuilder.h
/// @brief Generator formatting strongly-typed structures into valid NMEA 0183 sentences with checksums.

#include "NmeaChecksum.h"
#include "NmeaTypes.h"

#include <string>
#include <string_view>

namespace Nmea {

/// @class NmeaSentenceBuilder
/// @brief Factory methods producing valid NMEA 0183 sentences with proper formatting and checksums.
class NmeaSentenceBuilder {
public:
    /// @brief Builds an $--HDT sentence from true heading.
    /// @param[in] headingDeg Heading in degrees [0.0 .. 360.0).
    /// @param[in] talkerId 2-character talker ID (default "HE").
    /// @return Formatted NMEA sentence string (e.g. "$HEHDT,123.4,T*2B\r\n").
    [[nodiscard]] static std::string buildHdt(double headingDeg, std::string_view talkerId = "HE");

    /// @brief Builds an $--THS sentence from true heading and mode.
    /// @param[in] headingDeg Heading in degrees [0.0 .. 360.0).
    /// @param[in] mode FAA mode indicator (default Autonomous 'A').
    /// @param[in] talkerId 2-character talker ID (default "HE").
    /// @return Formatted NMEA sentence string.
    [[nodiscard]] static std::string buildThs(
        double headingDeg, NmeaFaaMode mode = NmeaFaaMode::Autonomous, std::string_view talkerId = "HE");

    /// @brief Builds an $--GGA sentence from position, fix quality, and altitude.
    /// @param[in] data GgaData struct containing navigation fields.
    /// @param[in] talkerId 2-character talker ID (default "GP").
    /// @return Formatted NMEA sentence string.
    [[nodiscard]] static std::string buildGga(const GgaData& data, std::string_view talkerId = "GP");

    /// @brief Builds an $--RMC sentence from GPS position, SOG, COG, and date.
    /// @param[in] data RmcData struct containing navigation fields.
    /// @param[in] talkerId 2-character talker ID (default "GP").
    /// @return Formatted NMEA sentence string.
    [[nodiscard]] static std::string buildRmc(const RmcData& data, std::string_view talkerId = "GP");

    /// @brief Builds an $--TTM sentence for an ARPA tracked radar target.
    /// @param[in] data TtmData struct.
    /// @param[in] talkerId 2-character talker ID (default "RA").
    /// @return Formatted NMEA sentence string.
    [[nodiscard]] static std::string buildTtm(const TtmData& data, std::string_view talkerId = "RA");

    /// @brief Builds an $--TLL sentence for target geodetic coordinates.
    /// @param[in] data TllData struct.
    /// @param[in] talkerId 2-character talker ID (default "RA").
    /// @return Formatted NMEA sentence string.
    [[nodiscard]] static std::string buildTll(const TllData& data, std::string_view talkerId = "RA");

    /// @brief Builds an $--XDR sentence reporting platform pitch and roll.
    /// @param[in] pitchDeg Pitch angle in degrees.
    /// @param[in] rollDeg Roll angle in degrees.
    /// @param[in] talkerId 2-character talker ID (default "II").
    /// @return Formatted NMEA sentence string.
    [[nodiscard]] static std::string buildXdrPitchRoll(
        double pitchDeg, double rollDeg, std::string_view talkerId = "II");

    /// @brief Helper to format decimal degrees to NMEA coordinate strings.
    /// @param[in] deg Decimal degrees.
    /// @param[in] isLatitude True for latitude (ddmm.mmmm), false for longitude (dddmm.mmmm).
    /// @param[out] outCoord Formatted coordinate digits string.
    /// @param[out] outHemi Hemisphere character ('N', 'S', 'E', or 'W').
    static void formatCoordinate(double deg, bool isLatitude, std::string& outCoord, char& outHemi);
};

} // namespace Nmea
