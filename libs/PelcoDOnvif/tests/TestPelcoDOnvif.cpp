/// @file TestPelcoDOnvif.cpp
/// @brief Comprehensive unit tests for ONVIF Profile S client, discovery, security, and PTZ controls.

#include "PelcoDOnvif/GeodesyUtils.h"
#include "PelcoDOnvif/OnvifClient.h"
#include "PelcoDOnvif/OnvifDiscovery.h"
#include "PelcoDOnvif/OnvifSecurity.h"
#include "PelcoDOnvif/OnvifTypes.h"

#include <gtest/gtest.h>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <crtdbg.h>
#include <cstdlib>
#endif

TEST(PelcoDOnvifTest, SecurityHeaderGeneration)
{
    // 1. Empty credentials should yield empty security header
    {
        PelcoD::Onvif::SecurityCredentials creds {};
        const std::string header = PelcoD::Onvif::OnvifSecurity::buildSoapSecurityHeader(creds);
        EXPECT_TRUE(header.empty());
    }

    // 2. Populated credentials should produce valid WS-Security UsernameToken header
    {
        PelcoD::Onvif::SecurityCredentials creds {};
        creds.username = "admin";
        creds.password = "secretPass123";

        const std::string header = PelcoD::Onvif::OnvifSecurity::buildSoapSecurityHeader(creds);
        EXPECT_TRUE(!header.empty());
        EXPECT_TRUE(header.find("wsse:Security") != std::string::npos);
        EXPECT_TRUE(header.find("wsse:UsernameToken") != std::string::npos);
        EXPECT_TRUE(header.find("<wsse:Username>admin</wsse:Username>") != std::string::npos);
        EXPECT_TRUE(header.find("PasswordDigest") != std::string::npos);
        EXPECT_TRUE(header.find("wsse:Nonce") != std::string::npos);
        EXPECT_TRUE(header.find("wsu:Created") != std::string::npos);
    }
}

TEST(PelcoDOnvifTest, DiscoveryProbeGenerationAndParsing)
{
    // 1. Probe payload generation
    const std::string probe = PelcoD::Onvif::OnvifDiscovery::createProbePayload("test-uuid-1234");
    EXPECT_TRUE(probe.find(":Action>") != std::string::npos);
    EXPECT_TRUE(probe.find("http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe") != std::string::npos);
    EXPECT_TRUE(probe.find("test-uuid-1234") != std::string::npos);
    EXPECT_TRUE(probe.find("dn:NetworkVideoTransmitter") != std::string::npos);

    // 2. Parse ProbeMatches response
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
          "      <d:ProbeMatch>\n"
          "        <wsa:EndpointReference>\n"
          "          <wsa:Address>urn:uuid:66666666-7777-8888-9999-000000000000</wsa:Address>\n"
          "        </wsa:EndpointReference>\n"
          "        <d:Types>dn:NetworkVideoTransmitter</d:Types>\n"
          "        <d:Scopes>\n"
          "          onvif://www.onvif.org/hardware/SX800\n"
          "          onvif://www.onvif.org/name/PerimeterCam\n"
          "        </d:Scopes>\n"
          "        <d:XAddrs>http://192.168.1.60:8080/onvif/device_service</d:XAddrs>\n"
          "        <d:MetadataVersion>1</d:MetadataVersion>\n"
          "      </d:ProbeMatch>\n"
          "    </d:ProbeMatches>\n"
          "  </soap:Body>\n"
          "</soap:Envelope>";

    const auto devices = PelcoD::Onvif::OnvifDiscovery::parseProbeMatches(probeMatchesXml, "192.168.1.50");
    EXPECT_TRUE(devices.size() == 2U);

    // Verify Device 1
    EXPECT_TRUE(devices[0].endpoint == "http://192.168.1.50/onvif/device_service");
    EXPECT_TRUE(devices[0].ip == "192.168.1.50");
    EXPECT_TRUE(devices[0].hardware == "Spectra_IV");
    EXPECT_TRUE(devices[0].name == "NorthGateCamera");
    EXPECT_TRUE(devices[0].location == "Building_A");
    EXPECT_TRUE(devices[0].scopes.size() == 5U);

    // Verify Device 2
    EXPECT_TRUE(devices[1].endpoint == "http://192.168.1.60:8080/onvif/device_service");
    EXPECT_TRUE(devices[1].ip == "192.168.1.60");
    EXPECT_TRUE(devices[1].hardware == "SX800");
    EXPECT_TRUE(devices[1].name == "PerimeterCam");
}

TEST(PelcoDOnvifTest, CapabilitiesParsing)
{
    const std::string xml = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                            "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\n"
                            "  <SOAP-ENV:Body>\n"
                            "    <tds:GetCapabilitiesResponse>\n"
                            "      <tds:Capabilities>\n"
                            "        <tt:Device>\n"
                            "          <tt:XAddr>http://192.168.1.100/onvif/device_service</tt:XAddr>\n"
                            "        </tt:Device>\n"
                            "        <tt:Media>\n"
                            "          <tt:XAddr>http://192.168.1.100/onvif/media_service</tt:XAddr>\n"
                            "        </tt:Media>\n"
                            "        <tt:PTZ>\n"
                            "          <tt:XAddr>http://192.168.1.100/onvif/ptz_service</tt:XAddr>\n"
                            "        </tt:PTZ>\n"
                            "        <tt:Events>\n"
                            "          <tt:XAddr>http://192.168.1.100/onvif/events_service</tt:XAddr>\n"
                            "        </tt:Events>\n"
                            "        <tt:Imaging>\n"
                            "          <tt:XAddr>http://192.168.1.100/onvif/imaging_service</tt:XAddr>\n"
                            "        </tt:Imaging>\n"
                            "      </tds:Capabilities>\n"
                            "    </tds:GetCapabilitiesResponse>\n"
                            "  </SOAP-ENV:Body>\n"
                            "</SOAP-ENV:Envelope>";

    const auto caps = PelcoD::Onvif::OnvifClient::parseCapabilitiesResponse(xml);
    EXPECT_TRUE(caps.has_value());
    EXPECT_TRUE(caps->deviceXAddr == "http://192.168.1.100/onvif/device_service");
    EXPECT_TRUE(caps->mediaXAddr == "http://192.168.1.100/onvif/media_service");
    EXPECT_TRUE(caps->ptzXAddr == "http://192.168.1.100/onvif/ptz_service");
    EXPECT_TRUE(caps->eventsXAddr == "http://192.168.1.100/onvif/events_service");
    EXPECT_TRUE(caps->imagingXAddr == "http://192.168.1.100/onvif/imaging_service");

    // Invalid XML test
    EXPECT_TRUE(!PelcoD::Onvif::OnvifClient::parseCapabilitiesResponse("<invalid>xml").has_value());
}

TEST(PelcoDOnvifTest, DeviceInformationParsing)
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

    const auto info = PelcoD::Onvif::OnvifClient::parseDeviceInformationResponse(xml);
    EXPECT_TRUE(info.has_value());
    EXPECT_TRUE(info->manufacturer == "Pelco");
    EXPECT_TRUE(info->model == "Esprit HD PTZ");
    EXPECT_TRUE(info->firmwareVersion == "2.5.0-build45");
    EXPECT_TRUE(info->serialNumber == "PELCO-SN-998877");
    EXPECT_TRUE(info->hardwareId == "HW-ESPRIT-REV3");
}

TEST(PelcoDOnvifTest, ProfilesParsing)
{
    const std::string xml = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
                            "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\n"
                            "  <SOAP-ENV:Body>\n"
                            "    <trt:GetProfilesResponse>\n"
                            "      <trt:Profiles token=\"Profile_1\">\n"
                            "        <tt:Name>MainStream_1080p</tt:Name>\n"
                            "        <tt:VideoEncoderConfiguration token=\"vec_1\">\n"
                            "          <tt:Encoding>H264</tt:Encoding>\n"
                            "          <tt:Resolution>\n"
                            "            <tt:Width>1920</tt:Width>\n"
                            "            <tt:Height>1080</tt:Height>\n"
                            "          </tt:Resolution>\n"
                            "        </tt:VideoEncoderConfiguration>\n"
                            "      </trt:Profiles>\n"
                            "      <trt:Profiles token=\"Profile_2\">\n"
                            "        <tt:Name>SubStream_VGA</tt:Name>\n"
                            "        <tt:VideoEncoderConfiguration token=\"vec_2\">\n"
                            "          <tt:Encoding>H265</tt:Encoding>\n"
                            "          <tt:Resolution>\n"
                            "            <tt:Width>640</tt:Width>\n"
                            "            <tt:Height>480</tt:Height>\n"
                            "          </tt:Resolution>\n"
                            "        </tt:VideoEncoderConfiguration>\n"
                            "      </trt:Profiles>\n"
                            "    </trt:GetProfilesResponse>\n"
                            "  </SOAP-ENV:Body>\n"
                            "</SOAP-ENV:Envelope>";

    const auto profiles = PelcoD::Onvif::OnvifClient::parseProfilesResponse(xml);
    EXPECT_TRUE(profiles.size() == 2U);

    EXPECT_TRUE(profiles[0].token == "Profile_1");
    EXPECT_TRUE(profiles[0].name == "MainStream_1080p");
    EXPECT_TRUE(profiles[0].videoWidth == 1920);
    EXPECT_TRUE(profiles[0].videoHeight == 1080);
    EXPECT_TRUE(profiles[0].videoEncoding == "H264");

    EXPECT_TRUE(profiles[1].token == "Profile_2");
    EXPECT_TRUE(profiles[1].name == "SubStream_VGA");
    EXPECT_TRUE(profiles[1].videoWidth == 640);
    EXPECT_TRUE(profiles[1].videoHeight == 480);
    EXPECT_TRUE(profiles[1].videoEncoding == "H265");
}

TEST(PelcoDOnvifTest, StreamUriParsing)
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

    const auto uriInfo = PelcoD::Onvif::OnvifClient::parseStreamUriResponse(xml);
    EXPECT_TRUE(uriInfo.has_value());
    EXPECT_TRUE(uriInfo->uri == "rtsp://192.168.1.100:554/live/ch0");
    EXPECT_TRUE(!uriInfo->invalidAfterConnect);
    EXPECT_TRUE(uriInfo->invalidAfterReboot);
    EXPECT_TRUE(uriInfo->timeout == "PT60S");
}

TEST(PelcoDOnvifTest, SnapshotUriParsing)
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

    const auto snapUri = PelcoD::Onvif::OnvifClient::parseSnapshotUriResponse(xml);
    EXPECT_TRUE(snapUri.has_value());
    EXPECT_TRUE(*snapUri == "http://192.168.1.100/onvif/snapshot/view.jpg");
}

TEST(PelcoDOnvifTest, PtzStatusParsing)
{
    const std::string xml = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" "
                            "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\n"
                            "  <SOAP-ENV:Body>\n"
                            "    <tptz:GetStatusResponse>\n"
                            "      <tptz:PTZStatus>\n"
                            "        <tt:Position>\n"
                            "          <tt:PanTilt x=\"0.3500\" y=\"-0.2000\"/>\n"
                            "          <tt:Zoom x=\"0.7500\"/>\n"
                            "        </tt:Position>\n"
                            "        <tt:MoveStatus>\n"
                            "          <tt:PanTilt>MOVING</tt:PanTilt>\n"
                            "          <tt:Zoom>IDLE</tt:Zoom>\n"
                            "        </tt:MoveStatus>\n"
                            "        <tt:UtcTime>2026-09-16T19:00:00Z</tt:UtcTime>\n"
                            "      </tptz:PTZStatus>\n"
                            "    </tptz:GetStatusResponse>\n"
                            "  </SOAP-ENV:Body>\n"
                            "</SOAP-ENV:Envelope>";

    const auto status = PelcoD::Onvif::OnvifClient::parsePtzStatusResponse(xml);
    EXPECT_TRUE(status.has_value());
    EXPECT_TRUE(status->pan > 0.349 && status->pan < 0.351);
    EXPECT_TRUE(status->tilt < -0.199 && status->tilt > -0.201);
    EXPECT_TRUE(status->zoom > 0.749 && status->zoom < 0.751);
    EXPECT_TRUE(status->isMoving);
    EXPECT_TRUE(status->utcTime == "2026-09-16T19:00:00Z");
}

