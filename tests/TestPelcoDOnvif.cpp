/// @file TestPelcoDOnvif.cpp
/// @brief Comprehensive unit tests for ONVIF Profile S client, discovery, security, and PTZ controls.

#include "PelcoDOnvif/OnvifClient.h"
#include "PelcoDOnvif/OnvifDiscovery.h"
#include "PelcoDOnvif/OnvifSecurity.h"
#include "PelcoDOnvif/OnvifTypes.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <crtdbg.h>
#include <cstdlib>
#endif

void testSecurityHeaderGeneration()
{
    // 1. Empty credentials should yield empty security header
    {
        PelcoD::Onvif::SecurityCredentials creds {};
        const std::string header = PelcoD::Onvif::OnvifSecurity::buildSoapSecurityHeader(creds);
        assert(header.empty());
    }

    // 2. Populated credentials should produce valid WS-Security UsernameToken header
    {
        PelcoD::Onvif::SecurityCredentials creds {};
        creds.username = "admin";
        creds.password = "secretPass123";

        const std::string header = PelcoD::Onvif::OnvifSecurity::buildSoapSecurityHeader(creds);
        assert(!header.empty());
        assert(header.find("wsse:Security") != std::string::npos);
        assert(header.find("wsse:UsernameToken") != std::string::npos);
        assert(header.find("<wsse:Username>admin</wsse:Username>") != std::string::npos);
        assert(header.find("PasswordDigest") != std::string::npos);
        assert(header.find("wsse:Nonce") != std::string::npos);
        assert(header.find("wsu:Created") != std::string::npos);
    }
}

void testDiscoveryProbeGenerationAndParsing()
{
    // 1. Probe payload generation
    const std::string probe = PelcoD::Onvif::OnvifDiscovery::createProbePayload("test-uuid-1234");
    assert(probe.find(":Action>") != std::string::npos);
    assert(probe.find("http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe") != std::string::npos);
    assert(probe.find("test-uuid-1234") != std::string::npos);
    assert(probe.find("dn:NetworkVideoTransmitter") != std::string::npos);

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
    assert(devices.size() == 2U);

    // Verify Device 1
    assert(devices[0].endpoint == "http://192.168.1.50/onvif/device_service");
    assert(devices[0].ip == "192.168.1.50");
    assert(devices[0].hardware == "Spectra_IV");
    assert(devices[0].name == "NorthGateCamera");
    assert(devices[0].location == "Building_A");
    assert(devices[0].scopes.size() == 5U);

    // Verify Device 2
    assert(devices[1].endpoint == "http://192.168.1.60:8080/onvif/device_service");
    assert(devices[1].ip == "192.168.1.60");
    assert(devices[1].hardware == "SX800");
    assert(devices[1].name == "PerimeterCam");
}

void testCapabilitiesParsing()
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
    assert(caps.has_value());
    assert(caps->deviceXAddr == "http://192.168.1.100/onvif/device_service");
    assert(caps->mediaXAddr == "http://192.168.1.100/onvif/media_service");
    assert(caps->ptzXAddr == "http://192.168.1.100/onvif/ptz_service");
    assert(caps->eventsXAddr == "http://192.168.1.100/onvif/events_service");
    assert(caps->imagingXAddr == "http://192.168.1.100/onvif/imaging_service");

    // Invalid XML test
    assert(!PelcoD::Onvif::OnvifClient::parseCapabilitiesResponse("<invalid>xml").has_value());
}

void testDeviceInformationParsing()
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
    assert(info.has_value());
    assert(info->manufacturer == "Pelco");
    assert(info->model == "Esprit HD PTZ");
    assert(info->firmwareVersion == "2.5.0-build45");
    assert(info->serialNumber == "PELCO-SN-998877");
    assert(info->hardwareId == "HW-ESPRIT-REV3");
}

void testProfilesParsing()
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
    assert(profiles.size() == 2U);

    assert(profiles[0].token == "Profile_1");
    assert(profiles[0].name == "MainStream_1080p");
    assert(profiles[0].videoWidth == 1920);
    assert(profiles[0].videoHeight == 1080);
    assert(profiles[0].videoEncoding == "H264");

    assert(profiles[1].token == "Profile_2");
    assert(profiles[1].name == "SubStream_VGA");
    assert(profiles[1].videoWidth == 640);
    assert(profiles[1].videoHeight == 480);
    assert(profiles[1].videoEncoding == "H265");
}

