#pragma once

/// @file NmeaSentenceParser.h
/// @brief Zero-allocation tokenizer and strongly-typed deserializer for NMEA 0183 sentences.

#include "NmeaChecksum.h"
#include "NmeaTypes.h"
#include "PfecTypes.h"
#include "bam/BridgeAlertTypes.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace Nmea {

/// @class NmeaSentenceParser
/// @brief High-performance stateless parser for NMEA 0183 and IEC 61162-1 navigation sentences.
class NmeaSentenceParser {
public:
    /// @brief Splits an NMEA sentence into comma-separated tokens without dynamic heap allocation.
    /// @param[in] sentence Input string view containing sentence.
    /// @param[out] outTokens Vector of string_view slices representing field tokens.
    /// @note Strips leading $/!, trailing *HH checksum, and \r\n before tokenizing.
    static void tokenize(std::string_view sentence, std::vector<std::string_view>& outTokens);

    /// @brief Identifies sentence type from sentence header (e.g. $GPGGA -> GGA).
    /// @param[in] sentence Input string view.
    /// @return Enum identifying the sentence, or NmeaSentenceId::Unknown if unrecognized.
    [[nodiscard]] static NmeaSentenceId identifySentence(std::string_view sentence) noexcept;

    /// @brief Extracts the 2-character talker identifier (e.g. "GP", "HE", "RA", "AI").
    /// @param[in] sentence Input string view.
    /// @return 2-character string_view or empty if not present.
    [[nodiscard]] static std::string_view extractTalkerId(std::string_view sentence) noexcept;

    /// @brief Parses NMEA coordinate format (ddmm.mmmm, N/S or dddmm.mmmm, E/W) into decimal degrees.
    /// @param[in] coordStr Coordinate field (e.g. "4807.038").
    /// @param[in] hemiStr Hemisphere field ('N', 'S', 'E', or 'W').
    /// @param[out] outDegrees Calculated decimal degrees.
    /// @return True if successfully parsed.
    [[nodiscard]] static bool parseCoordinate(
        std::string_view coordStr, std::string_view hemiStr, double& outDegrees) noexcept;

    /// @brief Parses UTC time from hhmmss.ss format.
    /// @param[in] timeStr Time field string_view.
    /// @param[out] outTime Parsed UTC time structure.
    /// @return True if successfully parsed.
    [[nodiscard]] static bool parseUtcTime(std::string_view timeStr, NmeaUtcTime& outTime) noexcept;

    /// @brief Parses calendar date from ddmmyy format.
    /// @param[in] dateStr Date field string_view.
    /// @param[out] outDate Parsed date structure.
    /// @return True if successfully parsed.
    [[nodiscard]] static bool parseDate(std::string_view dateStr, NmeaDate& outDate) noexcept;