TEST(PelcoDOnvifTest, PresetsParsing)
{
    // 1. Parse GetPresetsResponse
    const std::string getPresetsXml = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                      "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" "
                                      "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\n"
                                      "  <SOAP-ENV:Body>\n"
                                      "    <tptz:GetPresetsResponse>\n"
                                      "      <tptz:Preset token=\"1\">\n"
                                      "        <tt:Name>Gate Entrance</tt:Name>\n"
                                      "        <tt:PTZPosition>\n"
                                      "          <tt:PanTilt x=\"-0.5000\" y=\"0.1000\"/>\n"
                                      "          <tt:Zoom x=\"0.4000\"/>\n"
                                      "        </tt:PTZPosition>\n"
                                      "      </tptz:Preset>\n"
                                      "      <tptz:Preset token=\"2\">\n"
                                      "        <tt:Name>Parking Lot</tt:Name>\n"
                                      "        <tt:PTZPosition>\n"
                                      "          <tt:PanTilt x=\"0.8000\" y=\"-0.3000\"/>\n"
                                      "          <tt:Zoom x=\"1.0000\"/>\n"
                                      "        </tt:PTZPosition>\n"
                                      "      </tptz:Preset>\n"
                                      "    </tptz:GetPresetsResponse>\n"
                                      "  </SOAP-ENV:Body>\n"
                                      "</SOAP-ENV:Envelope>";

    const auto presets = PelcoD::Onvif::OnvifClient::parsePresetsResponse(getPresetsXml);
    EXPECT_TRUE(presets.size() == 2U);
    EXPECT_TRUE(presets[0].token == "1");
    EXPECT_TRUE(presets[0].name == "Gate Entrance");
    EXPECT_TRUE(presets[0].pan < -0.499 && presets[0].pan > -0.501);
    EXPECT_TRUE(presets[0].tilt > 0.099 && presets[0].tilt < 0.101);
    EXPECT_TRUE(presets[0].zoom > 0.399 && presets[0].zoom < 0.401);

    EXPECT_TRUE(presets[1].token == "2");
    EXPECT_TRUE(presets[1].name == "Parking Lot");
    EXPECT_TRUE(presets[1].pan > 0.799 && presets[1].pan < 0.801);

    // 2. Parse SetPresetResponse
    const std::string setPresetXml = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                     "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\n"
                                     "  <SOAP-ENV:Body>\n"
                                     "    <tptz:SetPresetResponse>\n"
                                     "      <tptz:PresetToken>preset_token_99</tptz:PresetToken>\n"
                                     "    </tptz:SetPresetResponse>\n"
                                     "  </SOAP-ENV:Body>\n"
                                     "</SOAP-ENV:Envelope>";

    const auto assignedToken = PelcoD::Onvif::OnvifClient::parseSetPresetResponse(setPresetXml);
    EXPECT_TRUE(assignedToken.has_value());
    EXPECT_TRUE(*assignedToken == "preset_token_99");
}

TEST(PelcoDOnvifTest, PresetToursParsing)
{
    const std::string toursXml = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                 "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" "
                                 "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\n"
                                 "  <SOAP-ENV:Body>\n"
                                 "    <tptz:GetPresetToursResponse>\n"
                                 "      <tptz:PresetTour token=\"Tour_1\">\n"
                                 "        <tt:Name>Perimeter Patrol</tt:Name>\n"
                                 "        <tt:Status>\n"
                                 "          <tt:State>Touring</tt:State>\n"
                                 "        </tt:Status>\n"
                                 "        <tt:TourSpot>\n"
                                 "          <tt:PresetDetail>\n"
                                 "            <tt:PresetToken>1</tt:PresetToken>\n"
                                 "          </tt:PresetDetail>\n"
                                 "          <tt:Speed>\n"
                                 "            <tt:PanTilt x=\"0.8\" y=\"0.8\"/>\n"
                                 "          </tt:Speed>\n"
                                 "          <tt:StayTime>PT5S</tt:StayTime>\n"
                                 "        </tt:TourSpot>\n"
                                 "        <tt:TourSpot>\n"
                                 "          <tt:PresetDetail>\n"
                                 "            <tt:PresetToken>2</tt:PresetToken>\n"
                                 "          </tt:PresetDetail>\n"
                                 "          <tt:Speed>\n"
                                 "            <tt:PanTilt x=\"0.5\" y=\"0.5\"/>\n"
                                 "          </tt:Speed>\n"
                                 "          <tt:StayTime>PT10S</tt:StayTime>\n"
                                 "        </tt:TourSpot>\n"
                                 "      </tptz:PresetTour>\n"
                                 "    </tptz:GetPresetToursResponse>\n"
                                 "  </SOAP-ENV:Body>\n"
                                 "</SOAP-ENV:Envelope>";

    const auto tours = PelcoD::Onvif::OnvifClient::parsePresetToursResponse(toursXml);
    EXPECT_TRUE(tours.size() == 1);
    EXPECT_TRUE(tours[0].token == "Tour_1");
    EXPECT_TRUE(tours[0].name == "Perimeter Patrol");
    EXPECT_TRUE(tours[0].status == PelcoD::Onvif::PresetTourState::Touring);
    EXPECT_TRUE(tours[0].spots.size() == 2);
    EXPECT_TRUE(tours[0].spots[0].presetToken == "1");
    EXPECT_TRUE(std::abs(tours[0].spots[0].speed - 0.8f) < 0.01f);
    EXPECT_TRUE(tours[0].spots[0].stayTimeSeconds == 5);
    EXPECT_TRUE(tours[0].spots[1].presetToken == "2");
    EXPECT_TRUE(std::abs(tours[0].spots[1].speed - 0.5f) < 0.01f);
    EXPECT_TRUE(tours[0].spots[1].stayTimeSeconds == 10);

    const std::string createXml = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                  "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\n"
                                  "  <SOAP-ENV:Body>\n"
                                  "    <tptz:CreatePresetTourResponse>\n"
                                  "      <tptz:PresetTourToken>Tour_99</tptz:PresetTourToken>\n"
                                  "    </tptz:CreatePresetTourResponse>\n"
                                  "  </SOAP-ENV:Body>\n"
                                  "</SOAP-ENV:Envelope>";

    const auto createdToken = PelcoD::Onvif::OnvifClient::parseCreatePresetTourResponse(createXml);
    EXPECT_TRUE(createdToken.has_value());
    EXPECT_TRUE(*createdToken == "Tour_99");
}

TEST(PelcoDOnvifTest, SystemRebootParsing)
{
    const std::string xml = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\n"
                            "  <SOAP-ENV:Body>\n"
                            "    <tds:SystemRebootResponse>\n"
                            "      <tds:Message>Device rebooting in 5 seconds</tds:Message>\n"
                            "    </tds:SystemRebootResponse>\n"
                            "  </SOAP-ENV:Body>\n"
                            "</SOAP-ENV:Envelope>";

    const auto rebootMsg = PelcoD::Onvif::OnvifClient::parseSystemRebootResponse(xml);
    EXPECT_TRUE(rebootMsg.has_value());
    EXPECT_TRUE(*rebootMsg == "Device rebooting in 5 seconds");
}

TEST(PelcoDOnvifTest, SoapEnvelopeWrapping)
{
    PelcoD::Onvif::SecurityCredentials creds {};
    creds.username = "operator";
    creds.password = "pass456";

    PelcoD::Onvif::OnvifClient client("http://192.168.1.100/onvif/device_service", creds);
    const std::string body = "<tds:GetDeviceInformation/>";
    const std::string env = client.wrapSoapEnvelope(body);

    EXPECT_TRUE(env.find("<s:Envelope") != std::string::npos);
    EXPECT_TRUE(env.find("<s:Header>") != std::string::npos);
    EXPECT_TRUE(env.find("<wsse:Username>operator</wsse:Username>") != std::string::npos);
    EXPECT_TRUE(env.find("<s:Body>") != std::string::npos);
    EXPECT_TRUE(env.find("<tds:GetDeviceInformation/>") != std::string::npos);
}

TEST(PelcoDOnvifTest, ImagingSettingsParsing)
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

    const auto settings = PelcoD::Onvif::OnvifClient::parseImagingSettingsResponse(xml);
    EXPECT_TRUE(settings.has_value());
    EXPECT_TRUE(settings->brightness == 65.0f);
    EXPECT_TRUE(settings->colorSaturation == 75.0f);
    EXPECT_TRUE(settings->contrast == 80.0f);
    EXPECT_TRUE(settings->sharpness == 45.0f);
    EXPECT_TRUE(settings->irCutFilter == "AUTO");
    EXPECT_TRUE(settings->backlightCompensation == true);
    EXPECT_TRUE(settings->backlightLevel == 50.0f);
    EXPECT_TRUE(settings->wideDynamicRange == false);
    EXPECT_TRUE(settings->autoFocusMode == "MANUAL");
}

TEST(PelcoDOnvifTest, PullPointEventsParsing)
{
    // 1. Parse subscription creation response
    const std::string subXml
        = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:wsa=\"http://www.w3.org/2005/08/addressing\" "
          "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\">\n"
          "  <SOAP-ENV:Body>\n"
          "    <tev:CreatePullPointSubscriptionResponse>\n"
          "      <tev:SubscriptionReference>\n"
          "        <wsa:Address>http://192.168.1.100:8080/onvif/Subscription?idx=42</wsa:Address>\n"
          "      </tev:SubscriptionReference>\n"
          "      <wsnt:CurrentTime>2026-09-16T20:40:00Z</wsnt:CurrentTime>\n"
          "      <wsnt:TerminationTime>2026-09-16T20:41:00Z</wsnt:TerminationTime>\n"
          "    </tev:CreatePullPointSubscriptionResponse>\n"
          "  </SOAP-ENV:Body>\n"
          "</SOAP-ENV:Envelope>";

    const auto subUrl = PelcoD::Onvif::OnvifClient::parseCreatePullPointSubscriptionResponse(subXml);
    EXPECT_TRUE(subUrl.has_value());
    EXPECT_TRUE(*subUrl == "http://192.168.1.100:8080/onvif/Subscription?idx=42");

    // 2. Parse pull messages response with motion & tamper events
    const std::string pullXml
        = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\" "
          "xmlns:tt=\"http://www.onvif.org/ver10/schema\" "
          "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\">\n"
          "  <SOAP-ENV:Body>\n"
          "    <tev:PullMessagesResponse>\n"
          "      <wsnt:NotificationMessage>\n"
          "        <wsnt:Topic>tns1:RuleEngine/CellMotionDetector/Motion</wsnt:Topic>\n"
          "        <wsnt:Message UtcTime=\"2026-09-16T20:40:05Z\">\n"
          "          <tt:Source>\n"
          "            <tt:SimpleItem Name=\"VideoSourceConfigurationToken\" Value=\"VideoSource_1\"/>\n"
          "          </tt:Source>\n"
          "          <tt:Data>\n"
          "            <tt:SimpleItem Name=\"IsMotion\" Value=\"true\"/>\n"
          "          </tt:Data>\n"
          "        </wsnt:Message>\n"
          "      </wsnt:NotificationMessage>\n"
          "      <wsnt:NotificationMessage>\n"
          "        <wsnt:Topic>tns1:VideoSource/ImageTooDark/AnalyticsService</wsnt:Topic>\n"
          "        <wsnt:Message UtcTime=\"2026-09-16T20:40:06Z\">\n"
          "          <tt:Source>\n"
          "            <tt:SimpleItem Name=\"Source\" Value=\"Source_0\"/>\n"
          "          </tt:Source>\n"
          "          <tt:Data>\n"
          "            <tt:SimpleItem Name=\"State\" Value=\"ACTIVE\"/>\n"
          "          </tt:Data>\n"
          "        </wsnt:Message>\n"
          "      </wsnt:NotificationMessage>\n"
          "    </tev:PullMessagesResponse>\n"
          "  </SOAP-ENV:Body>\n"
          "</SOAP-ENV:Envelope>";

    const auto events = PelcoD::Onvif::OnvifClient::parsePullMessagesResponse(pullXml);
    EXPECT_TRUE(events.size() == 2U);
    EXPECT_TRUE(events[0].topic == "tns1:RuleEngine/CellMotionDetector/Motion");
    EXPECT_TRUE(events[0].sourceName == "VideoSourceConfigurationToken");
    EXPECT_TRUE(events[0].sourceValue == "VideoSource_1");
    EXPECT_TRUE(events[0].dataName == "IsMotion");
    EXPECT_TRUE(events[0].dataValue == "true");
    EXPECT_TRUE(events[0].utcTime == "2026-09-16T20:40:05Z");

    EXPECT_TRUE(events[1].topic == "tns1:VideoSource/ImageTooDark/AnalyticsService");
    EXPECT_TRUE(events[1].dataName == "State");
    EXPECT_TRUE(events[1].dataValue == "ACTIVE");
}

TEST(PelcoDOnvifTest, SendAuxiliaryCommandParsing)
{
    const std::string xmlWithResponse = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                        "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\n"
                                        "  <SOAP-ENV:Body>\n"
                                        "    <tptz:SendAuxiliaryCommandResponse>\n"
                                        "      <tptz:AuxiliaryResponse>tt:Wiper|On</tptz:AuxiliaryResponse>\n"
                                        "    </tptz:SendAuxiliaryCommandResponse>\n"
                                        "  </SOAP-ENV:Body>\n"
                                        "</SOAP-ENV:Envelope>";

    const auto resp1 = PelcoD::Onvif::OnvifClient::parseSendAuxiliaryCommandResponse(xmlWithResponse);
    EXPECT_TRUE(resp1.has_value());
    EXPECT_TRUE(*resp1 == "tt:Wiper|On");

    const std::string xmlEmptyResponse
        = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\n"
          "  <SOAP-ENV:Body>\n"
          "    <tptz:SendAuxiliaryCommandResponse/>\n"
          "  </SOAP-ENV:Body>\n"
          "</SOAP-ENV:Envelope>";

    const auto resp2 = PelcoD::Onvif::OnvifClient::parseSendAuxiliaryCommandResponse(xmlEmptyResponse);
    EXPECT_TRUE(resp2.has_value());
    EXPECT_TRUE(resp2->empty());

    const std::string xmlInvalid = "<InvalidXml>";
    const auto resp3 = PelcoD::Onvif::OnvifClient::parseSendAuxiliaryCommandResponse(xmlInvalid);
    EXPECT_TRUE(!resp3.has_value());
}

