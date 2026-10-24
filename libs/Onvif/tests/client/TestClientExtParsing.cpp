/// @file TestClientExtParsing.cpp
/// @brief Modular unit tests for OnvifClient XML response parsers covering Extensions (Relays, Inputs, Metadata, PKI,
/// Analytics, Thermal).

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <Onvif/OnvifClient.h>
#include <Onvif/OnvifSecurity.h>
#include <Onvif/OnvifTypes.h>

using namespace Onvif;

TEST(ClientExtParsingTest, RelaysAndInputsParsing)
{
    // DigitalInputs
    const std::string xmlInput
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
          "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <SOAP-ENV:Body><tmd:GetDigitalInputsResponse><tmd:DigitalInputs token=\"Input_1\">"
          "    <tt:IdleState>open</tt:IdleState></tmd:DigitalInputs></tmd:GetDigitalInputsResponse></SOAP-ENV:Body>\r\n"
          "</SOAP-ENV:Envelope>";

    const auto inputs = Onvif::OnvifClient::parseDigitalInputsResponse(xmlInput);
    ASSERT_EQ(inputs.size(), 1U);
    EXPECT_EQ(inputs[0].token, "Input_1");
    EXPECT_EQ(inputs[0].idleState, Onvif::RelayIdleState::Open);

    // RelayOutputs
    const std::string xmlRelay
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
          "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <SOAP-ENV:Body><tmd:GetRelayOutputsResponse><tmd:RelayOutputs token=\"Relay_1\">"
          "    "
          "<tt:Properties><tt:Mode>Bistable</tt:Mode><tt:DelayTime>PT0S</tt:DelayTime><tt:IdleState>open</"
          "tt:IdleState></tt:Properties>"
          "    "
          "<tt:LogicalState>active</tt:LogicalState></tmd:RelayOutputs></tmd:GetRelayOutputsResponse></"
          "SOAP-ENV:Body>\r\n"
          "</SOAP-ENV:Envelope>";

    const auto relays = Onvif::OnvifClient::parseRelayOutputsResponse(xmlRelay);
    ASSERT_EQ(relays.size(), 1U);
    EXPECT_EQ(relays[0].token, "Relay_1");
    EXPECT_EQ(relays[0].mode, Onvif::RelayMode::Bistable);
    EXPECT_EQ(relays[0].logicalState, Onvif::RelayLogicalState::Active);
}

TEST(ClientExtParsingTest, MetadataConfigsAndStreams)
{
    // MetadataConfigurations
    const std::string xmlMeta
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
          "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <SOAP-ENV:Body><trt:GetMetadataConfigurationsResponse><trt:Configurations token=\"Meta_1\">"
          "    <tt:Name>MainMetadata</tt:Name><tt:UseCount>2</tt:UseCount>"
          "    <tt:PTZStatus><tt:Status>true</tt:Status></tt:PTZStatus>"
          "    <tt:Analytics>true</tt:Analytics><tt:Events>false</tt:Events>"
          "  </trt:Configurations></trt:GetMetadataConfigurationsResponse></SOAP-ENV:Body>\r\n"
          "</SOAP-ENV:Envelope>";

    const auto configs = Onvif::OnvifClient::parseMetaConfigsResp(xmlMeta);
    ASSERT_EQ(configs.size(), 1U);
    EXPECT_EQ(configs[0].token, "Meta_1");
    EXPECT_EQ(configs[0].name, "MainMetadata");
    EXPECT_TRUE(configs[0].ptzStatusEnabled);
    EXPECT_TRUE(configs[0].analyticsEnabled);
    EXPECT_FALSE(configs[0].eventsEnabled);

    // MetadataStream
    const std::string xmlStream
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
          "<tt:MetadataStream xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <tt:PTZStatus><tt:Position><tt:PanTilt x=\"0.55\" y=\"-0.25\"/><tt:Zoom x=\"0.75\"/></tt:Position>"
          "    <tt:MoveStatus><tt:PanTilt>MOVING</tt:PanTilt></tt:MoveStatus></tt:PTZStatus>"
          "  <tt:VideoAnalytics><tt:Frame UtcTime=\"2026-09-17T12:00:00Z\"><tt:Object ObjectId=\"42\">"
          "    <tt:Appearance><tt:Class><tt:Type>Car</tt:Type><tt:Likelihood>0.92</tt:Likelihood></tt:Class>"
          "      <tt:Shape><tt:BoundingBox left=\"0.1\" top=\"0.2\" right=\"0.4\" bottom=\"0.6\"/></tt:Shape>"
          "      <tt:GeoLocation lat=\"37.7749\" lon=\"-122.4194\" elevation=\"15.0\"/>"
          "    </tt:Appearance></tt:Object></tt:Frame></tt:VideoAnalytics></tt:MetadataStream>";

    const auto payload = Onvif::OnvifClient::parseMetadataStreamResponse(xmlStream);
    ASSERT_TRUE(payload.has_value());
    ASSERT_TRUE(payload->ptzStatus.has_value());
    EXPECT_LT(std::abs(payload->ptzStatus->pan - 0.55), 0.001);
    ASSERT_TRUE(payload->analyticsFrame.has_value());
    ASSERT_EQ(payload->analyticsFrame->objects.size(), 1U);
    EXPECT_EQ(payload->analyticsFrame->objects[0].objectId, 42);
    EXPECT_EQ(payload->analyticsFrame->objects[0].className, "Car");
}

