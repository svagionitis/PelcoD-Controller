#include <gtest/gtest.h>

#include "Misb0805.h"
#include "KlvTypes.h"
#include "RvtTypes.h"

#include <string>
#include <vector>

using namespace Klv;

TEST(TestMisb0805, Iso8601Formatting) {
    // 2023-11-14 22:13:20.123 UTC -> 1700000000123000 microseconds
    const std::uint64_t ts = 1700000000123000ULL;
    const std::string iso = Misb0805::formatIso8601(ts);

    EXPECT_EQ(iso.substr(0, 10), "2023-11-14");
    EXPECT_EQ(iso.back(), 'Z');
    EXPECT_NE(iso.find('T'), std::string::npos);
    EXPECT_NE(iso.find(".123Z"), std::string::npos);
}

TEST(TestMisb0805, ErrorEstimatesConversion) {
    // CE90 = 21.46 m -> 1-sigma CE = 10.0 m
    EXPECT_NEAR(Misb0805::ce90ToSigma1(21.46), 10.0, 1e-3);

    // LE90 = 16.45 m -> 1-sigma LE = 10.0 m
    EXPECT_NEAR(Misb0805::le90ToSigma1(16.45), 10.0, 1e-3);
}

TEST(TestMisb0805, PlatformPositionMessageConversion) {
    UasDatalinkMessage msg {};
    msg.platformDesignation = "PREDATOR";
    msg.missionId = "RECON_01";
    msg.precisionTimeStampUs = 1700000000000000ULL;
    msg.sensorLatitudeDeg = 36.1699;
    msg.sensorLongitudeDeg = -115.1398;
    msg.sensorAltitudeHaeM = 3200.0;
    msg.platformHeadingDeg = 90.0;
    msg.sensorRelAzimuthDeg = 45.0;
    msg.sensorHfovDeg = 12.5;
    msg.sensorVfovDeg = 8.0;
    msg.imageSourceSensor = "EO_NOSE_CAM";
    msg.slantRangeM = 4500.0;

    const CotEvent event = Misb0805::toPlatformPosition(msg, 15.0);

    EXPECT_EQ(event.version, "2.0");
    EXPECT_EQ(event.uid, "PREDATOR_RECON_01");
    EXPECT_EQ(event.type, "a-f-A-M-F");
    EXPECT_EQ(event.how, "m-p");
    EXPECT_NEAR(event.point.lat, 36.1699, 1e-4);
    EXPECT_NEAR(event.point.lon, -115.1398, 1e-4);
    EXPECT_NEAR(event.point.hae, 3200.0, 1e-2);
    EXPECT_EQ(event.point.ce, 9999999.0);
    EXPECT_EQ(event.point.le, 9999999.0);

    const std::string xml = event.toXml();
    EXPECT_NE(xml.find("uid=\"PREDATOR_RECON_01\""), std::string::npos);
    EXPECT_NE(xml.find("type=\"a-f-A-M-F\""), std::string::npos);
    EXPECT_NE(xml.find("azimuth=\"135.00\""), std::string::npos);
    EXPECT_NE(xml.find("model=\"EO_NOSE_CAM\""), std::string::npos);
    EXPECT_NE(xml.find("range=\"4500.00\""), std::string::npos);
}

TEST(TestMisb0805, SensorPointOfInterestConversion) {
    UasDatalinkMessage msg {};
    msg.platformDesignation = "REAPER";
    msg.missionId = "OVERWATCH_42";
    msg.imageSourceSensor = "FLIR_MTS_B";
    msg.precisionTimeStampUs = 1700000000000000ULL;
    msg.frameCenterLatDeg = 34.0500;
    msg.frameCenterLonDeg = -118.2500;
    msg.frameCenterElevHaeM = 250.0;
    msg.targetErrorCe90M = 21.46;
    msg.targetErrorLe90M = 16.45;

    const CotEvent event = Misb0805::toSensorPointOfInterest(msg, 5.0);

    EXPECT_EQ(event.version, "2.0");
    EXPECT_EQ(event.uid, "REAPER_OVERWATCH_42_FLIR_MTS_B");
    EXPECT_EQ(event.type, "b-m-p-s-p-i");
    EXPECT_NEAR(event.point.lat, 34.0500, 1e-4);
    EXPECT_NEAR(event.point.lon, -118.2500, 1e-4);
    EXPECT_NEAR(event.point.hae, 250.0, 1e-2);
    EXPECT_NEAR(event.point.ce, 10.0, 1e-2);
    EXPECT_NEAR(event.point.le, 10.0, 1e-2);

    const std::string xml = event.toXml();
    EXPECT_NE(xml.find("type=\"b-m-p-s-p-i\""), std::string::npos);
    EXPECT_NE(xml.find("relation=\"p-p\""), std::string::npos);
    EXPECT_NE(xml.find("uid=\"REAPER_OVERWATCH_42\""), std::string::npos);
}

TEST(TestMisb0805, RvtPoiToCotConversion) {
    PoiPack poi {};
    poi.poiNumber = 12U;
    poi.latitudeDeg = 35.5000;
    poi.longitudeDeg = -117.8000;
    poi.altitudeMslM = 800.0;
    poi.type = RvtTargetType::Hostile;
    poi.label = "TGT-BRAVO";
    poi.text = "Hostile SAM site";
    poi.sourceIcon = "SHG-UCI-----";

    const CotEvent event = Misb0805::toCot(poi, "REAPER_01", 1700000000000000ULL, 30.0);

    EXPECT_EQ(event.type, "a-h-G");
    EXPECT_EQ(event.uid, "REAPER_01_POI_12");
    EXPECT_NEAR(event.point.lat, 35.5000, 1e-4);
    EXPECT_NEAR(event.point.lon, -117.8000, 1e-4);
    EXPECT_NEAR(event.point.hae, 800.0, 1e-2);

    const std::string xml = event.toXml();
    EXPECT_NE(xml.find("callsign=\"TGT-BRAVO\""), std::string::npos);
    EXPECT_NE(xml.find("<remarks>Hostile SAM site</remarks>"), std::string::npos);
    EXPECT_NE(xml.find("iconsetpath=\"SHG-UCI-----\""), std::string::npos);
    EXPECT_NE(xml.find("relation=\"p-p\""), std::string::npos);
}

TEST(TestMisb0805, RvtLocalSetBatchConversion) {
    RvtLocalSet rvt {};
    rvt.precisionTimeStampUs = 1700000000000000ULL;

    PoiPack p1 {};
    p1.poiNumber = 1U;
    p1.latitudeDeg = 36.1;
    p1.longitudeDeg = -115.1;
    p1.type = RvtTargetType::Friendly;
    rvt.pois.push_back(p1);

    PoiPack p2 {};
    p2.poiNumber = 2U;
    p2.latitudeDeg = 36.2;
    p2.longitudeDeg = -115.2;
    p2.type = RvtTargetType::Hostile;
    rvt.pois.push_back(p2);

    const auto cotList = Misb0805::toCot(rvt, "PREDATOR_A");
    ASSERT_EQ(cotList.size(), 2U);
    EXPECT_EQ(cotList[0].type, "a-f-G");
    EXPECT_EQ(cotList[0].uid, "PREDATOR_A_POI_1");
    EXPECT_EQ(cotList[1].type, "a-h-G");
    EXPECT_EQ(cotList[1].uid, "PREDATOR_A_POI_2");
}
