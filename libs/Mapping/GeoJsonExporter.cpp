/// @file GeoJsonExporter.cpp
/// @brief Implementation of RFC 7946 GeoJSON spatial telemetry exporter.

#include "GeoJsonExporter.h"

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

    [[nodiscard]] std::string escapeJson(const std::string& input)
    {
        std::string out;
        out.reserve(input.size() + 8U);
        for (const char c : input) {
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
                out += c;
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

} // namespace

std::string GeoJsonExporter::exportToString(const SpatialDataRecorder& recorder, const GeoJsonConfig& config)
{
    std::ostringstream oss;
    const bool success = exportToStream(recorder, oss, config);
    if (!success) {
        return "";
    }
    return oss.str();
}

bool GeoJsonExporter::exportToStream(const SpatialDataRecorder& recorder, std::ostream& os, const GeoJsonConfig& config)
{
    const auto& tracks = recorder.trackPoints();
    const auto& frustums = recorder.frustums();

    os << "{\n";
    os << "  \"type\": \"FeatureCollection\",\n";
    os << "  \"name\": \"STANAG_4609_Mission_Export\",\n";
    os << "  \"crs\": {\n";
    os << "    \"type\": \"name\",\n";
    os << "    \"properties\": { \"name\": \"urn:ogc:def:crs:OGC:1.3:CRS84\" }\n";
    os << "  },\n";
    os << "  \"features\": [\n";

    bool firstFeature = true;

    // 1. Layer: Flight Track (LineString)
    if (config.includeFlightTrack && !tracks.empty()) {
        firstFeature = false;
        const double durationSec = (tracks.size() > 1U)
            ? static_cast<double>(tracks.back().timestampUs - tracks.front().timestampUs) / 1000000.0
            : 0.0;

        os << "    {\n";
        os << "      \"type\": \"Feature\",\n";
        os << "      \"id\": \"flight_track\",\n";
        os << "      \"properties\": {\n";
        os << "        \"layer\": \"FlightTrack\",\n";
        os << "        \"tailNumber\": \"" << escapeJson(tracks.front().tailNumber) << "\",\n";
        os << "        \"missionId\": \"" << escapeJson(tracks.front().missionId) << "\",\n";
        os << "        \"startTimeIso\": \"" << formatIsoTime(tracks.front().timestampUs) << "\",\n";
        os << "        \"endTimeIso\": \"" << formatIsoTime(tracks.back().timestampUs) << "\",\n";
        os << "        \"pointCount\": " << tracks.size() << ",\n";
        os << "        \"durationSec\": " << std::fixed << std::setprecision(2) << durationSec << "\n";
        os << "      },\n";
        os << "      \"geometry\": {\n";
        os << "        \"type\": \"LineString\",\n";
        os << "        \"coordinates\": [\n";

        for (std::size_t i = 0U; i < tracks.size(); ++i) {
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
        for (std::size_t i = 0U; i < frustums.size(); ++i) {
            const auto& fr = frustums[i];
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
            const std::array<std::size_t, 5> ccwIdx = { 0U, 3U, 2U, 1U, 0U };
            for (std::size_t k = 0U; k < ccwIdx.size(); ++k) {
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
        for (std::size_t i = 0U; i < frustums.size(); ++i) {
            const auto& fr = frustums[i];
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
        for (std::size_t i = 0U; i < frustums.size(); ++i) {
            const auto& fr = frustums[i];
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
