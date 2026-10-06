#pragma once

/// @file SightlineCalibration.h
/// @brief Sightline SLA camera intrinsic calibration and parameter file messages (0xC0 / 0xC2).
/// @details Moved out of SightlineNuc.h: geometric calibration is not part of NUC / DPR.

#include "../SightlineTypes.h"

#include <cstdint>
#include <string>

namespace Sightline {

/// @struct MsgCameraCalibration
/// @brief Geometric intrinsic pinhole camera calibration parameters (Message ID 0xC0).
struct MsgCameraCalibration {
    std::uint8_t cameraIndex { 0U }; ///< Camera index
    float focalLengthX { 0.0F }; ///< fx in pixels
    float focalLengthY { 0.0F }; ///< fy in pixels
    float principalPointX { 0.0F }; ///< cx in pixels
    float principalPointY { 0.0F }; ///< cy in pixels
    float radialDistortionK1 { 0.0F }; ///< k1 coefficient
    float radialDistortionK2 { 0.0F }; ///< k2 coefficient
    float tangentialP1 { 0.0F }; ///< p1 coefficient
    float tangentialP2 { 0.0F }; ///< p2 coefficient
};

/// @struct MsgCameraParameterFile
/// @brief Loads or saves sensor parameter calibration file on local storage (Message ID 0xC2).
struct MsgCameraParameterFile {
    std::uint8_t cameraIndex { 0U }; ///< Camera index
    std::uint8_t action { 0U }; ///< 0: Load file, 1: Save file, 2: Reset to default
    std::string filename {}; ///< Target parameter configuration filename
};

} // namespace Sightline
