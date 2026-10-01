#pragma once

/// @file SightlineNuc.h
/// @brief Sightline SLA NUC Module (Non-Uniformity Correction and Dead Pixel Replacement).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__nuc.html

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @struct MsgNucParameters
/// @brief Non-uniformity correction calibration and shutter state (Message ID 0x35).
struct MsgNucParameters {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t nucAction { 0U }; // 0: Query, 1: Trigger 1-point NUC, 2: Trigger 2-point NUC
    std::uint8_t shutterMode { 1U }; // 0: Manual, 1: Auto
};

/// @struct MsgDeadPixel
/// @brief Dead pixel map detection and replacement table (Message ID 0xA8).
struct MsgDeadPixel {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t mode { 1U }; // 0: Disable, 1: Enable replacement, 2: Auto-detect
    std::uint16_t deadPixelCount { 0U };
};

/// @struct MsgReadWriteNuc
/// @brief Reads, writes, or resets non-uniformity correction calibration tables in flash (Message ID 0x36).
struct MsgReadWriteNuc {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t action { 0U }; ///< 0: Read from flash, 1: Write to flash, 2: Restore factory default
    std::uint8_t tableIndex { 0U }; ///< NUC table slot index (0..3)
};

/// @struct MsgUserPalette
/// @brief Ingests or queries custom pseudo-color lookup table (LUT) for thermal sensors (Message ID 0x72 / 0x73).
struct MsgUserPalette {
    std::uint8_t paletteIndex { 0U }; ///< Palette slot index (0..3)
    std::vector<std::uint8_t> lutData {}; ///< Raw RGB or YUV palette table data
};

/// @struct MsgDeadPixelStats
/// @brief Dead pixel detection metrics and defect statistics (Message ID 0xA1).
struct MsgDeadPixelStats {
    std::uint8_t cameraIndex { 0U };
    std::uint16_t deadPixelCount { 0U };
    std::uint16_t badColumns { 0U };
    std::uint16_t badRows { 0U };
};

/// @struct MsgCameraCalibration
/// @brief Geometric intrinsic pinhole camera calibration parameters (Message ID 0xC0).
struct MsgCameraCalibration {
    std::uint8_t cameraIndex { 0U };
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
    std::uint8_t cameraIndex { 0U };
    std::uint8_t action { 0U }; ///< 0: Load file, 1: Save file, 2: Reset to default
    std::string filename {}; ///< Target parameter configuration filename
};

} // namespace Sightline
