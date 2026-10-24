/// @file TestOnvifMediaService.cpp
/// @brief Modular unit tests for OnvifServer Media and Media2 service operations.

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "OnvifTestFixtures.h"

using namespace Onvif;
using namespace Onvif::Test;

TEST_F(OnvifServerFixture, Media1ProfilesAndStreams)
{
    // 1. GetProfiles
    const std::string reqProf = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><trt:GetProfiles/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
    pugi::xml_document docProf { };
    int status = sendSoapReq("/onvif/media_service", reqProf, docProf);
    EXPECT_EQ(status, 200);
    EXPECT_TRUE(docProf.select_node("//*[local-name()='GetProfilesResponse']"));
    EXPECT_TRUE(docProf.select_node("//*[local-name()='Profiles'][@token='ProfileToken_1']"));

    // 2. GetStreamUri
    const std::string reqUri = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">\r\n"
                               "  "
                               "<SOAP-ENV:Body><trt:GetStreamUri><trt:ProfileToken>ProfileToken_1</trt:ProfileToken></"
                               "trt:GetStreamUri></SOAP-ENV:Body>\r\n"
                               "</SOAP-ENV:Envelope>";
    pugi::xml_document docUri { };
    status = sendSoapReq("/onvif/media_service", reqUri, docUri);
    EXPECT_EQ(status, 200);
    const auto uriNode = docUri.select_node("//*[local-name()='Uri']").node();
    EXPECT_TRUE(uriNode && std::string(uriNode.text().as_string()) == "rtsp://127.0.0.1:8554/test_stream");

    // 3. GetSnapshotUri
    const std::string reqSnap = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">\r\n"
                                "  "
                                "<SOAP-ENV:Body><trt:GetSnapshotUri><trt:ProfileToken>ProfileToken_1</"
                                "trt:ProfileToken></trt:GetSnapshotUri></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
    pugi::xml_document docSnap { };
    status = sendSoapReq("/onvif/media_service", reqSnap, docSnap);
    EXPECT_EQ(status, 200);
    const auto snapUri = docSnap.select_node("//*[local-name()='Uri']").node();
    EXPECT_TRUE(snapUri && std::string(snapUri.text().as_string()).find("/onvif/snapshot") != std::string::npos);
}

TEST_F(OnvifServerFixture, Media2ProfilesAndConfigs)
{
    // 1. GetProfiles (Media2)
    const std::string reqProf = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tr2:GetProfiles/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
    pugi::xml_document docProf { };
    int status = sendSoapReq("/onvif/media2_service", reqProf, docProf);
    EXPECT_EQ(status, 200);
    EXPECT_TRUE(docProf.select_node("//*[local-name()='GetProfilesResponse']"));
    EXPECT_TRUE(docProf.select_node("//*[local-name()='Profiles']"));
    EXPECT_TRUE(docProf.select_node("//*[local-name()='VideoEncoder']"));

    // 2. GetStreamUri (Media2)
    const std::string reqUri = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\r\n"
                               "  "
                               "<SOAP-ENV:Body><tr2:GetStreamUri><tr2:ProfileToken>ProfileToken_1</tr2:ProfileToken></"
                               "tr2:GetStreamUri></SOAP-ENV:Body>\r\n"
                               "</SOAP-ENV:Envelope>";
    pugi::xml_document docUri { };
    status = sendSoapReq("/onvif/media2_service", reqUri, docUri);
    EXPECT_EQ(status, 200);
    EXPECT_TRUE(docUri.select_node("//*[local-name()='Uri']"));

    // 3. GetVideoEncoderConfigurations (Media2)
    const std::string reqEnc = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\r\n"
                               "  <SOAP-ENV:Body><tr2:GetVideoEncoderConfigurations/></SOAP-ENV:Body>\r\n"
                               "</SOAP-ENV:Envelope>";
    pugi::xml_document docEnc { };
    status = sendSoapReq("/onvif/media2_service", reqEnc, docEnc);
    EXPECT_EQ(status, 200);
    EXPECT_TRUE(docEnc.select_node("//*[local-name()='Encoding']"));

    // 4. GetServiceCapabilities (Media2)
    const std::string reqCap = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\r\n"
                               "  <SOAP-ENV:Body><tr2:GetServiceCapabilities/></SOAP-ENV:Body>\r\n"
                               "</SOAP-ENV:Envelope>";
    pugi::xml_document docCap { };
    status = sendSoapReq("/onvif/media2_service", reqCap, docCap);
    EXPECT_EQ(status, 200);
    EXPECT_TRUE(docCap.select_node("//*[local-name()='Capabilities']"));
}

TEST_F(OnvifServerFixture, MediaOsdManagement)
{
    // 1. GetOSDOptions
    const std::string reqOpt = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">\r\n"
                               "  <SOAP-ENV:Body><trt:GetOSDOptions/></SOAP-ENV:Body>\r\n"
                               "</SOAP-ENV:Envelope>";
    pugi::xml_document docOpt { };
    int status = sendSoapReq("/onvif/media_service", reqOpt, docOpt);
    EXPECT_EQ(status, 200);
    EXPECT_TRUE(docOpt.select_node("//*[local-name()='PositionOption']"));

    // 2. CreateOSD
    const std::string reqCreate
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
          "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <SOAP-ENV:Body>\r\n"
          "    <trt:CreateOSD>\r\n"
          "      <trt:OSD token=\"OSD_TEST\">\r\n"
          "        <tt:VideoSourceConfigurationToken>VideoSource_1</tt:VideoSourceConfigurationToken>\r\n"
          "        <tt:Type>Text</tt:Type>\r\n"
          "        <tt:Position><tt:Type>LowerRight</tt:Type></tt:Position>\r\n"
          "        <tt:TextString>\r\n"
          "          <tt:Type>Plain</tt:Type>\r\n"
          "          <tt:PlainText>East Gate</tt:PlainText>\r\n"
          "          <tt:FontSize>22</tt:FontSize>\r\n"
          "        </tt:TextString>\r\n"
          "      </trt:OSD>\r\n"
          "    </trt:CreateOSD>\r\n"
          "  </SOAP-ENV:Body>\r\n"
          "</SOAP-ENV:Envelope>";
    pugi::xml_document docCreate { };
    status = sendSoapReq("/onvif/media_service", reqCreate, docCreate);
    EXPECT_EQ(status, 200);
    EXPECT_TRUE(docCreate.select_node("//*[local-name()='OSDToken'][text()='OSD_TEST']"));

    // 3. GetOSD
    const std::string reqGet
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
          "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">\r\n"
          "  <SOAP-ENV:Body><trt:GetOSD><trt:OSDToken>OSD_TEST</trt:OSDToken></trt:GetOSD></SOAP-ENV:Body>\r\n"
          "</SOAP-ENV:Envelope>";
    pugi::xml_document docGet { };
    status = sendSoapReq("/onvif/media_service", reqGet, docGet);
    EXPECT_EQ(status, 200);
    EXPECT_TRUE(docGet.select_node("//*[local-name()='PlainText'][text()='East Gate']"));

    // 4. DeleteOSD
    const std::string reqDel
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
          "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">\r\n"
          "  <SOAP-ENV:Body><trt:DeleteOSD><trt:OSDToken>OSD_TEST</trt:OSDToken></trt:DeleteOSD></SOAP-ENV:Body>\r\n"
          "</SOAP-ENV:Envelope>";
    pugi::xml_document docDel { };
    status = sendSoapReq("/onvif/media_service", reqDel, docDel);
    EXPECT_EQ(status, 200);
}
