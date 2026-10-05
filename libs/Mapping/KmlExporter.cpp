/// @file KmlExporter.cpp
/// @brief Implementation of OGC KML 2.2 / Google Earth spatial exporter.

#include "KmlExporter.h"

#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace Mapping {

namespace {

    [[nodiscard]] std::string formatIsoTime(std::uint64_t timestampUs)
    {
        if (timestampUs == 0U) {
            return "";
        }
        const std::time_t sec = static_cast<std::time_t>(timestampUs / 1000000ULL);
        const auto ms = static_cast<unsigned int>((timestampUs % 1000000ULL) / 1000ULL);
        std::tm tmBuf {};
#if defined(_WIN32)
        gmtime_s(&tmBuf, &sec);
#else
        gmtime_r(&sec, &tmBuf);
#endif
        char buf[64] {};
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", tmBuf.tm_year + 1900, tmBuf.tm_mon + 1,
            tmBuf.tm_mday, tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec, ms);
        return std::string(buf);
    }

    [[nodiscard]] std::string escapeXml(const std::string& input)
    {
        std::string out;
        out.reserve(input.size() + 8U);
        for (const char c : input) {
            switch (c) {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '\'':
                out += "&apos;";
                break;
            default:
                out += c;
                break;
            }
        }
        return out;
    }

    void writeKmlCoord(std::ostream& os, const SpatialPoint3D& pt)
    {
        os << std::fixed << std::setprecision(7) << pt.longitudeDeg << ',' << std::fixed << std::setprecision(7)
           << pt.latitudeDeg << ',' << std::fixed << std::setprecision(2) << pt.altitudeM;
    }

    void writeLinearRing(std::ostream& os, const SpatialPoint3D& p1, const SpatialPoint3D& p2, const SpatialPoint3D& p3,
        const SpatialPoint3D& p4)
    {
        os << "        <Polygon>\n";
        os << "          <altitudeMode>absolute</altitudeMode>\n";
        os << "          <outerBoundaryIs>\n";
        os << "            <LinearRing>\n";
        os << "              <coordinates>\n";
        os << "                ";
        writeKmlCoord(os, p1);
        os << " ";
        writeKmlCoord(os, p2);
        os << " ";
        writeKmlCoord(os, p3);
        os << " ";
        writeKmlCoord(os, p4);
        os << " ";
        writeKmlCoord(os, p1);
        os << "\n";
        os << "              </coordinates>\n";
        os << "            </LinearRing>\n";
        os << "          </outerBoundaryIs>\n";
        os << "        </Polygon>\n";
    }

    void writeTriangularRing(
        std::ostream& os, const SpatialPoint3D& p1, const SpatialPoint3D& p2, const SpatialPoint3D& p3)
    {
        os << "        <Polygon>\n";
        os << "          <altitudeMode>absolute</altitudeMode>\n";
        os << "          <outerBoundaryIs>\n";
        os << "            <LinearRing>\n";
        os << "              <coordinates>\n";
        os << "                ";
        writeKmlCoord(os, p1);
        os << " ";
        writeKmlCoord(os, p2);
        os << " ";
        writeKmlCoord(os, p3);
        os << " ";
        writeKmlCoord(os, p1);
        os << "\n";
        os << "              </coordinates>\n";
        os << "            </LinearRing>\n";
        os << "          </outerBoundaryIs>\n";
        os << "        </Polygon>\n";
    }

} // namespace

std::string KmlExporter::exportToString(const SpatialDataRecorder& recorder, const KmlConfig& config)
{
    std::ostringstream oss;
    const bool success = exportToStream(recorder, oss, config);
    if (!success) {
        return "";
    }
    return oss.str();
}

