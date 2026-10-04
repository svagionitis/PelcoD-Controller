#pragma once

/// @file SpatialDataRecorder.h
/// @brief Time-series accumulator and decimation filter for KLV spatial data.

#include "SpatialExportTypes.h"
#include "KlvTypes.h"

#include <cstddef>
#include <vector>

namespace Mapping {

/// @class SpatialDataRecorder
/// @brief Records incoming STANAG 4609 telemetry frames, computes missing geometry,
///        and downsamples data for optimal GIS rendering performance.
class SpatialDataRecorder {
public:
    SpatialDataRecorder() = default;
    ~SpatialDataRecorder() = default;

    /// @brief Appends a telemetry message to the recorder.
    /// @param[in] msg Decoded ST 0601 local set message.
    void addTelemetryFrame(const Klv::UasDatalinkMessage& msg);

    /// @brief Clears all recorded spatial data.
    void clear() noexcept;

    /// @brief Downsamples recorded points according to decimation policy.
    /// @param[in] config Decimation parameters.
    void decimate(const DecimationConfig& config);

    /// @brief Returns recorded track points.
    /// @return Const reference to track point vector.
    [[nodiscard]] const std::vector<SpatialTrackPoint>& trackPoints() const noexcept;

    /// @brief Returns recorded 3D frustum geometries.
    /// @return Const reference to frustum mesh vector.
    [[nodiscard]] const std::vector<FrustumMesh3D>& frustums() const noexcept;

    /// @brief Returns total number of recorded telemetry frames.
    /// @return Frame count.
    [[nodiscard]] std::size_t size() const noexcept;

    /// @brief Returns true if no frames are recorded.
    /// @return True if empty.
    [[nodiscard]] bool empty() const noexcept;

private:
    std::vector<SpatialTrackPoint> m_trackPoints {};
    std::vector<FrustumMesh3D> m_frustums {};
};

} // namespace Mapping
