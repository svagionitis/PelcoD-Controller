#pragma once

/// @file GeoJsonExporter.h
/// @brief Generates RFC 7946 GeoJSON layers with 3D flight tracks and footprint polygons.

#include "SpatialDataRecorder.h"
#include "SpatialExportTypes.h"

#include <ostream>
#include <string>

namespace Mapping {

/// @class GeoJsonExporter
/// @brief Serializes recorded flight telemetry and sensor footprints into standard GeoJSON.
class GeoJsonExporter {
public:
    /// @brief Serializes recorded data to formatted GeoJSON string.
    /// @param[in] recorder Spatial data provider.
    /// @param[in] config GeoJSON configuration and precision.
    /// @return RFC 7946 compliant GeoJSON string.
    [[nodiscard]] static std::string exportToString(const SpatialDataRecorder& recorder,
                                                    const GeoJsonConfig& config = {});

    /// @brief Serializes recorded data directly to an output stream.
    /// @param[in] recorder Spatial data provider.
    /// @param[out] os Destination output stream.
    /// @param[in] config GeoJSON configuration.
    /// @return True on success, false on I/O error.
    [[nodiscard]] static bool exportToStream(const SpatialDataRecorder& recorder,
                                             std::ostream& os,
                                             const GeoJsonConfig& config = {});

    /// @brief Exports recorded data directly to a `.geojson` file.
    /// @param[in] filePath Target destination file path.
    /// @param[in] recorder Spatial data provider.
    /// @param[in] config GeoJSON configuration.
    /// @return True if file was successfully written, false otherwise.
    [[nodiscard]] static bool exportToFile(const std::string& filePath,
                                           const SpatialDataRecorder& recorder,
                                           const GeoJsonConfig& config = {});
};

} // namespace Mapping
