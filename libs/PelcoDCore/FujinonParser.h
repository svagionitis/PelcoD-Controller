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

    /// @brief Checks if frame is an extended Fujinon response.
    [[nodiscard]] static bool isFujinonResponse(const std::vector<std::uint8_t>& frame) noexcept;

    /// @brief Decodes 7-byte focus position response (0x00 0x81).
    [[nodiscard]] static bool parseQueryFocus(const std::vector<std::uint8_t>& frame, std::uint16_t& focusPos) noexcept;

    /// @brief Decodes 7-byte zoom position response (0x00 0x83).
    [[nodiscard]] static bool parseQueryZoom(const std::vector<std::uint8_t>& frame, std::uint16_t& zoomPos) noexcept;

    /// @brief Decodes 18-byte Serial Number response (0x00 0x89).
    [[nodiscard]] static bool parseQuerySerialNumber(
        const std::vector<std::uint8_t>& frame, std::string& serialStr) noexcept;

    /// @brief Decodes 7-byte Firmware Version response (0x00 0x8B).
    [[nodiscard]] static bool parseQueryFirmwareVersion(
        const std::vector<std::uint8_t>& frame, std::string& fwStr) noexcept;

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
    [[nodiscard]] static bool parseQueryRTC(
        const std::vector<std::uint8_t>& frame, std::uint8_t& data1, std::uint8_t& data2) noexcept;

    /// @brief Updates FujinonStatus from any matching Fujinon response frame.
    [[nodiscard]] static bool updateFujinonStatus(
        const std::vector<std::uint8_t>& frame, FujinonStatus& status) noexcept;
};

} // namespace PelcoD
