#pragma once

/// @file FujinonParser.h
/// @brief Decoders for incoming Fujinon SX800 / SX801 extended response frames.

#include "FujinonTypes.h"
#include "PelcoDFrame.h"

#include <cstdint>
#include <string>
#include <vector>

namespace PelcoD {

/// @class FujinonParser
/// @brief Parser extracting optical, stabilization, and filter states from Fujinon responses.
class FujinonParser {
public:
    FujinonParser() = delete;

    /// @brief Validates frame length, synchronization byte, and Pelco-D checksum.
    /// @param[in] frame Raw frame bytes to validate.
    /// @return True if frame conforms to standard 7-byte or 18-byte protocol checksum.
    [[nodiscard]] static bool isValidResponse(const std::vector<std::uint8_t>& frame) noexcept;

    /// @brief Checks if frame is an extended Fujinon response packet.
    /// @details Validates checksum and verifies standard 7-byte or 18-byte Fujinon response opcodes.
    /// @param[in] frame Raw protocol bytes received from transport.
    /// @return True if frame is a valid Fujinon response; false if corrupted, foreign, or unrecognized.
    [[nodiscard]] static bool isFujinonResponse(const std::vector<std::uint8_t>& frame) noexcept;

    /// @brief Decodes 7-byte focus position response (opcode 0x00 0x81).
    /// @details Extracts 16-bit big-endian focus coordinate from payload bytes 4 and 5 after checksum verification.
    /// @param[in] frame Received 7-byte response packet.
    /// @param[out] focusPos Output variable receiving the decoded 16-bit focus position.
    /// @return True if frame matched opcode 0x00 0x81 and checksum was valid; false otherwise.
    [[nodiscard]] static bool parseQueryFocus(const std::vector<std::uint8_t>& frame, std::uint16_t& focusPos) noexcept;

    /// @brief Decodes 7-byte zoom position response (opcode 0x00 0x83).
    /// @details Extracts 16-bit big-endian optical zoom coordinate from payload bytes 4 and 5 after checksum
    /// verification.
    /// @param[in] frame Received 7-byte response packet.
    /// @param[out] zoomPos Output variable receiving the decoded 16-bit zoom position.
    /// @return True if frame matched opcode 0x00 0x83 and checksum was valid; false otherwise.
    [[nodiscard]] static bool parseQueryZoom(const std::vector<std::uint8_t>& frame, std::uint16_t& zoomPos) noexcept;

    /// @brief Decodes 18-byte Serial Number response (opcode 0x00 0x89).
    /// @details Extracts 15-byte ASCII serial string from payload bytes 2 through 16 after checksum verification.
    /// @param[in] frame Received 18-byte response packet.
    /// @param[out] serialStr Output string receiving the parsed hardware serial number.
    /// @return True if frame matched opcode 0x00 0x89 and checksum was valid; false otherwise.
    [[nodiscard]] static bool parseQuerySerialNumber(const std::vector<std::uint8_t>& frame, std::string& serialStr);

    /// @brief Decodes 7-byte Firmware Version response (opcode 0x00 0x8B).
    /// @details Formats major, minor, and patch revision bytes into semantic version string.
    /// @param[in] frame Received 7-byte response packet.
    /// @param[out] fwStr Output string receiving formatted firmware version string.
    /// @return True if frame matched opcode 0x00 0x8B and checksum was valid; false otherwise.
    [[nodiscard]] static bool parseQueryFirmwareVersion(const std::vector<std::uint8_t>& frame, std::string& fwStr);

    /// @brief Decodes 7-byte Lens Status response (opcode 0x00 0x8D).
    /// @details Extracts lens hardware diagnostic flags from payload byte 5.
    /// @param[in] frame Received 7-byte response packet.
    /// @param[out] statusVal Output variable receiving raw diagnostic bitmask.
    /// @return True if frame matched opcode 0x00 0x8D and checksum was valid; false otherwise.
    [[nodiscard]] static bool parseQueryLensStatus(
        const std::vector<std::uint8_t>& frame, std::uint8_t& statusVal) noexcept;

    /// @brief Decodes 7-byte or 18-byte Photo Setting response.
    /// @details Extracts exposure, stabilization, and filter states from photo configuration telemetry.
    /// @param[in] frame Received response packet.
    /// @param[out] settings Output structure receiving parsed photo settings.
    /// @return True if frame decoded successfully; false otherwise.
    [[nodiscard]] static bool parsePhotoSettings(
        const std::vector<std::uint8_t>& frame, FujinonPhotoSettings& settings) noexcept;

