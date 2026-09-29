#pragma once

/// @file NmeaSentenceParser.h
/// @brief Zero-allocation tokenizer and strongly-typed deserializer for NMEA 0183 sentences.

#include "NmeaChecksum.h"
#include "NmeaTypes.h"
#include "PfecTypes.h"

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
};

} // namespace Nmea