TEST(PelcoDOnvifTest, OsdParsing)
{
    const std::string osdListXml
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
          "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <SOAP-ENV:Body>\r\n"
          "    <trt:GetOSDsResponse>\r\n"
          "      <trt:OSD token=\"OSD_1\">\r\n"
          "        <tt:VideoSourceConfigurationToken>VideoSourceConfig_1</tt:VideoSourceConfigurationToken>\r\n"
          "        <tt:Type>Text</tt:Type>\r\n"
          "        <tt:Position>\r\n"
          "          <tt:Type>UpperLeft</tt:Type>\r\n"
          "        </tt:Position>\r\n"
          "        <tt:TextString>\r\n"
          "          <tt:Type>Plain</tt:Type>\r\n"
          "          <tt:PlainText>Entrance Gate</tt:PlainText>\r\n"
          "          <tt:FontSize>28</tt:FontSize>\r\n"
          "        </tt:TextString>\r\n"
          "      </trt:OSD>\r\n"
          "      <trt:OSD token=\"OSD_2\">\r\n"
          "        <tt:VideoSourceConfigurationToken>VideoSourceConfig_1</tt:VideoSourceConfigurationToken>\r\n"
          "        <tt:Type>Text</tt:Type>\r\n"
          "        <tt:Position>\r\n"
          "          <tt:Type>Custom</tt:Type>\r\n"
          "          <tt:Pos x=\"0.5\" y=\"0.5\"/>\r\n"
          "        </tt:Position>\r\n"
          "        <tt:TextString>\r\n"
          "          <tt:Type>DateAndTime</tt:Type>\r\n"
          "          <tt:DateFormat>YYYY/MM/DD</tt:DateFormat>\r\n"
          "          <tt:TimeFormat>HH:mm:ss</tt:TimeFormat>\r\n"
          "          <tt:FontSize>32</tt:FontSize>\r\n"
          "        </tt:TextString>\r\n"
          "      </trt:OSD>\r\n"
          "    </trt:GetOSDsResponse>\r\n"
          "  </SOAP-ENV:Body>\r\n"
          "</SOAP-ENV:Envelope>";

    const auto osds = PelcoD::Onvif::OnvifClient::parseOsdListResponse(osdListXml);
    EXPECT_TRUE(osds.size() == 2);
    EXPECT_TRUE(osds[0].token == "OSD_1");
    EXPECT_TRUE(osds[0].videoSourceToken == "VideoSourceConfig_1");
    EXPECT_TRUE(osds[0].position == PelcoD::Onvif::OsdPositionType::UpperLeft);
    EXPECT_TRUE(!osds[0].isDateAndTime);
    EXPECT_TRUE(osds[0].plainText == "Entrance Gate");
    EXPECT_TRUE(osds[0].fontSize == 28);

    EXPECT_TRUE(osds[1].token == "OSD_2");
    EXPECT_TRUE(osds[1].position == PelcoD::Onvif::OsdPositionType::Custom);
    EXPECT_TRUE(std::abs(osds[1].customX - 0.5f) < 0.001f);
    EXPECT_TRUE(std::abs(osds[1].customY - 0.5f) < 0.001f);
    EXPECT_TRUE(osds[1].isDateAndTime);
    EXPECT_TRUE(osds[1].dateFormat == "YYYY/MM/DD");
    EXPECT_TRUE(osds[1].timeFormat == "HH:mm:ss");
    EXPECT_TRUE(osds[1].fontSize == 32);

    // Test single OSD parser
    const std::string singleOsdXml
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
          "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <SOAP-ENV:Body>\r\n"
          "    <trt:GetOSDResponse>\r\n"
          "      <trt:OSD token=\"OSD_99\">\r\n"
          "        <tt:VideoSourceConfigurationToken>VideoSourceConfig_2</tt:VideoSourceConfigurationToken>\r\n"
          "        <tt:Type>Text</tt:Type>\r\n"
          "        <tt:Position>\r\n"
          "          <tt:Type>LowerRight</tt:Type>\r\n"
          "        </tt:Position>\r\n"
          "        <tt:TextString>\r\n"
          "          <tt:Type>Plain</tt:Type>\r\n"
          "          <tt:PlainText>Perimeter North</tt:PlainText>\r\n"
          "          <tt:FontSize>20</tt:FontSize>\r\n"
          "        </tt:TextString>\r\n"
          "      </trt:OSD>\r\n"
          "    </trt:GetOSDResponse>\r\n"
          "  </SOAP-ENV:Body>\r\n"
          "</SOAP-ENV:Envelope>";

    const auto singleOsd = PelcoD::Onvif::OnvifClient::parseOsdResponse(singleOsdXml);
    EXPECT_TRUE(singleOsd.has_value());
    EXPECT_TRUE(singleOsd->token == "OSD_99");
    EXPECT_TRUE(singleOsd->position == PelcoD::Onvif::OsdPositionType::LowerRight);
    EXPECT_TRUE(singleOsd->plainText == "Perimeter North");

    // Test CreateOSD response parser
    const std::string createOsdXml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                     "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                     "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">\r\n"
                                     "  <SOAP-ENV:Body>\r\n"
                                     "    <trt:CreateOSDResponse>\r\n"
                                     "      <trt:OSDToken>OSD_CREATED_42</trt:OSDToken>\r\n"
                                     "    </trt:CreateOSDResponse>\r\n"
                                     "  </SOAP-ENV:Body>\r\n"
                                     "</SOAP-ENV:Envelope>";

    const auto createdToken = PelcoD::Onvif::OnvifClient::parseCreateOsdResponse(createOsdXml);
    EXPECT_TRUE(createdToken.has_value());
    EXPECT_TRUE(*createdToken == "OSD_CREATED_42");
}

TEST(PelcoDOnvifTest, DeviceUsersParsing)
{
    const std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                            "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                            "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                            "  <SOAP-ENV:Body>\r\n"
                            "    <tds:GetUsersResponse>\r\n"
                            "      <tds:User>\r\n"
                            "        <tt:Username>admin</tt:Username>\r\n"
                            "        <tt:Password></tt:Password>\r\n"
                            "        <tt:UserLevel>Administrator</tt:UserLevel>\r\n"
                            "      </tds:User>\r\n"
                            "      <tds:User>\r\n"
                            "        <tt:Username>operator1</tt:Username>\r\n"
                            "        <tt:Password>secretpass</tt:Password>\r\n"
                            "        <tt:UserLevel>Operator</tt:UserLevel>\r\n"
                            "      </tds:User>\r\n"
                            "      <tds:User>\r\n"
                            "        <tt:Username>guest</tt:Username>\r\n"
                            "        <tt:UserLevel>User</tt:UserLevel>\r\n"
                            "      </tds:User>\r\n"
                            "    </tds:GetUsersResponse>\r\n"
                            "  </SOAP-ENV:Body>\r\n"
                            "</SOAP-ENV:Envelope>";

    const auto users = PelcoD::Onvif::OnvifClient::parseUsersResponse(xml);
    EXPECT_TRUE(users.size() == 3);
    EXPECT_TRUE(users[0].username == "admin");
    EXPECT_TRUE(users[0].level == PelcoD::Onvif::OnvifUserLevel::Administrator);
    EXPECT_TRUE(users[1].username == "operator1");
    EXPECT_TRUE(users[1].password == "secretpass");
    EXPECT_TRUE(users[1].level == PelcoD::Onvif::OnvifUserLevel::Operator);
    EXPECT_TRUE(users[2].username == "guest");
    EXPECT_TRUE(users[2].level == PelcoD::Onvif::OnvifUserLevel::User);
}

TEST(PelcoDOnvifTest, DeviceNetworkInterfacesParsing)
{
    const std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                            "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                            "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                            "  <SOAP-ENV:Body>\r\n"
                            "    <tds:GetNetworkInterfacesResponse>\r\n"
                            "      <tds:NetworkInterfaces token=\"eth0\">\r\n"
                            "        <tt:Enabled>true</tt:Enabled>\r\n"
                            "        <tt:Info>\r\n"
                            "          <tt:Name>eth0</tt:Name>\r\n"
                            "          <tt:HwAddress>00:11:22:33:44:55</tt:HwAddress>\r\n"
                            "          <tt:MTU>1500</tt:MTU>\r\n"
                            "        </tt:Info>\r\n"
                            "        <tt:IPv4>\r\n"
                            "          <tt:Enabled>true</tt:Enabled>\r\n"
                            "          <tt:Config>\r\n"
                            "            <tt:Manual>\r\n"
                            "              <tt:Address>192.168.1.50</tt:Address>\r\n"
                            "              <tt:PrefixLength>24</tt:PrefixLength>\r\n"
                            "            </tt:Manual>\r\n"
                            "            <tt:DHCP>false</tt:DHCP>\r\n"
                            "          </tt:Config>\r\n"
                            "        </tt:IPv4>\r\n"
                            "      </tds:NetworkInterfaces>\r\n"
                            "    </tds:GetNetworkInterfacesResponse>\r\n"
                            "  </SOAP-ENV:Body>\r\n"
                            "</SOAP-ENV:Envelope>";

    const auto ifaces = PelcoD::Onvif::OnvifClient::parseNetworkInterfacesResponse(xml);
    EXPECT_TRUE(ifaces.size() == 1);
    EXPECT_TRUE(ifaces[0].token == "eth0");
    EXPECT_TRUE(ifaces[0].enabled == true);
    EXPECT_TRUE(ifaces[0].name == "eth0");
    EXPECT_TRUE(ifaces[0].hwAddress == "00:11:22:33:44:55");
    EXPECT_TRUE(ifaces[0].mtu == 1500);
    EXPECT_TRUE(ifaces[0].ipv4.enabled == true);
    EXPECT_TRUE(ifaces[0].ipv4.dhcp == false);
    EXPECT_TRUE(ifaces[0].ipv4.manualAddress == "192.168.1.50");
    EXPECT_TRUE(ifaces[0].ipv4.prefixLength == 24);
}

TEST(PelcoDOnvifTest, DeviceDnsNtpParsing)
{
    const std::string dnsXml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                               "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                               "  <SOAP-ENV:Body>\r\n"
                               "    <tds:GetDNSResponse>\r\n"
                               "      <tds:DNSInformation>\r\n"
                               "        <tt:FromDHCP>false</tt:FromDHCP>\r\n"
                               "        <tt:SearchDomain>lan</tt:SearchDomain>\r\n"
                               "        <tt:DNSManual>\r\n"
                               "          <tt:Type>IPv4</tt:Type>\r\n"
                               "          <tt:IPv4Address>8.8.8.8</tt:IPv4Address>\r\n"
                               "        </tt:DNSManual>\r\n"
                               "        <tt:DNSManual>\r\n"
                               "          <tt:Type>IPv4</tt:Type>\r\n"
                               "          <tt:IPv4Address>1.1.1.1</tt:IPv4Address>\r\n"
                               "        </tt:DNSManual>\r\n"
                               "      </tds:DNSInformation>\r\n"
                               "    </tds:GetDNSResponse>\r\n"
                               "  </SOAP-ENV:Body>\r\n"
                               "</SOAP-ENV:Envelope>";

    const auto dns = PelcoD::Onvif::OnvifClient::parseDnsResponse(dnsXml);
    EXPECT_TRUE(dns.has_value());
    EXPECT_TRUE(dns->fromDhcp == false);
    EXPECT_TRUE(dns->searchDomains.size() == 1 && dns->searchDomains[0] == "lan");
    EXPECT_TRUE(dns->dnsServers.size() == 2 && dns->dnsServers[0] == "8.8.8.8" && dns->dnsServers[1] == "1.1.1.1");

    const std::string ntpXml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                               "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                               "  <SOAP-ENV:Body>\r\n"
                               "    <tds:GetNTPResponse>\r\n"
                               "      <tds:NTPInformation>\r\n"
                               "        <tt:FromDHCP>true</tt:FromDHCP>\r\n"
                               "        <tt:NTPManual>\r\n"
                               "          <tt:Type>DNS</tt:Type>\r\n"
                               "          <tt:DNSname>time.google.com</tt:DNSname>\r\n"
                               "        </tt:NTPManual>\r\n"
                               "      </tds:NTPInformation>\r\n"
                               "    </tds:GetNTPResponse>\r\n"
                               "  </SOAP-ENV:Body>\r\n"
                               "</SOAP-ENV:Envelope>";

    const auto ntp = PelcoD::Onvif::OnvifClient::parseNtpResponse(ntpXml);
    EXPECT_TRUE(ntp.has_value());
    EXPECT_TRUE(ntp->fromDhcp == true);
    EXPECT_TRUE(ntp->manualServers.size() == 1 && ntp->manualServers[0] == "time.google.com");
}