    /// @brief Decodes 7-byte or 18-byte Image Quality Setting response.
    /// @details Extracts brightness, contrast, saturation, sharpness, and white balance settings.
    /// @param[in] frame Received response packet.
    /// @param[out] settings Output structure receiving parsed image quality settings.
    /// @return True if frame decoded successfully; false otherwise.
    [[nodiscard]] static bool parseImageQualitySettings(
        const std::vector<std::uint8_t>& frame, FujinonImageQualitySettings& settings) noexcept;

    /// @brief Decodes 7-byte or 18-byte Manual Setting response.
    /// @details Extracts manual aperture, electronic shutter speed, and ISO sensitivity settings.
    /// @param[in] frame Received response packet.
    /// @param[out] settings Output structure receiving parsed manual settings.
    /// @return True if frame decoded successfully; false otherwise.
    [[nodiscard]] static bool parseManualSettings(
        const std::vector<std::uint8_t>& frame, FujinonManualSettings& settings) noexcept;

    /// @brief Decodes 7-byte standard Pelco-D zoom position response (opcode 0x00 0x5D).
    /// @details Extracts 16-bit zoom coordinate from standard Pelco-D position feedback frame.
    /// @param[in] frame Received 7-byte response packet.
    /// @param[out] zoomPos Output variable receiving the decoded 16-bit zoom coordinate.
    /// @return True if frame matched opcode 0x00 0x5D and checksum was valid; false otherwise.
    [[nodiscard]] static bool parseQueryZoomStandard(
        const std::vector<std::uint8_t>& frame, std::uint16_t& zoomPos) noexcept;

    /// @brief Decodes 7-byte Fine Image Quality response (opcode 0xF0 0xFF).
    /// @details Decodes 1..100 fine adjustment parameters for brightness, contrast, saturation, sharpness, or WB shift.
    /// @param[in] frame Received 7-byte response packet.
    /// @param[out] settings Output structure receiving parsed fine image settings.
    /// @param[in] target Expected setting opcode or 0x00 for wildcard.
    /// @return True if frame decoded successfully; false otherwise.
    [[nodiscard]] static bool parseFineImageSettings(const std::vector<std::uint8_t>& frame,
        FujinonFineImageSettings& settings, std::uint8_t target = 0x00U) noexcept;

    /// @brief Decodes 7-byte extended Day/Night Setting response (opcode 0xF1 0x1F).
    /// @details Decodes scheduled Day/Night modes, switching thresholds, auto delay, and optical filter settings.
    /// @param[in] frame Received 7-byte response packet.
    /// @param[out] settings Output structure receiving parsed extended day/night settings.
    /// @param[in] target Expected setting opcode or 0x00 for wildcard.
    /// @return True if frame decoded successfully; false otherwise.
    [[nodiscard]] static bool parseDayNightExSettings(const std::vector<std::uint8_t>& frame,
        FujinonDayNightExSettings& settings, std::uint8_t target = 0x00U) noexcept;

    /// @brief Decodes 7-byte extended Zoom / Focus Setting response (opcode 0xF1 0x2F).
    /// @details Decodes extended zoom speeds (1..8), focus speeds (1..5), and digital zoom operation modes.
    /// @param[in] frame Received 7-byte response packet.
    /// @param[out] settings Output structure receiving parsed extended zoom/focus settings.
    /// @param[in] target Expected setting opcode or 0x00 for wildcard.
    /// @return True if frame decoded successfully; false otherwise.
    [[nodiscard]] static bool parseZoomFocusExSettings(const std::vector<std::uint8_t>& frame,
        FujinonZoomFocusExSettings& settings, std::uint8_t target = 0x00U) noexcept;

    /// @brief Decodes 7-byte Real-Time Clock response (0x00 0x3B).
    /// @param[in] frame Raw response packet.
    /// @param[out] data1 High byte / first telemetry parameter.
    /// @param[out] data2 Low byte / second telemetry parameter.
    /// @return True if frame decoded successfully; false otherwise.
    [[nodiscard]] static bool parseQueryRTC(
        const std::vector<std::uint8_t>& frame, std::uint8_t& data1, std::uint8_t& data2) noexcept;

    /// @brief Decodes 7-byte manual electronic shutter speed response (0x00 0x97).
    /// @param[in] frame Raw response packet.
    /// @param[out] shutterVal Decoded manual shutter speed index.
    /// @return True if frame decoded successfully; false otherwise.
    [[nodiscard]] static bool parseQueryManualShutter(
        const std::vector<std::uint8_t>& frame, std::uint8_t& shutterVal) noexcept;

