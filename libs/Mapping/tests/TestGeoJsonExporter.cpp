/// @file TestGeoJsonExporter.cpp
/// @brief Unit tests for GeoJsonExporter RFC 7946 serialization and formatting.

#include "GeoJsonExporter.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "KlvTypes.h"
#include "SpatialDataRecorder.h"
#include "SpatialExportTypes.h"
#include "VmtiTypes.h"

using namespace Mapping;

TEST(TestGeoJsonExporter, ExportFeatureCollectionStructure)
{
    SpatialDataRecorder recorder {};

    Klv::UasDatalinkMessage msg1 {};
    msg1.precisionTimeStampUs = 1728043200000000ULL;
    msg1.sensorLatitudeDeg = 32.7157;
    msg1.sensorLongitudeDeg = -117.1611;
    msg1.sensorTrueAltitudeM = 1500.0;
    msg1.platformTailNumber = "TEST-UAV";
    msg1.missionId = "ALPHA";

    Klv::FrustumCorners corners {};
    corners.topLeft = { 32.720, -117.150 };
    corners.topRight = { 32.720, -117.140 };
    corners.bottomRight = { 32.710, -117.140 };
    corners.bottomLeft = { 32.710, -117.150 };
    msg1.cornerCoordinates = corners;
    msg1.frameCenterLatDeg = 32.715;
    msg1.frameCenterLonDeg = -117.145;
    msg1.frameCenterElevM = 25.0;
    msg1.slantRangeM = 1200.0;
    msg1.sensorHfovDeg = 25.0;
    msg1.sensorVfovDeg = 15.0;

    recorder.addTelemetryFrame(msg1);

    Klv::UasDatalinkMessage msg2 { msg1 };
    msg2.precisionTimeStampUs = 1728043201000000ULL;
    msg2.sensorLatitudeDeg = 32.7160;
    msg2.sensorLongitudeDeg = -117.1610;
    recorder.addTelemetryFrame(msg2);

    GeoJsonConfig config {};
    config.coordinatePrecision = 6;
    const std::string json { GeoJsonExporter::exportToString(recorder, config) };

    // Verify root FeatureCollection
    EXPECT_NE(json.find("\"type\": \"FeatureCollection\""), std::string::npos);
    EXPECT_NE(json.find("\"name\": \"STANAG_4609_Mission_Export\""), std::string::npos);
    EXPECT_NE(json.find("\"features\": ["), std::string::npos);

    // Verify FlightTrack layer LineString
    EXPECT_NE(json.find("\"layer\": \"FlightTrack\""), std::string::npos);
    EXPECT_NE(json.find("\"type\": \"LineString\""), std::string::npos);
    EXPECT_NE(json.find("\"tailNumber\": \"TEST-UAV\""), std::string::npos);
    EXPECT_NE(json.find("\"pointCount\": 2"), std::string::npos);

    // Coordinates in GeoJSON: [lon, lat, alt]
    EXPECT_NE(json.find("-117.161100, 32.715700, 1500.00"), std::string::npos);

    // Verify SensorFootprint layer Polygon
    EXPECT_NE(json.find("\"layer\": \"SensorFootprint\""), std::string::npos);
    EXPECT_NE(json.find("\"type\": \"Polygon\""), std::string::npos);
    EXPECT_NE(json.find("\"slantRangeM\": 1200.0"), std::string::npos);

    // Verify SensorFrustum3D layer MultiPolygon
    EXPECT_NE(json.find("\"layer\": \"SensorFrustum3D\""), std::string::npos);
    EXPECT_NE(json.find("\"type\": \"MultiPolygon\""), std::string::npos);

    // Verify TargetCenter layer Point
    EXPECT_NE(json.find("\"layer\": \"TargetCenter\""), std::string::npos);
    EXPECT_NE(json.find("\"type\": \"Point\""), std::string::npos);
    EXPECT_NE(json.find("-117.145000, 32.715000, 25.00"), std::string::npos);
}

TEST(TestGeoJsonExporter, DisableLayers)
{
    SpatialDataRecorder recorder {};
    Klv::UasDatalinkMessage msg {};
    msg.precisionTimeStampUs = 1728043200000000ULL;
    msg.sensorLatitudeDeg = 10.0;
    msg.sensorLongitudeDeg = 20.0;
    msg.sensorTrueAltitudeM = 500.0;
    recorder.addTelemetryFrame(msg);

    GeoJsonConfig config {};
    config.includeFlightTrack = false;
    config.includeFootprints = false;
    config.includeVolumetricFrustums = false;
    config.includeTargetPoints = false;

    const std::string json { GeoJsonExporter::exportToString(recorder, config) };
    EXPECT_EQ(json.find("\"layer\": \"FlightTrack\""), std::string::npos);
    EXPECT_EQ(json.find("\"layer\": \"SensorFootprint\""), std::string::npos);
    EXPECT_EQ(json.find("\"layer\": \"SensorFrustum3D\""), std::string::npos);
    EXPECT_EQ(json.find("\"layer\": \"TargetCenter\""), std::string::npos);
}