void testStreamUriParsing()
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
    assert(uriInfo.has_value());
    assert(uriInfo->uri == "rtsp://192.168.1.100:554/live/ch0");
    assert(!uriInfo->invalidAfterConnect);
    assert(uriInfo->invalidAfterReboot);
    assert(uriInfo->timeout == "PT60S");
}

void testSnapshotUriParsing()
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
    assert(snapUri.has_value());
    assert(*snapUri == "http://192.168.1.100/onvif/snapshot/view.jpg");
}

void testPtzStatusParsing()
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
    assert(status.has_value());
    assert(status->pan > 0.349 && status->pan < 0.351);
    assert(status->tilt < -0.199 && status->tilt > -0.201);
    assert(status->zoom > 0.749 && status->zoom < 0.751);
    assert(status->isMoving);
    assert(status->utcTime == "2026-09-16T19:00:00Z");
}

void testPresetsParsing()
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
    assert(presets.size() == 2U);
    assert(presets[0].token == "1");
    assert(presets[0].name == "Gate Entrance");
    assert(presets[0].pan < -0.499 && presets[0].pan > -0.501);
    assert(presets[0].tilt > 0.099 && presets[0].tilt < 0.101);
    assert(presets[0].zoom > 0.399 && presets[0].zoom < 0.401);

    assert(presets[1].token == "2");
    assert(presets[1].name == "Parking Lot");
    assert(presets[1].pan > 0.799 && presets[1].pan < 0.801);

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
    assert(assignedToken.has_value());
    assert(*assignedToken == "preset_token_99");
}

void testPresetToursParsing()
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
    assert(tours.size() == 1);
    assert(tours[0].token == "Tour_1");
    assert(tours[0].name == "Perimeter Patrol");
    assert(tours[0].status == PelcoD::Onvif::PresetTourState::Touring);
    assert(tours[0].spots.size() == 2);
    assert(tours[0].spots[0].presetToken == "1");
    assert(std::abs(tours[0].spots[0].speed - 0.8f) < 0.01f);
    assert(tours[0].spots[0].stayTimeSeconds == 5);
    assert(tours[0].spots[1].presetToken == "2");
    assert(std::abs(tours[0].spots[1].speed - 0.5f) < 0.01f);
    assert(tours[0].spots[1].stayTimeSeconds == 10);

    const std::string createXml = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                  "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\n"
                                  "  <SOAP-ENV:Body>\n"
                                  "    <tptz:CreatePresetTourResponse>\n"
                                  "      <tptz:PresetTourToken>Tour_99</tptz:PresetTourToken>\n"
                                  "    </tptz:CreatePresetTourResponse>\n"
                                  "  </SOAP-ENV:Body>\n"
                                  "</SOAP-ENV:Envelope>";

    const auto createdToken = PelcoD::Onvif::OnvifClient::parseCreatePresetTourResponse(createXml);
    assert(createdToken.has_value());
    assert(*createdToken == "Tour_99");
}

void testSystemRebootParsing()
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
    assert(rebootMsg.has_value());
    assert(*rebootMsg == "Device rebooting in 5 seconds");
}

void testSoapEnvelopeWrapping()
{
    PelcoD::Onvif::SecurityCredentials creds {};
    creds.username = "operator";
    creds.password = "pass456";

    PelcoD::Onvif::OnvifClient client("http://192.168.1.100/onvif/device_service", creds);
    const std::string body = "<tds:GetDeviceInformation/>";
    const std::string env = client.wrapSoapEnvelope(body);

    assert(env.find("<s:Envelope") != std::string::npos);
    assert(env.find("<s:Header>") != std::string::npos);
    assert(env.find("<wsse:Username>operator</wsse:Username>") != std::string::npos);
    assert(env.find("<s:Body>") != std::string::npos);
    assert(env.find("<tds:GetDeviceInformation/>") != std::string::npos);
}

