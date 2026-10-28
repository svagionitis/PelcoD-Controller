/// @file TestKmlExporter.cpp
/// @brief Unit tests for KmlExporter OGC KML 2.2 and gx:Track serialization.

#include "KmlExporter.h"

#include <string>

#include <gtest/gtest.h>

using namespace Mapping;

TEST(TestKmlExporter, ExportKmlStructureAndGxTrack)
{
    SpatialDataRecorder recorder;

    Klv::UasDatalinkMessage msg1 { };
    msg1.precisionTimeStampUs = 1728043200000000ULL;
    msg1.sensorLatitudeDeg = 32.7157;
    msg1.sensorLongitudeDeg = -117.1611;
    msg1.sensorTrueAltitudeM = 1500.0;
    msg1.platformHeadingDeg = 45.0;
    msg1.platformPitchDeg = 3.0;
    msg1.platformRollDeg = -2.0;

    Klv::FrustumCorners corners { };
    corners.topLeft = { 32.720, -117.150 };
    corners.topRight = { 32.720, -117.140 };
    corners.bottomRight = { 32.710, -117.140 };
    corners.bottomLeft = { 32.710, -117.150 };
    msg1.cornerCoordinates = corners;
    msg1.frameCenterLatDeg = 32.715;
    msg1.frameCenterLonDeg = -117.145;
    msg1.frameCenterElevM = 10.0;

    recorder.addTelemetryFrame(msg1);

    KmlConfig config { };
    config.documentName = "Test Mission Flight";
    config.enableGxTrack = true;
    config.enableVolumetricPyramid = true;
    config.enableGroundFootprint = true;
    config.enableBoresightRay = true;
    config.enableTimeSpan = true;

    const std::string kml = KmlExporter::exportToString(recorder, config);

    // Verify XML declaration and KML namespaces
    EXPECT_NE(kml.find("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"), std::string::npos);
    EXPECT_NE(kml.find("xmlns=\"http://www.opengis.net/kml/2.2\""), std::string::npos);
    EXPECT_NE(kml.find("xmlns:gx=\"http://www.google.com/kml/ext/2.2\""), std::string::npos);
    EXPECT_NE(kml.find("<name>Test Mission Flight</name>"), std::string::npos);

    // Verify Style elements
    EXPECT_NE(kml.find("<Style id=\"trackStyle\">"), std::string::npos);
    EXPECT_NE(kml.find("<Style id=\"frustumStyle\">"), std::string::npos);
    EXPECT_NE(kml.find("<Style id=\"footprintStyle\">"), std::string::npos);
    EXPECT_NE(kml.find("<Style id=\"boresightStyle\">"), std::string::npos);

    // Verify gx:Track elements
    EXPECT_NE(kml.find("<gx:Track>"), std::string::npos);
    EXPECT_NE(kml.find("<when>"), std::string::npos);
    EXPECT_NE(kml.find("<gx:coord>-117.1611000 32.7157000 1500.00</gx:coord>"), std::string::npos);
    EXPECT_NE(kml.find("<gx:angles>45.0 3.0 -2.0</gx:angles>"), std::string::npos);

    // Verify static LineString fallback
    EXPECT_NE(kml.find("<LineString>"), std::string::npos);
    EXPECT_NE(kml.find("-117.1611000,32.7157000,1500.00"), std::string::npos);

    // Verify Sensor Frustums MultiGeometry
    EXPECT_NE(kml.find("<name>Sensor Frustums</name>"), std::string::npos);
    EXPECT_NE(kml.find("<TimeSpan>"), std::string::npos);
    EXPECT_NE(kml.find("<MultiGeometry>"), std::string::npos);
    EXPECT_NE(kml.find("<Polygon>"), std::string::npos);
    EXPECT_NE(kml.find("<LinearRing>"), std::string::npos);
}

