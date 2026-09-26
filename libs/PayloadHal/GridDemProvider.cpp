/// @file GridDemProvider.cpp
/// @brief Implementation of GridDemProvider and ProceduralDemProvider for PayloadHal.

#include "GridDemProvider.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace PayloadHal {

GridDemProvider::GridDemProvider(double minLatDeg, double maxLatDeg,
                                 double minLonDeg, double maxLonDeg,
                                 std::size_t rows, std::size_t cols,
                                 std::vector<float> elevations,
                                 float noDataValue) noexcept
{
    loadData(minLatDeg, maxLatDeg, minLonDeg, maxLonDeg, rows, cols, std::move(elevations), noDataValue);
}

bool GridDemProvider::loadData(double minLatDeg, double maxLatDeg,
                               double minLonDeg, double maxLonDeg,
                               std::size_t rows, std::size_t cols,
                               std::vector<float> elevations,
                               float noDataValue) noexcept
{
    if (rows < 2 || cols < 2 || elevations.size() != (rows * cols) ||
        maxLatDeg <= minLatDeg || maxLonDeg <= minLonDeg) {
        m_valid = false;
        return false;
    }

    m_minLat = minLatDeg;
    m_maxLat = maxLatDeg;
    m_minLon = minLonDeg;
    m_maxLon = maxLonDeg;
    m_rows = rows;
    m_cols = cols;
    m_data = std::move(elevations);
    m_noDataValue = noDataValue;

    double minVal = std::numeric_limits<double>::infinity();
    double maxVal = -std::numeric_limits<double>::infinity();
    bool hasValidSample = false;

    for (const float val : m_data) {
        if (std::abs(val - m_noDataValue) > 0.001f && !std::isnan(val)) {
            minVal = std::min(minVal, static_cast<double>(val));
            maxVal = std::max(maxVal, static_cast<double>(val));
            hasValidSample = true;
        }
    }

    if (!hasValidSample) {
        m_minElevation = 0.0;
        m_maxElevation = 0.0;
    } else {
        m_minElevation = minVal;
        m_maxElevation = maxVal;
    }

    m_valid = true;
    return true;
}

bool GridDemProvider::hasCoverage(double latDeg, double lonDeg) const noexcept
{
    if (!m_valid) {
        return false;
    }
    return (latDeg >= m_minLat && latDeg <= m_maxLat &&
            lonDeg >= m_minLon && lonDeg <= m_maxLon);
}

std::optional<double> GridDemProvider::getElevationM(double latDeg, double lonDeg) const noexcept
{
    if (!hasCoverage(latDeg, lonDeg)) {
        return std::nullopt;
    }

    // Normalized fractional indices [0 .. rows-1] and [0 .. cols-1]
    const double rFrac = ((latDeg - m_minLat) / (m_maxLat - m_minLat)) * static_cast<double>(m_rows - 1);
    const double cFrac = ((lonDeg - m_minLon) / (m_maxLon - m_minLon)) * static_cast<double>(m_cols - 1);

    const std::size_t r0 = std::min(static_cast<std::size_t>(std::floor(rFrac)), m_rows - 2);
    const std::size_t c0 = std::min(static_cast<std::size_t>(std::floor(cFrac)), m_cols - 2);
    const std::size_t r1 = r0 + 1;
    const std::size_t c1 = c0 + 1;

    const double u = std::clamp(rFrac - static_cast<double>(r0), 0.0, 1.0);
    const double v = std::clamp(cFrac - static_cast<double>(c0), 0.0, 1.0);

    const float h00 = m_data[r0 * m_cols + c0];
    const float h01 = m_data[r0 * m_cols + c1];
    const float h10 = m_data[r1 * m_cols + c0];
    const float h11 = m_data[r1 * m_cols + c1];

    const auto isNoData = [this](float val) {
        return (std::abs(val - m_noDataValue) < 0.001f || std::isnan(val));
    };

    const bool nd00 = isNoData(h00);
    const bool nd01 = isNoData(h01);
    const bool nd10 = isNoData(h10);
    const bool nd11 = isNoData(h11);

    if (nd00 && nd01 && nd10 && nd11) {
        return std::nullopt;
    }

    // If some corners are nodata, replace them with the nearest valid corner
    float v00 = nd00 ? (nd01 ? (nd10 ? h11 : h10) : h01) : h00;
    float v01 = nd01 ? (nd00 ? (nd11 ? h10 : h11) : h00) : h01;
    float v10 = nd10 ? (nd11 ? (nd00 ? h01 : h00) : h11) : h10;
    float v11 = nd11 ? (nd10 ? (nd01 ? h00 : h01) : h10) : h11;

    // Bilinear interpolation
    const double h = (1.0 - u) * ((1.0 - v) * v00 + v * v01) +
                     u * ((1.0 - v) * v10 + v * v11);

    return h;
}

double GridDemProvider::minElevationM() const noexcept
{
    return m_minElevation;
}

double GridDemProvider::maxElevationM() const noexcept
{
    return m_maxElevation;
}

// =============================================================================
// ProceduralDemProvider Implementation
// =============================================================================

ProceduralDemProvider::ProceduralDemProvider(ElevationFunc func,
                                             double minElev, double maxElev,
                                             double minLat, double maxLat,
                                             double minLon, double maxLon) noexcept
    : m_func(std::move(func))
    , m_minElev(minElev)
    , m_maxElev(maxElev)
    , m_minLat(minLat)
    , m_maxLat(maxLat)
    , m_minLon(minLon)
    , m_maxLon(maxLon)
{
}

bool ProceduralDemProvider::hasCoverage(double latDeg, double lonDeg) const noexcept
{
    return (m_func &&
            latDeg >= m_minLat && latDeg <= m_maxLat &&
            lonDeg >= m_minLon && lonDeg <= m_maxLon);
}

std::optional<double> ProceduralDemProvider::getElevationM(double latDeg, double lonDeg) const noexcept
{
    if (!hasCoverage(latDeg, lonDeg)) {
        return std::nullopt;
    }
    return m_func(latDeg, lonDeg);
}

double ProceduralDemProvider::minElevationM() const noexcept
{
    return m_minElev;
}

double ProceduralDemProvider::maxElevationM() const noexcept
{
    return m_maxElev;
}

} // namespace PayloadHal
