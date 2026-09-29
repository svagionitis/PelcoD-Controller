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

    /// @brief Builds an $--RSD sentence for radar system data and cursor.
    /// @param[in] data RsdData struct.
    /// @param[in] talkerId 2-character talker ID (default "RA").
    /// @return Formatted NMEA sentence string.
    [[nodiscard]] static std::string buildRsd(const RsdData& data, std::string_view talkerId = "RA");

    /// @brief Builds an $--OSD sentence for own ship data.
    /// @param[in] data OsdData struct.
    /// @param[in] talkerId 2-character talker ID (default "RA").
    /// @return Formatted NMEA sentence string.
    [[nodiscard]] static std::string buildOsd(const OsdData& data, std::string_view talkerId = "RA");

    /// @brief Builds an $--APB sentence for autopilot route navigation.
    /// @param[in] data ApbData struct.
    /// @param[in] talkerId 2-character talker ID (default "AP").
    /// @return Formatted NMEA sentence string.
    [[nodiscard]] static std::string buildApb(const ApbData& data, std::string_view talkerId = "AP");

    /// @brief Builds an $--BWC sentence for bearing and distance to waypoint.
    /// @param[in] data BwcData struct.
    /// @param[in] talkerId 2-character talker ID (default "GP").
    /// @return Formatted NMEA sentence string.
    [[nodiscard]] static std::string buildBwc(const BwcData& data, std::string_view talkerId = "GP");

    /// @brief Builds an $--MWV sentence for wind speed and angle.
    /// @param[in] data MwvData struct.
    /// @param[in] talkerId 2-character talker ID (default "WI").
    /// @return Formatted NMEA sentence string.
    [[nodiscard]] static std::string buildMwv(const MwvData& data, std::string_view talkerId = "WI");

    /// @brief Builds an $--HDG sentence for magnetic heading, deviation and variation.
    /// @param[in] data HdgData struct.
    /// @param[in] talkerId 2-character talker ID (default "HC").
    /// @return Formatted NMEA sentence string.
    [[nodiscard]] static std::string buildHdg(const HdgData& data, std::string_view talkerId = "HC");

    /// @brief Builds an $--RMB sentence for recommended minimum navigation info.
    /// @param[in] data RmbData struct.
    /// @param[in] talkerId 2-character talker ID (default "GP").
    /// @return Formatted NMEA sentence string with checksum.
    [[nodiscard]] static std::string buildRmb(const RmbData& data, std::string_view talkerId = "GP");

    /// @brief Builds an $--RTE sentence for route waypoint sequence.
    /// @param[in] data RteData struct.
    /// @param[in] talkerId 2-character talker ID (default "GP").
    /// @return Formatted NMEA sentence string with checksum.
    [[nodiscard]] static std::string buildRte(const RteData& data, std::string_view talkerId = "GP");

    /// @brief Builds an $--WPL sentence for waypoint location.
    /// @param[in] data WplData struct.
    /// @param[in] talkerId 2-character talker ID (default "GP").
    /// @return Formatted NMEA sentence string with checksum.
    [[nodiscard]] static std::string buildWpl(const WplData& data, std::string_view talkerId = "GP");

    /// @brief Builds an $--MTW sentence for mean water temperature.
    /// @param[in] data MtwData struct.
    /// @param[in] talkerId 2-character talker ID (default "WI").
    /// @return Formatted NMEA sentence string with checksum.
    [[nodiscard]] static std::string buildMtw(const MtwData& data, std::string_view talkerId = "WI");

    /// @brief Builds an $--MMB sentence for barometric pressure.
    /// @param[in] data MmbData struct.
    /// @param[in] talkerId 2-character talker ID (default "WI").
    /// @return Formatted NMEA sentence string with checksum.
    [[nodiscard]] static std::string buildMmb(const MmbData& data, std::string_view talkerId = "WI");

    /// @brief Builds an $--MDA sentence for meteorological composite data.
    /// @param[in] data MdaData struct.
    /// @param[in] talkerId 2-character talker ID (default "WI").
    /// @return Formatted NMEA sentence string with checksum.
    [[nodiscard]] static std::string buildMda(const MdaData& data, std::string_view talkerId = "WI");

    /// @brief Builds a $PFEC,GPcmd,p pan/tilt velocity drive command.
    /// @param[in] panSpeed Pan speed [-100 .. 100].
    /// @param[in] tiltSpeed Tilt speed [-100 .. 100].
    /// @return Formatted NMEA sentence string with checksum.
    [[nodiscard]] static std::string buildPfecVelocity(int panSpeed, int tiltSpeed);

    /// @brief Builds a $PFEC,GPcmd,a absolute slew angle command.
    /// @param[in] panDeg Target azimuth angle [0.0 .. 360.0).
    /// @param[in] tiltDeg Target elevation angle [-90.0 .. +90.0].
    /// @return Formatted NMEA sentence string with checksum.
    [[nodiscard]] static std::string buildPfecAbsolute(double panDeg, double tiltDeg);

    /// @brief Builds a $PFEC,GPcmd preset store ('s') or goto ('g') command.
    /// @param[in] action 's' for save, 'g' for goto.
    /// @param[in] presetId Preset number [1 .. 255].
    /// @return Formatted NMEA sentence string with checksum.
    [[nodiscard]] static std::string buildPfecPreset(char action, std::uint8_t presetId);

    /// @brief Builds a $PFEC,GPcmd,z zoom speed command.
    /// @param[in] speed Zoom velocity [-100 .. 100].
    /// @return Formatted NMEA sentence string with checksum.
    [[nodiscard]] static std::string buildPfecZoom(int speed);

    /// @brief Builds a $PFEC,GPpos gimbal position query command.
    /// @return Formatted NMEA sentence string "$PFEC,GPpos*5B\r\n".
    [[nodiscard]] static std::string buildPfecQueryPos();

    /// @brief Builds a $PFEC,GPpos position report sentence.
    /// @param[in] panDeg Current pan angle.
    /// @param[in] tiltDeg Current tilt angle.
    /// @return Formatted NMEA sentence string with checksum.
    [[nodiscard]] static std::string buildPfecPosReport(double panDeg, double tiltDeg);

    /// @brief Builds a $PFEC,GPcam sensor/palette command.
    /// @param[in] command Parameter string (e.g. "c,vis", "c,ir", "p,1", "nuc").
    /// @return Formatted NMEA sentence string with checksum.
    [[nodiscard]] static std::string buildPfecCameraCommand(std::string_view command);

    /// @brief Helper to format decimal degrees to NMEA coordinate strings.
    /// @param[in] deg Decimal degrees.
    /// @param[in] isLatitude True for latitude (ddmm.mmmm), false for longitude (dddmm.mmmm).
    /// @param[out] outCoord Formatted coordinate digits string.
    /// @param[out] outHemi Hemisphere character ('N', 'S', 'E', or 'W').
    static void formatCoordinate(double deg, bool isLatitude, std::string& outCoord, char& outHemi);
};

} // namespace Nmea
