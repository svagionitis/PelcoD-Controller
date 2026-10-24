/// @file TestClientMediaParsing.cpp
/// @brief Modular unit tests for OnvifClient XML response parsers covering Media profiles, streams, and OSDs.

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <Onvif/OnvifClient.h>
#include <Onvif/OnvifTypes.h>

using namespace Onvif;

TEST(ClientMediaParsingTest, ProfilesParsing)
{
    const std::string xml
        = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
          "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\n"
          "  <SOAP-ENV:Body>\n"
          "    <trt:GetProfilesResponse>\n"
          "      <trt:Profiles token=\"Profile_1\">\n"
          "        <tt:Name>MainStream_1080p</tt:Name>\n"
          "        <tt:VideoEncoderConfiguration token=\"vec_1\">\n"
          "          <tt:Encoding>H264</tt:Encoding>\n"
          "          <tt:Resolution><tt:Width>1920</tt:Width><tt:Height>1080</tt:Height></tt:Resolution>\n"
          "        </tt:VideoEncoderConfiguration>\n"
          "      </trt:Profiles>\n"
          "      <trt:Profiles token=\"Profile_2\">\n"
          "        <tt:Name>SubStream_VGA</tt:Name>\n"
          "        <tt:VideoEncoderConfiguration token=\"vec_2\">\n"
          "          <tt:Encoding>H265</tt:Encoding>\n"
          "          <tt:Resolution><tt:Width>640</tt:Width><tt:Height>480</tt:Height></tt:Resolution>\n"
          "        </tt:VideoEncoderConfiguration>\n"
          "      </trt:Profiles>\n"
          "    </trt:GetProfilesResponse>\n"
          "  </SOAP-ENV:Body>\n"
          "</SOAP-ENV:Envelope>";

    const auto profiles = Onvif::OnvifClient::parseProfilesResponse(xml);
    ASSERT_EQ(profiles.size(), 2U);
    EXPECT_EQ(profiles[0].token, "Profile_1");
    EXPECT_EQ(profiles[0].name, "MainStream_1080p");
    EXPECT_EQ(profiles[0].videoWidth, 1920);
    EXPECT_EQ(profiles[0].videoHeight, 1080);
    EXPECT_EQ(profiles[0].videoEncoding, "H264");

    EXPECT_EQ(profiles[1].token, "Profile_2");
    EXPECT_EQ(profiles[1].videoWidth, 640);
    EXPECT_EQ(profiles[1].videoHeight, 480);
    EXPECT_EQ(profiles[1].videoEncoding, "H265");
}

TEST(ClientMediaParsingTest, StreamUriParsing)
{
    const std::string xml = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
                            "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\n"
                            "  <SOAP-ENV:Body>\n"
                            "    <trt:GetStreamUriResponse>\n"
                            "      <trt:MediaUri>\n"
                            "        <tt:Uri>rtsp://192.168.1.100:554/live/ch0</tt:Uri>\n"
                            "        <tt:InvalidAfterConnect>false</tt:InvalidAfterConnect>\n"
                            "        <tt:InvalidAfterReboot>true</tt:InvalidAfterReboot>\n"
                            "        <tt:Timeout>PT60S</tt:Timeout>\n"
                            "      </trt:MediaUri>\n"
                            "    </trt:GetStreamUriResponse>\n"
                            "  </SOAP-ENV:Body>\n"
                            "</SOAP-ENV:Envelope>";

    const auto uriInfo = Onvif::OnvifClient::parseStreamUriResponse(xml);
    ASSERT_TRUE(uriInfo.has_value());
    EXPECT_EQ(uriInfo->uri, "rtsp://192.168.1.100:554/live/ch0");
    EXPECT_FALSE(uriInfo->invalidAfterConnect);
    EXPECT_TRUE(uriInfo->invalidAfterReboot);
    EXPECT_EQ(uriInfo->timeout, "PT60S");
}

TEST(ClientMediaParsingTest, SnapshotUriParsing)
{
    const std::string xml = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
                            "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\n"
                            "  <SOAP-ENV:Body>\n"
                            "    <trt:GetSnapshotUriResponse>\n"
                            "      <trt:MediaUri>\n"
                            "        <tt:Uri>http://192.168.1.100/onvif/snapshot/view.jpg</tt:Uri>\n"
                            "        <tt:Timeout>PT30S</tt:Timeout>\n"
                            "      </trt:MediaUri>\n"
                            "    </trt:GetSnapshotUriResponse>\n"
                            "  </SOAP-ENV:Body>\n"
                            "</SOAP-ENV:Envelope>";

    const auto snapUri = Onvif::OnvifClient::parseSnapshotUriResponse(xml);
    ASSERT_TRUE(snapUri.has_value());
    EXPECT_EQ(*snapUri, "http://192.168.1.100/onvif/snapshot/view.jpg");
}

TEST(ClientMediaParsingTest, OsdParsing)
{
    const std::string osdListXml
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
          "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <SOAP-ENV:Body>\n"
          "    <trt:GetOSDsResponse>\r\n"
          "      <trt:OSD token=\"OSD_1\">\r\n"
          "        <tt:VideoSourceConfigurationToken>VideoSourceConfig_1</tt:VideoSourceConfigurationToken>\r\n"
          "        <tt:Type>Text</tt:Type>\r\n"
          "        <tt:Position><tt:Type>UpperLeft</tt:Type></tt:Position>\r\n"
          "        <tt:TextString><tt:Type>Plain</tt:Type><tt:PlainText>Entrance "
          "Gate</tt:PlainText><tt:FontSize>28</tt:FontSize></tt:TextString>\r\n"
          "      </trt:OSD>\r\n"
          "    </trt:GetOSDsResponse>\r\n"
          "  </SOAP-ENV:Body>\r\n"
          "</SOAP-ENV:Envelope>";

    const auto osds = Onvif::OnvifClient::parseOsdListResponse(osdListXml);
    ASSERT_EQ(osds.size(), 1U);
    EXPECT_EQ(osds[0].token, "OSD_1");
    EXPECT_EQ(osds[0].plainText, "Entrance Gate");
}

TEST(ClientMediaParsingTest, SoapEnvelopeWrapping)
{
    Onvif::SecurityCredentials creds { };
    creds.username = "operator";
    creds.password = "pass456";

    Onvif::OnvifClient client("http://192.168.1.100/onvif/device_service", creds);
    const std::string body = "<tds:GetDeviceInformation/>";
    const std::string env = client.wrapSoapEnvelope(body);

    EXPECT_NE(env.find("<s:Envelope"), std::string::npos);
    EXPECT_NE(env.find("<s:Header>"), std::string::npos);
    EXPECT_NE(env.find("<wsse:Username>operator</wsse:Username>"), std::string::npos);
    EXPECT_NE(env.find("<s:Body>"), std::string::npos);
    EXPECT_NE(env.find("<tds:GetDeviceInformation/>"), std::string::npos);
}
