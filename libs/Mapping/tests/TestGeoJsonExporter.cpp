/// @file TestGeoJsonExporter.cpp
/// @brief Unit tests for GeoJsonExporter RFC 7946 serialization and formatting.

#include "GeoJsonExporter.h"
#include <gtest/gtest.h>

using namespace Mapping;

TEST(TestGeoJsonExporter, ExportFeatureCollectionStructure) {
    SpatialDataRecorder recorder;

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

    Klv::UasDatalinkMessage msg2 = msg1;
    msg2.precisionTimeStampUs = 1728043201000000ULL;
    msg2.sensorLatitudeDeg = 32.7160;
    msg2.sensorLongitudeDeg = -117.1610;
    recorder.addTelemetryFrame(msg2);

    GeoJsonConfig config {};
    config.coordinatePrecision = 6;
    const std::string json = GeoJsonExporter::exportToString(recorder, config);

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

TEST(TestGeoJsonExporter, DisableLayers) {
    SpatialDataRecorder recorder;
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

    const std::string json = GeoJsonExporter::exportToString(recorder, config);
    EXPECT_EQ(json.find("\"layer\": \"FlightTrack\""), std::string::npos);
    EXPECT_EQ(json.find("\"layer\": \"SensorFootprint\""), std::string::npos);
    EXPECT_EQ(json.find("\"layer\": \"SensorFrustum3D\""), std::string::npos);
    EXPECT_EQ(json.find("\"layer\": \"TargetCenter\""), std::string::npos);
}