TEST(PelcoDOnvifTest, DeviceGatewayAndHostnameParsing)
{
    const std::string gwXml
        = "<tds:GetNetworkDefaultGatewayResponse xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
          "xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
          "<tds:NetworkGateway><tt:IPv4Address>192.168.1.254</tt:IPv4Address></tds:NetworkGateway>"
          "</tds:GetNetworkDefaultGatewayResponse>";
    const std::string gw = PelcoD::Onvif::OnvifClient::parseNetworkDefaultGatewayResponse(gwXml);
    EXPECT_TRUE(gw == "192.168.1.254");

    const std::string hnXml = "<tds:GetHostnameResponse xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                              "xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
                              "<tds:HostnameInformation><tt:Name>CamFrontGate</tt:Name></tds:HostnameInformation>"
                              "</tds:GetHostnameResponse>";
    const std::string hn = PelcoD::Onvif::OnvifClient::parseHostnameResponse(hnXml);
    EXPECT_TRUE(hn == "CamFrontGate");

    const std::string scXml = "<tds:GetScopesResponse xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                              "xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
                              "<tds:Scopes><tt:ScopeItem>onvif://www.onvif.org/type/ptz</tt:ScopeItem></tds:Scopes>"
                              "<tds:Scopes><tt:ScopeItem>onvif://www.onvif.org/name/Cam1</tt:ScopeItem></tds:Scopes>"
                              "</tds:GetScopesResponse>";
    const auto scopes = PelcoD::Onvif::OnvifClient::parseScopesResponse(scXml);
    EXPECT_TRUE(scopes.size() == 2);
    EXPECT_TRUE(scopes[0] == "onvif://www.onvif.org/type/ptz");
    EXPECT_TRUE(scopes[1] == "onvif://www.onvif.org/name/Cam1");
}

TEST(PelcoDOnvifTest, FocusStatusParsing)
{
    const std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                            "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" "
                            "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                            "  <SOAP-ENV:Body>\r\n"
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

    const auto status = PelcoD::Onvif::OnvifClient::parseFocusStatusResponse(xml);
    EXPECT_TRUE(status.has_value());
    EXPECT_TRUE(std::abs(status->position - 0.75f) < 0.01f);
    EXPECT_TRUE(status->moveStatus == "IDLE");
}

TEST(PelcoDOnvifTest, ImagingPresetsParsing)
{
    const std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                            "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" "
                            "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                            "  <SOAP-ENV:Body>\r\n"
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

    const auto presets = PelcoD::Onvif::OnvifClient::parseImagingPresetsResponse(xml);
    EXPECT_TRUE(presets.size() == 2);
    EXPECT_TRUE(presets[0].token == "Preset_Day");
    EXPECT_TRUE(presets[0].name == "Day Outdoor");
    EXPECT_TRUE(presets[0].type == "Custom");
    EXPECT_TRUE(presets[1].token == "Preset_Night");
    EXPECT_TRUE(presets[1].name == "Night IR");
}

TEST(PelcoDOnvifTest, RelayOutputsParsing)
{
    const std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                            "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\" "
                            "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                            "  <SOAP-ENV:Body>\r\n"
                            "    <tmd:GetRelayOutputsResponse>\r\n"
                            "      <tmd:RelayOutputs token=\"Relay_1\">\r\n"
                            "        <tt:Properties>\r\n"
                            "          <tt:Mode>Bistable</tt:Mode>\r\n"
                            "          <tt:DelayTime>PT0S</tt:DelayTime>\r\n"
                            "          <tt:IdleState>open</tt:IdleState>\r\n"
                            "        </tt:Properties>\r\n"
                            "        <tt:LogicalState>active</tt:LogicalState>\r\n"
                            "      </tmd:RelayOutputs>\r\n"
                            "      <tmd:RelayOutputs token=\"Relay_2\">\r\n"
                            "        <tt:Properties>\r\n"
                            "          <tt:Mode>Monostable</tt:Mode>\r\n"
                            "          <tt:DelayTime>PT5S</tt:DelayTime>\r\n"
                            "          <tt:IdleState>closed</tt:IdleState>\r\n"
                            "        </tt:Properties>\r\n"
                            "        <tt:LogicalState>inactive</tt:LogicalState>\r\n"
                            "      </tmd:RelayOutputs>\r\n"
                            "    </tmd:GetRelayOutputsResponse>\r\n"
                            "  </SOAP-ENV:Body>\r\n"
                            "</SOAP-ENV:Envelope>";

    const auto relays = PelcoD::Onvif::OnvifClient::parseRelayOutputsResponse(xml);
    EXPECT_TRUE(relays.size() == 2);
    EXPECT_TRUE(relays[0].token == "Relay_1");
    EXPECT_TRUE(relays[0].mode == PelcoD::Onvif::RelayMode::Bistable);
    EXPECT_TRUE(relays[0].idleState == PelcoD::Onvif::RelayIdleState::Open);
    EXPECT_TRUE(relays[0].logicalState == PelcoD::Onvif::RelayLogicalState::Active);

    EXPECT_TRUE(relays[1].token == "Relay_2");
    EXPECT_TRUE(relays[1].mode == PelcoD::Onvif::RelayMode::Monostable);
    EXPECT_TRUE(std::abs(relays[1].delayTimeSeconds - 5.0f) < 0.01f);
    EXPECT_TRUE(relays[1].idleState == PelcoD::Onvif::RelayIdleState::Closed);
    EXPECT_TRUE(relays[1].logicalState == PelcoD::Onvif::RelayLogicalState::Inactive);
}

TEST(PelcoDOnvifTest, DigitalInputsParsing)
{
    const std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                            "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\" "
                            "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                            "  <SOAP-ENV:Body>\r\n"
                            "    <tmd:GetDigitalInputsResponse>\r\n"
                            "      <tmd:DigitalInputs token=\"Input_1\">\r\n"
                            "        <tt:IdleState>open</tt:IdleState>\r\n"
                            "      </tmd:DigitalInputs>\r\n"
                            "    </tmd:GetDigitalInputsResponse>\r\n"
                            "  </SOAP-ENV:Body>\r\n"
                            "</SOAP-ENV:Envelope>";

    const auto inputs = PelcoD::Onvif::OnvifClient::parseDigitalInputsResponse(xml);
    EXPECT_TRUE(inputs.size() == 1);
    EXPECT_TRUE(inputs[0].token == "Input_1");
    EXPECT_TRUE(inputs[0].idleState == PelcoD::Onvif::RelayIdleState::Open);
}

TEST(PelcoDOnvifTest, MetadataConfigurationsParsing)
{
    const std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                            "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
                            "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                            "  <SOAP-ENV:Body>\r\n"
                            "    <trt:GetMetadataConfigurationsResponse>\r\n"
                            "      <trt:Configurations token=\"Meta_1\">\r\n"
                            "        <tt:Name>MainMetadata</tt:Name>\r\n"
                            "        <tt:UseCount>2</tt:UseCount>\r\n"
                            "        <tt:PTZStatus>\r\n"
                            "          <tt:Status>true</tt:Status>\r\n"
                            "        </tt:PTZStatus>\r\n"
                            "        <tt:Analytics>true</tt:Analytics>\r\n"
                            "        <tt:Events>false</tt:Events>\r\n"
                            "      </trt:Configurations>\r\n"
                            "    </trt:GetMetadataConfigurationsResponse>\r\n"
                            "  </SOAP-ENV:Body>\r\n"
                            "</SOAP-ENV:Envelope>";

    const auto configs = PelcoD::Onvif::OnvifClient::parseMetadataConfigurationsResponse(xml);
    EXPECT_TRUE(configs.size() == 1);
    EXPECT_TRUE(configs[0].token == "Meta_1");
    EXPECT_TRUE(configs[0].name == "MainMetadata");
    EXPECT_TRUE(configs[0].useCount == 2);
    EXPECT_TRUE(configs[0].ptzStatusEnabled == true);
    EXPECT_TRUE(configs[0].analyticsEnabled == true);
    EXPECT_TRUE(configs[0].eventsEnabled == false);
}

TEST(PelcoDOnvifTest, MetadataConfigurationOptionsParsing)
{
    const std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                            "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
                            "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                            "  <SOAP-ENV:Body>\r\n"
                            "    <trt:GetMetadataConfigurationOptionsResponse>\r\n"
                            "      <trt:Options>\r\n"
                            "        <tt:PTZStatusSupported>true</tt:PTZStatusSupported>\r\n"
                            "        <tt:AnalyticsSupported>true</tt:AnalyticsSupported>\r\n"
                            "      </trt:Options>\r\n"
                            "    </trt:GetMetadataConfigurationOptionsResponse>\r\n"
                            "  </SOAP-ENV:Body>\r\n"
                            "</SOAP-ENV:Envelope>";

    const auto opts = PelcoD::Onvif::OnvifClient::parseMetadataConfigurationOptionsResponse(xml);
    EXPECT_TRUE(opts.has_value());
    EXPECT_TRUE(opts->ptzStatusSupported == true);
    EXPECT_TRUE(opts->analyticsSupported == true);
}

TEST(PelcoDOnvifTest, MetadataStreamParsing)
{
    const std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                            "<tt:MetadataStream xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                            "  <tt:PTZStatus>\r\n"
                            "    <tt:Position>\r\n"
                            "      <tt:PanTilt x=\"0.55\" y=\"-0.25\"/>\r\n"
                            "      <tt:Zoom x=\"0.75\"/>\r\n"
                            "    </tt:Position>\r\n"
                            "    <tt:MoveStatus>\r\n"
                            "      <tt:PanTilt>MOVING</tt:PanTilt>\r\n"
                            "    </tt:MoveStatus>\r\n"
                            "  </tt:PTZStatus>\r\n"
                            "  <tt:VideoAnalytics>\r\n"
                            "    <tt:Frame UtcTime=\"2026-09-17T12:00:00Z\">\r\n"
                            "      <tt:Object ObjectId=\"42\">\r\n"
                            "        <tt:Appearance>\r\n"
                            "          <tt:Class>\r\n"
                            "            <tt:Type>Car</tt:Type>\r\n"
                            "            <tt:Likelihood>0.92</tt:Likelihood>\r\n"
                            "          </tt:Class>\r\n"
                            "          <tt:Shape>\r\n"
                            "            <tt:BoundingBox left=\"0.1\" top=\"0.2\" right=\"0.4\" bottom=\"0.6\"/>\r\n"
                            "          </tt:Shape>\r\n"
                            "          <tt:GeoLocation lat=\"37.7749\" lon=\"-122.4194\" elevation=\"15.0\"/>\r\n"
                            "        </tt:Appearance>\r\n"
                            "      </tt:Object>\r\n"
                            "    </tt:Frame>\r\n"
                            "  </tt:VideoAnalytics>\r\n"
                            "</tt:MetadataStream>";

    const auto payload = PelcoD::Onvif::OnvifClient::parseMetadataStreamResponse(xml);
    EXPECT_TRUE(payload.has_value());
    EXPECT_TRUE(payload->ptzStatus.has_value());
    EXPECT_TRUE(std::abs(payload->ptzStatus->pan - 0.55) < 0.001);
    EXPECT_TRUE(std::abs(payload->ptzStatus->tilt - (-0.25)) < 0.001);
    EXPECT_TRUE(std::abs(payload->ptzStatus->zoom - 0.75) < 0.001);
    EXPECT_TRUE(payload->ptzStatus->isMoving == true);

    EXPECT_TRUE(payload->analyticsFrame.has_value());
    EXPECT_TRUE(payload->analyticsFrame->utcTime == "2026-09-17T12:00:00Z");
    EXPECT_TRUE(payload->analyticsFrame->objects.size() == 1);
    EXPECT_TRUE(payload->analyticsFrame->objects[0].objectId == 42);
    EXPECT_TRUE(payload->analyticsFrame->objects[0].className == "Car");
    EXPECT_TRUE(std::abs(static_cast<double>(payload->analyticsFrame->objects[0].confidence) - 0.92) < 0.01);
    EXPECT_TRUE(std::abs(static_cast<double>(payload->analyticsFrame->objects[0].boundingBox.left) - 0.1) < 0.01);
    EXPECT_TRUE(std::abs(payload->analyticsFrame->objects[0].geoLocation.latitude - 37.7749) < 0.001);
}

TEST(PelcoDOnvifTest, SystemLogsParsing)
{
    const std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                            "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                            "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                            "  <SOAP-ENV:Body>\r\n"
                            "    <tds:GetSystemLogResponse>\r\n"
                            "      <tds:SystemLog>\r\n"
                            "        <tt:String>2026-09-17 12:00:00 [INFO] System boot completed</tt:String>\r\n"
                            "      </tds:SystemLog>\r\n"
                            "    </tds:GetSystemLogResponse>\r\n"
                            "  </SOAP-ENV:Body>\r\n"
                            "</SOAP-ENV:Envelope>";

    const auto logData = PelcoD::Onvif::OnvifClient::parseSystemLogResponse(xml);
    EXPECT_TRUE(logData.has_value());
    EXPECT_TRUE(logData->find("System boot completed") != std::string::npos);
}

