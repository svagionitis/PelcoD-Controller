#pragma once

/// @file SightlineNucParser.h
/// @brief Parser deserializing raw Sightline SLA NUC and dead pixel configuration telemetry packets.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineNucParser
/// @brief Deserializes NUC parameters and dead pixel replacement status frames.
class SightlineNucParser {
public:
    /// @brief Parses NUC calibration parameters (Message ID 0x35).
    /// @details Deserializes camera index, NUC mode action, and shutter mode state.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized NUC structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseNucParameters(ByteView packet, MsgNucParameters& out);

    /// @brief Parses dead pixel replacement configuration (Message ID 0xA8).
    /// @details Deserializes camera index, mode, and dead pixel count.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized dead pixel structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseDeadPixel(ByteView packet, MsgDeadPixel& out);

    /// @brief Parses NUC read/write flash response (Message ID 0x36).
    /// @details Deserializes camera index, action status, and table slot index.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized read/write NUC structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseReadWriteNuc(ByteView packet, MsgReadWriteNuc& out);

    /// @brief Parses custom thermal pseudo-color palette (Message ID 0x72 / 0x73).
    /// @details Deserializes palette slot index and lookup table bytes.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized user palette structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseUserPalette(ByteView packet, MsgUserPalette& out);

    /// @brief Parses dead pixel metrics and defect statistics (Message ID 0xA1).
    /// @details Deserializes total defect count, bad columns, and bad rows.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized dead pixel stats structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseDeadPixelStats(ByteView packet, MsgDeadPixelStats& out);

    /// @brief Parses camera intrinsic geometric calibration (Message ID 0xC0).
    /// @details Deserializes focal lengths (fx, fy), principal points (cx, cy), and distortion (k1, k2, p1, p2).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized camera calibration structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseCameraCalibration(ByteView packet, MsgCameraCalibration& out);

    /// @brief Parses camera parameter file status or reply (Message ID 0xC2).
    /// @details Deserializes camera index, action code, and filename string.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized parameter file structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseCameraParameterFile(ByteView packet, MsgCameraParameterFile& out);
};

} // namespace Sightline
