#pragma once

/// @file MetadataXmpBuilder.h
/// @brief Standards-compliant Adobe XMP and ISO/IEC 12234-2 geospatial metadata builder.
/// @details Generates XMP RDF XML metadata for drone/photogrammetry/GIS applications and embeds into JPEG APP1.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Sightline {

/// @struct XmpGeospatialMetadata
/// @brief Comprehensive platform, sensor, and target spatial metrics.
struct XmpGeospatialMetadata {
    double latitudeDeg { 0.0 };              ///< Platform WGS84 latitude (-90.0 .. +90.0)
    double longitudeDeg { 0.0 };             ///< Platform WGS84 longitude (-180.0 .. +180.0)
    double absoluteAltitudeM { 0.0 };        ///< Height above ellipsoid in meters
    double relativeAltitudeM { 0.0 };        ///< Altitude relative to launch/home point
    float flightRollDeg { 0.0F };            ///< Platform roll angle in degrees
    float flightPitchDeg { 0.0F };           ///< Platform pitch angle in degrees
    float flightYawDeg { 0.0F };             ///< Platform yaw/heading (0.0 .. 360.0)
    float gimbalRollDeg { 0.0F };            ///< Sensor gimbal roll in degrees
    float gimbalPitchDeg { 0.0F };           ///< Sensor gimbal pitch (e.g. -90 = nadir)
    float gimbalYawDeg { 0.0F };             ///< Sensor gimbal absolute azimuth
    float horizontalFovDeg { 0.0F };         ///< Sensor horizontal field of view
    float verticalFovDeg { 0.0F };           ///< Sensor vertical field of view
    double slantRangeM { 0.0 };              ///< Distance to slant optical target
    double targetLatitudeDeg { 0.0 };        ///< Calculated optical line-of-sight latitude
    double targetLongitudeDeg { 0.0 };       ///< Calculated optical line-of-sight longitude
    double targetAltitudeM { 0.0 };          ///< Calculated optical line-of-sight altitude
    std::string modelName {};                ///< Camera or OEM model designation
};

/// @class MetadataXmpBuilder
/// @brief Serializes geospatial metadata into Adobe XMP RDF packets and injects into JPEG APP1 markers.
class MetadataXmpBuilder {
public:
    /// @brief Generates valid Adobe XMP RDF XML string.
    /// @details Encodes geospatial metrics using standard drone-dji and Camera photogrammetry namespaces.
    /// @param[in] meta Spatial and telemetry metrics.
    /// @return Formatted UTF-8 RDF XML packet with xpacket wrappers.
    [[nodiscard]] static std::string buildRdfXml(const XmpGeospatialMetadata& meta);

    /// @brief Builds a complete JPEG APP1 binary marker carrying XMP metadata.
    /// @param[in] meta Spatial and telemetry metrics.
    /// @return APP1 byte sequence including 0xFFE1 marker, length header, and XMP payload.
    [[nodiscard]] static std::vector<std::uint8_t> buildApp1Packet(const XmpGeospatialMetadata& meta);

    /// @brief Injects XMP APP1 packet into an existing JPEG byte stream.
    /// @details Inserts XMP marker immediately following SOI or existing Exif APP1 marker.
    /// @param[in,out] jpegStream Target JPEG image buffer.
    /// @param[in] meta Spatial and telemetry metrics.
    /// @return True if injection succeeded, false if buffer is not a valid JPEG stream.
    [[nodiscard]] static bool injectXmpIntoJpeg(
        std::vector<std::uint8_t>& jpegStream, const XmpGeospatialMetadata& meta);

    /// @brief Extracts raw XMP RDF XML from a JPEG byte stream.
    /// @param[in] jpegStream Input JPEG buffer.
    /// @param[out] outRdfXml Extracted RDF XML string.
    /// @return True if XMP APP1 marker was located and extracted.
    [[nodiscard]] static bool extractXmpFromJpeg(
        const std::vector<std::uint8_t>& jpegStream, std::string& outRdfXml);

    /// @brief Parses an XMP RDF XML string into structured spatial metadata.
    /// @param[in] rdfXml Input XML document.
    /// @param[out] outMeta Parsed spatial metadata.
    /// @return True if valid XMP with geospatial tags was parsed.
    [[nodiscard]] static bool parseRdfXml(
        std::string_view rdfXml, XmpGeospatialMetadata& outMeta);
};

} // namespace Sightline
