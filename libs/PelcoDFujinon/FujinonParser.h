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

    /// @brief Checks if frame is an extended Fujinon response.
    [[nodiscard]] static bool isFujinonResponse(const std::vector<std::uint8_t>& frame) noexcept;

    /// @brief Decodes 7-byte focus position response (0x00 0x81).
    [[nodiscard]] static bool parseQueryFocus(const std::vector<std::uint8_t>& frame, std::uint16_t& focusPos) noexcept;

    /// @brief Decodes 7-byte zoom position response (0x00 0x83).
    [[nodiscard]] static bool parseQueryZoom(const std::vector<std::uint8_t>& frame, std::uint16_t& zoomPos) noexcept;

    /// @brief Decodes 18-byte Serial Number response (0x00 0x89).
    [[nodiscard]] static bool parseQuerySerialNumber(const std::vector<std::uint8_t>& frame, std::string& serialStr);

    /// @brief Decodes 7-byte Firmware Version response (0x00 0x8B).
    [[nodiscard]] static bool parseQueryFirmwareVersion(const std::vector<std::uint8_t>& frame, std::string& fwStr);

    /// @brief Decodes 7-byte Lens Status response (0x00 0x8D).
    [[nodiscard]] static bool parseQueryLensStatus(
        const std::vector<std::uint8_t>& frame, std::uint8_t& statusVal) noexcept;

    /// @brief Decodes 7-byte or 18-byte Photo Setting response.
    [[nodiscard]] static bool parsePhotoSettings(
        const std::vector<std::uint8_t>& frame, FujinonPhotoSettings& settings) noexcept;

    /// @brief Decodes 7-byte or 18-byte Image Quality Setting response.
    [[nodiscard]] static bool parseImageQualitySettings(
        const std::vector<std::uint8_t>& frame, FujinonImageQualitySettings& settings) noexcept;

    /// @brief Decodes 7-byte or 18-byte Manual Setting response.
    [[nodiscard]] static bool parseManualSettings(
        const std::vector<std::uint8_t>& frame, FujinonManualSettings& settings) noexcept;

    /// @brief Decodes 7-byte standard Pelco-D zoom position response (0x00 0x5D).
    [[nodiscard]] static bool parseQueryZoomStandard(
        const std::vector<std::uint8_t>& frame, std::uint16_t& zoomPos) noexcept;

    /// @brief Decodes 7-byte Fine Image Quality response (0xF0 0xFF).
    [[nodiscard]] static bool parseFineImageSettings(const std::vector<std::uint8_t>& frame,
        FujinonFineImageSettings& settings, std::uint8_t target = 0x00U) noexcept;

    /// @brief Decodes 7-byte extended Day/Night Setting response (0xF1 0x1F).
    [[nodiscard]] static bool parseDayNightExSettings(const std::vector<std::uint8_t>& frame,
        FujinonDayNightExSettings& settings, std::uint8_t target = 0x00U) noexcept;

    /// @brief Decodes 7-byte extended Zoom / Focus Setting response (0xF1 0x2F).
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
