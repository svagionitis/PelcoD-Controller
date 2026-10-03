#pragma once

/// @file GeoRegistrationTypes.h
/// @brief MISB ST 1601.2 Geo-Registration Local Set data structures and constants.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Klv {

/// @brief 16-byte Universal Label for MISB ST 1601 Geo-Registration Local Set.
/// @details 06.0E.2B.34.02.0B.01.01.0E.01.03.03.01.00.00.00 (CRC 39238).
inline constexpr std::array<std::uint8_t, 16> GeoRegistrationUl = {
    0x06U, 0x0EU, 0x2BU, 0x34U, 0x02U, 0x0BU, 0x01U, 0x01U,
    0x0EU, 0x01U, 0x03U, 0x03U, 0x01U, 0x00U, 0x00U, 0x00U
};

/// @enum GeoRegistrationTag
/// @brief Tag identifiers for MISB ST 1601.2 Geo-Registration Local Set items (Table 1).
enum class GeoRegistrationTag : std::uint8_t {
    DocumentVersion = 1U,      ///< uint (Mandatory, value 2 for ST 1601.2)
    AlgorithmName = 2U,        ///< utf8 string (Mandatory)
    AlgorithmVersion = 3U,     ///< utf8 string (Mandatory)
    RowColPoints = 4U,         ///< MDARRAY pixels (Optional)
    LatLonPoints = 5U,         ///< MDARRAY degrees (Optional)
    SecondImageName = 6U,      ///< utf8 string (Optional)
    AlgorithmConfigId = 7U,    ///< UUID 16 bytes (Optional)
    ElevationPoints = 8U,      ///< MDARRAY meters HAE (Optional)
    RowColCovariance = 9U,     ///< MDARRAY standard deviation & correlation (Optional)
    LatLonElevCovariance = 10U ///< MDARRAY standard deviation & correlation (Optional)
};

/// @struct TiePointPixel
/// @brief Represents row/column pixel coordinates for a tie point (ST 1601 Tag 4).
struct TiePointPixel {
    std::uint32_t row1 { 0U };               ///< Image 1 Row coordinate
    std::uint32_t col1 { 0U };               ///< Image 1 Column coordinate
    std::optional<std::uint32_t> row2 {};   ///< Image 2 Row coordinate (if 2 images)
    std::optional<std::uint32_t> col2 {};   ///< Image 2 Column coordinate (if 2 images)
};

/// @struct TiePointGeo
/// @brief Represents geographic coordinates and optional elevation for a tie point (Tags 5 & 8).
struct TiePointGeo {
    double lat { 0.0 };                      ///< Latitude in degrees [-90.0, +90.0]
    double lon { 0.0 };                      ///< Longitude in degrees [-180.0, +180.0]
    std::optional<double> elevation {};      ///< Height Above Ellipsoid (HAE) in meters (Tag 8)
};

/// @struct TiePointPixelCovariance
/// @brief Pixel-space standard deviations and cross-correlation coefficients (Tag 9).
struct TiePointPixelCovariance {
    double sigmaRow1 { 0.0 };                ///< Standard deviation for Image 1 Row (pixels)
    double sigmaCol1 { 0.0 };                ///< Standard deviation for Image 1 Column (pixels)
    double rho1 { 0.0 };                     ///< Cross-correlation coefficient for Image 1 [-1, 1]
    std::optional<double> sigmaRow2 {};      ///< Standard deviation for Image 2 Row (pixels)
    std::optional<double> sigmaCol2 {};      ///< Standard deviation for Image 2 Column (pixels)
    std::optional<double> rho2 {};           ///< Cross-correlation coefficient for Image 2 [-1, 1]
};

/// @struct TiePointGeoCovariance
/// @brief Geographic standard deviations and cross-correlation coefficients (Tag 10).
struct TiePointGeoCovariance {
    double sigmaLat { 0.0 };                 ///< Standard deviation for Latitude in meters [0, 650]
    double sigmaLon { 0.0 };                 ///< Standard deviation for Longitude in meters [0, 650]
    double rhoLatLon { 0.0 };                ///< Correlation coefficient between Lat and Lon [-1, 1]
    std::optional<double> sigmaElev {};      ///< Standard deviation for Elevation in meters [0, 1000]
    std::optional<double> rhoLatElev {};     ///< Correlation coefficient between Lat and Elev [-1, 1]
    std::optional<double> rhoLonElev {};     ///< Correlation coefficient between Lon and Elev [-1, 1]
};

