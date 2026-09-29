/// @file TestNmeaJsonSerializer.cpp
/// @brief Unit tests for JSON serialization and command parsing.

#include "network/NmeaJsonSerializer.h"

#include <gtest/gtest.h>

using namespace Nmea::Network;

TEST(TestNmeaJsonSerializer, SerializeVesselTelemetry)
{
    const std::string json = NmeaJsonSerializer::serializeVessel(37.7749, -122.4194, 14.5, 210.2, 209.8, 1.5, -0.7, 45.2);
    EXPECT_NE(json.find("\"type\":\"vessel\""), std::string::npos);
    EXPECT_NE(json.find("\"lat\":37.774900"), std::string::npos);
    EXPECT_NE(json.find("\"lon\":-122.419400"), std::string::npos);
    EXPECT_NE(json.find("\"sog\":14.50"), std::string::npos);
    EXPECT_NE(json.find("\"hdg\":209.80"), std::string::npos);
}

TEST(TestNmeaJsonSerializer, SerializeGimbalTelemetry)
{
    const std::string json = NmeaJsonSerializer::serializeGimbal(182.4, -5.2, 4.0, 12.5, true, 42);
    EXPECT_NE(json.find("\"type\":\"gimbal\""), std::string::npos);
    EXPECT_NE(json.find("\"pan\":182.40"), std::string::npos);
    EXPECT_NE(json.find("\"tilt\":-5.20"), std::string::npos);
    EXPECT_NE(json.find("\"tracking\":true"), std::string::npos);
    EXPECT_NE(json.find("\"targetId\":42"), std::string::npos);
}

TEST(TestNmeaJsonSerializer, SerializeTargetTelemetry)
{
    const std::string json = NmeaJsonSerializer::serializeTarget(101, 45.6, 1250.0, 320.5, 140.0, "Warning");
    EXPECT_NE(json.find("\"type\":\"target\""), std::string::npos);
    EXPECT_NE(json.find("\"id\":101"), std::string::npos);
    EXPECT_NE(json.find("\"bearing\":45.60"), std::string::npos);
    EXPECT_NE(json.find("\"range\":1250.00"), std::string::npos);
    EXPECT_NE(json.find("\"threat\":\"Warning\""), std::string::npos);
}

TEST(TestNmeaJsonSerializer, SerializeAlertTelemetry)
{
    const std::string json = NmeaJsonSerializer::serializeAlert(46001U, 1U, "Warning", "B", "ActiveUnack", "Unauthorized IP access");
    EXPECT_NE(json.find("\"type\":\"alert\""), std::string::npos);
    EXPECT_NE(json.find("\"alertId\":46001"), std::string::npos);
    EXPECT_NE(json.find("\"desc\":\"Unauthorized IP access\""), std::string::npos);
}

TEST(TestNmeaJsonSerializer, ParseSlewCommand)
{
    const std::string cmdJson = "{\"cmd\":\"slewToCue\",\"lat\":37.7749,\"lon\":-122.4194,\"zoom\":8.0}";
    const auto cmdOpt = NmeaJsonSerializer::parseCommand(cmdJson);
    ASSERT_TRUE(cmdOpt.has_value());
    EXPECT_EQ(cmdOpt->commandType, "slewToCue");
    EXPECT_NEAR(cmdOpt->targetLat, 37.7749, 1e-4);
    EXPECT_NEAR(cmdOpt->targetLon, -122.4194, 1e-4);
    EXPECT_DOUBLE_EQ(cmdOpt->zoomLevel, 8.0);
}
