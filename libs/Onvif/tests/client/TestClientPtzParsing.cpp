/// @file TestClientPtzParsing.cpp
/// @brief Modular unit tests for OnvifClient XML response parsers covering PTZ status, presets, tours, and geodesy.

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <Onvif/GeodesyUtils.h>
#include <Onvif/OnvifClient.h>
#include <Onvif/OnvifTypes.h>

using namespace Onvif;

TEST(ClientPtzParsingTest, PtzStatusParsing)
{
    const std::string xml
        = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" "
          "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\n"
          "  <SOAP-ENV:Body>\n"
          "    <tptz:GetStatusResponse>\n"
          "      <tptz:PTZStatus>\n"
          "        <tt:Position><tt:PanTilt x=\"0.3500\" y=\"-0.2000\"/><tt:Zoom x=\"0.7500\"/></tt:Position>\n"
          "        <tt:MoveStatus><tt:PanTilt>MOVING</tt:PanTilt><tt:Zoom>IDLE</tt:Zoom></tt:MoveStatus>\n"
          "        <tt:UtcTime>2026-09-16T19:00:00Z</tt:UtcTime>\n"
          "      </tptz:PTZStatus>\n"
          "    </tptz:GetStatusResponse>\n"
          "  </SOAP-ENV:Body>\n"
          "</SOAP-ENV:Envelope>";

    const auto status = Onvif::OnvifClient::parsePtzStatusResponse(xml);
    ASSERT_TRUE(status.has_value());
    EXPECT_GT(status->pan, 0.349f);
    EXPECT_LT(status->pan, 0.351f);
    EXPECT_LT(status->tilt, -0.199f);
    EXPECT_GT(status->tilt, -0.201f);
    EXPECT_GT(status->zoom, 0.749f);
    EXPECT_LT(status->zoom, 0.751f);
    EXPECT_TRUE(status->isMoving);
    EXPECT_EQ(status->utcTime, "2026-09-16T19:00:00Z");
}

TEST(ClientPtzParsingTest, PresetsParsing)
{
    const std::string getPresetsXml
        = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" "
          "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\n"
          "  <SOAP-ENV:Body>\n"
          "    <tptz:GetPresetsResponse>\n"
          "      <tptz:Preset token=\"1\">\n"
          "        <tt:Name>Gate Entrance</tt:Name>\n"
          "        <tt:PTZPosition><tt:PanTilt x=\"-0.5000\" y=\"0.1000\"/><tt:Zoom x=\"0.4000\"/></tt:PTZPosition>\n"
          "      </tptz:Preset>\n"
          "      <tptz:Preset token=\"2\">\n"
          "        <tt:Name>Parking Lot</tt:Name>\n"
          "        <tt:PTZPosition><tt:PanTilt x=\"0.8000\" y=\"-0.3000\"/><tt:Zoom x=\"1.0000\"/></tt:PTZPosition>\n"
          "      </tptz:Preset>\n"
          "    </tptz:GetPresetsResponse>\n"
          "  </SOAP-ENV:Body>\n"
          "</SOAP-ENV:Envelope>";

    const auto presets = Onvif::OnvifClient::parsePresetsResponse(getPresetsXml);
    ASSERT_EQ(presets.size(), 2U);
    EXPECT_EQ(presets[0].token, "1");
    EXPECT_EQ(presets[0].name, "Gate Entrance");
    EXPECT_EQ(presets[1].token, "2");
    EXPECT_EQ(presets[1].name, "Parking Lot");

    const std::string setPresetXml = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                     "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\n"
                                     "  <SOAP-ENV:Body>\n"
                                     "    <tptz:SetPresetResponse>\n"
                                     "      <tptz:PresetToken>preset_token_99</tptz:PresetToken>\n"
                                     "    </tptz:SetPresetResponse>\n"
                                     "  </SOAP-ENV:Body>\n"
                                     "</SOAP-ENV:Envelope>";

    const auto assignedToken = Onvif::OnvifClient::parseSetPresetResponse(setPresetXml);
    ASSERT_TRUE(assignedToken.has_value());
    EXPECT_EQ(*assignedToken, "preset_token_99");
}

TEST(ClientPtzParsingTest, PresetToursParsing)
{
    const std::string toursXml
        = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" "
          "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\n"
          "  <SOAP-ENV:Body>\n"
          "    <tptz:GetPresetToursResponse>\n"
          "      <tptz:PresetTour token=\"Tour_1\">\n"
          "        <tt:Name>Perimeter Patrol</tt:Name>\n"
          "        <tt:Status><tt:State>Touring</tt:State></tt:Status>\n"
          "        <tt:TourSpot><tt:PresetDetail><tt:PresetToken>1</tt:PresetToken></tt:PresetDetail>"
          "          <tt:Speed><tt:PanTilt x=\"0.8\" y=\"0.8\"/></tt:Speed><tt:StayTime>PT5S</tt:StayTime>"
          "        </tt:TourSpot>\n"
          "      </tptz:PresetTour>\n"
          "    </tptz:GetPresetToursResponse>\n"
          "  </SOAP-ENV:Body>\n"
          "</SOAP-ENV:Envelope>";

    const auto tours = Onvif::OnvifClient::parsePresetToursResponse(toursXml);
    ASSERT_EQ(tours.size(), 1U);
    EXPECT_EQ(tours[0].token, "Tour_1");
    EXPECT_EQ(tours[0].name, "Perimeter Patrol");
    EXPECT_EQ(tours[0].status, Onvif::PresetTourState::Touring);
    ASSERT_EQ(tours[0].spots.size(), 1U);
    EXPECT_EQ(tours[0].spots[0].presetToken, "1");
}

TEST(ClientPtzParsingTest, AuxiliaryAndGeodesy)
{
    // SendAuxiliaryCommand
    const std::string xmlWithResponse = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                        "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\n"
                                        "  <SOAP-ENV:Body>\n"
                                        "    <tptz:SendAuxiliaryCommandResponse>\n"
                                        "      <tptz:AuxiliaryResponse>tt:Wiper|On</tptz:AuxiliaryResponse>\n"
                                        "    </tptz:SendAuxiliaryCommandResponse>\n"
                                        "  </SOAP-ENV:Body>\n"
                                        "</SOAP-ENV:Envelope>";

    const auto resp1 = Onvif::OnvifClient::parseSendAuxCommandResp(xmlWithResponse);
    ASSERT_TRUE(resp1.has_value());
    EXPECT_EQ(*resp1, "tt:Wiper|On");

    // Geodesy calculations
    GeoLocation camLoc { 0.0, 0.0, 0.0 };
    GeoOrientation camOri { 0.0, 0.0, 0.0 };
    GeoLocation targetNorth { 0.01, 0.0, 0.0 };

    double pan { 0.0 };
    double tilt { 0.0 };
    double slant { 0.0 };
    const bool ok = Geodesy::computeTargetAzimuthElevation(camLoc, camOri, targetNorth, pan, tilt, slant);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(pan >= 359.9 || pan <= 0.1);
    EXPECT_LT(std::fabs(slant - 1113.0), 50.0);
}
