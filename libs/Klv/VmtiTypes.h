#pragma once

/// @file VmtiTypes.h
/// @brief Core data structures, enumerations, and constants for MISB ST 0903 Video Moving Target Indicator (VMTI).
/// @see MISB ST 0903.6 "Video Moving Target Indicator Metadata"

#include "GeoTypes.h"
#include "MiisCoreId.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Klv {

/// @brief Universal Label (UL) 16-byte key size defined by SMPTE ST 336.
inline constexpr std::size_t kVmtiUniversalLabelSize { 16U };

/// @brief Standard MISB ST 0903 16-byte Universal Label key (VMTI Local Set).
/// @details 06 0E 2B 34 02 0B 01 01 0E 01 03 03 01 00 00 00
inline constexpr std::array<std::uint8_t, kVmtiUniversalLabelSize> kMisb0903UniversalLabel {
    0x06, 0x0E, 0x2B, 0x34, 0x02, 0x0B, 0x01, 0x01, 0x0E, 0x01, 0x03, 0x03, 0x01, 0x00, 0x00, 0x00
};

/// @brief Standard prefix length for MISB ST 0903 Universal Label matching (first 12 bytes).
inline constexpr std::size_t kMisb0903PrefixSize { 12U };

/// @enum VmtiTag
/// @brief MISB ST 0903 top-level VMTI Local Set Tag identifiers.
enum class VmtiTag : std::uint32_t {
    Checksum = 1U,              ///< CRC-16-CCITT packet checksum (2 bytes)
    PrecisionTimeStamp = 2U,    ///< Microseconds since UNIX epoch (8 bytes)
    SystemName = 3U,            ///< VMTI system name / mode string (variable)
    VmtiLsVersion = 4U,         ///< VMTI LS version number (1 byte, e.g. 5 or 6)
    TotalTargets = 5U,          ///< Total targets detected in frame (variable uint)
    ReportedTargets = 6U,       ///< Number of reported targets in series (variable uint)
    FrameWidth = 8U,            ///< Video frame width in pixels (variable uint)
    FrameHeight = 9U,           ///< Video frame height in pixels (variable uint)
    SourceSensor = 10U,         ///< Source sensor name string (variable)
    HorizontalFov = 11U,        ///< Sensor horizontal FOV (IMAPB 2 bytes)
    VerticalFov = 12U,          ///< Sensor vertical FOV (IMAPB 2 bytes)
    MiisId = 13U,               ///< MISB ST 1204 MIIS Core Identifier
    VTargetSeries = 101U,       ///< Series of VTarget Packs (variable length Series)
    AlgorithmSeries = 102U,     ///< Series of Algorithm Local Sets (optional)
    OntologySeries = 103U       ///< Series of Ontology Local Sets (optional)
};

/// @enum VTargetTag
/// @brief MISB ST 0903 VTarget Pack / Local Set element tags.
enum class VTargetTag : std::uint32_t {
    TargetCentroid = 1U,            ///< Centroid pixel number: Col + (Row - 1) * FrameWidth (variable uint)
    BoundingBoxTopLeft = 2U,        ///< Bounding box top-left pixel number (variable uint)
    BoundingBoxBottomRight = 3U,    ///< Bounding box bottom-right pixel number (variable uint)
    TargetPriority = 4U,            ///< Target priority level (1 byte uint8)
    TargetConfidence = 5U,          ///< Target confidence level [0, 100]% (1 byte uint8)
    TargetHistory = 6U,             ///< Target detection persistence count (uint16)
    PercentageTargetPixels = 7U,    ///< Percentage of target pixels [1, 100]% (1 byte uint8)
    TargetColor = 8U,               ///< Dominant target color RGB (3 bytes)
    TargetIntensity = 9U,           ///< Target intensity
    TargetLocationOffsetLat = 10U,  ///< Target latitude offset from Frame Center (IMAPB 3 bytes)
    TargetLocationOffsetLon = 11U,  ///< Target longitude offset from Frame Center (IMAPB 3 bytes)
    TargetHae = 12U,                ///< Target Height Above Ellipsoid (2 bytes)
    TargetLocation = 17U,           ///< Target geodetic location pack (lat, lon, HAE, CE90, LE90)
    CentroidPixRow = 19U,           ///< Centroid pixel row (1-indexed, variable uint)
    CentroidPixCol = 20U,           ///< Centroid pixel column (1-indexed, variable uint)
    DetectionStatus = 23U           ///< Detection status (1=Moving, 2=Stopped, etc.)
};

/// @struct PixelCoord
/// @brief 1-indexed pixel coordinate in a Motion Imagery frame per MISB ST 0903 Section 9.2.3.1.
struct PixelCoord {
    std::uint32_t col { 1U }; ///< Horizontal pixel column [1, FrameWidth]
    std::uint32_t row { 1U }; ///< Vertical pixel row [1, FrameHeight]