/// @struct GeoRegistrationLocalSet
/// @brief Complete MISB ST 1601.2 Geo-Registration Local Set representation.
struct GeoRegistrationLocalSet {
    std::uint8_t documentVersion { 2U };     ///< Tag 1: MISB ST 1601 version (default 2)
    std::string algorithmName {};             ///< Tag 2: Algorithm name
    std::string algorithmVersion {};          ///< Tag 3: Algorithm version
    std::vector<TiePointPixel> pixelPoints {}; ///< Tag 4: Pixel correspondence points
    std::vector<TiePointGeo> geoPoints {};   ///< Tag 5 & 8: Geographic correspondence points
    std::optional<std::string> secondImageName {}; ///< Tag 6: Second image name
    std::optional<std::array<std::uint8_t, 16>> configUuid {}; ///< Tag 7: UUID
    std::vector<TiePointPixelCovariance> pixelCovariances {}; ///< Tag 9: Pixel uncertainties
    std::vector<TiePointGeoCovariance> geoCovariances {};     ///< Tag 10: Geo uncertainties

    /// @brief Gets the number of tie points represented in this local set.
    /// @return Number of tie points, or 0 if no correspondence points are present.
    [[nodiscard]] std::size_t tiePointCount() const noexcept {
        if (!pixelPoints.empty()) {
            return pixelPoints.size();
        }
        if (!geoPoints.empty()) {
            return geoPoints.size();
        }
        if (!pixelCovariances.empty()) {
            return pixelCovariances.size();
        }
        if (!geoCovariances.empty()) {
            return geoCovariances.size();
        }
        return 0U;
    }

    /// @brief Validates the local set against MISB ST 1601 constraints (e.g. ST 1601.1-03).
    /// @return True if valid, false otherwise.
    [[nodiscard]] bool validate() const noexcept {
        if (documentVersion == 0U) {
            return false;
        }
        if (algorithmName.empty() || algorithmVersion.empty()) {
            return false;
        }

        // ST 1601.1-03: All included array items must have the same number of tie points
        std::size_t expectedCount { 0U };
        bool countSet { false };

        auto checkCount = [&](std::size_t count) noexcept -> bool {
            if (count == 0U) {
                return true;
            }
            if (!countSet) {
                expectedCount = count;
                countSet = true;
                return true;
            }
            return count == expectedCount;
        };

        if (!checkCount(pixelPoints.size())) {
            return false;
        }
        if (!checkCount(geoPoints.size())) {
            return false;
        }
        if (!checkCount(pixelCovariances.size())) {
            return false;
        }
        if (!checkCount(geoCovariances.size())) {
            return false;
        }

        if (!pixelPoints.empty()) {
            const bool hasImg2 = pixelPoints[0].row2.has_value();
            for (const auto& pt : pixelPoints) {
                if (pt.row2.has_value() != hasImg2 || pt.col2.has_value() != hasImg2) {
                    return false;
                }
            }
        }

        if (!geoPoints.empty()) {
            const bool hasElev = geoPoints[0].elevation.has_value();
            for (const auto& pt : geoPoints) {
                if (pt.elevation.has_value() != hasElev) {
                    return false;
                }
                if (pt.lat < -90.0 || pt.lat > 90.0 || pt.lon < -180.0 || pt.lon > 180.0) {
                    return false;
                }
            }
        }

        if (!pixelCovariances.empty()) {
            const bool hasImg2 = pixelCovariances[0].sigmaRow2.has_value();
            for (const auto& cov : pixelCovariances) {
                if (cov.sigmaRow2.has_value() != hasImg2 ||
                    cov.sigmaCol2.has_value() != hasImg2 ||
                    cov.rho2.has_value() != hasImg2) {
                    return false;
                }
            }
        }

        if (!geoCovariances.empty()) {
            const bool hasElev = geoCovariances[0].sigmaElev.has_value();
            for (const auto& cov : geoCovariances) {
                if (cov.sigmaElev.has_value() != hasElev ||
                    cov.rhoLatElev.has_value() != hasElev ||
                    cov.rhoLonElev.has_value() != hasElev) {
                    return false;
                }
            }
        }

        return true;
    }
};

} // namespace Klv