TEST(PelcoDOnvifTest, SystemSupportInfoAndBackupParsing)
{
    const std::string supportXml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                                   "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <tds:GetSystemSupportInformationResponse>\r\n"
                                   "      <tds:SupportInformation>\r\n"
                                   "        <tt:String>CPU: 12.5%, Mem: 128MB/512MB, Connections: 3</tt:String>\r\n"
                                   "      </tds:SupportInformation>\r\n"
                                   "    </tds:GetSystemSupportInformationResponse>\r\n"
                                   "  </SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";

    const auto info = PelcoD::Onvif::OnvifClient::parseSystemSupportInformationResponse(supportXml);
    EXPECT_TRUE(info.has_value());
    EXPECT_TRUE(info->rawDiagnostics.find("CPU: 12.5%") != std::string::npos);

    const std::string backupXml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                  "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                  "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                                  "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                  "  <SOAP-ENV:Body>\r\n"
                                  "    <tds:GetSystemBackupResponse>\r\n"
                                  "      <tds:BackupFiles>\r\n"
                                  "        <tt:Data>BASE64BACKUPDATA12345</tt:Data>\r\n"
                                  "      </tds:BackupFiles>\r\n"
                                  "    </tds:GetSystemBackupResponse>\r\n"
                                  "  </SOAP-ENV:Body>\r\n"
                                  "</SOAP-ENV:Envelope>";

    const auto backupData = PelcoD::Onvif::OnvifClient::parseSystemBackupResponse(backupXml);
    EXPECT_TRUE(backupData.has_value());
    EXPECT_TRUE(*backupData == "BASE64BACKUPDATA12345");

    const std::string epRefXml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                 "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                 "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                                 "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                 "  <SOAP-ENV:Body>\r\n"
                                 "    <tds:GetEndpointReferenceResponse>\r\n"
                                 "      <tds:GUID>urn:uuid:11223344-5566-7788-99aa-bbccddeeff00</tds:GUID>\r\n"
                                 "    </tds:GetEndpointReferenceResponse>\r\n"
                                 "  </SOAP-ENV:Body>\r\n"
                                 "</SOAP-ENV:Envelope>";

    const auto guid = PelcoD::Onvif::OnvifClient::parseEndpointReferenceResponse(epRefXml);
    EXPECT_TRUE(guid.has_value());
    EXPECT_TRUE(guid->find("11223344-5566-7788-99aa-bbccddeeff00") != std::string::npos);
}

TEST(PelcoDOnvifTest, PkiSecurityAndCrypto)
{
    // 1. Test generateSelfSignedCertificate
    const auto cert
        = PelcoD::Onvif::OnvifSecurity::generateSelfSignedCertificate("Cert_Test", "CN=TestCamera, O=Security", 365);
    EXPECT_TRUE(cert.certificateId == "Cert_Test");
    EXPECT_TRUE(!cert.x509DerBase64.empty());
    EXPECT_TRUE(cert.info.subject.find("TestCamera") != std::string::npos);
    EXPECT_TRUE(!cert.info.validNotBefore.empty());
    EXPECT_TRUE(!cert.info.validNotAfter.empty());

    // 2. Test parseCertificateInfo
    const auto info = PelcoD::Onvif::OnvifSecurity::parseCertificateInfo("Cert_Test", cert.x509DerBase64);
    EXPECT_TRUE(info.certificateId == "Cert_Test");
    EXPECT_TRUE(info.subject.find("TestCamera") != std::string::npos);

    // 3. Test generatePkcs10Csr
    const auto csr = PelcoD::Onvif::OnvifSecurity::generatePkcs10Csr("Cert_Test", "CN=TestCamera, O=Security");
    EXPECT_TRUE(csr.certificateId == "Cert_Test");
    EXPECT_TRUE(!csr.csrBase64.empty());
    EXPECT_TRUE(csr.subject.find("TestCamera") != std::string::npos);
}

TEST(PelcoDOnvifTest, PkiXmlParsing)
{
    // 1. GetCertificates response
    const std::string certsXml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                 "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                 "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                                 "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                 "  <SOAP-ENV:Body>\r\n"
                                 "    <tds:GetCertificatesResponse>\r\n"
                                 "      <tds:NvtCertificate>\r\n"
                                 "        <tt:CertificateID>Cert_Main</tt:CertificateID>\r\n"
                                 "        <tt:Certificate>\r\n"
                                 "          <tt:Data>MIIBjjCCATSgAwIBAgIU...</tt:Data>\r\n"
                                 "        </tt:Certificate>\r\n"
                                 "      </tds:NvtCertificate>\r\n"
                                 "    </tds:GetCertificatesResponse>\r\n"
                                 "  </SOAP-ENV:Body>\r\n"
                                 "</SOAP-ENV:Envelope>";

    const auto certList = PelcoD::Onvif::OnvifClient::parseCertificatesResponse(certsXml);
    EXPECT_TRUE(certList.size() == 1U);
    EXPECT_TRUE(certList[0].certificateId == "Cert_Main");
    EXPECT_TRUE(certList[0].x509DerBase64 == "MIIBjjCCATSgAwIBAgIU...");

    // 2. GetCertificateInformation response
    const std::string infoXml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                                "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:GetCertificateInformationResponse>\r\n"
                                "      <tds:CertificateInformation>\r\n"
                                "        <tt:CertificateID>Cert_Main</tt:CertificateID>\r\n"
                                "        <tt:IssuerDN>CN=RootCA</tt:IssuerDN>\r\n"
                                "        <tt:SubjectDN>CN=TestCamera</tt:SubjectDN>\r\n"
                                "        <tt:Validity>\r\n"
                                "          <tt:From>2026-01-01T00:00:00Z</tt:From>\r\n"
                                "          <tt:Until>2027-01-01T00:00:00Z</tt:Until>\r\n"
                                "        </tt:Validity>\r\n"
                                "        <tt:Extension>\r\n"
                                "          <tt:KeyUsage>Digital Signature</tt:KeyUsage>\r\n"
                                "        </tt:Extension>\r\n"
                                "      </tds:CertificateInformation>\r\n"
                                "    </tds:GetCertificateInformationResponse>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

    const auto infoOpt = PelcoD::Onvif::OnvifClient::parseCertificateInformationResponse(infoXml);
    EXPECT_TRUE(infoOpt.has_value());
    EXPECT_TRUE(infoOpt->certificateId == "Cert_Main");
    EXPECT_TRUE(infoOpt->issuer == "CN=RootCA");
    EXPECT_TRUE(infoOpt->subject == "CN=TestCamera");
    EXPECT_TRUE(infoOpt->validNotBefore == "2026-01-01T00:00:00Z");
    EXPECT_TRUE(infoOpt->validNotAfter == "2027-01-01T00:00:00Z");
    EXPECT_TRUE(infoOpt->keyAlgorithm == "Digital Signature");

    // 3. GetPkcs10Request response
    const std::string csrXml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                               "  <SOAP-ENV:Body>\r\n"
                               "    <tds:GetPkcs10RequestResponse>\r\n"
                               "      <tds:Pkcs10Request>MIICvDCCAaQCAQAw</tds:Pkcs10Request>\r\n"
                               "    </tds:GetPkcs10RequestResponse>\r\n"
                               "  </SOAP-ENV:Body>\r\n"
                               "</SOAP-ENV:Envelope>";

    const auto csrOpt = PelcoD::Onvif::OnvifClient::parsePkcs10RequestResponse(csrXml);
    EXPECT_TRUE(csrOpt.has_value());
    EXPECT_TRUE(csrOpt->csrBase64 == "MIICvDCCAaQCAQAw");

    // 4. ClientCertificateMode response
    const std::string modeXml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:GetClientCertificateModeResponse>\r\n"
                                "      <tds:ClientCertificateMode>Required</tds:ClientCertificateMode>\r\n"
                                "    </tds:GetClientCertificateModeResponse>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

    const auto modeOpt = PelcoD::Onvif::OnvifClient::parseClientCertificateModeResponse(modeXml);
    EXPECT_TRUE(modeOpt.has_value());
    EXPECT_TRUE(*modeOpt == PelcoD::Onvif::ClientCertificateMode::Required);
}

TEST(PelcoDOnvifTest, ProfileGRecordingXmlParsing)
{
    // 1. GetRecordings response
    const std::string recsXml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:trc=\"http://www.onvif.org/ver10/recording/wsdl\" "
                                "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <trc:GetRecordingsResponse>\r\n"
                                "      <trc:RecordingItem>\r\n"
                                "        <trc:RecordingToken>Rec_Main</trc:RecordingToken>\r\n"
                                "        <trc:Configuration>\r\n"
                                "          <tt:Source>\r\n"
                                "            <tt:SourceId>VideoSource_1</tt:SourceId>\r\n"
                                "          </tt:Source>\r\n"
                                "          <tt:Content>MainStream</tt:Content>\r\n"
                                "          <tt:MaximumRetentionTime>P30D</tt:MaximumRetentionTime>\r\n"
                                "        </trc:Configuration>\r\n"
                                "        <trc:Tracks>\r\n"
                                "          <trc:Track>\r\n"
                                "            <trc:TrackToken>Track_Video_1</trc:TrackToken>\r\n"
                                "            <trc:Configuration>\r\n"
                                "              <tt:TrackType>Video</tt:TrackType>\r\n"
                                "              <tt:Description>H264 Main Profile</tt:Description>\r\n"
                                "            </trc:Configuration>\r\n"
                                "          </trc:Track>\r\n"
                                "        </trc:Tracks>\r\n"
                                "      </trc:RecordingItem>\r\n"
                                "    </trc:GetRecordingsResponse>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

    const auto recs = PelcoD::Onvif::OnvifClient::parseRecordingsResponse(recsXml);
    EXPECT_TRUE(recs.size() == 1U);
    EXPECT_TRUE(recs[0].recordingToken == "Rec_Main");
    EXPECT_TRUE(recs[0].sourceToken == "VideoSource_1");
    EXPECT_TRUE(recs[0].content == "MainStream");
    EXPECT_TRUE(recs[0].maximumRetentionTime == "P30D");
    EXPECT_TRUE(recs[0].tracks.size() == 1U);
    EXPECT_TRUE(recs[0].tracks[0].trackToken == "Track_Video_1");
    EXPECT_TRUE(recs[0].tracks[0].trackType == PelcoD::Onvif::RecordingTrackType::Video);

    // 2. CreateRecording response
    const std::string createRecXml
        = "<trc:CreateRecordingResponse xmlns:trc=\"http://www.onvif.org/ver10/recording/wsdl\">"
          "<trc:RecordingToken>Rec_New_1</trc:RecordingToken>"
          "</trc:CreateRecordingResponse>";
    const auto createdRecTok = PelcoD::Onvif::OnvifClient::parseCreateRecordingResponse(createRecXml);
    EXPECT_TRUE(createdRecTok.has_value());
    EXPECT_TRUE(*createdRecTok == "Rec_New_1");

    // 3. GetRecordingJobs response
    const std::string jobsXml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:trc=\"http://www.onvif.org/ver10/recording/wsdl\" "
                                "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <trc:GetRecordingJobsResponse>\r\n"
                                "      <trc:JobItem>\r\n"
                                "        <trc:JobToken>Job_1</trc:JobToken>\r\n"
                                "        <trc:JobConfiguration>\r\n"
                                "          <tt:RecordingToken>Rec_Main</tt:RecordingToken>\r\n"
                                "          <tt:Mode>Active</tt:Mode>\r\n"
                                "          <tt:Priority>5</tt:Priority>\r\n"
                                "          <tt:Source>\r\n"
                                "            <tt:SourceToken>VideoSource_1</tt:SourceToken>\r\n"
                                "          </tt:Source>\r\n"
                                "        </trc:JobConfiguration>\r\n"
                                "      </trc:JobItem>\r\n"
                                "    </trc:GetRecordingJobsResponse>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

    const auto jobs = PelcoD::Onvif::OnvifClient::parseRecordingJobsResponse(jobsXml);
    EXPECT_TRUE(jobs.size() == 1U);
    EXPECT_TRUE(jobs[0].jobToken == "Job_1");
    EXPECT_TRUE(jobs[0].recordingToken == "Rec_Main");
    EXPECT_TRUE(jobs[0].mode == PelcoD::Onvif::RecordingJobMode::Active);
    EXPECT_TRUE(jobs[0].priority == 5);
    EXPECT_TRUE(jobs[0].sourceToken == "VideoSource_1");

    // 4. GetRecordingSummary response
    const std::string sumXml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:trc=\"http://www.onvif.org/ver10/recording/wsdl\" "
                               "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                               "  <SOAP-ENV:Body>\r\n"
                               "    <trc:GetRecordingSummaryResponse>\r\n"
                               "      <trc:Summary>\r\n"
                               "        <tt:DataFrom>2026-01-01T00:00:00Z</tt:DataFrom>\r\n"
                               "        <tt:DataUntil>2026-09-17T00:00:00Z</tt:DataUntil>\r\n"
                               "        <tt:NumberRecordings>3</tt:NumberRecordings>\r\n"
                               "      </trc:Summary>\r\n"
                               "    </trc:GetRecordingSummaryResponse>\r\n"
                               "  </SOAP-ENV:Body>\r\n"
                               "</SOAP-ENV:Envelope>";

    const auto sumOpt = PelcoD::Onvif::OnvifClient::parseRecordingSummaryResponse(sumXml);
    EXPECT_TRUE(sumOpt.has_value());
    EXPECT_TRUE(sumOpt->dataFrom == "2026-01-01T00:00:00Z");
    EXPECT_TRUE(sumOpt->dataUntil == "2026-09-17T00:00:00Z");
    EXPECT_TRUE(sumOpt->numberRecordings == 3);
}