void testImagingSettingsParsing()
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
    assert(settings.has_value());
    assert(settings->brightness == 65.0f);
    assert(settings->colorSaturation == 75.0f);
    assert(settings->contrast == 80.0f);
    assert(settings->sharpness == 45.0f);
    assert(settings->irCutFilter == "AUTO");
    assert(settings->backlightCompensation == true);
    assert(settings->backlightLevel == 50.0f);
    assert(settings->wideDynamicRange == false);
    assert(settings->autoFocusMode == "MANUAL");
}

void testPullPointEventsParsing()
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
    assert(subUrl.has_value());
    assert(*subUrl == "http://192.168.1.100:8080/onvif/Subscription?idx=42");

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
    assert(events.size() == 2U);
    assert(events[0].topic == "tns1:RuleEngine/CellMotionDetector/Motion");
    assert(events[0].sourceName == "VideoSourceConfigurationToken");
    assert(events[0].sourceValue == "VideoSource_1");
    assert(events[0].dataName == "IsMotion");
    assert(events[0].dataValue == "true");
    assert(events[0].utcTime == "2026-09-16T20:40:05Z");

    assert(events[1].topic == "tns1:VideoSource/ImageTooDark/AnalyticsService");
    assert(events[1].dataName == "State");
    assert(events[1].dataValue == "ACTIVE");
}

int main()
{
#ifdef _WIN32
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif

    std::cout << "[RUN] Testing ONVIF WS-Security Header Generation...\n";
    testSecurityHeaderGeneration();
    std::cout << "[PASS] WS-Security Header Generation\n";

    std::cout << "[RUN] Testing ONVIF WS-Discovery Probe & Match Parsing...\n";
    testDiscoveryProbeGenerationAndParsing();
    std::cout << "[PASS] WS-Discovery Probe & Match Parsing\n";

    std::cout << "[RUN] Testing ONVIF Capabilities XML Parsing...\n";
    testCapabilitiesParsing();
    std::cout << "[PASS] Capabilities XML Parsing\n";

    std::cout << "[RUN] Testing ONVIF DeviceInformation XML Parsing...\n";
    testDeviceInformationParsing();
    std::cout << "[PASS] DeviceInformation XML Parsing\n";

    std::cout << "[RUN] Testing ONVIF Media Profiles XML Parsing...\n";
    testProfilesParsing();
    std::cout << "[PASS] Media Profiles XML Parsing\n";

    std::cout << "[RUN] Testing ONVIF StreamUri XML Parsing...\n";
    testStreamUriParsing();
    std::cout << "[PASS] StreamUri XML Parsing\n";

    std::cout << "[RUN] Testing ONVIF SnapshotUri XML Parsing...\n";
    testSnapshotUriParsing();
    std::cout << "[PASS] SnapshotUri XML Parsing\n";

    std::cout << "[RUN] Testing ONVIF PTZ Status XML Parsing...\n";
    testPtzStatusParsing();
    std::cout << "[PASS] PTZ Status XML Parsing\n";

    std::cout << "[RUN] Testing ONVIF PTZ Presets XML Parsing...\n";
    testPresetsParsing();
    std::cout << "[PASS] PTZ Presets XML Parsing\n";

    std::cout << "[RUN] Testing ONVIF PTZ Preset Tours XML Parsing...\n";
    testPresetToursParsing();
    std::cout << "[PASS] PTZ Preset Tours XML Parsing\n";

    std::cout << "[RUN] Testing ONVIF SystemReboot XML Parsing...\n";
    testSystemRebootParsing();
    std::cout << "[PASS] SystemReboot XML Parsing\n";

    std::cout << "[RUN] Testing ONVIF SOAP Envelope Wrapping...\n";
    testSoapEnvelopeWrapping();
    std::cout << "[PASS] SOAP Envelope Wrapping\n";

    std::cout << "[RUN] Testing ONVIF ImagingSettings XML Parsing (Profile T)...\n";
    testImagingSettingsParsing();
    std::cout << "[PASS] ImagingSettings XML Parsing (Profile T)\n";

    std::cout << "[RUN] Testing ONVIF PullPoint Events XML Parsing (Profile T)...\n";
    testPullPointEventsParsing();
    std::cout << "[PASS] PullPoint Events XML Parsing (Profile T)\n";

    std::cout << "\nAll PelcoDOnvif unit tests PASSED successfully!\n";
    return 0;
}
