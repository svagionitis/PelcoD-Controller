#pragma once

/// @file SpatialDataRecorder.h
/// @brief Time-series accumulator and decimation filter for KLV spatial data.

#include <cstddef>

#include <vector>

#include "KlvTypes.h"
#include "SpatialExportTypes.h"

namespace Mapping {

/// @class SpatialDataRecorder
/// @brief Records incoming STANAG 4609 telemetry frames, computes missing geometry,
///        and downsamples data for optimal GIS rendering performance.
/// @details Maintains synchronized time-series buffers for platform track points, 3D frustum pyramids,
///          and MISB ST 0903 VMTI moving target observations.
class SpatialDataRecorder {
public:
    /// @brief Default constructor.
    /// @details Initializes empty track, frustum, and VMTI target buffers.
    SpatialDataRecorder() = default;

    /// @brief Default destructor.
    /// @details Releases all recorded spatial buffers.
    ~SpatialDataRecorder() = default;

    /// @brief Appends a telemetry message to the recorder.
    /// @details Extracts platform 3D trajectory, sensor footprint frustums (with KlvGeodesy fallback),
    ///          and MISB ST 0903 VMTI moving target observations.
    /// @param[in] msg Decoded ST 0601 local set message.
    void addTelemetryFrame(const Klv::UasDatalinkMessage& msg);

    /// @brief Clears all recorded spatial data.
    /// @details Empties trackPoints, frustums, and vmtiTargets buffers.
    void clear() noexcept;

    /// @brief Downsamples recorded points according to decimation policy.
    /// @details Always retains the first and last recorded points while filtering intermediate points
    ///          by time interval, ground distance, or compass heading change.
    /// @param[in] config Decimation parameters.
    void decimate(const DecimationConfig& config);

    /// @brief Returns recorded track points.
    /// @details Provides read-only access to the time-ordered platform trajectory points.
    /// @return Const reference to track point vector.
    [[nodiscard]] const std::vector<SpatialTrackPoint>& trackPoints() const noexcept;

    /// @brief Returns recorded 3D frustum geometries.
    /// @details Provides read-only access to the time-ordered 3D sensor frustum meshes.
    /// @return Const reference to frustum mesh vector.
    [[nodiscard]] const std::vector<FrustumMesh3D>& frustums() const noexcept;

    /// @brief Returns recorded MISB ST 0903 VMTI target observations.
    /// @details Provides read-only access to all geodetically resolved VMTI target detections.
    /// @return Const reference to VMTI target vector.
    [[nodiscard]] const std::vector<SpatialVmtiTarget>& vmtiTargets() const noexcept;

    /// @brief Returns total number of recorded telemetry frames.
    /// @details Equivalent to trackPoints().size().
    /// @return Frame count.
    [[nodiscard]] std::size_t size() const noexcept;

    /// @brief Returns true if no frames are recorded.
    /// @details Equivalent to trackPoints().empty().
    /// @return True if empty.
    [[nodiscard]] bool empty() const noexcept;

private:
    std::vector<SpatialTrackPoint> m_trackPoints { };
    std::vector<FrustumMesh3D> m_frustums { };
    std::vector<SpatialVmtiTarget> m_vmtiTargets { };
};

} // namespace Mapping