TEST(PelcoDOnvifTest, ProfileGSearchAndReplayXmlParsing)
{
    // 1. FindRecordings response
    const std::string findRecXml = "<tse:FindRecordingsResponse xmlns:tse=\"http://www.onvif.org/ver10/search/wsdl\">"
                                   "<tse:SearchToken>Search_Rec_Session_1</tse:SearchToken>"
                                   "</tse:FindRecordingsResponse>";
    const auto searchTok = PelcoD::Onvif::OnvifClient::parseFindRecordingsResponse(findRecXml);
    EXPECT_TRUE(searchTok.has_value());
    EXPECT_TRUE(*searchTok == "Search_Rec_Session_1");

    // 2. GetRecordingSearchResults response
    const std::string searchResXml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                     "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                     "xmlns:tse=\"http://www.onvif.org/ver10/search/wsdl\" "
                                     "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                     "  <SOAP-ENV:Body>\r\n"
                                     "    <tse:GetRecordingSearchResultsResponse>\r\n"
                                     "      <tse:ResultList SearchState=\"Completed\">\r\n"
                                     "        <tse:RecordingInformation>\r\n"
                                     "          <tt:RecordingToken>Rec_Main</tt:RecordingToken>\r\n"
                                     "          <tt:TrackToken>Track_Video_1</tt:TrackToken>\r\n"
                                     "          <tt:EarliestRecording>2026-01-01T00:00:00Z</tt:EarliestRecording>\r\n"
                                     "          <tt:LatestRecording>2026-09-17T00:00:00Z</tt:LatestRecording>\r\n"
                                     "          <tt:SearchState>Completed</tt:SearchState>\r\n"
                                     "        </tse:RecordingInformation>\r\n"
                                     "      </tse:ResultList>\r\n"
                                     "    </tse:GetRecordingSearchResultsResponse>\r\n"
                                     "  </SOAP-ENV:Body>\r\n"
                                     "</SOAP-ENV:Envelope>";

    const auto searchResults = PelcoD::Onvif::OnvifClient::parseRecordingSearchResultsResponse(searchResXml);
    EXPECT_TRUE(searchResults.size() == 1U);
    EXPECT_TRUE(searchResults[0].recordingToken == "Rec_Main");
    EXPECT_TRUE(searchResults[0].trackToken == "Track_Video_1");
    EXPECT_TRUE(searchResults[0].earliestTime == "2026-01-01T00:00:00Z");
    EXPECT_TRUE(searchResults[0].latestTime == "2026-09-17T00:00:00Z");
    EXPECT_TRUE(searchResults[0].searchState == "Completed");

    // 3. GetEventSearchResults response
    const std::string eventResXml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:tse=\"http://www.onvif.org/ver10/search/wsdl\" "
                                    "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                    "  <SOAP-ENV:Body>\r\n"
                                    "    <tse:GetEventSearchResultsResponse>\r\n"
                                    "      <tse:ResultList SearchState=\"Completed\">\r\n"
                                    "        <tse:EventInformation>\r\n"
                                    "          <tt:RecordingToken>Rec_Main</tt:RecordingToken>\r\n"
                                    "          <tt:UtcTime>2026-09-17T12:00:00Z</tt:UtcTime>\r\n"
                                    "          <tt:Topic>tns1:VideoAnalytics/Motion</tt:Topic>\r\n"
                                    "          <tt:Source>VideoSource_1</tt:Source>\r\n"
                                    "          <tt:Data>State=true</tt:Data>\r\n"
                                    "        </tse:EventInformation>\r\n"
                                    "      </tse:ResultList>\r\n"
                                    "    </tse:GetEventSearchResultsResponse>\r\n"
                                    "  </SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";

    const auto eventResults = PelcoD::Onvif::OnvifClient::parseEventSearchResultsResponse(eventResXml);
    EXPECT_TRUE(eventResults.size() == 1U);
    EXPECT_TRUE(eventResults[0].recordingToken == "Rec_Main");
    EXPECT_TRUE(eventResults[0].eventTime == "2026-09-17T12:00:00Z");
    EXPECT_TRUE(eventResults[0].topic == "tns1:VideoAnalytics/Motion");
    EXPECT_TRUE(eventResults[0].source == "VideoSource_1");
    EXPECT_TRUE(eventResults[0].data == "State=true");

    // 4. GetReplayUri response
    const std::string replayUriXml = "<trp:GetReplayUriResponse xmlns:trp=\"http://www.onvif.org/ver10/replay/wsdl\">"
                                     "<trp:Uri>rtsp://192.168.1.50:8554/replay?recording=Rec_Main</trp:Uri>"
                                     "</trp:GetReplayUriResponse>";
    const auto replayUri = PelcoD::Onvif::OnvifClient::parseReplayUriResponse(replayUriXml);
    EXPECT_TRUE(replayUri.has_value());
    EXPECT_TRUE(*replayUri == "rtsp://192.168.1.50:8554/replay?recording=Rec_Main");

    // 5. GetReplayConfiguration response
    const std::string replayCfgXml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                     "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                     "xmlns:trp=\"http://www.onvif.org/ver10/replay/wsdl\" "
                                     "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                     "  <SOAP-ENV:Body>\r\n"
                                     "    <trp:GetReplayConfigurationResponse>\r\n"
                                     "      <trp:Configuration>\r\n"
                                     "        <tt:SessionTimeout>PT60S</tt:SessionTimeout>\r\n"
                                     "      </trp:Configuration>\r\n"
                                     "    </trp:GetReplayConfigurationResponse>\r\n"
                                     "  </SOAP-ENV:Body>\r\n"
                                     "</SOAP-ENV:Envelope>";

    const auto replayCfg = PelcoD::Onvif::OnvifClient::parseReplayConfigurationResponse(replayCfgXml);
    EXPECT_TRUE(replayCfg.has_value());
    EXPECT_TRUE(replayCfg->sessionTimeout == "PT60S");
}

TEST(PelcoDOnvifTest, VideoAnalyticsRulesAndModulesParsing)
{
    // 1. Parse GetSupportedRulesResponse
    const std::string suppRulesXml
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
          "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:tan=\"http://www.onvif.org/ver20/analytics/wsdl\" "
          "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <SOAP-ENV:Body>\r\n"
          "    <tan:GetSupportedRulesResponse>\r\n"
          "      <tan:SupportedRules>\r\n"
          "        <tan:RuleDescription Name=\"tt:LineDetector\">\r\n"
          "          <tan:Parameters>\r\n"
          "            <tt:SimpleItemDescription Name=\"Direction\" Type=\"xs:string\"/>\r\n"
          "            <tt:SimpleItemDescription Name=\"Classes\" Type=\"xs:string\"/>\r\n"
          "          </tan:Parameters>\r\n"
          "        </tan:RuleDescription>\r\n"
          "        <tan:RuleDescription Name=\"tt:FieldDetector\">\r\n"
          "          <tan:Parameters>\r\n"
          "            <tt:SimpleItemDescription Name=\"Field\" Type=\"xs:string\"/>\r\n"
          "          </tan:Parameters>\r\n"
          "        </tan:RuleDescription>\r\n"
          "      </tan:SupportedRules>\r\n"
          "    </tan:GetSupportedRulesResponse>\r\n"
          "  </SOAP-ENV:Body>\r\n"
          "</SOAP-ENV:Envelope>";

    const auto suppRules = PelcoD::Onvif::OnvifClient::parseSupportedRulesResponse(suppRulesXml);
    EXPECT_TRUE(suppRules.size() == 2U);
    EXPECT_TRUE(suppRules[0].ruleType == "tt:LineDetector");
    EXPECT_TRUE(suppRules[0].supportedParameters.size() == 2U);
    EXPECT_TRUE(suppRules[0].supportedParameters[0] == "Direction");
    EXPECT_TRUE(suppRules[0].supportedParameters[1] == "Classes");
    EXPECT_TRUE(suppRules[1].ruleType == "tt:FieldDetector");

    // 2. Parse GetRulesResponse
    const std::string rulesXml
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
          "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:tan=\"http://www.onvif.org/ver20/analytics/wsdl\" "
          "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <SOAP-ENV:Body>\r\n"
          "    <tan:GetRulesResponse>\r\n"
          "      <tan:Rule Name=\"PerimeterTripwire\" Type=\"tt:LineDetector\">\r\n"
          "        <tan:Parameters>\r\n"
          "          <tt:SimpleItem Name=\"Direction\" Value=\"LeftToRight\"/>\r\n"
          "          <tt:SimpleItem Name=\"Classes\" Value=\"Human,Vehicle\"/>\r\n"
          "          <tt:SimpleItem Name=\"MinConfidence\" Value=\"0.60\"/>\r\n"
          "          <tt:SimpleItem Name=\"Enabled\" Value=\"true\"/>\r\n"
          "          <tt:ElementItem Name=\"Segment\">\r\n"
          "            <tt:Point x=\"0.1000\" y=\"0.5000\"/>\r\n"
          "            <tt:Point x=\"0.9000\" y=\"0.5000\"/>\r\n"
          "          </tt:ElementItem>\r\n"
          "        </tan:Parameters>\r\n"
          "      </tan:Rule>\r\n"
          "      <tan:Rule Name=\"CourtyardLoiter\" Type=\"tt:LoiteringDetector\">\r\n"
          "        <tan:Parameters>\r\n"
          "          <tt:SimpleItem Name=\"DwellTime\" Value=\"10.50\"/>\r\n"
          "          <tt:SimpleItem Name=\"Classes\" Value=\"Human\"/>\r\n"
          "          <tt:SimpleItem Name=\"Enabled\" Value=\"true\"/>\r\n"
          "          <tt:ElementItem Name=\"Field\">\r\n"
          "            <tt:Polygon>\r\n"
          "              <tt:Point x=\"0.2000\" y=\"0.2000\"/>\r\n"
          "              <tt:Point x=\"0.8000\" y=\"0.2000\"/>\r\n"
          "              <tt:Point x=\"0.8000\" y=\"0.8000\"/>\r\n"
          "              <tt:Point x=\"0.2000\" y=\"0.8000\"/>\r\n"
          "            </tt:Polygon>\r\n"
          "          </tt:ElementItem>\r\n"
          "        </tan:Parameters>\r\n"
          "      </tan:Rule>\r\n"
          "    </tan:GetRulesResponse>\r\n"
          "  </SOAP-ENV:Body>\r\n"
          "</SOAP-ENV:Envelope>";

    const auto rules = PelcoD::Onvif::OnvifClient::parseRulesResponse(rulesXml);
    EXPECT_TRUE(rules.size() == 2U);

    // Rule 1: Tripwire
    EXPECT_TRUE(rules[0].name == "PerimeterTripwire");
    EXPECT_TRUE(rules[0].type == "tt:LineDetector");
    EXPECT_TRUE(rules[0].direction == "LeftToRight");
    EXPECT_TRUE(rules[0].objectClasses.size() == 2U);
    EXPECT_TRUE(rules[0].objectClasses[0] == "Human");
    EXPECT_TRUE(rules[0].objectClasses[1] == "Vehicle");
    EXPECT_TRUE(std::fabs(rules[0].minConfidence - 0.60f) < 0.01f);
    EXPECT_TRUE(rules[0].enabled == true);
    EXPECT_TRUE(std::fabs(rules[0].lineStart.x - 0.1000f) < 0.001f);
    EXPECT_TRUE(std::fabs(rules[0].lineStart.y - 0.5000f) < 0.001f);
    EXPECT_TRUE(std::fabs(rules[0].lineEnd.x - 0.9000f) < 0.001f);
    EXPECT_TRUE(std::fabs(rules[0].lineEnd.y - 0.5000f) < 0.001f);

    // Rule 2: Loitering
    EXPECT_TRUE(rules[1].name == "CourtyardLoiter");
    EXPECT_TRUE(rules[1].type == "tt:LoiteringDetector");
    EXPECT_TRUE(std::fabs(rules[1].dwellTimeSeconds - 10.50) < 0.01);
    EXPECT_TRUE(rules[1].objectClasses.size() == 1U);
    EXPECT_TRUE(rules[1].objectClasses[0] == "Human");
    EXPECT_TRUE(rules[1].polygon.size() == 4U);
    EXPECT_TRUE(std::fabs(rules[1].polygon[0].x - 0.2000f) < 0.001f);
    EXPECT_TRUE(std::fabs(rules[1].polygon[2].y - 0.8000f) < 0.001f);

    // 3. Parse GetSupportedAnalyticsModulesResponse
    const std::string suppModsXml
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
          "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:tan=\"http://www.onvif.org/ver20/analytics/wsdl\" "
          "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <SOAP-ENV:Body>\r\n"
          "    <tan:GetSupportedAnalyticsModulesResponse>\r\n"
          "      <tan:SupportedAnalyticsModules>\r\n"
          "        <tan:AnalyticsModuleDescription Name=\"tt:ObjectClassificationModule\">\r\n"
          "          <tan:Parameters>\r\n"
          "            <tt:SimpleItemDescription Name=\"Classes\" Type=\"xs:string\"/>\r\n"
          "          </tan:Parameters>\r\n"
          "        </tan:AnalyticsModuleDescription>\r\n"
          "      </tan:SupportedAnalyticsModules>\r\n"
          "    </tan:GetSupportedAnalyticsModulesResponse>\r\n"
          "  </SOAP-ENV:Body>\r\n"
          "</SOAP-ENV:Envelope>";

    const auto suppMods = PelcoD::Onvif::OnvifClient::parseSupportedAnalyticsModulesResponse(suppModsXml);
    EXPECT_TRUE(suppMods.size() == 1U);
    EXPECT_TRUE(suppMods[0].moduleType == "tt:ObjectClassificationModule");
    EXPECT_TRUE(suppMods[0].supportedParameters.size() == 1U);
    EXPECT_TRUE(suppMods[0].supportedParameters[0] == "Classes");

    // 4. Parse GetAnalyticsModulesResponse
    const std::string modsXml
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
          "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:tan=\"http://www.onvif.org/ver20/analytics/wsdl\" "
          "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <SOAP-ENV:Body>\r\n"
          "    <tan:GetAnalyticsModulesResponse>\r\n"
          "      <tan:AnalyticsModule Name=\"Classifier_1\" Type=\"tt:ObjectClassificationModule\">\r\n"
          "        <tan:Parameters>\r\n"
          "          <tt:SimpleItem Name=\"Classes\" Value=\"Human,Vehicle,TwoWheeler\"/>\r\n"
          "          <tt:SimpleItem Name=\"MinConfidence\" Value=\"0.75\"/>\r\n"
          "        </tan:Parameters>\r\n"
          "      </tan:AnalyticsModule>\r\n"
          "    </tan:GetAnalyticsModulesResponse>\r\n"
          "  </SOAP-ENV:Body>\r\n"
          "</SOAP-ENV:Envelope>";

    const auto mods = PelcoD::Onvif::OnvifClient::parseAnalyticsModulesResponse(modsXml);
    EXPECT_TRUE(mods.size() == 1U);
    EXPECT_TRUE(mods[0].name == "Classifier_1");
    EXPECT_TRUE(mods[0].type == "tt:ObjectClassificationModule");
    EXPECT_TRUE(mods[0].parameters.at("Classes") == "Human,Vehicle,TwoWheeler");
    EXPECT_TRUE(mods[0].parameters.at("MinConfidence") == "0.75");
}

