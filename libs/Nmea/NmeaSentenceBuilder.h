#pragma once

/// @file NmeaSentenceBuilder.h
/// @brief Generator formatting strongly-typed structures into valid NMEA 0183 sentences with checksums.

#include "NmeaChecksum.h"
#include "NmeaTypes.h"
#include "PfecTypes.h"
#include "bam/BridgeAlertTypes.h"

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

    /// @brief Builds an $--GSA sentence from GNSS DOP and active satellites.
    [[nodiscard]] static std::string buildGsa(const GsaData& data, std::string_view talkerId = "GP");

    /// @brief Builds an $--GSV sentence slice from satellites in view.
    [[nodiscard]] static std::string buildGsv(const GsvData& data, std::string_view talkerId = "GP");

    /// @brief Builds an $--ZDA sentence from UTC time, date and local zone.
    [[nodiscard]] static std::string buildZda(const ZdaData& data, std::string_view talkerId = "GP");

    /// @brief Builds an $--VBW sentence from dual ground and water speed.
    [[nodiscard]] static std::string buildVbw(const VbwData& data, std::string_view talkerId = "VD");

    /// @brief Builds an $--VHW sentence from water speed and heading.
    [[nodiscard]] static std::string buildVhw(const VhwData& data, std::string_view talkerId = "VW");

    /// @brief Builds an $--DPT sentence from water depth and transducer offset.
    [[nodiscard]] static std::string buildDpt(const DptData& data, std::string_view talkerId = "SD");

    /// @brief Builds an $--DBT sentence from depth below transducer.
    [[nodiscard]] static std::string buildDbt(const DbtData& data, std::string_view talkerId = "SD");

    /// @brief Builds an $--RSA sentence from rudder sensor angle.
    [[nodiscard]] static std::string buildRsa(const RsaData& data, std::string_view talkerId = "RA");

    /// @brief Builds an $--ALF alert sentence (IEC 62923 BAM).
    [[nodiscard]] static std::string buildAlf(const Bam::AlfData& data, std::string_view talkerId = "BN");

    /// @brief Builds an $--ALC cyclic alert list sentence (IEC 62923 BAM).
    [[nodiscard]] static std::string buildAlc(const Bam::AlcData& data, std::string_view talkerId = "BN");

    /// @brief Builds an $--ARC alert command request sentence (IEC 62923 BAM).
    [[nodiscard]] static std::string buildArc(const Bam::ArcData& data, std::string_view talkerId = "BN");

    /// @brief Builds an $--HBT heartbeat supervision sentence (IEC 61162-1).
    [[nodiscard]] static std::string buildHbt(const Bam::HbtData& data, std::string_view talkerId = "BN");

    /// @brief Builds an $--ALR legacy alert sentence.
    [[nodiscard]] static std::string buildAlr(const Bam::AlrData& data, std::string_view talkerId = "BN");

    /// @brief Builds an $--ACK legacy alert acknowledge sentence.
    [[nodiscard]] static std::string buildAck(const Bam::AckData& data, std::string_view talkerId = "BN");

    /// @brief Builds a $PFEC,GPcam,p color palette command.
    [[nodiscard]] static std::string buildPfecPalette(std::uint8_t paletteIndex);

    /// @brief Builds a $PFEC,GPcam,s gyro stabilization enable/disable command.
    [[nodiscard]] static std::string buildPfecStabilization(bool enable);

    /// @brief Builds a $PFEC,GPcam,nuc thermal Non-Uniformity Correction trigger command.
    [[nodiscard]] static std::string buildPfecNuc();

    /// @brief Builds a $PFEC,GPcam,z digital zoom factor command.
    [[nodiscard]] static std::string buildPfecDigitalZoom(double zoomFactor);

    /// @brief Helper to format decimal degrees to NMEA coordinate strings.
    /// @param[in] deg Decimal degrees.
    /// @param[in] isLatitude True for latitude (ddmm.mmmm), false for longitude (dddmm.mmmm).
    /// @param[out] outCoord Formatted coordinate digits string.
    /// @param[out] outHemi Hemisphere character ('N', 'S', 'E', or 'W').
    static void formatCoordinate(double deg, bool isLatitude, std::string& outCoord, char& outHemi);
};

} // namespace Nmea
