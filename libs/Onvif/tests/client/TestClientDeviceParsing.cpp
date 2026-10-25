/// @file TestClientDeviceParsing.cpp
/// @brief Modular unit tests for OnvifClient XML response parsers covering Device service operations.

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <Onvif/OnvifClient.h>
#include <Onvif/OnvifDiscovery.h>
#include <Onvif/OnvifSecurity.h>
#include <Onvif/OnvifTypes.h>

using namespace Onvif;

TEST(ClientDeviceParsingTest, SecurityHeaderGeneration)
{
    // Empty credentials
    {
        Onvif::SecurityCredentials creds { };
        const std::string header = Onvif::OnvifSecurity::buildSoapSecurityHeader(creds);
        EXPECT_TRUE(header.empty());
    }

    // Populated credentials
    {
        Onvif::SecurityCredentials creds { };
        creds.username = "admin";
        creds.password = "secretPass123";

        const std::string header = Onvif::OnvifSecurity::buildSoapSecurityHeader(creds);
        EXPECT_FALSE(header.empty());
        EXPECT_NE(header.find("wsse:Security"), std::string::npos);
        EXPECT_NE(header.find("wsse:UsernameToken"), std::string::npos);
        EXPECT_NE(header.find("<wsse:Username>admin</wsse:Username>"), std::string::npos);
        EXPECT_NE(header.find("PasswordDigest"), std::string::npos);
        EXPECT_NE(header.find("wsse:Nonce"), std::string::npos);
        EXPECT_NE(header.find("wsu:Created"), std::string::npos);
    }
}

TEST(ClientDeviceParsingTest, DiscoveryProbeGeneration)
{
    const std::string probe = Onvif::OnvifDiscovery::createProbePayload("test-uuid-1234");
    EXPECT_NE(probe.find(":Action>"), std::string::npos);
    EXPECT_NE(probe.find("http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe"), std::string::npos);
    EXPECT_NE(probe.find("test-uuid-1234"), std::string::npos);
    EXPECT_NE(probe.find("dn:NetworkVideoTransmitter"), std::string::npos);
}

TEST(ClientDeviceParsingTest, DiscoveryProbeMatches)
{
    const std::string probeMatchesXml
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
          "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
          "xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" "
          "xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\">\n"
          "  <soap:Body>\n"
          "    <d:ProbeMatches>\n"
          "      <d:ProbeMatch>\n"
          "        <wsa:EndpointReference>\n"
          "          <wsa:Address>urn:uuid:11111111-2222-3333-4444-555555555555</wsa:Address>\n"
          "        </wsa:EndpointReference>\n"
          "        <d:Types>dn:NetworkVideoTransmitter</d:Types>\n"
          "        <d:Scopes>\n"
          "          onvif://www.onvif.org/type/video_encoder\n"
          "          onvif://www.onvif.org/type/ptz\n"
          "          onvif://www.onvif.org/hardware/Spectra_IV\n"
          "          onvif://www.onvif.org/name/NorthGateCamera\n"
          "          onvif://www.onvif.org/location/Building_A\n"
          "        </d:Scopes>\n"
          "        <d:XAddrs>http://192.168.1.50/onvif/device_service</d:XAddrs>\n"
          "        <d:MetadataVersion>1</d:MetadataVersion>\n"
          "      </d:ProbeMatch>\n"
          "    </d:ProbeMatches>\n"
          "  </soap:Body>\n"
          "</soap:Envelope>";

    const auto devices = Onvif::OnvifDiscovery::parseProbeMatches(probeMatchesXml, "192.168.1.50");
    ASSERT_EQ(devices.size(), 1U);
    EXPECT_EQ(devices[0].endpoint, "http://192.168.1.50/onvif/device_service");
    EXPECT_EQ(devices[0].ip, "192.168.1.50");
    EXPECT_EQ(devices[0].hardware, "Spectra_IV");
    EXPECT_EQ(devices[0].name, "NorthGateCamera");
    EXPECT_EQ(devices[0].location, "Building_A");
    EXPECT_EQ(devices[0].scopes.size(), 5U);
}

TEST(ClientDeviceParsingTest, ProbeMatchesWithMalformedPercentEncodingDoesNotThrow)
{
    const std::string malformedXml
        = "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" "
          "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\">\n"
          "  <soap:Body>\n"
          "    <d:ProbeMatches>\n"
          "      <d:ProbeMatch>\n"
          "        <wsa:EndpointReference>\n"
          "          <wsa:Address>urn:uuid:11111111-2222-3333-4444-555555555555</wsa:Address>\n"
          "        </wsa:EndpointReference>\n"
          "        <d:Types>dn:NetworkVideoTransmitter</d:Types>\n"
          "        <d:Scopes>\n"
          "          onvif://www.onvif.org/hardware/%zz\n"
          "          onvif://www.onvif.org/name/Cam%2\n"
          "          onvif://www.onvif.org/location/Hall%20B\n"
          "        </d:Scopes>\n"
          "        <d:XAddrs>http://192.168.1.50/onvif/device_service</d:XAddrs>\n"
          "      </d:ProbeMatch>\n"
          "    </d:ProbeMatches>\n"
          "  </soap:Body>\n"
          "</soap:Envelope>";

    EXPECT_NO_THROW({
        const auto devices = Onvif::OnvifDiscovery::parseProbeMatches(malformedXml, "192.168.1.50");
        ASSERT_EQ(devices.size(), 1U);
        EXPECT_EQ(devices[0].hardware, "%zz");
        EXPECT_EQ(devices[0].name, "Cam%2");
        EXPECT_EQ(devices[0].location, "Hall B");
    });
}

