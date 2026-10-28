/// @file KmlExporter.cpp
/// @brief Implementation of OGC KML 2.2 / Google Earth spatial exporter.

#include "KmlExporter.h"

#include <cstddef>
#include <cstdint>

#include <fstream>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace Mapping {

namespace {

    [[nodiscard]] std::string escapeXml(const std::string& input)
    {
        std::string out { };
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

    void writeTimeSpan(std::ostream& os, const std::vector<FrustumMesh3D>& frustums, std::size_t idx, bool enabled)
    {
        const auto& fr { frustums[idx] };
        if (!enabled || fr.timestampUs == 0U) {
            return;
        }
        const std::string beginIso { formatIsoTime(fr.timestampUs) };
        const std::uint64_t endUs {
            (idx + 1U < frustums.size() && frustums[idx + 1U].timestampUs > fr.timestampUs)
                ? frustums[idx + 1U].timestampUs
                : fr.timestampUs + 200000ULL // 200ms default window
        };
        const std::string endIso { formatIsoTime(endUs) };

        os << "      <TimeSpan>\n";
        os << "        <begin>" << beginIso << "</begin>\n";
        os << "        <end>" << endIso << "</end>\n";
        os << "      </TimeSpan>\n";
    }

    [[nodiscard]] bool hasAnyTimestamp(const std::vector<SpatialTrackPoint>& tracks) noexcept
    {
        for (const auto& tp : tracks) {
            if (tp.timestampUs > 0U) {
                return true;
            }
        }
        return false;
    }

} // namespace

std::string KmlExporter::exportToString(const SpatialDataRecorder& recorder, const KmlConfig& config)
{
    std::ostringstream oss { };
    const bool success { exportToStream(recorder, oss, config) };
    if (!success) {
        return "";
    }
    return oss.str();
}

bool KmlExporter::exportToStream(const SpatialDataRecorder& recorder, std::ostream& os, const KmlConfig& config)
{
    const auto& tracks { recorder.trackPoints() };
    const auto& frustums { recorder.frustums() };
    const auto& vmtiTargets { recorder.vmtiTargets() };

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

    os << "  <Style id=\"vmtiStyle\">\n";
    os << "    <IconStyle>\n";
    os << "      <color>" << config.vmtiColor.toKmlColor() << "</color>\n";
    os << "    </IconStyle>\n";
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

            if (hasAnyTimestamp(tracks)) {
                for (const auto& tp : tracks) {
                    const std::string iso { (tp.timestampUs > 0U) ? formatIsoTime(tp.timestampUs)
                                                                  : "1970-01-01T00:00:00.000Z" };
                    os << "        <when>" << iso << "</when>\n";
                }
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
    if (!frustums.empty()
        && (config.enableVolumetricPyramid || config.enableGroundFootprint || config.enableBoresightRay)) {
        os << "  <Folder>\n";
        os << "    <name>Sensor Frustums</name>\n";

        for (std::size_t i { 0U }; i < frustums.size(); ++i) {
            const auto& fr { frustums[i] };

            // 3A. Ground Base Footprint Placemark (#footprintStyle)
            if (config.enableGroundFootprint && fr.valid) {
                os << "    <Placemark>\n";
                os << "      <name>Footprint #" << i << "</name>\n";
                os << "      <styleUrl>#footprintStyle</styleUrl>\n";
                writeTimeSpan(os, frustums, i, config.enableTimeSpan);
                writeLinearRing(os, fr.base[0], fr.base[1], fr.base[2], fr.base[3]);
                os << "    </Placemark>\n";
            }

            // 3B. Volumetric Pyramid Side Walls Placemark (#frustumStyle)
            if (config.enableVolumetricPyramid && fr.valid) {
                os << "    <Placemark>\n";
                os << "      <name>Frustum #" << i << "</name>\n";
                os << "      <styleUrl>#frustumStyle</styleUrl>\n";
                writeTimeSpan(os, frustums, i, config.enableTimeSpan);
                os << "      <MultiGeometry>\n";
                // Wall 1: Apex -> C1 -> C2 -> Apex
                writeTriangularRing(os, fr.apex, fr.base[0], fr.base[1]);
                // Wall 2: Apex -> C2 -> C3 -> Apex
                writeTriangularRing(os, fr.apex, fr.base[1], fr.base[2]);
                // Wall 3: Apex -> C3 -> C4 -> Apex
                writeTriangularRing(os, fr.apex, fr.base[2], fr.base[3]);
                // Wall 4: Apex -> C4 -> C1 -> Apex
                writeTriangularRing(os, fr.apex, fr.base[3], fr.base[0]);
                os << "      </MultiGeometry>\n";
                os << "    </Placemark>\n";
            }

            // 3C. Boresight Line of Sight Ray Placemark (#boresightStyle)
            if (config.enableBoresightRay && fr.hasTargetCenter) {
                os << "    <Placemark>\n";
                os << "      <name>Boresight #" << i << "</name>\n";
                os << "      <styleUrl>#boresightStyle</styleUrl>\n";
                writeTimeSpan(os, frustums, i, config.enableTimeSpan);
                os << "      <LineString>\n";
                os << "        <altitudeMode>absolute</altitudeMode>\n";
                os << "        <coordinates>\n";
                os << "          ";
                writeKmlCoord(os, fr.apex);
                os << " ";
                writeKmlCoord(os, fr.targetCenter);
                os << "\n";
                os << "        </coordinates>\n";
                os << "      </LineString>\n";
                os << "    </Placemark>\n";
            }
        }

        os << "  </Folder>\n";
    }

    // 4. VMTI Moving Targets Folder (MISB ST 0903)
    if (config.enableVmtiTargets && !vmtiTargets.empty()) {
        os << "  <Folder>\n";
        os << "    <name>VMTI Targets</name>\n";

        for (const auto& vt : vmtiTargets) {
            os << "    <Placemark>\n";
            os << "      <name>TRK-" << vt.targetId << "</name>\n";
            os << "      <styleUrl>#vmtiStyle</styleUrl>\n";
            if (vt.timestampUs > 0U) {
                os << "      <TimeStamp>\n";
                os << "        <when>" << formatIsoTime(vt.timestampUs) << "</when>\n";
                os << "      </TimeStamp>\n";
            }
            os << "      <Point>\n";
            os << "        <altitudeMode>absolute</altitudeMode>\n";
            os << "        <coordinates>";
            writeKmlCoord(os, vt.position);
            os << "</coordinates>\n";
            os << "      </Point>\n";
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
