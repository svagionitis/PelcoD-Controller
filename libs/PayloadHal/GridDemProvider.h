#pragma once

/// @file GridDemProvider.h
/// @brief In-memory raster grid digital elevation model provider with bilinear interpolation.

#include "IDemProvider.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace PayloadHal {

/// @class GridDemProvider
/// @brief Regular raster geodetic elevation grid with bilinear sub-cell interpolation.
class GridDemProvider : public IDemProvider {
public:
    /// @brief Constructs an empty GridDemProvider.
    GridDemProvider() noexcept = default;

    /// @brief Constructs a GridDemProvider with populated raster data.
    /// @param[in] minLatDeg Southern boundary latitude in degrees.
    /// @param[in] maxLatDeg Northern boundary latitude in degrees.
    /// @param[in] minLonDeg Western boundary longitude in degrees.
    /// @param[in] maxLonDeg Eastern boundary longitude in degrees.
    /// @param[in] rows Number of grid rows (latitude dimension, >= 2).
    /// @param[in] cols Number of grid columns (longitude dimension, >= 2).
    /// @param[in] elevations Row-major elevation samples in meters MSL (size == rows * cols).
    /// @param[in] noDataValue Sentinel value denoting missing or unmapped elevation.
    GridDemProvider(double minLatDeg, double maxLatDeg,
                    double minLonDeg, double maxLonDeg,
                    std::size_t rows, std::size_t cols,
                    std::vector<float> elevations,
                    float noDataValue = -32767.0f) noexcept;

    ~GridDemProvider() override = default;

    // Movable, non-copyable for efficient memory management
    GridDemProvider(const GridDemProvider&) = delete;
    GridDemProvider& operator=(const GridDemProvider&) = delete;
    GridDemProvider(GridDemProvider&&) noexcept = default;
    GridDemProvider& operator=(GridDemProvider&&) noexcept = default;

    /// @brief Loads or updates raster grid data.
    bool loadData(double minLatDeg, double maxLatDeg,
                  double minLonDeg, double maxLonDeg,
                  std::size_t rows, std::size_t cols,
                  std::vector<float> elevations,
                  float noDataValue = -32767.0f) noexcept;

    // IDemProvider implementation
    [[nodiscard]] bool hasCoverage(double latDeg, double lonDeg) const noexcept override;
    [[nodiscard]] std::optional<double> getElevationM(double latDeg, double lonDeg) const noexcept override;
    [[nodiscard]] double minElevationM() const noexcept override;
    [[nodiscard]] double maxElevationM() const noexcept override;

    [[nodiscard]] std::size_t rows() const noexcept { return m_rows; }
    [[nodiscard]] std::size_t cols() const noexcept { return m_cols; }
    [[nodiscard]] double minLatitudeDeg() const noexcept { return m_minLat; }
    [[nodiscard]] double maxLatitudeDeg() const noexcept { return m_maxLat; }
    [[nodiscard]] double minLongitudeDeg() const noexcept { return m_minLon; }
    [[nodiscard]] double maxLongitudeDeg() const noexcept { return m_maxLon; }

private:
    double m_minLat { 0.0 };
    double m_maxLat { 0.0 };
    double m_minLon { 0.0 };
    double m_maxLon { 0.0 };
    std::size_t m_rows { 0U };
    std::size_t m_cols { 0U };
    std::vector<float> m_data {};
    float m_noDataValue { -32767.0f };
    double m_minElevation { 0.0 };
    double m_maxElevation { 0.0 };
    bool m_valid { false };
};

/// @class ProceduralDemProvider
/// @brief Analytical / procedural elevation model defined by an elevation function.
class ProceduralDemProvider : public IDemProvider {
public:
    using ElevationFunc = std::function<double(double latDeg, double lonDeg)>;

    /// @brief Constructs a procedural elevation model.
    /// @param[in] func Callable computing elevation at (lat, lon).
    /// @param[in] minElev Minimum expected elevation in dataset.
    /// @param[in] maxElev Maximum expected elevation in dataset.
    /// @param[in] minLat Southern boundary latitude (default -90).
    /// @param[in] maxLat Northern boundary latitude (default +90).
    /// @param[in] minLon Western boundary longitude (default -180).
    /// @param[in] maxLon Eastern boundary longitude (default +180).
    explicit ProceduralDemProvider(ElevationFunc func,
                                   double minElev = 0.0,
                                   double maxElev = 1000.0,
                                   double minLat = -90.0,
                                   double maxLat = 90.0,
                                   double minLon = -180.0,
                                   double maxLon = 180.0) noexcept;

    ~ProceduralDemProvider() override = default;

    [[nodiscard]] bool hasCoverage(double latDeg, double lonDeg) const noexcept override;
    [[nodiscard]] std::optional<double> getElevationM(double latDeg, double lonDeg) const noexcept override;
    [[nodiscard]] double minElevationM() const noexcept override;
    [[nodiscard]] double maxElevationM() const noexcept override;

private:
    ElevationFunc m_func {};
    double m_minElev { 0.0 };
    double m_maxElev { 1000.0 };
    double m_minLat { -90.0 };
    double m_maxLat { 90.0 };
    double m_minLon { -180.0 };
    double m_maxLon { 180.0 };
};

} // namespace PayloadHal