bool KmlExporter::exportToStream(const SpatialDataRecorder& recorder, std::ostream& os, const KmlConfig& config)
{
    const auto& tracks = recorder.trackPoints();
    const auto& frustums = recorder.frustums();

    os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    os << "<kml xmlns=\"http://www.opengis.net/kml/2.2\" "
       << "xmlns:gx=\"http://www.google.com/kml/ext/2.2\">\n";
    os << "<Document>\n";
    os << "  <name>" << escapeXml(config.documentName) << "</name>\n";
    os << "  <description>" << escapeXml(config.documentDescription) << "</description>\n";

    // 1. Style definitions
    os << "  <Style id=\"trackStyle\">\n";
    os << "    <LineStyle>\n";
    os << "      <color>" << config.trackColor.toKmlColor() << "</color>\n";
    os << "      <width>3</width>\n";
    os << "    </LineStyle>\n";
    os << "  </Style>\n";

    os << "  <Style id=\"frustumStyle\">\n";
    os << "    <LineStyle>\n";
    os << "      <color>" << config.frustumColor.toKmlColor() << "</color>\n";
    os << "      <width>1</width>\n";
    os << "    </LineStyle>\n";
    os << "    <PolyStyle>\n";
    os << "      <color>" << config.frustumColor.toKmlColor() << "</color>\n";
    os << "      <fill>1</fill>\n";
    os << "      <outline>1</outline>\n";
    os << "    </PolyStyle>\n";
    os << "  </Style>\n";

    os << "  <Style id=\"footprintStyle\">\n";
    os << "    <LineStyle>\n";
    os << "      <color>" << config.footprintColor.toKmlColor() << "</color>\n";
    os << "      <width>2</width>\n";
    os << "    </LineStyle>\n";
    os << "    <PolyStyle>\n";
    os << "      <color>" << config.footprintColor.toKmlColor() << "</color>\n";
    os << "      <fill>1</fill>\n";
    os << "      <outline>1</outline>\n";
    os << "    </PolyStyle>\n";
    os << "  </Style>\n";

    os << "  <Style id=\"boresightStyle\">\n";
    os << "    <LineStyle>\n";
    os << "      <color>" << config.boresightColor.toKmlColor() << "</color>\n";
    os << "      <width>2</width>\n";
    os << "    </LineStyle>\n";
    os << "  </Style>\n";

    // 2. Flight Track Folder
    if (!tracks.empty()) {
        os << "  <Folder>\n";
        os << "    <name>Flight Track</name>\n";

        // 2A. Google Earth gx:Track
        if (config.enableGxTrack) {
            os << "    <Placemark>\n";
            os << "      <name>Trajectory (gx:Track)</name>\n";
            os << "      <styleUrl>#trackStyle</styleUrl>\n";
            os << "      <gx:Track>\n";
            os << "        <altitudeMode>absolute</altitudeMode>\n";

            for (const auto& tp : tracks) {
                os << "        <when>" << formatIsoTime(tp.timestampUs) << "</when>\n";
            }
            for (const auto& tp : tracks) {
                os << "        <gx:coord>" << std::fixed << std::setprecision(7) << tp.position.longitudeDeg << " "
                   << std::fixed << std::setprecision(7) << tp.position.latitudeDeg << " " << std::fixed
                   << std::setprecision(2) << tp.position.altitudeM << "</gx:coord>\n";
            }
            for (const auto& tp : tracks) {
                os << "        <gx:angles>" << std::fixed << std::setprecision(1) << tp.headingDeg << " " << std::fixed
                   << std::setprecision(1) << tp.pitchDeg << " " << std::fixed << std::setprecision(1) << tp.rollDeg
                   << "</gx:angles>\n";
            }

            os << "      </gx:Track>\n";
            os << "    </Placemark>\n";
        }

        // 2B. Static LineString
        os << "    <Placemark>\n";
        os << "      <name>Flight Path (LineString)</name>\n";
        os << "      <styleUrl>#trackStyle</styleUrl>\n";
        os << "      <LineString>\n";
        os << "        <altitudeMode>absolute</altitudeMode>\n";
        os << "        <coordinates>\n";
        for (const auto& tp : tracks) {
            os << "          ";
            writeKmlCoord(os, tp.position);
            os << "\n";
        }
        os << "        </coordinates>\n";
        os << "      </LineString>\n";
        os << "    </Placemark>\n";

        os << "  </Folder>\n";
    }

    // 3. Sensor Frustums Folder
    if (!frustums.empty() && (config.enableVolumetricPyramid || config.enableGroundFootprint)) {
        os << "  <Folder>\n";
        os << "    <name>Sensor Frustums</name>\n";

        for (std::size_t i = 0U; i < frustums.size(); ++i) {
            const auto& fr = frustums[i];
            if (!fr.valid) {
                continue;
            }

            os << "    <Placemark>\n";
            os << "      <name>Frustum #" << i << "</name>\n";
            os << "      <styleUrl>#frustumStyle</styleUrl>\n";

            if (config.enableTimeSpan && fr.timestampUs > 0U) {
                const std::string beginIso = formatIsoTime(fr.timestampUs);
                const std::uint64_t endUs = (i + 1U < frustums.size() && frustums[i + 1U].timestampUs > fr.timestampUs)
                    ? frustums[i + 1U].timestampUs
                    : fr.timestampUs + 200000ULL; // 200ms default window
                const std::string endIso = formatIsoTime(endUs);

                os << "      <TimeSpan>\n";
                os << "        <begin>" << beginIso << "</begin>\n";
                os << "        <end>" << endIso << "</end>\n";
                os << "      </TimeSpan>\n";
            }

            os << "      <MultiGeometry>\n";

            // Ground base footprint
            if (config.enableGroundFootprint) {
                writeLinearRing(os, fr.base[0], fr.base[1], fr.base[2], fr.base[3]);
            }

            // Volumetric pyramid side walls
            if (config.enableVolumetricPyramid) {
                // Wall 1: Apex -> C1 -> C2 -> Apex
                writeTriangularRing(os, fr.apex, fr.base[0], fr.base[1]);
                // Wall 2: Apex -> C2 -> C3 -> Apex
                writeTriangularRing(os, fr.apex, fr.base[1], fr.base[2]);
                // Wall 3: Apex -> C3 -> C4 -> Apex
                writeTriangularRing(os, fr.apex, fr.base[2], fr.base[3]);
                // Wall 4: Apex -> C4 -> C1 -> Apex
                writeTriangularRing(os, fr.apex, fr.base[3], fr.base[0]);
            }

            // Boresight Line of Sight Ray
            if (config.enableBoresightRay && fr.hasTargetCenter) {
                os << "        <LineString>\n";
                os << "          <altitudeMode>absolute</altitudeMode>\n";
                os << "          <coordinates>\n";
                os << "            ";
                writeKmlCoord(os, fr.apex);
                os << " ";
                writeKmlCoord(os, fr.targetCenter);
                os << "\n";
                os << "          </coordinates>\n";
                os << "        </LineString>\n";
            }

            os << "      </MultiGeometry>\n";
            os << "    </Placemark>\n";
        }

        os << "  </Folder>\n";
    }

    os << "</Document>\n";
    os << "</kml>\n";

    return os.good();
}

bool KmlExporter::exportToFile(
    const std::string& filePath, const SpatialDataRecorder& recorder, const KmlConfig& config)
{
    std::ofstream ofs(filePath, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }
    return exportToStream(recorder, ofs, config);
}

} // namespace Mapping