TEST(TestKmlExporter, ColorRgbaConversion)
{
    ColorRgba red { 255U, 0U, 0U, 255U };
    // KML is aabbggrr: alpha=ff, blue=00, green=00, red=ff
    EXPECT_EQ(red.toKmlColor(), "ff0000ff");
    EXPECT_EQ(red.toHexColor(), "#ff0000");

    ColorRgba semiTransCyan { 0U, 255U, 255U, 128U };
    // alpha=80, blue=ff, green=ff, red=00
    EXPECT_EQ(semiTransCyan.toKmlColor(), "80ffff00");
}

TEST(TestKmlExporter, DistinctStyleBindingsAndBoresightOnlyExport)
{
    SpatialDataRecorder recorder;

    Klv::UasDatalinkMessage msg1 { };
    msg1.precisionTimeStampUs = 0ULL; // Zero timestamp to test <when> conformance
    msg1.sensorLatitudeDeg = 37.9838;
    msg1.sensorLongitudeDeg = 23.7275;
    msg1.sensorTrueAltitudeM = 1200.0;
    msg1.frameCenterLatDeg = 37.9900;
    msg1.frameCenterLonDeg = 23.7300;
    msg1.frameCenterElevM = 50.0;

    Klv::FrustumCorners corners { };
    corners.topLeft = { 37.995, 23.725 };
    corners.topRight = { 37.995, 23.735 };
    corners.bottomRight = { 37.985, 23.735 };
    corners.bottomLeft = { 37.985, 23.725 };
    msg1.cornerCoordinates = corners;
    recorder.addTelemetryFrame(msg1);

    // 1. All 3 frustum layers enabled -> must bind #footprintStyle, #frustumStyle, and #boresightStyle
    KmlConfig allCfg { };
    const std::string fullKml { KmlExporter::exportToString(recorder, allCfg) };
    EXPECT_NE(fullKml.find("<styleUrl>#footprintStyle</styleUrl>"), std::string::npos);
    EXPECT_NE(fullKml.find("<styleUrl>#frustumStyle</styleUrl>"), std::string::npos);
    EXPECT_NE(fullKml.find("<styleUrl>#boresightStyle</styleUrl>"), std::string::npos);
    // Zero timestamp must never emit invalid empty <when></when>
    EXPECT_EQ(fullKml.find("<when></when>"), std::string::npos);

    // 2. Boresight-only export -> must still emit Sensor Frustums folder and #boresightStyle
    KmlConfig boresightOnlyCfg { };
    boresightOnlyCfg.enableVolumetricPyramid = false;
    boresightOnlyCfg.enableGroundFootprint = false;
    boresightOnlyCfg.enableBoresightRay = true;
    const std::string boresightKml { KmlExporter::exportToString(recorder, boresightOnlyCfg) };
    EXPECT_NE(boresightKml.find("<name>Sensor Frustums</name>"), std::string::npos);
    EXPECT_NE(boresightKml.find("<styleUrl>#boresightStyle</styleUrl>"), std::string::npos);
    EXPECT_EQ(boresightKml.find("<styleUrl>#footprintStyle</styleUrl>"), std::string::npos);
    EXPECT_EQ(boresightKml.find("<styleUrl>#frustumStyle</styleUrl>"), std::string::npos);
}

TEST(TestKmlExporter, VmtiTargetFolderExport)
{
    SpatialDataRecorder recorder;
    Klv::UasDatalinkMessage msg { };
    msg.precisionTimeStampUs = 1728043200000000ULL;
    msg.sensorLatitudeDeg = 37.9838;
    msg.sensorLongitudeDeg = 23.7275;

    Klv::VmtiLocalSet vmti { };
    Klv::VTargetPack pack { };
    pack.targetId = 77U;
    pack.targetLocation = Klv::GeoPoint3D { 37.9910, 23.7310, 20.0 };
    vmti.targets.push_back(pack);
    msg.vmti = vmti;

    recorder.addTelemetryFrame(msg);

    const std::string kml { KmlExporter::exportToString(recorder) };
    EXPECT_NE(kml.find("<name>VMTI Targets</name>"), std::string::npos);
    EXPECT_NE(kml.find("<name>TRK-77</name>"), std::string::npos);
    EXPECT_NE(kml.find("<styleUrl>#vmtiStyle</styleUrl>"), std::string::npos);
}
