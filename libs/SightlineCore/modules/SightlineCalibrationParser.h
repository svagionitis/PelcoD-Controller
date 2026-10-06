#pragma once

/// @file SightlineCalibrationParser.h
/// @brief Deserializer for Sightline camera calibration packets (0xC0 / 0xC2).

#include "SightlineCalibration.h"
#include "SightlineFraming.h"
#include "SightlineTypes.h"

namespace Sightline {

/// @class SightlineCalibrationParser
/// @brief Decodes camera intrinsic calibration and parameter file packets. Stateless and thread-safe.
class SightlineCalibrationParser {
public:
    /// @brief Parses camera intrinsic geometric calibration (Message ID 0xC0).
    /// @details Deserializes focal lengths (fx, fy), principal points (cx, cy), and distortion (k1, k2, p1, p2).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized camera calibration structure.
    /// @return True on successful parse.
    /// @retval false Wrong message ID or payload shorter than 33 bytes.
    [[nodiscard]] static bool parseCameraCalibration(ByteView packet, MsgCameraCalibration& out);

    /// @brief Parses camera parameter file status or reply (Message ID 0xC2).
    /// @details Deserializes camera index, action code, and filename string (trailing NULs removed).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized parameter file structure.
    /// @return True on successful parse.
    /// @retval false Wrong message ID or payload shorter than 2 bytes.
    [[nodiscard]] static bool parseCameraParameterFile(ByteView packet, MsgCameraParameterFile& out);
};

} // namespace Sightline
