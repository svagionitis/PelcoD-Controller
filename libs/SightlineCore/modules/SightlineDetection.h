#pragma once

/// @file SightlineDetection.h
/// @brief Sightline SLA Detection Module (Automatic object detection, MTI, marine).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__detect.html
/// @see https://knowledge.sightlineintelligence.com/wp-content/uploads/EAN-Detection-Modes.pdf

#include "../SightlineTypes.h"

#include <array>
#include <cstdint>

namespace Sightline {

/// @enum DetectionMode
/// @brief Sightline EAN-Detection-Modes algorithmic modes.
/// @details Supported algorithms across Sightline firmware 2.22.x - 3.11.x.
enum class DetectionMode : std::uint8_t {
    Off = 0U, ///< Target detection disabled
    Vehicle = 1U, ///< Vehicle Moving Target Indication (10-100px moving targets)
    Drone = 2U, ///< Drone MTI with rotary/fixed-wing classifier assistance
    Staring = 3U, ///< Staring MTI (ground / stationary low drift camera)
    Aerial = 4U, ///< Aerial MTI (perspective compensation for airborne platforms)
    Anomaly = 5U, ///< Color and intensity anomaly detection (moving or stationary)
    Radiometric = 6U, ///< Temperature/luminance gray value thresholding (8/14/16-bit)
    Maritime = 7U, ///< Maritime horizon / surface target detection
    Blob = 8U, ///< Blob hotspot detection (light or dark vs background)
    Gas = 9U, ///< Optical gas imaging enhancement (methane band)
    Person = 10U, ///< Person MTI (slow, irregular human movement)
    AIDetection = 11U, ///< Deep learning neural network full-frame detection
    Motion = 12U ///< Decoupled motion energy detection without required tracks
};

/// @enum SensitivityMode
/// @brief Automatic target detection sensitivity configuration mode.
enum class SensitivityMode : std::uint8_t {
    Auto = 0U, ///< Auto sensitivity directly computes internal noise floors
    Manual = 1U ///< Manual mode exposing background threshold, watch frames & suspicious score
};

/// @enum BlobDirection
/// @brief Target contrast direction for Blob detection mode (IDD 3.5.x+).
enum class BlobDirection : std::uint8_t {
    Bright = 0U, ///< Detect objects brighter than surroundings
    Dark = 1U, ///< Detect objects darker than surroundings
    Both = 2U ///< Detect both bright and dark objects
};

/// @enum DetectionDownsample
/// @brief Image downsampling factor for detection performance vs resolution tradeoffs.
enum class DetectionDownsample : std::uint8_t {
    Auto = 0U, ///< Automatic downsampling determined by resolution and algorithm
    None = 1U, ///< Full resolution processing (required for small 2x2 targets)
    Downsample2x = 2U, ///< 2x downsampling
    Downsample4x = 3U ///< 4x downsampling
};

/// @enum RoiGeometryMode
/// @brief Geometric definition of the region of interest (IDD 3.5.x - 3.6.x+).
enum class RoiGeometryMode : std::uint8_t {
    BoundingBox = 0U, ///< Rectangular bounding box coordinates
    DetectionLine = 1U, ///< Directional vector line (Left/Right coordinates)
    MaskedGrid = 2U ///< Block-based grid mask (up to 256 cells, 4x 64-bit masks)
};

/// @enum LineReportSide
/// @brief Reporting boundary side relative to detection line.
enum class LineReportSide : std::uint8_t {
    Above = 0U, ///< Report detections located above the line
    Below = 1U, ///< Report detections located below the line
    Both = 2U ///< Report detections crossing in either direction
};

/// @struct MsgSetDetectionParameters
/// @brief Moving Target Indication (MTI) sensitivity and thresholds (Message ID 0x2D).
/// @details Configures primary (index 0) or secondary (index 1) detection algorithms.
struct MsgSetDetectionParameters {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t detectionIndex { 0U }; ///< 0: Primary (All modes), 1: Secondary (Dual mode)
    DetectionMode mode { DetectionMode::Off };
    SensitivityMode sensitivityMode { SensitivityMode::Auto };
    std::uint8_t threshold { 20U }; ///< Sensitivity threshold (1..100)
    std::uint16_t minTargetSize { 4U }; ///< Minimum target size in pixels
    std::uint16_t maxTargetSize { 200U }; ///< Maximum target size in pixels
    std::uint8_t bkgdThreshold { 20U }; ///< Manual mode: Background difference threshold
    std::uint8_t watchFrames { 3U }; ///< Manual mode: Consecutive frames before reporting
    std::uint8_t suspiciousScore { 10U }; ///< Manual mode: Track stability score threshold
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
/// @brief Region of Interest: Bounding Box, Detection Line, or Masked Grid (Message ID 0x7C / 0x7D).
/// @details Supports search regions, detection regions, directional lines, and 256-block grid masks.
struct MsgDetectionROI {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t detectionIndex { 0U }; ///< 0: Primary ROI, 1: Secondary ROI
    std::uint8_t roiIndex { 0U }; ///< ROI slot index (0..3)
    std::uint8_t roiType { 0U }; ///< 0: Inclusion / Search ROI, 1: Exclusion / Detection ROI
    RoiGeometryMode geometryMode { RoiGeometryMode::BoundingBox }; ///< BoundingBox, Line, or Grid
    std::uint16_t left { 0U }; ///< Bounding box left column
    std::uint16_t top { 0U }; ///< Bounding box top row
    std::uint16_t width { 0U }; ///< Bounding box width
    std::uint16_t height { 0U }; ///< Bounding box height
    std::uint16_t lineLeftX { 0U }; ///< Detection line left point X
    std::uint16_t lineLeftY { 0U }; ///< Detection line left point Y
    std::uint16_t lineRightX { 0U }; ///< Detection line right point X (>= lineLeftX)
    std::uint16_t lineRightY { 0U }; ///< Detection line right point Y
    LineReportSide lineSide { LineReportSide::Both }; ///< Reporting side for detection line
    std::uint8_t blocksWide { 8U }; ///< Grid mask columns (Blocks Wide)
    std::uint8_t blocksHigh { 8U }; ///< Grid mask rows (Blocks High)
    std::array<std::uint64_t, 4U> gridMasks { 0U, 0U, 0U, 0U }; ///< 4x 64-bit masks (256 blocks)
    bool showRegions { false }; ///< Visual debug display of masked blocks
};

/// @struct MsgAdvancedDetectionParameters
/// @brief Advanced algorithmic tuning, velocity gating, and filtering (Message ID 0x76 / 0x77).
/// @details Supports mode-specific configurations per EAN-Detection-Modes.
struct MsgAdvancedDetectionParameters {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t detectionIndex { 0U }; ///< 0: Primary, 1: Secondary
    std::uint16_t minVelocity { 0U }; ///< Minimum target velocity (1/256 pixels/frame)
    std::uint16_t maxVelocity { 65535U }; ///< Maximum target velocity (1/256 pixels/frame)
    std::uint8_t persistenceFrames { 3U }; ///< Minimum consecutive frames required
    std::uint16_t mergeDistance { 10U }; ///< Merge proximity in pixels
    bool hideOverlapTracks { true }; ///< Suppress MTI detections under active tracks
    bool detectNearTrack { false }; ///< Associate detections with nearest track (>=50% IoU)
    std::uint8_t averageTimeConstant { 10U }; ///< Staring / Gas background model time constant
    std::uint8_t edgePenalty { 64U }; ///< Staring / Gas background edge false alarm penalty
    std::uint8_t nFramesBack { 10U }; ///< Aerial / Motion temporal history frame depth
    bool useRegistration { true }; ///< Anomaly / Maritime / Radiometric / Blob frame alignment
    std::uint8_t updateRate { 32U }; ///< Anomaly / Maritime background update rate (8..64)
    std::uint8_t surroundSize { 25U }; ///< Blob detection surround kernel size (pixels)
    BlobDirection blobDirection { BlobDirection::Both }; ///< Blob contrast direction
    bool use8BitImages { false }; ///< Speed up 16-bit sensors by using 8-bit image
    std::uint8_t gasAddOriginal { 128U }; ///< Gas enhancement fraction of original image (0..255)
    std::uint8_t gasColor { 0U }; ///< Gas enhancement overlay color (0: White)
    std::uint8_t aiIouThreshold { 45U }; ///< AI detection box merge IoU threshold (1..100)
    bool enableMtd { false }; ///< AI-MTD / Aerial / Staring automatic track creation
    DetectionDownsample downsample { DetectionDownsample::Auto }; ///< Resolution downsampling
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
