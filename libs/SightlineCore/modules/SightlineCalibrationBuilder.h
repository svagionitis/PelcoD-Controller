#pragma once

/// @file SightlineCalibrationBuilder.h
/// @brief Serializer for Sightline camera calibration commands (0xC0 / 0xC2).

#include "SightlineCalibration.h"
#include "SightlineFraming.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineCalibrationBuilder
/// @brief Encodes camera intrinsic calibration and parameter file commands. Stateless and thread-safe.
class SightlineCalibrationBuilder {
public:
    /// @brief Encodes geometric camera intrinsic calibration parameters (Message ID 0xC0).
    /// @param[in] msg Camera calibration parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildCameraCalibration(const MsgCameraCalibration& msg);

    /// @brief Encodes query for camera calibration parameters (Message ID 0x28 query 0xC0).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetCameraCalibration(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes camera parameter file load/save command (Message ID 0xC2).
    /// @param[in] msg Camera parameter file parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildCameraParameterFile(const MsgCameraParameterFile& msg);
};

} // namespace Sightline