    /// @brief Computes 1-indexed row-major pixel address: Col + (Row - 1) * FrameWidth.
    /// @param[in] frameWidth Video frame width in pixels.
    /// @return 1-indexed linear pixel number.
    [[nodiscard]] std::uint32_t toPixelNumber(std::uint32_t frameWidth) const noexcept {
        if (frameWidth == 0U) return 1U;
        return col + ((row > 0U ? row - 1U : 0U) * frameWidth);
    }

    /// @brief Reconstructs 2D pixel coordinates from a 1-indexed linear pixel address.
    /// @param[in] pixelNum Linear pixel address.
    /// @param[in] frameWidth Video frame width in pixels.
    /// @return 2D PixelCoord (col, row).
    [[nodiscard]] static PixelCoord fromPixelNumber(std::uint32_t pixelNum, std::uint32_t frameWidth) noexcept {
        if (frameWidth == 0U || pixelNum == 0U) return PixelCoord { 1U, 1U };
        const std::uint32_t zeroIdx = pixelNum - 1U;
        const std::uint32_t r = (zeroIdx / frameWidth) + 1U;
        const std::uint32_t c = (zeroIdx % frameWidth) + 1U;
        return PixelCoord { c, r };
    }
};

/// @struct PixelBoundingBox
/// @brief 2D rectangle in image pixel space defined by top-left and bottom-right coordinates.
struct PixelBoundingBox {
    PixelCoord topLeft {};     ///< Top-left corner (min col, min row)
    PixelCoord bottomRight {}; ///< Bottom-right corner (max col, max row)

    [[nodiscard]] std::uint32_t width() const noexcept {
        return (bottomRight.col >= topLeft.col) ? (bottomRight.col - topLeft.col + 1U) : 0U;
    }

    [[nodiscard]] std::uint32_t height() const noexcept {
        return (bottomRight.row >= topLeft.row) ? (bottomRight.row - topLeft.row + 1U) : 0U;
    }
};

/// @struct VTargetPack
/// @brief Strongly-typed representation of an individual MISB ST 0903 VTarget Pack.
struct VTargetPack {
    std::uint32_t targetId { 0U };                       ///< Mandatory unique target ID (BER-OID)
    std::optional<PixelCoord> centroid;                 ///< Target centroid image coordinate
    std::optional<PixelBoundingBox> boundingBox;        ///< Target image bounding box
    std::optional<std::uint8_t> priority;               ///< Target priority level
    std::optional<std::uint8_t> confidence;             ///< Target confidence level [0, 100]%
    std::optional<std::uint16_t> history;               ///< Target detection count (frames)
    std::optional<std::uint8_t> percentagePixels;       ///< Ratio of target pixels in bounding box [1, 100]%
    std::optional<std::array<std::uint8_t, 3>> colorRgb;///< Target RGB color
    std::optional<float> targetIntensity;               ///< Tag 9: Target intensity / radiometric measurement (MISB ST 0903)
    std::optional<GeoPoint2D> locationOffsetDeg;        ///< Target lat/lon offset from Frame Center
    std::optional<double> heightAboveEllipsoidM;        ///< Target HAE in meters
    std::optional<GeoPoint3D> targetLocation;           ///< Target geodetic position (lat, lon, HAE)
    std::optional<double> targetCe90M;                  ///< Target horizontal error CE90 in meters
    std::optional<double> targetLe90M;                  ///< Target vertical error LE90 in meters
    std::optional<std::uint8_t> detectionStatus;        ///< Detection status (1=Moving, 2=Stopped)
};

/// @struct VmtiLocalSet
/// @brief Strongly-typed representation of a MISB ST 0903 VMTI Local Set.
struct VmtiLocalSet {
    std::optional<std::uint64_t> precisionTimeStampUs; ///< Tag 2: Timestamp in microseconds
    std::optional<std::string> systemName;             ///< Tag 3: System identifier string
    std::uint8_t version { 6U };                       ///< Tag 4: VMTI LS version (default 6)
    std::optional<std::uint32_t> totalTargetsDetected; ///< Tag 5: Total targets detected
    std::optional<std::uint32_t> numTargetsReported;   ///< Tag 6: Number of reported targets
    std::optional<MiisCoreId> miisId {};               ///< Tag 13: MISB ST 1204 MIIS Core Identifier
    std::vector<VTargetPack> targets {};               ///< Tag 101: VTarget Series
    std::uint32_t frameWidth { 1920U };                ///< Frame width for pixel conversions
    std::uint32_t frameHeight { 1080U };               ///< Frame height for pixel conversions
};

} // namespace Klv