    /// @brief Decodes 7-byte manual ISO / sensor gain response (0x00 0x9B).
    /// @param[in] frame Raw response packet.
    /// @param[out] isoVal Decoded manual sensor gain index.
    /// @return True if frame decoded successfully; false otherwise.
    [[nodiscard]] static bool parseQueryManualISO(
        const std::vector<std::uint8_t>& frame, std::uint8_t& isoVal) noexcept;

    /// @brief Decodes 7-byte manual aperture iris response (0x00 0x9D).
    /// @param[in] frame Raw response packet.
    /// @param[out] irisVal Decoded manual aperture iris index.
    /// @return True if frame decoded successfully; false otherwise.
    [[nodiscard]] static bool parseQueryManualIris(
        const std::vector<std::uint8_t>& frame, std::uint8_t& irisVal) noexcept;

    /// @brief Decodes 7-byte OSD display configuration response (0xF0 0x5F).
    /// @param[in] frame Raw response packet.
    /// @param[out] target Setting target opcode.
    /// @param[out] settingVal Decoded setting parameter.
    /// @return True if frame decoded successfully; false otherwise.
    [[nodiscard]] static bool parseQueryDisplay(
        const std::vector<std::uint8_t>& frame, std::uint8_t& target, std::uint8_t& settingVal) noexcept;

    /// @brief Decodes 7-byte key setting configuration response (0xF0 0xAF).
    /// @param[in] frame Raw response packet.
    /// @param[out] target Setting target opcode.
    /// @param[out] keyVal Decoded key configuration parameter.
    /// @return True if frame decoded successfully; false otherwise.
    [[nodiscard]] static bool parseQueryKeySetting(
        const std::vector<std::uint8_t>& frame, std::uint8_t& target, std::uint8_t& keyVal) noexcept;

    /// @brief Decodes 7-byte recorded movie count response (0xF0 0xB3).
    /// @param[in] frame Raw response packet.
    /// @param[out] count Total count of recorded movies.
    /// @return True if frame decoded successfully; false otherwise.
    [[nodiscard]] static bool parseQueryMovieCount(
        const std::vector<std::uint8_t>& frame, std::uint16_t& count) noexcept;

    /// @brief Decodes 7-byte recorded movie year response (0xF0 0xB7).
    /// @param[in] frame Raw response packet.
    /// @param[out] year 4-digit recording year.
    /// @return True if frame decoded successfully; false otherwise.
    [[nodiscard]] static bool parseQueryMovieYear(const std::vector<std::uint8_t>& frame, std::uint16_t& year) noexcept;

    /// @brief Decodes 7-byte recorded movie month and day response (0xF0 0xBB).
    /// @param[in] frame Raw response packet.
    /// @param[out] month Recording month (1..12).
    /// @param[out] day Recording day (1..31).
    /// @return True if frame decoded successfully; false otherwise.
    [[nodiscard]] static bool parseQueryMovieDate(
        const std::vector<std::uint8_t>& frame, std::uint8_t& month, std::uint8_t& day) noexcept;

    /// @brief Decodes 7-byte recorded movie hour and minute response (0xF0 0xBF).
    /// @param[in] frame Raw response packet.
    /// @param[out] hour Recording hour (0..23).
    /// @param[out] min Recording minute (0..59).
    /// @return True if frame decoded successfully; false otherwise.
    [[nodiscard]] static bool parseQueryMovieTime(
        const std::vector<std::uint8_t>& frame, std::uint8_t& hour, std::uint8_t& min) noexcept;

    /// @brief Decodes 7-byte internal diagnostic log data response (0xF0 0xD3).
    /// @param[in] frame Raw response packet.
    /// @param[out] data1 First log payload parameter.
    /// @param[out] data2 Second log payload parameter.
    /// @return True if frame decoded successfully; false otherwise.
    [[nodiscard]] static bool parseQueryLogData(
        const std::vector<std::uint8_t>& frame, std::uint8_t& data1, std::uint8_t& data2) noexcept;

    /// @brief Updates FujinonStatus from any matching Fujinon response frame.
    /// @param[in] frame Raw response packet.
    /// @param[in,out] status Telemetry status structure to update.
    /// @return True if frame matched and updated status; false otherwise.
    [[nodiscard]] static bool updateFujinonStatus(const std::vector<std::uint8_t>& frame, FujinonStatus& status);
};

} // namespace PelcoD
