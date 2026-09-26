#pragma once

/// @file IDemProvider.h
/// @brief Abstract Digital Elevation Model (DEM) interface for terrain elevation queries.

#include <optional>

namespace PayloadHal {

/// @class IDemProvider
/// @brief Polymorphic abstraction for raster, procedural, or networked elevation models.
class IDemProvider {
public:
    virtual ~IDemProvider() = default;

    /// @brief Checks whether elevation data is available at the specified geodetic coordinate.
    /// @param[in] latDeg Latitude in degrees [-90.0 .. +90.0].
    /// @param[in] lonDeg Longitude in degrees [-180.0 .. +180.0].
    /// @return True if coordinate falls within coverage area and has valid data.
    [[nodiscard]] virtual bool hasCoverage(double latDeg, double lonDeg) const noexcept = 0;

    /// @brief Samples the terrain ground elevation above Mean Sea Level (MSL) in meters.
    /// @param[in] latDeg Latitude in degrees.
    /// @param[in] lonDeg Longitude in degrees.
    /// @return Ground elevation in meters MSL, or std::nullopt if outside coverage or nodata.
    [[nodiscard]] virtual std::optional<double> getElevationM(double latDeg, double lonDeg) const noexcept = 0;

    /// @brief Returns the minimum terrain elevation within the dataset.
    /// @return Minimum elevation in meters MSL.
    [[nodiscard]] virtual double minElevationM() const noexcept = 0;

    /// @brief Returns the maximum terrain elevation within the dataset.
    /// @return Maximum elevation in meters MSL.
    [[nodiscard]] virtual double maxElevationM() const noexcept = 0;
};

} // namespace PayloadHal