TEST(PelcoDOnvifTest, GeodesyAndGeoMoveParsing)
{
    using namespace PelcoD::Onvif;

    // 1. Test Geodesy Azimuth / Elevation calculation
    // Camera at equator/prime meridian (0, 0, 0), Target slightly north (0.01, 0, 0)
    GeoLocation camLoc {0.0, 0.0, 0.0};
    GeoOrientation camOri {0.0, 0.0, 0.0}; // Pointing true North
    GeoLocation targetNorth {0.01, 0.0, 0.0}; // ~1111 meters North

    double pan = 0.0;
    double tilt = 0.0;
    double slant = 0.0;
    bool ok = Geodesy::computeTargetAzimuthElevation(camLoc, camOri, targetNorth, pan, tilt, slant);
    EXPECT_TRUE(ok);
    // Bearing should be ~0 deg (North), slant range ~1111 m
    EXPECT_TRUE(pan >= 359.9 || pan <= 0.1);
    EXPECT_TRUE(std::fabs(slant - 1113.0) < 50.0);
    EXPECT_TRUE(std::fabs(tilt) < 1.0);

    // Target directly East (0, 0.01, 0)
    GeoLocation targetEast {0.0, 0.01, 0.0};
    ok = Geodesy::computeTargetAzimuthElevation(camLoc, camOri, targetEast, pan, tilt, slant);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(std::fabs(pan - 90.0) < 0.5);

    // Camera with yaw = 90.0 (camera base mounted pointing East)
    // Target North should now have relative azimuth of 270 degrees (counter-clockwise or 360 - 90)
    GeoOrientation camOriEast {90.0, 0.0, 0.0};
    ok = Geodesy::computeTargetAzimuthElevation(camLoc, camOriEast, targetNorth, pan, tilt, slant);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(std::fabs(pan - 270.0) < 0.5);

    // Camera at 100m elevation, target at 0m elevation directly 100m away ground distance
    // ground distance = 100m => lat ~ 100 / 111319.5 ~ 0.0008983 deg
    GeoLocation camHigh {0.0, 0.0, 100.0};
    GeoLocation targetGround {0.0008983, 0.0, 0.0};
    ok = Geodesy::computeTargetAzimuthElevation(camHigh, camOri, targetGround, pan, tilt, slant);
    EXPECT_TRUE(ok);
    // Elevation should be approximately -45 degrees (downward)
    EXPECT_TRUE(tilt < -40.0 && tilt > -50.0);

    // 2. Test computeZoomFromTargetArea
    const double zoomVal = Geodesy::computeZoomFromTargetArea(10.0, 100.0, 60.0, 2.0);
    EXPECT_TRUE(zoomVal > 0.8 && zoomVal <= 1.0);

    const double zoomWide = Geodesy::computeZoomFromTargetArea(100.0, 10.0, 60.0, 2.0);
    EXPECT_TRUE(zoomWide == 0.0); // Clamped to 0.0

    // 3. Test anglesToPelcoCentidegrees
    std::uint16_t panCdeg = 0;
    std::uint16_t tiltCdeg = 0;
    Geodesy::anglesToPelcoCentidegrees(0.0, 0.0, panCdeg, tiltCdeg);
    EXPECT_TRUE(panCdeg == 0);
    EXPECT_TRUE(tiltCdeg == 0);

    Geodesy::anglesToPelcoCentidegrees(90.0, 45.0, panCdeg, tiltCdeg);
    EXPECT_TRUE(panCdeg == 9000);
    EXPECT_TRUE(tiltCdeg == 4500);

    Geodesy::anglesToPelcoCentidegrees(359.5, -10.0, panCdeg, tiltCdeg);
    EXPECT_TRUE(panCdeg == 35950);
    EXPECT_TRUE(tiltCdeg == 35000); // 360 - 10 = 350 deg = 35000 centidegrees

    // 4. Test parseGetGeoLocationResponse
    const std::string geoXml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                               "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                               "  <SOAP-ENV:Body>\r\n"
                               "    <tds:GetGeoLocationResponse>\r\n"
                               "      <tds:Location Entity=\"Device\" Fixed=\"true\">\r\n"
                               "        <tt:GeoLocation lat=\"37.9838\" lon=\"23.7275\" elevation=\"150.5\"/>\r\n"
                               "        <tt:GeoOrientation yaw=\"45.0\" pitch=\"-5.0\" roll=\"0.0\"/>\r\n"
                               "      </tds:Location>\r\n"
                               "    </tds:GetGeoLocationResponse>\r\n"
                               "  </SOAP-ENV:Body>\r\n"
                               "</SOAP-ENV:Envelope>";

    const auto locEntity = OnvifClient::parseGetGeoLocationResponse(geoXml);
    EXPECT_TRUE(locEntity.has_value());
    EXPECT_TRUE(locEntity->entity == "Device");
    EXPECT_TRUE(locEntity->fixed == true);
    EXPECT_TRUE(std::fabs(locEntity->location.latitude - 37.9838) < 0.0001);
    EXPECT_TRUE(std::fabs(locEntity->location.longitude - 23.7275) < 0.0001);
    EXPECT_TRUE(std::fabs(locEntity->location.elevation - 150.5) < 0.01);
    EXPECT_TRUE(std::fabs(locEntity->orientation.yaw - 45.0) < 0.01);
    EXPECT_TRUE(std::fabs(locEntity->orientation.pitch - -5.0) < 0.01);
    EXPECT_TRUE(std::fabs(locEntity->orientation.roll - 0.0) < 0.01);
}