TEST(TestGeoJsonExporter, OutOfOrderTimestampsZeroDuration)
{
    SpatialDataRecorder recorder {};

    Klv::UasDatalinkMessage msg1 {};
    msg1.precisionTimeStampUs = 5000000ULL;
    msg1.sensorLatitudeDeg = 10.0;
    msg1.sensorLongitudeDeg = 20.0;
    recorder.addTelemetryFrame(msg1);

    Klv::UasDatalinkMessage msg2 {};
    msg2.precisionTimeStampUs = 1000000ULL; // Earlier than msg1
    msg2.sensorLatitudeDeg = 10.01;
    msg2.sensorLongitudeDeg = 20.01;
    recorder.addTelemetryFrame(msg2);

    const std::string json { GeoJsonExporter::exportToString(recorder) };
    EXPECT_NE(json.find("\"durationSec\": 0.00"), std::string::npos);
}

TEST(TestGeoJsonExporter, Rfc7946NoCrsControlCharEscapingAndDelayedMetadata)
{
    SpatialDataRecorder recorder {};

    // Frame 0 omits tailNumber and missionId
    Klv::UasDatalinkMessage msg1 {};
    msg1.precisionTimeStampUs = 1728043200000000ULL;
    msg1.sensorLatitudeDeg = 37.9838;
    msg1.sensorLongitudeDeg = 23.7275;
    recorder.addTelemetryFrame(msg1);

    // Frame 1 provides tailNumber and missionId with RFC 8259 control characters (\x01, \x1f)
    Klv::UasDatalinkMessage msg2 {};
    msg2.precisionTimeStampUs = 1728043201000000ULL;
    msg2.sensorLatitudeDeg = 37.9840;
    msg2.sensorLongitudeDeg = 23.7280;
    msg2.platformTailNumber = std::string("UAV\x01-99");
    msg2.missionId = std::string("MSN\x1f-ALPHA");
    recorder.addTelemetryFrame(msg2);

    const std::string json { GeoJsonExporter::exportToString(recorder) };

    // 1. Pre-RFC 7946 "crs" member must NOT be emitted
    EXPECT_EQ(json.find("\"crs\""), std::string::npos);

    // 2. Delayed tailNumber and missionId on Frame 1 must be discovered and control-escaped
    EXPECT_NE(json.find("\"tailNumber\": \"UAV\\u0001-99\""), std::string::npos);
    EXPECT_NE(json.find("\"missionId\": \"MSN\\u001f-ALPHA\""), std::string::npos);
}

TEST(TestGeoJsonExporter, VmtiTargetLayerExport)
{
    SpatialDataRecorder recorder {};
    Klv::UasDatalinkMessage msg {};
    msg.precisionTimeStampUs = 1728043200000000ULL;
    msg.sensorLatitudeDeg = 37.9838;
    msg.sensorLongitudeDeg = 23.7275;

    Klv::VmtiLocalSet vmti {};
    Klv::VTargetPack pack {};
    pack.targetId = 42U;
    pack.targetLocation = Klv::GeoPoint3D { 37.9900, 23.7300, 15.0 };
    pack.confidence = 88U;
    pack.targetCe90M = 4.25;
    vmti.targets.push_back(pack);
    msg.vmti = vmti;

    recorder.addTelemetryFrame(msg);

    const std::string json { GeoJsonExporter::exportToString(recorder) };
    EXPECT_NE(json.find("\"layer\": \"VmtiTarget\""), std::string::npos);
    EXPECT_NE(json.find("\"targetId\": 42"), std::string::npos);
    EXPECT_NE(json.find("\"confidence\": 88"), std::string::npos);
    EXPECT_NE(json.find("\"ce90M\": 4.25"), std::string::npos);
}

TEST(TestGeoJsonExporter, ExportToFileAndSpecialCharEscaping)
{
    SpatialDataRecorder recorder {};
    Klv::UasDatalinkMessage msg {};
    msg.precisionTimeStampUs = 1728043200000000ULL;
    msg.sensorLatitudeDeg = 37.9838;
    msg.sensorLongitudeDeg = 23.7275;
    msg.platformTailNumber = "UAV-\"QUOTE\"\\SLASH";
    msg.missionId = "LINE1\nLINE2\tTAB";
    recorder.addTelemetryFrame(msg);

    const std::filesystem::path outPath { std::filesystem::temp_directory_path()
        / "pelcod_test_mission_export.geojson" };
    std::filesystem::remove(outPath);

    EXPECT_TRUE(GeoJsonExporter::exportToFile(outPath.string(), recorder));
    ASSERT_TRUE(std::filesystem::exists(outPath));

    std::string content {};
    {
        std::ifstream ifs(outPath);
        std::ostringstream buffer {};
        buffer << ifs.rdbuf();
        content = buffer.str();
    }

    EXPECT_NE(content.find("\"tailNumber\": \"UAV-\\\"QUOTE\\\"\\\\SLASH\""), std::string::npos);
    EXPECT_NE(content.find("\"missionId\": \"LINE1\\nLINE2\\tTAB\""), std::string::npos);

    std::filesystem::remove(outPath);
}
