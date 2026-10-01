#pragma once

/// @file SightlineDetection.h
/// @brief Sightline SLA Detection Module (Automatic object detection, MTI, marine).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__detect.html

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @struct MsgSetDetectionParameters
/// @brief Moving Target Indication (MTI) sensitivity and thresholds (Message ID 0x2D).
struct MsgSetDetectionParameters {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t mode { 0U }; // 0: Off, 1: MTI, 2: Vehicle, 3: Person, 4: Maritime
    std::uint8_t threshold { 20U };
    std::uint16_t minTargetSize { 4U };
    std::uint16_t maxTargetSize { 200U };
};

/// @struct MsgSetVMTI
/// @brief Video Moving Target Indication (VMTI) pipeline configuration (Message ID 0x84).
struct MsgSetVMTI {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t enable { 0U }; ///< 0: Disable, 1: Enable
    std::uint8_t sensitivity { 50U }; ///< Detection sensitivity (1..100)
    std::uint16_t minTargetArea { 4U }; ///< Minimum target size in pixels
    std::uint16_t maxTargetArea { 5000U }; ///< Maximum target size in pixels
    std::uint8_t mode { 0U }; ///< 0: Standard, 1: Maritime, 2: Aerial
};

/// @struct MsgDetectionROI
/// @brief Polygonal or rectangular region of interest bounding box (Message ID 0x7C / 0x7D).
struct MsgDetectionROI {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t roiIndex { 0U }; ///< ROI slot index (0..3)
    std::uint8_t roiType { 0U }; ///< 0: Inclusion ROI, 1: Exclusion ROI
    std::uint16_t left { 0U }; ///< Bounding box left column
    std::uint16_t top { 0U }; ///< Bounding box top row
    std::uint16_t width { 0U }; ///< Bounding box width
    std::uint16_t height { 0U }; ///< Bounding box height
};

/// @struct MsgAdvancedDetectionParameters
/// @brief Motion thresholding, velocity gating, and frame persistence bounds (Message ID 0x76 / 0x77).
struct MsgAdvancedDetectionParameters {
    std::uint8_t cameraIndex { 0U };
    std::uint16_t minVelocity { 0U }; ///< 1/256 pixels/frame
    std::uint16_t maxVelocity { 65535U }; ///< 1/256 pixels/frame
    std::uint8_t persistenceFrames { 3U }; ///< Minimum consecutive frames required
    std::uint16_t mergeDistance { 10U }; ///< Merge proximity in pixels
};

/// @struct MsgTrackingBoxPixelStats
/// @brief Grayscale and luminance pixel statistics inside tracking gate (Message ID 0x78).
struct MsgTrackingBoxPixelStats {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t trackId { 0U };
    std::uint16_t meanIntensity { 0U }; ///< Mean pixel luminance (0..65535)
    std::uint16_t stdDevIntensity { 0U }; ///< Standard deviation of luminance
    std::uint8_t minIntensity { 0U }; ///< Minimum pixel luminance
    std::uint8_t maxIntensity { 255U }; ///< Maximum pixel luminance
};

/// @struct MsgDoDetectSnapShot
/// @brief Triggers automated high-res image capture of target detection (Message ID 0xAB).
struct MsgDoDetectSnapShot {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t detectionIndex { 0U };
};

} // namespace Sightline
