#pragma once

/// @file KmlExporter.h
/// @brief Generates OGC KML 2.2 / Google Earth XML with 4D tracks and 3D frustum pyramids.

#include <ostream>
#include <string>

#include "SpatialDataRecorder.h"
#include "SpatialExportTypes.h"

namespace Mapping {

/// @class KmlExporter
/// @brief Serializes recorded flight telemetry and frustum pyramids into standard KML.
class KmlExporter {
public:
    /// @brief Serializes recorded data to formatted KML string.
    /// @details Generates an OGC KML 2.2 document with Google Earth gx:Track extensions,
    ///          distinct styles for ground footprints, 3D frustum pyramids, boresight rays,
    ///          and MISB ST 0903 VMTI target placemarks.
    /// @param[in] recorder Spatial data provider.
    /// @param[in] config KML configuration and styling.
    /// @return Formatted KML 2.2 XML string.
    [[nodiscard]] static std::string exportToString(const SpatialDataRecorder& recorder, const KmlConfig& config = { });

    /// @brief Serializes recorded data directly to an output stream.
    /// @param[in] recorder Spatial data provider.
    /// @param[out] os Destination output stream.
    /// @param[in] config KML configuration and styling.
    /// @return True on success, false on I/O error.
    [[nodiscard]] static bool exportToStream(
        const SpatialDataRecorder& recorder, std::ostream& os, const KmlConfig& config = { });

    /// @brief Exports recorded data directly to a `.kml` file.
    /// @param[in] filePath Target destination file path.
    /// @param[in] recorder Spatial data provider.
    /// @param[in] config KML configuration and styling.
    /// @return True if file was successfully written, false otherwise.
    [[nodiscard]] static bool exportToFile(
        const std::string& filePath, const SpatialDataRecorder& recorder, const KmlConfig& config = { });
};

} // namespace Mapping
