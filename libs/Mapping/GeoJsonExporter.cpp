/// @file GeoJsonExporter.cpp
/// @brief Implementation of RFC 7946 GeoJSON spatial telemetry exporter.

#include "GeoJsonExporter.h"

#include <cstddef>
#include <cstdint>

#include <array>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "SpatialDataRecorder.h"
#include "SpatialExportTypes.h"

namespace Mapping {

namespace {

    [[nodiscard]] std::string escapeJson(const std::string& input)
    {
        static constexpr std::array<char, 16> kHex { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c',
            'd', 'e', 'f' };
        std::string out { };
        out.reserve(input.size() + 8U);
        for (const char c : input) {
            const auto uc { static_cast<unsigned char>(c) };
            switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (uc < 0x20U) {
                    out += "\\u00";
                    out += kHex[static_cast<std::size_t>((uc >> 4U) & 0x0FU)];
                    out += kHex[static_cast<std::size_t>(uc & 0x0FU)];
                } else {
                    out += c;
                }
                break;
            }
        }
        return out;
    }

    void writeCoord(std::ostream& os, const SpatialPoint3D& pt, int precision)
    {
        os << '[' << std::fixed << std::setprecision(precision) << pt.longitudeDeg << ", " << std::fixed
           << std::setprecision(precision) << pt.latitudeDeg << ", " << std::fixed << std::setprecision(2)
           << pt.altitudeM << ']';
    }

    [[nodiscard]] std::string findTailNumber(const std::vector<SpatialTrackPoint>& tracks)
    {
        for (const auto& tp : tracks) {
            if (!tp.tailNumber.empty()) {
                return tp.tailNumber;
            }
        }
        return "";
    }

    [[nodiscard]] std::string findMissionId(const std::vector<SpatialTrackPoint>& tracks)
    {
        for (const auto& tp : tracks) {
            if (!tp.missionId.empty()) {
                return tp.missionId;
            }
        }
        return "";
    }

} // namespace

std::string GeoJsonExporter::exportToString(const SpatialDataRecorder& recorder, const GeoJsonConfig& config)
{
    std::ostringstream oss { };
    const bool success { exportToStream(recorder, oss, config) };
    if (!success) {
        return "";
    }
    return oss.str();
}

