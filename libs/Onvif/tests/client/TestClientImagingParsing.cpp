/// @file TestClientImagingParsing.cpp
/// @brief Modular unit tests for OnvifClient XML response parsers covering Imaging settings, focus, and presets.

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <Onvif/OnvifClient.h>
#include <Onvif/OnvifTypes.h>

using namespace Onvif;

TEST(ClientImagingParsingTest, ImagingSettingsParsing)
{
    const std::string xml = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" "
                            "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\n"
                            "  <SOAP-ENV:Body>\n"
                            "    <timg:GetImagingSettingsResponse>\n"
                            "      <timg:ImagingSettings>\n"
                            "        <tt:Brightness>65.0</tt:Brightness>\n"
                            "        <tt:ColorSaturation>75.0</tt:ColorSaturation>\n"
                            "        <tt:Contrast>80.0</tt:Contrast>\n"
                            "        <tt:Sharpness>45.0</tt:Sharpness>\n"
                            "        <tt:IrCutFilter>AUTO</tt:IrCutFilter>\n"
                            "        <tt:BacklightCompensation>\n"
                            "          <tt:Mode>ON</tt:Mode>\n"
                            "          <tt:Level>50.0</tt:Level>\n"
                            "        </tt:BacklightCompensation>\n"
                            "        <tt:WideDynamicRange>\n"
                            "          <tt:Mode>OFF</tt:Mode>\n"
                            "          <tt:Level>0.0</tt:Level>\n"
                            "        </tt:WideDynamicRange>\n"
                            "        <tt:Focus>\n"
                            "          <tt:AutoFocusMode>MANUAL</tt:AutoFocusMode>\n"
                            "        </tt:Focus>\n"
                            "      </timg:ImagingSettings>\n"
                            "    </timg:GetImagingSettingsResponse>\n"
                            "  </SOAP-ENV:Body>\n"
                            "</SOAP-ENV:Envelope>";

    const auto settings = Onvif::OnvifClient::parseImagingSettingsResponse(xml);
    ASSERT_TRUE(settings.has_value());
    EXPECT_FLOAT_EQ(settings->brightness, 65.0f);
    EXPECT_FLOAT_EQ(settings->colorSaturation, 75.0f);
    EXPECT_FLOAT_EQ(settings->contrast, 80.0f);
    EXPECT_FLOAT_EQ(settings->sharpness, 45.0f);
    EXPECT_EQ(settings->irCutFilter, "AUTO");
    EXPECT_TRUE(settings->backlightCompensation);
    EXPECT_FLOAT_EQ(settings->backlightLevel, 50.0f);
    EXPECT_FALSE(settings->wideDynamicRange);
    EXPECT_EQ(settings->autoFocusMode, "MANUAL");
}

TEST(ClientImagingParsingTest, FocusStatusParsing)
{
    const std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                            "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" "
                            "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                            "  <SOAP-ENV:Body>\n"
                            "    <timg:GetStatusResponse>\r\n"
                            "      <timg:Status>\r\n"
                            "        <tt:FocusStatus20>\r\n"
                            "          <tt:Position>0.75</tt:Position>\r\n"
                            "          <tt:MoveStatus>IDLE</tt:MoveStatus>\r\n"
                            "        </tt:FocusStatus20>\r\n"
                            "      </timg:Status>\r\n"
                            "    </timg:GetStatusResponse>\r\n"
                            "  </SOAP-ENV:Body>\r\n"
                            "</SOAP-ENV:Envelope>";

    const auto status = Onvif::OnvifClient::parseFocusStatusResponse(xml);
    ASSERT_TRUE(status.has_value());
    EXPECT_LT(std::abs(status->position - 0.75f), 0.01f);
    EXPECT_EQ(status->moveStatus, "IDLE");
}

TEST(ClientImagingParsingTest, ImagingPresetsParsing)
{
    const std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                            "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" "
                            "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                            "  <SOAP-ENV:Body>\n"
                            "    <timg:GetPresetsResponse>\r\n"
                            "      <timg:Preset token=\"Preset_Day\" type=\"Custom\">\r\n"
                            "        <tt:Name>Day Outdoor</tt:Name>\r\n"
                            "      </timg:Preset>\r\n"
                            "      <timg:Preset token=\"Preset_Night\" type=\"Custom\">\r\n"
                            "        <tt:Name>Night IR</tt:Name>\r\n"
                            "      </timg:Preset>\r\n"
                            "    </timg:GetPresetsResponse>\r\n"
                            "  </SOAP-ENV:Body>\r\n"
                            "</SOAP-ENV:Envelope>";

    const auto presets = Onvif::OnvifClient::parseImagingPresetsResponse(xml);
    ASSERT_EQ(presets.size(), 2U);
    EXPECT_EQ(presets[0].token, "Preset_Day");
    EXPECT_EQ(presets[0].name, "Day Outdoor");
    EXPECT_EQ(presets[0].type, "Custom");
    EXPECT_EQ(presets[1].token, "Preset_Night");
    EXPECT_EQ(presets[1].name, "Night IR");
}