TEST(PelcoDOnvifTest, PrivacyMasksAndVideoSourceModesParsing)
{
    using namespace PelcoD::Onvif;

    // 1. Enum string conversions
    EXPECT_TRUE(maskTypeToString(MaskType::Color) == "Color");
    EXPECT_TRUE(maskTypeToString(MaskType::Pixelated) == "Pixelated");
    EXPECT_TRUE(maskTypeToString(MaskType::Blurred) == "Blurred");
    EXPECT_TRUE(stringToMaskType("Color") == MaskType::Color);
    EXPECT_TRUE(stringToMaskType("Pixelated") == MaskType::Pixelated);
    EXPECT_TRUE(stringToMaskType("Blurred") == MaskType::Blurred);
    EXPECT_TRUE(stringToMaskType("Unknown") == MaskType::Color);

    // 2. parseMaskOptionsResponse
    {
        const std::string xml =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\n"
            "  <soap:Body>\n"
            "    <tr2:GetMaskOptionsResponse>\n"
            "      <tr2:Options Rectangle=\"true\" Polygon=\"false\" SingleRule=\"false\">\n"
            "        <tr2:MaxMasks>8</tr2:MaxMasks>\n"
            "        <tr2:MaxPoints>4</tr2:MaxPoints>\n"
            "        <tr2:Types>Color</tr2:Types>\n"
            "        <tr2:Types>Pixelated</tr2:Types>\n"
            "        <tr2:Types>Blurred</tr2:Types>\n"
            "        <tr2:Color Supported=\"true\"/>\n"
            "      </tr2:Options>\n"
            "    </tr2:GetMaskOptionsResponse>\n"
            "  </soap:Body>\n"
            "</soap:Envelope>";

        const auto opt = OnvifClient::parseMaskOptionsResponse(xml);
        EXPECT_TRUE(opt.has_value());
        EXPECT_TRUE(opt->maxMasks == 8);
        EXPECT_TRUE(opt->maxPoints == 4);
        EXPECT_TRUE(opt->rectangleSupported == true);
        EXPECT_TRUE(opt->polygonSupported == false);
        EXPECT_TRUE(opt->supportedTypes.size() == 3U);
        EXPECT_TRUE(opt->supportedTypes[0] == MaskType::Color);
        EXPECT_TRUE(opt->supportedTypes[1] == MaskType::Pixelated);
        EXPECT_TRUE(opt->supportedTypes[2] == MaskType::Blurred);
    }

    // 3. parseMasksResponse
    {
        const std::string xml =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\n"
            "  <soap:Body>\n"
            "    <tr2:GetMasksResponse>\n"
            "      <tr2:Mask token=\"Mask_1\" Enabled=\"true\" Type=\"Color\">\n"
            "        <tr2:Configuration token=\"Mask_1\">\n"
            "          <tr2:Polygon>\n"
            "            <tr2:Point x=\"0.1\" y=\"0.2\"/>\n"
            "            <tr2:Point x=\"0.5\" y=\"0.2\"/>\n"
            "            <tr2:Point x=\"0.5\" y=\"0.6\"/>\n"
            "            <tr2:Point x=\"0.1\" y=\"0.6\"/>\n"
            "          </tr2:Polygon>\n"
            "          <tr2:Color X=\"128\" Y=\"64\" Z=\"32\"/>\n"
            "        </tr2:Configuration>\n"
            "      </tr2:Mask>\n"
            "      <tr2:Mask token=\"Mask_2\" Enabled=\"false\" Type=\"Blurred\">\n"
            "        <tr2:Configuration token=\"Mask_2\">\n"
            "          <tr2:Polygon>\n"
            "            <tr2:Point x=\"0.6\" y=\"0.6\"/>\n"
            "            <tr2:Point x=\"0.9\" y=\"0.9\"/>\n"
            "          </tr2:Polygon>\n"
            "        </tr2:Configuration>\n"
            "      </tr2:Mask>\n"
            "    </tr2:GetMasksResponse>\n"
            "  </soap:Body>\n"
            "</soap:Envelope>";

        const auto masks = OnvifClient::parseMasksResponse(xml);
        EXPECT_TRUE(masks.size() == 2U);
        EXPECT_TRUE(masks[0].token == "Mask_1");
        EXPECT_TRUE(masks[0].enabled == true);
        EXPECT_TRUE(masks[0].type == MaskType::Color);
        EXPECT_TRUE(masks[0].polygon.size() == 4U);
        EXPECT_TRUE(std::fabs(masks[0].polygon[0].x - 0.1f) < 0.001f);
        EXPECT_TRUE(std::fabs(masks[0].polygon[0].y - 0.2f) < 0.001f);
        EXPECT_TRUE(masks[0].color.x == 128);
        EXPECT_TRUE(masks[0].color.y == 64);
        EXPECT_TRUE(masks[0].color.z == 32);

        EXPECT_TRUE(masks[1].token == "Mask_2");
        EXPECT_TRUE(masks[1].enabled == false);
        EXPECT_TRUE(masks[1].type == MaskType::Blurred);
        EXPECT_TRUE(masks[1].polygon.size() == 2U);
    }

    // 4. parseMaskResponse
    {
        const std::string xml =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\n"
            "  <soap:Body>\n"
            "    <tr2:GetMaskResponse>\n"
            "      <tr2:Mask token=\"Mask_Single\" Enabled=\"true\" Type=\"Pixelated\">\n"
            "        <tr2:Configuration token=\"Mask_Single\">\n"
            "          <tr2:Polygon>\n"
            "            <tr2:Point x=\"0.2\" y=\"0.2\"/>\n"
            "            <tr2:Point x=\"0.4\" y=\"0.4\"/>\n"
            "          </tr2:Polygon>\n"
            "        </tr2:Configuration>\n"
            "      </tr2:Mask>\n"
            "    </tr2:GetMaskResponse>\n"
            "  </soap:Body>\n"
            "</soap:Envelope>";

        const auto mask = OnvifClient::parseMaskResponse(xml);
        EXPECT_TRUE(mask.has_value());
        EXPECT_TRUE(mask->token == "Mask_Single");
        EXPECT_TRUE(mask->enabled == true);
        EXPECT_TRUE(mask->type == MaskType::Pixelated);
        EXPECT_TRUE(mask->polygon.size() == 2U);
    }

    // 5. parseCreateMaskResponse
    {
        const std::string xml =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\n"
            "  <soap:Body>\n"
            "    <tr2:CreateMaskResponse>\n"
            "      <tr2:Token>Created_Mask_999</tr2:Token>\n"
            "    </tr2:CreateMaskResponse>\n"
            "  </soap:Body>\n"
            "</soap:Envelope>";

        const auto token = OnvifClient::parseCreateMaskResponse(xml);
        EXPECT_TRUE(token.has_value());
        EXPECT_TRUE(*token == "Created_Mask_999");
    }

    // 6. parseVideoSourceModesResponse
    {
        const std::string xml =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\n"
            "  <soap:Body>\n"
            "    <tr2:GetVideoSourceModesResponse>\n"
            "      <tr2:VideoSourceModes token=\"Mode_1080p60\" Enabled=\"true\">\n"
            "        <tr2:MaxFramerate>60.0</tr2:MaxFramerate>\n"
            "        <tr2:MaxResolution Width=\"1920\" Height=\"1080\"/>\n"
            "        <tr2:Encodings>H264 H265</tr2:Encodings>\n"
            "        <tr2:Description>1080p 60fps Mode</tr2:Description>\n"
            "      </tr2:VideoSourceModes>\n"
            "      <tr2:VideoSourceModes token=\"Mode_4k30\" Enabled=\"false\">\n"
            "        <tr2:MaxFramerate>30.0</tr2:MaxFramerate>\n"
            "        <tr2:MaxResolution Width=\"3840\" Height=\"2160\"/>\n"
            "        <tr2:Encodings>H265</tr2:Encodings>\n"
            "        <tr2:Description>4K 30fps Mode</tr2:Description>\n"
            "      </tr2:VideoSourceModes>\n"
            "    </tr2:GetVideoSourceModesResponse>\n"
            "  </soap:Body>\n"
            "</soap:Envelope>";

        const auto modes = OnvifClient::parseVideoSourceModesResponse(xml);
        EXPECT_TRUE(modes.size() == 2U);
        EXPECT_TRUE(modes[0].token == "Mode_1080p60");
        EXPECT_TRUE(modes[0].enabled == true);
        EXPECT_TRUE(std::fabs(modes[0].maxFramerate - 60.0f) < 0.001f);
        EXPECT_TRUE(modes[0].width == 1920);
        EXPECT_TRUE(modes[0].height == 1080);
        EXPECT_TRUE(modes[0].encodings.size() == 2U);
        EXPECT_TRUE(modes[0].encodings[0] == "H264");
        EXPECT_TRUE(modes[0].encodings[1] == "H265");
        EXPECT_TRUE(modes[0].description == "1080p 60fps Mode");

        EXPECT_TRUE(modes[1].token == "Mode_4k30");
        EXPECT_TRUE(modes[1].enabled == false);
        EXPECT_TRUE(modes[1].width == 3840);
        EXPECT_TRUE(modes[1].height == 2160);
        EXPECT_TRUE(std::fabs(modes[1].maxFramerate - 30.0f) < 0.001f);
        EXPECT_TRUE(modes[1].encodings.size() == 1U);
        EXPECT_TRUE(modes[1].encodings[0] == "H265");
        EXPECT_TRUE(modes[1].description == "4K 30fps Mode");
    }

    // 7. parseSetVideoSourceModeResponse
    {
        const std::string xmlWithReboot =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\n"
            "  <soap:Body>\n"
            "    <tr2:SetVideoSourceModeResponse>\n"
            "      <tr2:Reboot>true</tr2:Reboot>\n"
            "    </tr2:SetVideoSourceModeResponse>\n"
            "  </soap:Body>\n"
            "</soap:Envelope>";

        const auto reboot1 = OnvifClient::parseSetVideoSourceModeResponse(xmlWithReboot);
        EXPECT_TRUE(reboot1.has_value());
        EXPECT_TRUE(*reboot1 == true);

        const std::string xmlNoReboot =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\n"
            "  <soap:Body>\n"
            "    <tr2:SetVideoSourceModeResponse>\n"
            "      <tr2:Reboot>false</tr2:Reboot>\n"
            "    </tr2:SetVideoSourceModeResponse>\n"
            "  </soap:Body>\n"
            "</soap:Envelope>";

        const auto reboot2 = OnvifClient::parseSetVideoSourceModeResponse(xmlNoReboot);
        EXPECT_TRUE(reboot2.has_value());
        EXPECT_TRUE(*reboot2 == false);
    }
}

TEST(PelcoDOnvifTest, ThermalAndRadiometryParsing)
{
    using namespace PelcoD::Onvif;

    // 1. parseCapabilitiesResponse with Thermal extension
    {
        const std::string xml =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">\n"
            "  <soap:Body>\n"
            "    <tt:GetCapabilitiesResponse>\n"
            "      <tt:Capabilities>\n"
            "        <tt:Extension>\n"
            "          <tt:Thermal>\n"
            "            <tt:XAddr>http://192.168.1.100/onvif/thermal_service</tt:XAddr>\n"
            "          </tt:Thermal>\n"
            "        </tt:Extension>\n"
            "      </tt:Capabilities>\n"
            "    </tt:GetCapabilitiesResponse>\n"
            "  </soap:Body>\n"
            "</soap:Envelope>";

        const auto caps = OnvifClient::parseCapabilitiesResponse(xml);
        EXPECT_TRUE(caps.has_value());
        EXPECT_TRUE(caps->thermalXAddr == "http://192.168.1.100/onvif/thermal_service");
    }

    // 2. parseRadiometryConfigurationResponse
    {
        const std::string xml =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tth=\"http://www.onvif.org/ver10/thermal/wsdl\">\n"
            "  <soap:Body>\n"
            "    <tth:GetRadiometryConfigurationResponse>\n"
            "      <tth:Configuration>\n"
            "        <tth:Emissivity>0.92</tth:Emissivity>\n"
            "        <tth:Distance>7.5</tth:Distance>\n"
            "        <tth:ReflectedTemperature>22.0</tth:ReflectedTemperature>\n"
            "        <tth:AtmosphericTemperature>21.5</tth:AtmosphericTemperature>\n"
            "        <tth:RelativeHumidity>45.0</tth:RelativeHumidity>\n"
            "        <tth:WindowTransmission>0.98</tth:WindowTransmission>\n"
            "      </tth:Configuration>\n"
            "    </tth:GetRadiometryConfigurationResponse>\n"
            "  </soap:Body>\n"
            "</soap:Envelope>";

        const auto cfg = OnvifClient::parseRadiometryConfigurationResponse(xml);
        EXPECT_TRUE(cfg.has_value());
        EXPECT_TRUE(std::fabs(cfg->emissivity - 0.92f) < 0.001f);
        EXPECT_TRUE(std::fabs(cfg->distance - 7.5f) < 0.001f);
        EXPECT_TRUE(std::fabs(cfg->reflectedTemperature - 22.0f) < 0.001f);
        EXPECT_TRUE(std::fabs(cfg->atmosphericTemperature - 21.5f) < 0.001f);
        EXPECT_TRUE(std::fabs(cfg->relativeHumidity - 45.0f) < 0.001f);
        EXPECT_TRUE(std::fabs(cfg->windowTransmission - 0.98f) < 0.001f);
    }

    // 3. parseRadiometrySpotsResponse
    {
        const std::string xml =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tth=\"http://www.onvif.org/ver10/thermal/wsdl\">\n"
            "  <soap:Body>\n"
            "    <tth:GetRadiometrySpotsResponse>\n"
            "      <tth:Spot token=\"Spot_1\">\n"
            "        <tth:Position x=\"0.35\" y=\"0.45\"/>\n"
            "        <tth:Label>Target Spot</tth:Label>\n"
            "        <tth:Temperature>37.2</tth:Temperature>\n"
            "      </tth:Spot>\n"
            "    </tth:GetRadiometrySpotsResponse>\n"
            "  </soap:Body>\n"
            "</soap:Envelope>";

        const auto spots = OnvifClient::parseRadiometrySpotsResponse(xml);
        EXPECT_TRUE(spots.size() == 1U);
        EXPECT_TRUE(spots[0].token == "Spot_1");
        EXPECT_TRUE(spots[0].label == "Target Spot");
        EXPECT_TRUE(std::fabs(spots[0].position.x - 0.35f) < 0.001f);
        EXPECT_TRUE(std::fabs(spots[0].position.y - 0.45f) < 0.001f);
        EXPECT_TRUE(std::fabs(spots[0].temperature - 37.2f) < 0.001f);
    }

    // 4. parseRadiometryBoxesResponse
    {
        const std::string xml =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tth=\"http://www.onvif.org/ver10/thermal/wsdl\">\n"
            "  <soap:Body>\n"
            "    <tth:GetRadiometryBoxesResponse>\n"
            "      <tth:Box token=\"Box_1\">\n"
            "        <tth:TopLeft x=\"0.2\" y=\"0.3\"/>\n"
            "        <tth:BottomRight x=\"0.6\" y=\"0.8\"/>\n"
            "        <tth:Label>Engine Zone</tth:Label>\n"
            "        <tth:MinTemperature>25.0</tth:MinTemperature>\n"
            "        <tth:MaxTemperature>85.4</tth:MaxTemperature>\n"
            "        <tth:AvgTemperature>54.2</tth:AvgTemperature>\n"
            "      </tth:Box>\n"
            "    </tth:GetRadiometryBoxesResponse>\n"
            "  </soap:Body>\n"
            "</soap:Envelope>";

        const auto boxes = OnvifClient::parseRadiometryBoxesResponse(xml);
        EXPECT_TRUE(boxes.size() == 1U);
        EXPECT_TRUE(boxes[0].token == "Box_1");
        EXPECT_TRUE(boxes[0].label == "Engine Zone");
        EXPECT_TRUE(std::fabs(boxes[0].topLeft.x - 0.2f) < 0.001f);
        EXPECT_TRUE(std::fabs(boxes[0].topLeft.y - 0.3f) < 0.001f);
        EXPECT_TRUE(std::fabs(boxes[0].bottomRight.x - 0.6f) < 0.001f);
        EXPECT_TRUE(std::fabs(boxes[0].bottomRight.y - 0.8f) < 0.001f);
        EXPECT_TRUE(std::fabs(boxes[0].minTemperature - 25.0f) < 0.001f);
        EXPECT_TRUE(std::fabs(boxes[0].maxTemperature - 85.4f) < 0.001f);
        EXPECT_TRUE(std::fabs(boxes[0].avgTemperature - 54.2f) < 0.001f);
    }

    // 5. parseColorPalettesResponse
    {
        const std::string xml =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tth=\"http://www.onvif.org/ver10/thermal/wsdl\">\n"
            "  <soap:Body>\n"
            "    <tth:GetColorPalettesResponse>\n"
            "      <tth:ColorPalette token=\"Ironbow\">\n"
            "        <tth:Name>Ironbow</tth:Name>\n"
            "      </tth:ColorPalette>\n"
            "      <tth:ColorPalette token=\"WhiteHot\">\n"
            "        <tth:Name>White Hot</tth:Name>\n"
            "      </tth:ColorPalette>\n"
            "    </tth:GetColorPalettesResponse>\n"
            "  </soap:Body>\n"
            "</soap:Envelope>";

        const auto palettes = OnvifClient::parseColorPalettesResponse(xml);
        EXPECT_TRUE(palettes.size() == 2U);
        EXPECT_TRUE(palettes[0].token == "Ironbow");
        EXPECT_TRUE(palettes[0].name == "Ironbow");
        EXPECT_TRUE(palettes[1].token == "WhiteHot");
        EXPECT_TRUE(palettes[1].name == "White Hot");
    }
}