    /// @brief Parses $--GGA GPS Fix Data sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized GgaData struct.
    /// @param[in] verifyChecksum True to enforce 8-bit XOR checksum validation.
    /// @return True if valid GGA sentence.
    [[nodiscard]] static bool parseGga(
        std::string_view sentence, GgaData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--RMC Recommended Minimum Specific GNSS Data sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized RmcData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid RMC sentence.
    [[nodiscard]] static bool parseRmc(
        std::string_view sentence, RmcData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--HDT Heading - True sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized HdtData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid HDT sentence.
    [[nodiscard]] static bool parseHdt(
        std::string_view sentence, HdtData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--THS True Heading and Status sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized ThsData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid THS sentence.
    [[nodiscard]] static bool parseThs(
        std::string_view sentence, ThsData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--TTM Tracked Target Message sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized TtmData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid TTM sentence.
    [[nodiscard]] static bool parseTtm(
        std::string_view sentence, TtmData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--TLL Target Latitude and Longitude sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized TllData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid TLL sentence.
    [[nodiscard]] static bool parseTll(
        std::string_view sentence, TllData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--XDR Transducer Measurement sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized XdrData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid XDR sentence.
    [[nodiscard]] static bool parseXdr(
        std::string_view sentence, XdrData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $PFEC,GPpos pan/tilt position report sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outPos Deserialized PfecGimbalPosition struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid PFEC GPpos sentence.
    [[nodiscard]] static bool parsePfecPos(
        std::string_view sentence, PfecGimbalPosition& outPos, bool verifyChecksum = true) noexcept;

    /// @brief Parses $PASHR inertial attitude sentence.
    /// @details Extracts UTC time, heading, roll, pitch, heave, and accuracy statistics.
    /// @param[in] sentence Raw or tokenized sentence string.
    /// @param[out] outData Deserialized PashrData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid PASHR sentence.
    [[nodiscard]] static bool parsePashr(
        std::string_view sentence, PashrData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses FLIR $PFEC,GPatt proprietary vessel attitude report sentence.
    /// @details Extracts yaw, pitch, and roll angular orientation.
    /// @param[in] sentence Raw or tokenized sentence string.
    /// @param[out] outAtt Deserialized PfecAttitudeData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid PFEC GPatt sentence.
    [[nodiscard]] static bool parsePfecAtt(
        std::string_view sentence, PfecAttitudeData& outAtt, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--RSD Radar System Data (radar cursor) sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized RsdData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid RSD sentence.
    [[nodiscard]] static bool parseRsd(
        std::string_view sentence, RsdData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--OSD Own Ship Data sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized OsdData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid OSD sentence.
    [[nodiscard]] static bool parseOsd(
        std::string_view sentence, OsdData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--APB Autopilot Sentence "B".
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized ApbData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid APB sentence.
    [[nodiscard]] static bool parseApb(
        std::string_view sentence, ApbData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--BWC Bearing and Distance to Waypoint (Great Circle) sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized BwcData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid BWC sentence.
    [[nodiscard]] static bool parseBwc(
        std::string_view sentence, BwcData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--MWV Wind Speed and Angle sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized MwvData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid MWV sentence.
    [[nodiscard]] static bool parseMwv(
        std::string_view sentence, MwvData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--HDG Heading, Deviation & Variation sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized HdgData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid HDG sentence.
    [[nodiscard]] static bool parseHdg(
        std::string_view sentence, HdgData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--RMB Recommended Minimum Navigation Information sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized RmbData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid RMB sentence.
    [[nodiscard]] static bool parseRmb(
        std::string_view sentence, RmbData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--RTE Route sentence (single slice of route).
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized RteData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid RTE sentence.
    [[nodiscard]] static bool parseRte(
        std::string_view sentence, RteData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--WPL Waypoint Location sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized WplData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid WPL sentence.
    [[nodiscard]] static bool parseWpl(
        std::string_view sentence, WplData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--MTW Mean Water Temperature sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized MtwData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid MTW sentence.
    [[nodiscard]] static bool parseMtw(
        std::string_view sentence, MtwData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--MMB Barometric Pressure sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized MmbData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid MMB sentence.
    [[nodiscard]] static bool parseMmb(
        std::string_view sentence, MmbData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--MDA Meteorological Composite sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized MdaData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid MDA sentence.
    [[nodiscard]] static bool parseMda(
        std::string_view sentence, MdaData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--GSA GNSS DOP and Active Satellites sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized GsaData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid GSA sentence.
    [[nodiscard]] static bool parseGsa(
        std::string_view sentence, GsaData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--GSV GNSS Satellites in View sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized GsvData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid GSV sentence.
    [[nodiscard]] static bool parseGsv(
        std::string_view sentence, GsvData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--ZDA UTC Time and Date sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized ZdaData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid ZDA sentence.
    [[nodiscard]] static bool parseZda(
        std::string_view sentence, ZdaData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--VBW Dual Ground/Water Speed sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized VbwData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid VBW sentence.
    [[nodiscard]] static bool parseVbw(
        std::string_view sentence, VbwData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--VHW Water Speed and Heading sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized VhwData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid VHW sentence.
    [[nodiscard]] static bool parseVhw(
        std::string_view sentence, VhwData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--DPT Depth of Water sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized DptData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid DPT sentence.
    [[nodiscard]] static bool parseDpt(
        std::string_view sentence, DptData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--DBT Depth Below Transducer sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized DbtData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid DBT sentence.
    [[nodiscard]] static bool parseDbt(
        std::string_view sentence, DbtData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--RSA Rudder Sensor Angle sentence.
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized RsaData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid RSA sentence.
    [[nodiscard]] static bool parseRsa(
        std::string_view sentence, RsaData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--ALF Alert sentence (IEC 62923 BAM).
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized AlfData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid ALF sentence.
    [[nodiscard]] static bool parseAlf(
        std::string_view sentence, Bam::AlfData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--ALC Alert Cyclic List sentence (IEC 62923 BAM).
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized AlcData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid ALC sentence.
    [[nodiscard]] static bool parseAlc(
        std::string_view sentence, Bam::AlcData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--ARC Alert Command Request sentence (IEC 62923 BAM).
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized ArcData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid ARC sentence.
    [[nodiscard]] static bool parseArc(
        std::string_view sentence, Bam::ArcData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--HBT Heartbeat Supervision sentence (IEC 61162-1).
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized HbtData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid HBT sentence.
    [[nodiscard]] static bool parseHbt(
        std::string_view sentence, Bam::HbtData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--ALR Set Alarm State sentence (Legacy IEC 61162-1).
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized AlrData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid ALR sentence.
    [[nodiscard]] static bool parseAlr(
        std::string_view sentence, Bam::AlrData& outData, bool verifyChecksum = true) noexcept;

    /// @brief Parses $--ACK Acknowledge Alarm sentence (Legacy IEC 61162-1).
    /// @param[in] sentence Raw or tokenized sentence.
    /// @param[out] outData Deserialized AckData struct.
    /// @param[in] verifyChecksum True to enforce checksum validation.
    /// @return True if valid ACK sentence.
    [[nodiscard]] static bool parseAck(
        std::string_view sentence, Bam::AckData& outData, bool verifyChecksum = true) noexcept;
};

} // namespace Nmea
