#pragma once

/// @file ProtocolParser.h
/// @brief Parser interpreting incoming Pelco-D response packets and updating device status.

#include "DeviceStatus.h"
#include "PelcoDTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace PelcoD {

/// @class ProtocolParser
/// @brief Decodes Pelco-D 4-byte general, 7-byte extended, and 18-byte query responses.
class ProtocolParser {
public:
    ProtocolParser() = default;
    ~ProtocolParser() = default;

    /// @brief Decodes 4-byte general response packet.
    /// @param[in] frame Raw response frame.
    /// @param[out] address Received device address.
    /// @param[out] alarms Bitmask of active alarms.
    /// @return True if frame is a valid 4-byte general response.
    [[nodiscard]] static bool parseGeneral(
        const std::vector<std::uint8_t>& frame, std::uint8_t& address, std::uint8_t& alarms) noexcept;

    /// @brief Decodes 7-byte Pan position response (opcode 0x59).
    /// @param[in] frame Raw response frame.
    /// @param[out] panCentidegrees Pan position in 0.01 degrees.
    /// @return True if opcode is 0x59 and frame is valid.
    [[nodiscard]] static bool parsePan(const std::vector<std::uint8_t>& frame, std::uint16_t& panCentidegrees) noexcept;

    /// @brief Decodes 7-byte Tilt position response (opcode 0x5B).
    /// @param[in] frame Raw response frame.
    /// @param[out] tiltCentidegrees Tilt position in 0.01 degrees.
    /// @return True if opcode is 0x5B and frame is valid.
    [[nodiscard]] static bool parseTilt(
        const std::vector<std::uint8_t>& frame, std::uint16_t& tiltCentidegrees) noexcept;

    /// @brief Decodes 7-byte Zoom position response (opcode 0x5D).
    /// @param[in] frame Raw response frame.
    /// @param[out] zoomPosition Zoom pulse/coordinate value.
    /// @return True if opcode is 0x5D and frame is valid.
    [[nodiscard]] static bool parseZoom(const std::vector<std::uint8_t>& frame, std::uint16_t& zoomPosition) noexcept;

    /// @brief Decodes 7-byte Magnification response (opcode 0x63).
    /// @param[in] frame Raw response frame.
    /// @param[out] magnification Magnification raw value.
    /// @return True if opcode is 0x63 and frame is valid.
    [[nodiscard]] static bool parseMag(const std::vector<std::uint8_t>& frame, std::uint16_t& magnification) noexcept;

    /// @brief Decodes 7-byte Device Type response (opcode 0x6D).
    /// @param[in] frame Raw response frame.
    /// @param[out] swType Software version / model code.
    /// @param[out] hwType Hardware version / revision code.
    /// @return True if opcode is 0x6D and frame is valid.
    [[nodiscard]] static bool parseDevType(
        const std::vector<std::uint8_t>& frame, std::uint8_t& swType, std::uint8_t& hwType) noexcept;

    /// @brief Decodes 18-byte Query response packet.
    /// @param[in] frame Raw response frame.
    /// @param[out] payload Extracted text payload string.
    /// @return True if frame is a valid 18-byte query response.
    [[nodiscard]] static bool parseQuery(const std::vector<std::uint8_t>& frame, std::string& payload);

    /// @brief Updates device status from any recognized response packet.
    /// @param[in] frame Raw incoming frame.
    /// @param[in,out] status Device status to update.
    /// @param[in,out] info Device info to update.
    /// @return True if frame was recognized and applied.
    [[nodiscard]] static bool updateStatus(
        const std::vector<std::uint8_t>& frame, DeviceStatus& status, DeviceInfo& info);
};

} // namespace PelcoD