TEST(ClientDeviceParsingTest, CapabilitiesParsing)
{
    const std::string xml
        = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
          "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\n"
          "  <SOAP-ENV:Body>\n"
          "    <tds:GetCapabilitiesResponse>\n"
          "      <tds:Capabilities>\n"
          "        <tt:Device><tt:XAddr>http://192.168.1.100/onvif/device_service</tt:XAddr></tt:Device>\n"
          "        <tt:Media><tt:XAddr>http://192.168.1.100/onvif/media_service</tt:XAddr></tt:Media>\n"
          "        <tt:PTZ><tt:XAddr>http://192.168.1.100/onvif/ptz_service</tt:XAddr></tt:PTZ>\n"
          "        <tt:Events><tt:XAddr>http://192.168.1.100/onvif/events_service</tt:XAddr></tt:Events>\n"
          "        <tt:Imaging><tt:XAddr>http://192.168.1.100/onvif/imaging_service</tt:XAddr></tt:Imaging>\n"
          "      </tds:Capabilities>\n"
          "    </tds:GetCapabilitiesResponse>\n"
          "  </SOAP-ENV:Body>\n"
          "</SOAP-ENV:Envelope>";

    const auto caps = Onvif::OnvifClient::parseCapabilitiesResponse(xml);
    ASSERT_TRUE(caps.has_value());
    EXPECT_EQ(caps->deviceXAddr, "http://192.168.1.100/onvif/device_service");
    EXPECT_EQ(caps->mediaXAddr, "http://192.168.1.100/onvif/media_service");
    EXPECT_EQ(caps->ptzXAddr, "http://192.168.1.100/onvif/ptz_service");
    EXPECT_EQ(caps->eventsXAddr, "http://192.168.1.100/onvif/events_service");
    EXPECT_EQ(caps->imagingXAddr, "http://192.168.1.100/onvif/imaging_service");
}

TEST(ClientDeviceParsingTest, DeviceInformationParsing)
{
    const std::string xml = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\n"
                            "  <SOAP-ENV:Body>\n"
                            "    <tds:GetDeviceInformationResponse>\n"
                            "      <tds:Manufacturer>Pelco</tds:Manufacturer>\n"
                            "      <tds:Model>Esprit HD PTZ</tds:Model>\n"
                            "      <tds:FirmwareVersion>2.5.0-build45</tds:FirmwareVersion>\n"
                            "      <tds:SerialNumber>PELCO-SN-998877</tds:SerialNumber>\n"
                            "      <tds:HardwareId>HW-ESPRIT-REV3</tds:HardwareId>\n"
                            "    </tds:GetDeviceInformationResponse>\n"
                            "  </SOAP-ENV:Body>\n"
                            "</SOAP-ENV:Envelope>";

    const auto info = Onvif::OnvifClient::parseDeviceInformationResp(xml);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->manufacturer, "Pelco");
    EXPECT_EQ(info->model, "Esprit HD PTZ");
    EXPECT_EQ(info->firmwareVersion, "2.5.0-build45");
    EXPECT_EQ(info->serialNumber, "PELCO-SN-998877");
    EXPECT_EQ(info->hardwareId, "HW-ESPRIT-REV3");
}

TEST(ClientDeviceParsingTest, SystemLogsAndSupportInfo)
{
    const std::string logXml
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
          "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <SOAP-ENV:Body><tds:GetSystemLogResponse><tds:SystemLog>"
          "    <tt:String>2026-09-17 12:00:00 [INFO] System boot completed</tt:String>"
          "  </tds:SystemLog></tds:GetSystemLogResponse></SOAP-ENV:Body>\r\n"
          "</SOAP-ENV:Envelope>";
    const auto logData = Onvif::OnvifClient::parseSystemLogResponse(logXml);
    ASSERT_TRUE(logData.has_value());
    EXPECT_NE(logData->find("System boot completed"), std::string::npos);

    const std::string supportXml
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
          "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <SOAP-ENV:Body><tds:GetSystemSupportInformationResponse><tds:SupportInformation>"
          "    <tt:String>CPU: 12.5%, Mem: 128MB/512MB, Connections: 3</tt:String>"
          "  </tds:SupportInformation></tds:GetSystemSupportInformationResponse></SOAP-ENV:Body>\r\n"
          "</SOAP-ENV:Envelope>";
    const auto info = Onvif::OnvifClient::parseSystemSupportInfoResp(supportXml);
    ASSERT_TRUE(info.has_value());
    EXPECT_NE(info->rawDiagnostics.find("CPU: 12.5%"), std::string::npos);
}
