/// @file TestOnvifServerHttp.cpp
/// @brief Modular unit tests for OnvifServer HTTP wire protocol and WS-Discovery payloads.

#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "OnvifTestFixtures.h"
#include <Onvif/WsDiscoveryServer.h>

using namespace Onvif;
using namespace Onvif::Test;

TEST(OnvifServerHttpTest, WsDiscoveryPayloads)
{
    OnvifServerConfig config { };
    config.port = 8080;
    config.deviceName = "Unit Test Camera";
    config.model = "Virtual-Camera";
    config.serviceUuid = "11111111-2222-3333-4444-555555555555";

    WsDiscoveryServer discServer(config);

    const std::string probeMatches = discServer.createProbeMatchesPayload("urn:uuid:test-probe-1234", "192.168.1.100");

    pugi::xml_document doc { };
    const auto res = doc.load_string(probeMatches.c_str());
    EXPECT_TRUE(res);

    const pugi::xml_node relatesNode = doc.select_node("//*[local-name()='RelatesTo']").node();
    EXPECT_TRUE(relatesNode);
    EXPECT_TRUE(std::string(relatesNode.text().as_string()) == "urn:uuid:test-probe-1234");

    const pugi::xml_node xaddrsNode = doc.select_node("//*[local-name()='XAddrs']").node();
    EXPECT_TRUE(xaddrsNode);
    EXPECT_TRUE(std::string(xaddrsNode.text().as_string()).find("http://192.168.1.100:8080/onvif/device_service")
        != std::string::npos);

    // Verify Profile S & T in scopes
    const pugi::xml_node scopesNode = doc.select_node("//*[local-name()='Scopes']").node();
    EXPECT_TRUE(scopesNode);
    const std::string scopesStr = scopesNode.text().as_string();
    EXPECT_TRUE(scopesStr.find("onvif://www.onvif.org/Profile/S") != std::string::npos);
    EXPECT_TRUE(scopesStr.find("onvif://www.onvif.org/Profile/T") != std::string::npos);

    const std::string hello = discServer.createHelloPayload("192.168.1.100");
    EXPECT_TRUE(doc.load_string(hello.c_str()));
    const pugi::xml_node helloNode = doc.select_node("//*[local-name()='Hello']").node();
    EXPECT_TRUE(helloNode);

    const std::string bye = discServer.createByePayload();
    EXPECT_TRUE(doc.load_string(bye.c_str()));
    const pugi::xml_node byeNode = doc.select_node("//*[local-name()='Bye']").node();
    EXPECT_TRUE(byeNode);
}

TEST_F(OnvifServerFixture, HttpSoapDateAndTime)
{
    const std::string soapReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:GetSystemDateAndTime/>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

    pugi::xml_document doc { };
    const int status = sendSoapReq("/onvif/device_service", soapReq, doc);
    EXPECT_EQ(status, 200);
    EXPECT_TRUE(doc.select_node("//*[local-name()='GetSystemDateAndTimeResponse']"));
    EXPECT_TRUE(doc.select_node("//*[local-name()='UTCDateTime']"));
}

TEST_F(OnvifServerFixture, HttpSoapDeviceInfo)
{
    const std::string soapReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:GetDeviceInformation/>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

    pugi::xml_document doc { };
    const int status = sendSoapReq("/onvif/device_service", soapReq, doc);
    EXPECT_EQ(status, 200);

    const auto mfgNode = doc.select_node("//*[local-name()='Manufacturer']").node();
    EXPECT_TRUE(mfgNode && std::string(mfgNode.text().as_string()) == "PelcoD-Test");
    const auto modelNode = doc.select_node("//*[local-name()='Model']").node();
    EXPECT_TRUE(modelNode && std::string(modelNode.text().as_string()) == "Model-XYZ");
}

TEST_F(OnvifServerFixture, HttpSoapCapabilities)
{
    const std::string soapReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:GetCapabilities/>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

    pugi::xml_document doc { };
    const int status = sendSoapReq("/onvif/device_service", soapReq, doc);
    EXPECT_EQ(status, 200);
    EXPECT_TRUE(doc.select_node("//*[local-name()='PTZ']/*[local-name()='XAddr']"));
    EXPECT_TRUE(doc.select_node("//*[local-name()='Media']/*[local-name()='XAddr']"));
    EXPECT_TRUE(doc.select_node("//*[local-name()='Imaging']/*[local-name()='XAddr']"));
    EXPECT_TRUE(doc.select_node("//*[local-name()='Events']/*[local-name()='XAddr']"));
}

TEST_F(OnvifServerFixture, HttpSoapProfilesStream)
{
    const std::string profilesReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body>\r\n"
                                    "    <trt:GetProfiles/>\r\n"
                                    "  </SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";

    pugi::xml_document doc { };
    const int statusProfiles = sendSoapReq("/onvif/media_service", profilesReq, doc);
    EXPECT_EQ(statusProfiles, 200);
    EXPECT_TRUE(doc.select_node("//*[local-name()='Profiles'][@token='ProfileToken_1']"));

    const std::string streamReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                  "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                  "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">\r\n"
                                  "  <SOAP-ENV:Body>\r\n"
                                  "    <trt:GetStreamUri/>\r\n"
                                  "  </SOAP-ENV:Body>\r\n"
                                  "</SOAP-ENV:Envelope>";

    pugi::xml_document docStream { };
    const int statusStream = sendSoapReq("/onvif/media_service", streamReq, docStream);
    EXPECT_EQ(statusStream, 200);
    const auto uriNode = docStream.select_node("//*[local-name()='Uri']").node();
    EXPECT_TRUE(uriNode && std::string(uriNode.text().as_string()) == "rtsp://127.0.0.1:8554/test_stream");
}

TEST_F(OnvifServerFixture, HttpSoapContinuousStop)
{
    const std::string moveReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" "
                                "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tptz:ContinuousMove>\r\n"
                                "      <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                "      <tptz:Velocity>\r\n"
                                "        <tt:PanTilt x=\"0.75\" y=\"-0.5\"/>\r\n"
                                "        <tt:Zoom x=\"0.2\"/>\r\n"
                                "      </tptz:Velocity>\r\n"
                                "    </tptz:ContinuousMove>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

    pugi::xml_document docMove { };
    const int statusMove = sendSoapReq("/onvif/ptz_service", moveReq, docMove);
    EXPECT_EQ(statusMove, 200);
    EXPECT_EQ(m_ptzHandler->moveCount, 1);
    EXPECT_LT(std::abs(m_ptzHandler->lastPan - 0.75f), 0.001f);
    EXPECT_LT(std::abs(m_ptzHandler->lastTilt - (-0.5f)), 0.001f);
    EXPECT_LT(std::abs(m_ptzHandler->lastZoom - 0.2f), 0.001f);

    const std::string stopReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tptz:Stop>\r\n"
                                "      <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                "      <tptz:PanTilt>true</tptz:PanTilt>\r\n"
                                "      <tptz:Zoom>true</tptz:Zoom>\r\n"
                                "    </tptz:Stop>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

    pugi::xml_document docStop { };
    const int statusStop = sendSoapReq("/onvif/ptz_service", stopReq, docStop);
    EXPECT_EQ(statusStop, 200);
    EXPECT_EQ(m_ptzHandler->stopCount, 1);
    EXPECT_TRUE(m_ptzHandler->lastStopPt);
    EXPECT_TRUE(m_ptzHandler->lastStopZ);
}