TEST(ClientExtParsingTest, PkiCertificatesAndCrypto)
{
    // 1. generateSelfSignedCertificate
    const auto cert
        = Onvif::OnvifSecurity::generateSelfSignedCertificate("Cert_Test", "CN=TestCamera, O=Security", 365);
    EXPECT_EQ(cert.certificateId, "Cert_Test");
    EXPECT_FALSE(cert.x509DerBase64.empty());
    EXPECT_NE(cert.info.subject.find("TestCamera"), std::string::npos);

    // 2. parseCertificateInfo
    const auto info = Onvif::OnvifSecurity::parseCertificateInfo("Cert_Test", cert.x509DerBase64);
    EXPECT_EQ(info.certificateId, "Cert_Test");
    EXPECT_NE(info.subject.find("TestCamera"), std::string::npos);

    // 3. parseCertificatesResponse
    const std::string certsXml
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
          "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <SOAP-ENV:Body><tds:GetCertificatesResponse><tds:NvtCertificate>"
          "    "
          "<tt:CertificateID>Cert_Main</tt:CertificateID><tt:Certificate><tt:Data>MIIBjjCCATSgAwIBAgIU...</tt:Data></"
          "tt:Certificate>"
          "  </tds:NvtCertificate></tds:GetCertificatesResponse></SOAP-ENV:Body>\r\n"
          "</SOAP-ENV:Envelope>";

    const auto certList = Onvif::OnvifClient::parseCertificatesResponse(certsXml);
    ASSERT_EQ(certList.size(), 1U);
    EXPECT_EQ(certList[0].certificateId, "Cert_Main");
    EXPECT_EQ(certList[0].x509DerBase64, "MIIBjjCCATSgAwIBAgIU...");
}

TEST(ClientExtParsingTest, ProfileGAndThermalParsing)
{
    // Profile G Recording
    const std::string recXml = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:trc=\"http://www.onvif.org/ver10/recording/wsdl\">\n"
                               "  "
                               "<SOAP-ENV:Body><trc:CreateRecordingResponse><trc:RecordingToken>Rec_99</"
                               "trc:RecordingToken></trc:CreateRecordingResponse></SOAP-ENV:Body>\n"
                               "</SOAP-ENV:Envelope>";
    const auto recToken = Onvif::OnvifClient::parseCreateRecordingResponse(recXml);
    ASSERT_TRUE(recToken.has_value());
    EXPECT_EQ(*recToken, "Rec_99");

    // Thermal extension capabilities
    const std::string thermalCapXml
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
          "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\n"
          "  <soap:Body><tt:GetCapabilitiesResponse><tt:Capabilities><tt:Extension><tt:Thermal>"
          "    <tt:XAddr>http://192.168.1.100/onvif/thermal_service</tt:XAddr>"
          "  </tt:Thermal></tt:Extension></tt:Capabilities></tt:GetCapabilitiesResponse></soap:Body>\n"
          "</soap:Envelope>";

    const auto caps = Onvif::OnvifClient::parseCapabilitiesResponse(thermalCapXml);
    ASSERT_TRUE(caps.has_value());
    EXPECT_EQ(caps->thermalXAddr, "http://192.168.1.100/onvif/thermal_service");
}