bool GeoJsonExporter::exportToStream(const SpatialDataRecorder& recorder, std::ostream& os, const GeoJsonConfig& config)
{
    const auto& tracks { recorder.trackPoints() };
    const auto& frustums { recorder.frustums() };
    const auto& vmtiTargets { recorder.vmtiTargets() };

    os << "{\n";
    os << "  \"type\": \"FeatureCollection\",\n";
    os << "  \"name\": \"STANAG_4609_Mission_Export\",\n";
    os << "  \"features\": [\n";

    bool firstFeature { true };

    // 1. Layer: Flight Track (LineString)
    if (config.includeFlightTrack && !tracks.empty()) {
        firstFeature = false;
        const double durationSec { (tracks.size() > 1U && tracks.back().timestampUs > tracks.front().timestampUs)
                ? static_cast<double>(tracks.back().timestampUs - tracks.front().timestampUs) / 1000000.0
                : 0.0 };

        const std::string tailNumber { findTailNumber(tracks) };
        const std::string missionId { findMissionId(tracks) };

        os << "    {\n";
        os << "      \"type\": \"Feature\",\n";
        os << "      \"id\": \"flight_track\",\n";
        os << "      \"properties\": {\n";
        os << "        \"layer\": \"FlightTrack\",\n";
        os << "        \"tailNumber\": \"" << escapeJson(tailNumber) << "\",\n";
        os << "        \"missionId\": \"" << escapeJson(missionId) << "\",\n";
        os << "        \"startTimeIso\": \"" << formatIsoTime(tracks.front().timestampUs) << "\",\n";
        os << "        \"endTimeIso\": \"" << formatIsoTime(tracks.back().timestampUs) << "\",\n";
        os << "        \"pointCount\": " << tracks.size() << ",\n";
        os << "        \"durationSec\": " << std::fixed << std::setprecision(2) << durationSec << "\n";
        os << "      },\n";
        os << "      \"geometry\": {\n";
        os << "        \"type\": \"LineString\",\n";
        os << "        \"coordinates\": [\n";

        for (std::size_t i { 0U }; i < tracks.size(); ++i) {
            os << "          ";
            writeCoord(os, tracks[i].position, config.coordinatePrecision);
            if (i + 1U < tracks.size()) {
                os << ',';
            }
            os << '\n';
        }

        os << "        ]\n";
        os << "      }\n";
        os << "    }";
    }

    // 2. Layer: Sensor Footprints (2D Ground Polygons)
    if (config.includeFootprints) {
        for (std::size_t i { 0U }; i < frustums.size(); ++i) {
            const auto& fr { frustums[i] };
            if (!fr.valid) {
                continue;
            }

            if (!firstFeature) {
                os << ",\n";
            }
            firstFeature = false;

            os << "    {\n";
            os << "      \"type\": \"Feature\",\n";
            os << "      \"id\": \"footprint_" << i << "\",\n";
            os << "      \"properties\": {\n";
            os << "        \"layer\": \"SensorFootprint\",\n";
            os << "        \"frameIndex\": " << i << ",\n";
            os << "        \"timestampUs\": " << fr.timestampUs << ",\n";
            os << "        \"timestampIso\": \"" << formatIsoTime(fr.timestampUs) << "\",\n";
            os << "        \"slantRangeM\": " << std::fixed << std::setprecision(1) << fr.slantRangeM << ",\n";
            os << "        \"hfovDeg\": " << std::fixed << std::setprecision(2) << fr.hfovDeg << ",\n";
            os << "        \"vfovDeg\": " << std::fixed << std::setprecision(2) << fr.vfovDeg << "\n";
            os << "      },\n";
            os << "      \"geometry\": {\n";
            os << "        \"type\": \"Polygon\",\n";
            os << "        \"coordinates\": [[\n";

            // CCW order: C1 (TL) -> C4 (BL) -> C3 (BR) -> C2 (TR) -> C1 (TL)
            const std::array<std::size_t, 5> ccwIdx { 0U, 3U, 2U, 1U, 0U };
            for (std::size_t k { 0U }; k < ccwIdx.size(); ++k) {
                os << "          ";
                writeCoord(os, fr.base[ccwIdx[k]], config.coordinatePrecision);
                if (k + 1U < ccwIdx.size()) {
                    os << ',';
                }
                os << '\n';
            }

            os << "        ]]\n";
            os << "      }\n";
            os << "    }";
        }
    }

    // 3. Layer: Volumetric 3D Frustums (MultiPolygon)
    if (config.includeVolumetricFrustums) {
        for (std::size_t i { 0U }; i < frustums.size(); ++i) {
            const auto& fr { frustums[i] };
            if (!fr.valid) {
                continue;
            }

            if (!firstFeature) {
                os << ",\n";
            }
            firstFeature = false;

            os << "    {\n";
            os << "      \"type\": \"Feature\",\n";
            os << "      \"id\": \"frustum3d_" << i << "\",\n";
            os << "      \"properties\": {\n";
            os << "        \"layer\": \"SensorFrustum3D\",\n";
            os << "        \"frameIndex\": " << i << ",\n";
            os << "        \"timestampUs\": " << fr.timestampUs << "\n";
            os << "      },\n";
            os << "      \"geometry\": {\n";
            os << "        \"type\": \"MultiPolygon\",\n";
            os << "        \"coordinates\": [\n";

            // 5 Polygons: 1 ground base + 4 side walls
            // Face 0: Base [C1, C4, C3, C2, C1]
            os << "          [[";
            writeCoord(os, fr.base[0], config.coordinatePrecision);
            os << ", ";
            writeCoord(os, fr.base[3], config.coordinatePrecision);
            os << ", ";
            writeCoord(os, fr.base[2], config.coordinatePrecision);
            os << ", ";
            writeCoord(os, fr.base[1], config.coordinatePrecision);
            os << ", ";
            writeCoord(os, fr.base[0], config.coordinatePrecision);
            os << "]],\n";

            // Face 1: Left wall [Apex, C1, C4, Apex]
            os << "          [[";
            writeCoord(os, fr.apex, config.coordinatePrecision);
            os << ", ";
            writeCoord(os, fr.base[0], config.coordinatePrecision);
            os << ", ";
            writeCoord(os, fr.base[3], config.coordinatePrecision);
            os << ", ";
            writeCoord(os, fr.apex, config.coordinatePrecision);
            os << "]],\n";

            // Face 2: Bottom wall [Apex, C4, C3, Apex]
            os << "          [[";
            writeCoord(os, fr.apex, config.coordinatePrecision);
            os << ", ";
            writeCoord(os, fr.base[3], config.coordinatePrecision);
            os << ", ";
            writeCoord(os, fr.base[2], config.coordinatePrecision);
            os << ", ";
            writeCoord(os, fr.apex, config.coordinatePrecision);
            os << "]],\n";

            // Face 3: Right wall [Apex, C3, C2, Apex]
            os << "          [[";
            writeCoord(os, fr.apex, config.coordinatePrecision);
            os << ", ";
            writeCoord(os, fr.base[2], config.coordinatePrecision);
            os << ", ";
            writeCoord(os, fr.base[1], config.coordinatePrecision);
            os << ", ";
            writeCoord(os, fr.apex, config.coordinatePrecision);
            os << "]],\n";

            // Face 4: Top wall [Apex, C2, C1, Apex]
            os << "          [[";
            writeCoord(os, fr.apex, config.coordinatePrecision);
            os << ", ";
            writeCoord(os, fr.base[1], config.coordinatePrecision);
            os << ", ";
            writeCoord(os, fr.base[0], config.coordinatePrecision);
            os << ", ";
            writeCoord(os, fr.apex, config.coordinatePrecision);
            os << "]]\n";

            os << "        ]\n";
            os << "      }\n";
            os << "    }";
        }
    }

    // 4. Layer: Target Points (Frame Centers)
    if (config.includeTargetPoints) {
        for (std::size_t i { 0U }; i < frustums.size(); ++i) {
            const auto& fr { frustums[i] };
            if (!fr.hasTargetCenter) {
                continue;
            }

            if (!firstFeature) {
                os << ",\n";
            }
            firstFeature = false;

            os << "    {\n";
            os << "      \"type\": \"Feature\",\n";
            os << "      \"id\": \"target_" << i << "\",\n";
            os << "      \"properties\": {\n";
            os << "        \"layer\": \"TargetCenter\",\n";
            os << "        \"frameIndex\": " << i << ",\n";
            os << "        \"timestampUs\": " << fr.timestampUs << ",\n";
            os << "        \"timestampIso\": \"" << formatIsoTime(fr.timestampUs) << "\"\n";
            os << "      },\n";
            os << "      \"geometry\": {\n";
            os << "        \"type\": \"Point\",\n";
            os << "        \"coordinates\": ";
            writeCoord(os, fr.targetCenter, config.coordinatePrecision);
            os << "\n";
            os << "      }\n";
            os << "    }";
        }
    }

    // 5. Layer: VMTI Moving Targets (MISB ST 0903)
    if (config.includeVmtiTargets) {
        for (std::size_t i { 0U }; i < vmtiTargets.size(); ++i) {
            const auto& vt { vmtiTargets[i] };

            if (!firstFeature) {
                os << ",\n";
            }
            firstFeature = false;

            os << "    {\n";
            os << "      \"type\": \"Feature\",\n";
            os << "      \"id\": \"vmti_target_" << i << "\",\n";
            os << "      \"properties\": {\n";
            os << "        \"layer\": \"VmtiTarget\",\n";
            os << "        \"targetId\": " << vt.targetId << ",\n";
            os << "        \"timestampUs\": " << vt.timestampUs << ",\n";
            os << "        \"timestampIso\": \"" << formatIsoTime(vt.timestampUs) << "\",\n";
            os << "        \"confidence\": " << static_cast<unsigned int>(vt.confidence) << ",\n";
            os << "        \"status\": " << static_cast<unsigned int>(vt.status) << ",\n";
            os << "        \"ce90M\": " << std::fixed << std::setprecision(2) << vt.ce90M << "\n";
            os << "      },\n";
            os << "      \"geometry\": {\n";
            os << "        \"type\": \"Point\",\n";
            os << "        \"coordinates\": ";
            writeCoord(os, vt.position, config.coordinatePrecision);
            os << "\n";
            os << "      }\n";
            os << "    }";
        }
    }

    os << "\n  ]\n";
    os << "}\n";

    return os.good();
}

bool GeoJsonExporter::exportToFile(
    const std::string& filePath, const SpatialDataRecorder& recorder, const GeoJsonConfig& config)
{
    std::ofstream ofs(filePath, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }
    return exportToStream(recorder, ofs, config);
}

} // namespace Mapping
