#include <PelcoDCore/MockPelcoDDevice.h>
#include <PelcoDOnvif/OnvifClient.h>
#include <PelcoDOnvif/OnvifServer.h>
#include <PelcoDOnvif/PelcoDPtzAdapter.h>

#include <httplib.h>
#include <pugixml.hpp>

#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace PelcoD::Onvif;

class MockPtzHandler : public IPtzHandler {
public:
    void handleContinuousMove(float panSpeed, float tiltSpeed, float zoomSpeed) override
    {
        lastPan = panSpeed;
        lastTilt = tiltSpeed;
        lastZoom = zoomSpeed;
        moveCount++;
    }

    void handleAbsoluteMove(float pan, float tilt, float zoom) override
    {
        absPan = pan;
        absTilt = tilt;
        absZoom = zoom;
        absMoveCount++;
    }

    void handleStop(bool stopPanTilt, bool stopZoom) override
    {
        lastStopPt = stopPanTilt;
        lastStopZ = stopZoom;
        stopCount++;
    }

    std::string handleSetPreset(const std::string& name, const std::string& token) override
    {
        std::string tok = token.empty() ? "1" : token;
        presets.push_back({ tok, name });
        return tok;
    }

    bool handleGotoPreset(const std::string& token) override
    {
        lastGotoPreset = token;
        return true;
    }

    bool handleRemovePreset(const std::string& token) override
    {
        lastRemovePreset = token;
        return true;
    }

    std::vector<PtzPreset> handleGetPresets() override
    {
        return presets;
    }

    PtzStatus handleGetStatus() override
    {
        PtzStatus st {};
        st.pan = 180.0f;
        st.tilt = 45.0f;
        st.zoom = 0.5f;
        st.isMoving = false;
        return st;
    }

    float lastPan { 0.0f };
    float lastTilt { 0.0f };
    float lastZoom { 0.0f };
    int moveCount { 0 };

    float absPan { 0.0f };
    float absTilt { 0.0f };
    float absZoom { 0.0f };
    int absMoveCount { 0 };

    bool lastStopPt { false };
    bool lastStopZ { false };
    int stopCount { 0 };

    std::string lastGotoPreset {};
    std::string lastRemovePreset {};
    std::vector<PtzPreset> presets {};
};

class MockImagingHandler : public IImagingHandler {
public:
    ImagingSettings handleGetImagingSettings(const std::string& /*videoSourceToken*/) override
    {
        return settings;
    }

    bool handleSetImagingSettings(const std::string& /*videoSourceToken*/, const ImagingSettings& newSettings) override
    {
        settings = newSettings;
        setSettingsCount++;
        return true;
    }

    void handleMoveFocus(const std::string& /*videoSourceToken*/, float speed) override
    {
        lastFocusSpeed = speed;
        moveFocusCount++;
    }

    void handleStopFocus(const std::string& /*videoSourceToken*/) override
    {
        stopFocusCount++;
    }

    ImagingSettings settings {};
    int setSettingsCount { 0 };
    float lastFocusSpeed { 0.0f };
    int moveFocusCount { 0 };
    int stopFocusCount { 0 };
};

void testWsDiscoveryPayloads()
{
    std::cout << "[RUN] testWsDiscoveryPayloads..." << std::endl;

    OnvifServerConfig config;
    config.port = 8080;
    config.deviceName = "Unit Test Camera";
    config.model = "Virtual-Camera";
    config.serviceUuid = "11111111-2222-3333-4444-555555555555";

    WsDiscoveryServer discServer(config);

    const std::string probeMatches = discServer.createProbeMatchesPayload("urn:uuid:test-probe-1234", "192.168.1.100");

    pugi::xml_document doc;
    const auto res = doc.load_string(probeMatches.c_str());
    assert(res);

    const pugi::xml_node relatesNode = doc.select_node("//*[local-name()='RelatesTo']").node();
    assert(relatesNode);
    assert(std::string(relatesNode.text().as_string()) == "urn:uuid:test-probe-1234");

    const pugi::xml_node xaddrsNode = doc.select_node("//*[local-name()='XAddrs']").node();
    assert(xaddrsNode);
    assert(std::string(xaddrsNode.text().as_string()).find("http://192.168.1.100:8080/onvif/device_service")
        != std::string::npos);

    // Verify Profile S & T in scopes
    const pugi::xml_node scopesNode = doc.select_node("//*[local-name()='Scopes']").node();
    assert(scopesNode);
    const std::string scopesStr = scopesNode.text().as_string();
    assert(scopesStr.find("onvif://www.onvif.org/Profile/S") != std::string::npos);
    assert(scopesStr.find("onvif://www.onvif.org/Profile/T") != std::string::npos);

    const std::string hello = discServer.createHelloPayload("192.168.1.100");
    assert(doc.load_string(hello.c_str()));
    const pugi::xml_node helloNode = doc.select_node("//*[local-name()='Hello']").node();
    assert(helloNode);

    const std::string bye = discServer.createByePayload();
    assert(doc.load_string(bye.c_str()));
    const pugi::xml_node byeNode = doc.select_node("//*[local-name()='Bye']").node();
    assert(byeNode);

    std::cout << "[PASS] testWsDiscoveryPayloads" << std::endl;
}

void testHttpSoapEndpoints()
{
    std::cout << "[RUN] testHttpSoapEndpoints (Profile S)..." << std::endl;

    OnvifServerConfig config;
    config.bindAddress = "127.0.0.1";
    config.port = 18080;
    config.deviceName = "Test Bridge Camera";
    config.manufacturer = "PelcoD-Test";
    config.model = "Model-XYZ";
    config.rtspStreamUri = "rtsp://127.0.0.1:8554/test_stream";

    auto mockHandler = std::make_shared<MockPtzHandler>();
    OnvifServer server(config, mockHandler);

    assert(server.start());
    assert(server.isRunning());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    httplib::Client client("127.0.0.1", 18080);
    client.set_connection_timeout(std::chrono::seconds(2));
    client.set_read_timeout(std::chrono::seconds(2));

    // 1. GetSystemDateAndTime
    {
        const std::string soapReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body>\r\n"
                                    "    <tds:GetSystemDateAndTime/>\r\n"
                                    "  </SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/device_service", soapReq, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);

        pugi::xml_document doc;
        assert(doc.load_string(res->body.c_str()));
        assert(doc.select_node("//*[local-name()='GetSystemDateAndTimeResponse']"));
        assert(doc.select_node("//*[local-name()='UTCDateTime']"));
    }

    // 2. GetDeviceInformation
    {
        const std::string soapReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body>\r\n"
                                    "    <tds:GetDeviceInformation/>\r\n"
                                    "  </SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/device_service", soapReq, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);

        pugi::xml_document doc;
        assert(doc.load_string(res->body.c_str()));
        const auto mfgNode = doc.select_node("//*[local-name()='Manufacturer']").node();
        assert(mfgNode && std::string(mfgNode.text().as_string()) == "PelcoD-Test");
        const auto modelNode = doc.select_node("//*[local-name()='Model']").node();
        assert(modelNode && std::string(modelNode.text().as_string()) == "Model-XYZ");
    }

    // 3. GetCapabilities (including Profile T Imaging and Events)
    {
        const std::string soapReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body>\r\n"
                                    "    <tds:GetCapabilities/>\r\n"
                                    "  </SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/device_service", soapReq, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);

        pugi::xml_document doc;
        assert(doc.load_string(res->body.c_str()));
        assert(doc.select_node("//*[local-name()='PTZ']/*[local-name()='XAddr']"));
        assert(doc.select_node("//*[local-name()='Media']/*[local-name()='XAddr']"));
        assert(doc.select_node("//*[local-name()='Imaging']/*[local-name()='XAddr']"));
        assert(doc.select_node("//*[local-name()='Events']/*[local-name()='XAddr']"));
    }

    // 4. Media GetProfiles & GetStreamUri
    {
        const std::string profilesReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                        "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                        "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">\r\n"
                                        "  <SOAP-ENV:Body>\r\n"
                                        "    <trt:GetProfiles/>\r\n"
                                        "  </SOAP-ENV:Body>\r\n"
                                        "</SOAP-ENV:Envelope>";

        auto resProfiles = client.Post("/onvif/media_service", profilesReq, "application/soap+xml; charset=utf-8");
        assert(resProfiles && resProfiles->status == 200);

        pugi::xml_document doc;
        assert(doc.load_string(resProfiles->body.c_str()));
        assert(doc.select_node("//*[local-name()='Profiles'][@token='ProfileToken_1']"));

        const std::string streamReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                      "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                      "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">\r\n"
                                      "  <SOAP-ENV:Body>\r\n"
                                      "    <trt:GetStreamUri/>\r\n"
                                      "  </SOAP-ENV:Body>\r\n"
                                      "</SOAP-ENV:Envelope>";

        auto resStream = client.Post("/onvif/media_service", streamReq, "application/soap+xml; charset=utf-8");
        assert(resStream && resStream->status == 200);
        assert(doc.load_string(resStream->body.c_str()));
        const auto uriNode = doc.select_node("//*[local-name()='Uri']").node();
        assert(uriNode && std::string(uriNode.text().as_string()) == "rtsp://127.0.0.1:8554/test_stream");
    }

    // 5. PTZ ContinuousMove & Stop
    {
        const std::string moveReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" "
                                    "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                    "  <SOAP-ENV:Body>\r\n"
                                    "    <tptz:ContinuousMove>\r\n"
                                    "      <tptz:Velocity>\r\n"
                                    "        <tt:PanTilt x=\"0.75\" y=\"-0.5\"/>\r\n"
                                    "        <tt:Zoom x=\"0.2\"/>\r\n"
                                    "      </tptz:Velocity>\r\n"
                                    "    </tptz:ContinuousMove>\r\n"
                                    "  </SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";

        auto resMove = client.Post("/onvif/ptz_service", moveReq, "application/soap+xml; charset=utf-8");
        assert(resMove && resMove->status == 200);
        assert(mockHandler->moveCount == 1);
        assert(std::abs(mockHandler->lastPan - 0.75f) < 0.001f);
        assert(std::abs(mockHandler->lastTilt - (-0.5f)) < 0.001f);
        assert(std::abs(mockHandler->lastZoom - 0.2f) < 0.001f);

        const std::string stopReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body>\r\n"
                                    "    <tptz:Stop>\r\n"
                                    "      <tptz:PanTilt>true</tptz:PanTilt>\r\n"
                                    "      <tptz:Zoom>true</tptz:Zoom>\r\n"
                                    "    </tptz:Stop>\r\n"
                                    "  </SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";

        auto resStop = client.Post("/onvif/ptz_service", stopReq, "application/soap+xml; charset=utf-8");
        assert(resStop && resStop->status == 200);
        assert(mockHandler->stopCount == 1);
        assert(mockHandler->lastStopPt == true);
        assert(mockHandler->lastStopZ == true);
    }

    server.stop();
    assert(!server.isRunning());

    std::cout << "[PASS] testHttpSoapEndpoints (Profile S)" << std::endl;
}

void testProfileTImagingAndEvents()
{
    std::cout << "[RUN] testProfileTImagingAndEvents..." << std::endl;

    OnvifServerConfig config;
    config.bindAddress = "127.0.0.1";
    config.port = 18081; // Unique test port
    config.deviceName = "Profile T Camera";

    auto mockImaging = std::make_shared<MockImagingHandler>();
    mockImaging->settings.brightness = 60.0f;
    mockImaging->settings.contrast = 70.0f;

    OnvifServer server(config, nullptr, mockImaging);
    assert(server.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    httplib::Client client("127.0.0.1", 18081);
    client.set_connection_timeout(std::chrono::seconds(2));
    client.set_read_timeout(std::chrono::seconds(3));

    // 1. GetImagingSettings
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <timg:GetImagingSettings>\r\n"
                                "      <timg:VideoSourceToken>VideoSource_1</timg:VideoSourceToken>\r\n"
                                "    </timg:GetImagingSettings>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/imaging_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);

        pugi::xml_document doc;
        assert(doc.load_string(res->body.c_str()));
        const auto bNode = doc.select_node("//*[local-name()='Brightness']").node();
        assert(bNode && std::abs(bNode.text().as_float() - 60.0f) < 0.1f);
    }

    // 2. SetImagingSettings
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" "
                                "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <timg:SetImagingSettings>\r\n"
                                "      <timg:VideoSourceToken>VideoSource_1</timg:VideoSourceToken>\r\n"
                                "      <timg:ImagingSettings>\r\n"
                                "        <tt:Brightness>85.0</tt:Brightness>\r\n"
                                "        <tt:Contrast>45.0</tt:Contrast>\r\n"
                                "      </timg:ImagingSettings>\r\n"
                                "    </timg:SetImagingSettings>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/imaging_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(mockImaging->setSettingsCount == 1);
        assert(std::abs(mockImaging->settings.brightness - 85.0f) < 0.1f);
        assert(std::abs(mockImaging->settings.contrast - 45.0f) < 0.1f);
    }

    // 3. Move Focus & Stop Focus
    {
        const std::string moveReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" "
                                    "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                    "  <SOAP-ENV:Body>\r\n"
                                    "    <timg:Move>\r\n"
                                    "      <timg:VideoSourceToken>VideoSource_1</timg:VideoSourceToken>\r\n"
                                    "      <timg:Focus>\r\n"
                                    "        <tt:Continuous><tt:Speed>0.8</tt:Speed></tt:Continuous>\r\n"
                                    "      </timg:Focus>\r\n"
                                    "    </timg:Move>\r\n"
                                    "  </SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";

        auto resMove = client.Post("/onvif/imaging_service", moveReq, "application/soap+xml; charset=utf-8");
        assert(resMove && resMove->status == 200);
        assert(mockImaging->moveFocusCount == 1);
        assert(std::abs(mockImaging->lastFocusSpeed - 0.8f) < 0.01f);

        const std::string stopReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body>\r\n"
                                    "    <timg:Stop>\r\n"
                                    "      <timg:VideoSourceToken>VideoSource_1</timg:VideoSourceToken>\r\n"
                                    "    </timg:Stop>\r\n"
                                    "  </SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";

        auto resStop = client.Post("/onvif/imaging_service", stopReq, "application/soap+xml; charset=utf-8");
        assert(resStop && resStop->status == 200);
        assert(mockImaging->stopFocusCount == 1);
    }

    // 4. Events: CreatePullPointSubscription, PublishEvent, PullMessages
    {
        const std::string subReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <tev:CreatePullPointSubscription/>\r\n"
                                   "  </SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";

        auto resSub = client.Post("/onvif/event_service", subReq, "application/soap+xml; charset=utf-8");
        assert(resSub && resSub->status == 200);

        pugi::xml_document doc;
        assert(doc.load_string(resSub->body.c_str()));
        const auto addrNode = doc.select_node("//*[local-name()='Address']").node();
        assert(addrNode);
        const std::string subUrl = addrNode.text().as_string();
        assert(subUrl.find("/onvif/events/subscription/") != std::string::npos);

        // Publish mock motion detection event
        OnvifEvent motionEv {};
        motionEv.topic = "tns1:RuleEngine/CellMotionDetector/Motion";
        motionEv.sourceName = "VideoSourceToken";
        motionEv.sourceValue = "VideoSource_1";
        motionEv.dataName = "IsMotion";
        motionEv.dataValue = "true";
        server.publishEvent(motionEv);

        // Pull messages from subscription endpoint
        const std::string pullReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body>\r\n"
                                    "    <tev:PullMessages>\r\n"
                                    "      <tev:Timeout>PT1S</tev:Timeout>\r\n"
                                    "      <tev:MessageLimit>5</tev:MessageLimit>\r\n"
                                    "    </tev:PullMessages>\r\n"
                                    "  </SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";

        const auto slashPos = subUrl.find("/onvif/events/subscription/");
        const std::string subPath = subUrl.substr(slashPos);

        auto resPull = client.Post(subPath.c_str(), pullReq, "application/soap+xml; charset=utf-8");
        assert(resPull && resPull->status == 200);

        pugi::xml_document pullDoc;
        assert(pullDoc.load_string(resPull->body.c_str()));
        const auto topicNode = pullDoc.select_node("//*[local-name()='Topic']").node();
        assert(topicNode && std::string(topicNode.text().as_string()) == "tns1:RuleEngine/CellMotionDetector/Motion");

        const auto dataNode = pullDoc.select_node("//*[local-name()='Data']/*[local-name()='SimpleItem']").node();
        assert(dataNode && std::string(dataNode.attribute("Value").as_string()) == "true");

        // Unsubscribe
        const std::string unsubReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                     "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                     "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\">\r\n"
                                     "  <SOAP-ENV:Body>\r\n"
                                     "    <wsnt:Unsubscribe/>\r\n"
                                     "  </SOAP-ENV:Body>\r\n"
                                     "</SOAP-ENV:Envelope>";

        auto resUnsub = client.Post(subPath.c_str(), unsubReq, "application/soap+xml; charset=utf-8");
        assert(resUnsub && resUnsub->status == 200);
    }

    server.stop();
    assert(!server.isRunning());
    std::cout << "[PASS] testProfileTImagingAndEvents" << std::endl;
}

void testPelcoDPtzAdapter()
{
    std::cout << "[RUN] testPelcoDPtzAdapter..." << std::endl;

    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, 1U);
    assert(device->start());

    PelcoDPtzAdapter adapter(device);

    // Continuous move right + up
    adapter.handleContinuousMove(1.0f, 0.5f, 0.0f);
    // Stop
    adapter.handleStop(true, true);

    // Absolute move
    adapter.handleAbsoluteMove(90.0f, 45.0f, 0.5f);

    // Presets
    const std::string tok = adapter.handleSetPreset("Preset A", "2");
    assert(tok == "2");

    auto presets = adapter.handleGetPresets();
    assert(presets.size() == 1);
    assert(presets[0].token == "2");

    // Test Profile T event emission on preset recall
    std::vector<OnvifEvent> capturedEvents;
    adapter.setEventPublisher([&capturedEvents](const OnvifEvent& ev) { capturedEvents.push_back(ev); });

    assert(adapter.handleGotoPreset("2"));
    assert(!capturedEvents.empty());
    assert(capturedEvents.back().topic == "tns1:PTZController/PTZPresets/Reached");
    assert(capturedEvents.back().sourceValue == "2");

    assert(adapter.handleRemovePreset("2"));
    assert(adapter.handleGetPresets().empty());

    // Optical Focus (Profile T)
    adapter.handleMoveFocus("VideoSource_1", 1.0f);
    adapter.handleStopFocus("VideoSource_1");

    // Imaging Settings
    ImagingSettings imgSettings {};
    imgSettings.brightness = 75.0f;
    imgSettings.backlightCompensation = true;
    assert(adapter.handleSetImagingSettings("VideoSource_1", imgSettings));
    const auto readSettings = adapter.handleGetImagingSettings("VideoSource_1");
    assert(std::abs(readSettings.brightness - 75.0f) < 0.1f);
    assert(readSettings.backlightCompensation == true);

    const auto status = adapter.handleGetStatus();
    (void)status;

    device->stop();
    std::cout << "[PASS] testPelcoDPtzAdapter" << std::endl;
}

void testPresetToursServerAndAdapter()
{
    std::cout << "[RUN] testPresetToursServerAndAdapter..." << std::endl;

    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, 1U);
    assert(device->start());

    auto adapter = std::make_shared<PelcoDPtzAdapter>(device);
    const std::string testDbPath = "test_tours.json";
    std::remove(testDbPath.c_str());
    adapter->setPersistencePath(testDbPath);

    OnvifServerConfig config;
    config.bindAddress = "127.0.0.1";
    config.port = 18082; // Unique port
    config.deviceName = "Preset Tour Test Camera";

    OnvifServer server(config, adapter, adapter);
    assert(server.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    httplib::Client client("127.0.0.1", 18082);
    client.set_connection_timeout(std::chrono::seconds(2));
    client.set_read_timeout(std::chrono::seconds(2));

    // 1. GetPresetTourOptions
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tptz:GetPresetTourOptions>\r\n"
                                "      <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                "    </tptz:GetPresetTourOptions>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);

        pugi::xml_document doc;
        assert(doc.load_string(res->body.c_str()));
        assert(doc.select_node("//*[local-name()='GetPresetTourOptionsResponse']"));
        assert(doc.select_node("//*[local-name()='Options']"));
    }

    // 2. CreatePresetTour
    std::string tourToken;
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tptz:CreatePresetTour>\r\n"
                                "      <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                "    </tptz:CreatePresetTour>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);

        pugi::xml_document doc;
        assert(doc.load_string(res->body.c_str()));
        const auto tokenNode = doc.select_node("//*[local-name()='PresetTourToken']").node();
        assert(tokenNode);
        tourToken = tokenNode.text().as_string();
        assert(!tourToken.empty());
    }

    // 3. ModifyPresetTour
    {
        const std::string req
            = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
              "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
              "  <SOAP-ENV:Body>\r\n"
              "    <tptz:ModifyPresetTour>\r\n"
              "      <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
              "      <tptz:PresetTour token=\""
            + tourToken
            + "\">\r\n"
              "        <tt:Name>Perimeter Scan</tt:Name>\r\n"
              "        <tt:TourSpot>\r\n"
              "          <tt:PresetDetail>\r\n"
              "            <tt:PresetToken>1</tt:PresetToken>\r\n"
              "          </tt:PresetDetail>\r\n"
              "          <tt:Speed>\r\n"
              "            <tt:PanTilt x=\"0.7\" y=\"0.7\"/>\r\n"
              "          </tt:Speed>\r\n"
              "          <tt:StayTime>PT5S</tt:StayTime>\r\n"
              "        </tt:TourSpot>\r\n"
              "      </tptz:PresetTour>\r\n"
              "    </tptz:ModifyPresetTour>\r\n"
              "  </SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
    }

    // 4. GetPresetTours
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tptz:GetPresetTours>\r\n"
                                "      <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                "    </tptz:GetPresetTours>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);

        pugi::xml_document doc;
        assert(doc.load_string(res->body.c_str()));
        const auto tourNode = doc.select_node("//*[local-name()='PresetTour']").node();
        assert(tourNode);
        assert(std::string(tourNode.attribute("token").as_string()) == tourToken);
        const auto nameNode = doc.select_node("//*[local-name()='Name']").node();
        assert(nameNode && std::string(nameNode.text().as_string()) == "Perimeter Scan");
    }

    // 5. OperatePresetTour (Start, Pause, Stop)
    for (const auto& op : { "Start", "Pause", "Stop" }) {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tptz:OperatePresetTour>\r\n"
                                "      <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                "      <tptz:PresetTourToken>"
            + tourToken
            + "</tptz:PresetTourToken>\r\n"
              "      <tptz:Operation>"
            + std::string(op)
            + "</tptz:Operation>\r\n"
              "    </tptz:OperatePresetTour>\r\n"
              "  </SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
    }

    // 6. Test Persistence reload
    {
        PelcoDPtzAdapter adapterReloaded(device);
        adapterReloaded.setPersistencePath(testDbPath);
        const auto loadedTours = adapterReloaded.handleGetPresetTours();
        assert(loadedTours.size() == 1);
        assert(loadedTours[0].token == tourToken);
        assert(loadedTours[0].name == "Perimeter Scan");
        assert(loadedTours[0].spots.size() == 1);
        assert(loadedTours[0].spots[0].presetToken == "1");
        assert(loadedTours[0].spots[0].stayTimeSeconds == 5);
    }

    // 7. RemovePresetTour
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tptz:RemovePresetTour>\r\n"
                                "      <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                "      <tptz:PresetTourToken>"
            + tourToken
            + "</tptz:PresetTourToken>\r\n"
              "    </tptz:RemovePresetTour>\r\n"
              "  </SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);

        const auto remaining = adapter->handleGetPresetTours();
        assert(remaining.empty());
    }

    server.stop();
    assert(!server.isRunning());
    device->stop();

    // Clean up test file
    std::remove(testDbPath.c_str());

    std::cout << "[PASS] testPresetToursServerAndAdapter" << std::endl;
}

void testPtzServiceExtensionsServerAndAdapter()
{
    std::cout << "[RUN] testPtzServiceExtensionsServerAndAdapter..." << std::endl;

    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, 1U);
    assert(device->start());

    auto adapter = std::make_shared<PelcoDPtzAdapter>(device);

    OnvifServerConfig config;
    config.bindAddress = "127.0.0.1";
    config.port = 18083; // Unique port
    config.deviceName = "PTZ Extensions Test Camera";

    OnvifServer server(config, adapter, adapter);
    assert(server.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    httplib::Client client("127.0.0.1", 18083);
    client.set_connection_timeout(std::chrono::seconds(2));
    client.set_read_timeout(std::chrono::seconds(2));

    // 1. Verify GetNodes reports HomeSupported=true and AuxiliaryCommands
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tptz:GetNodes/>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);

        pugi::xml_document doc;
        assert(doc.load_string(res->body.c_str()));
        const auto homeNode = doc.select_node("//*[local-name()='HomeSupported']").node();
        assert(homeNode && homeNode.text().as_bool());
        const auto auxNodes = doc.select_nodes("//*[local-name()='AuxiliaryCommands']");
        assert(auxNodes.size() >= 4);
    }

    // 2. Verify GetConfigurationOptions
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tptz:GetConfigurationOptions>\r\n"
                                "      <tptz:ConfigurationToken>PTZConfig_1</tptz:ConfigurationToken>\r\n"
                                "    </tptz:GetConfigurationOptions>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);

        pugi::xml_document doc;
        assert(doc.load_string(res->body.c_str()));
        assert(doc.select_node("//*[local-name()='GetConfigurationOptionsResponse']"));
        assert(doc.select_node("//*[local-name()='PTZConfigurationOptions']"));
        assert(doc.select_node("//*[local-name()='Spaces']"));
    }

    // 3. Test RelativeMove
    {
        const std::string req
            = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
              "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
              "  <SOAP-ENV:Body>\r\n"
              "    <tptz:RelativeMove>\r\n"
              "      <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
              "      <tptz:Translation>\r\n"
              "        <tt:PanTilt x=\"10.0\" y=\"5.0\"/>\r\n"
              "        <tt:Zoom x=\"0.1\"/>\r\n"
              "      </tptz:Translation>\r\n"
              "      <tptz:Speed>\r\n"
              "        <tt:PanTilt x=\"0.8\" y=\"0.8\"/>\r\n"
              "      </tptz:Speed>\r\n"
              "    </tptz:RelativeMove>\r\n"
              "  </SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);

        pugi::xml_document doc;
        assert(doc.load_string(res->body.c_str()));
        assert(doc.select_node("//*[local-name()='RelativeMoveResponse']"));
    }

    // 4. Test SetHomePosition and GotoHomePosition
    {
        const std::string reqSet = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <tptz:SetHomePosition>\r\n"
                                   "      <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                   "    </tptz:SetHomePosition>\r\n"
                                   "  </SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";

        auto resSet = client.Post("/onvif/ptz_service", reqSet, "application/soap+xml; charset=utf-8");
        assert(resSet && resSet->status == 200);

        pugi::xml_document docSet;
        assert(docSet.load_string(resSet->body.c_str()));
        assert(docSet.select_node("//*[local-name()='SetHomePositionResponse']"));

        const std::string reqGoto = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body>\r\n"
                                    "    <tptz:GotoHomePosition>\r\n"
                                    "      <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                    "    </tptz:GotoHomePosition>\r\n"
                                    "  </SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";

        auto resGoto = client.Post("/onvif/ptz_service", reqGoto, "application/soap+xml; charset=utf-8");
        assert(resGoto && resGoto->status == 200);

        pugi::xml_document docGoto;
        assert(docGoto.load_string(resGoto->body.c_str()));
        assert(docGoto.select_node("//*[local-name()='GotoHomePositionResponse']"));
    }

    // 5. Test SendAuxiliaryCommand
    {
        for (const auto& cmd : { "tt:Wiper|On", "tt:Wiper|Off", "tt:Washer|On", "Aux1On", "Aux1Off" }) {
            const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body>\r\n"
                                    "    <tptz:SendAuxiliaryCommand>\r\n"
                                    "      <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                    "      <tptz:AuxiliaryData>"
                + std::string(cmd)
                + "</tptz:AuxiliaryData>\r\n"
                  "    </tptz:SendAuxiliaryCommand>\r\n"
                  "  </SOAP-ENV:Body>\r\n"
                  "</SOAP-ENV:Envelope>";

            auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
            assert(res && res->status == 200);

            pugi::xml_document doc;
            assert(doc.load_string(res->body.c_str()));
            const auto respNode = doc.select_node("//*[local-name()='AuxiliaryResponse']").node();
            assert(respNode);
            assert(std::string(respNode.text().as_string()) == cmd);
        }
    }

    server.stop();
    assert(!server.isRunning());
    device->stop();

    std::cout << "[PASS] testPtzServiceExtensionsServerAndAdapter" << std::endl;
}

void testMedia2OsdAndAnalytics()
{
    OnvifServerConfig config;
    config.port = 18588;
    config.bindAddress = "127.0.0.1";
    config.deviceName = "Media2Cam";
    config.model = "ONVIF-M2";
    config.rtspStreamUri = "rtsp://127.0.0.1:8554/live2";

    OnvifServer server(config);
    assert(server.start());
    assert(server.isRunning());

    httplib::Client client("127.0.0.1", config.port);
    client.set_connection_timeout(2, 0);
    client.set_read_timeout(2, 0);

    // 1. Verify GetServices advertises Media2 and Analytics
    {
        const std::string req
            = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
              "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
              "  <SOAP-ENV:Body><tds:GetServices><tds:IncludeCapability>false</tds:IncludeCapability></"
              "tds:GetServices></SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("http://www.onvif.org/ver20/media/wsdl") != std::string::npos);
        assert(res->body.find("/onvif/media2_service") != std::string::npos);
        assert(res->body.find("http://www.onvif.org/ver20/analytics/wsdl") != std::string::npos);
        assert(res->body.find("/onvif/analytics_service") != std::string::npos);
    }

    // 2. Media2 Service (/onvif/media2_service)
    {
        // GetProfiles
        const std::string reqProf = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body><tr2:GetProfiles/></SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";
        auto resProf = client.Post("/onvif/media2_service", reqProf, "application/soap+xml; charset=utf-8");
        assert(resProf && resProf->status == 200);
        pugi::xml_document docProf;
        assert(docProf.load_string(resProf->body.c_str()));
        assert(docProf.select_node("//*[local-name()='GetProfilesResponse']"));
        assert(docProf.select_node("//*[local-name()='Profiles']"));
        assert(docProf.select_node("//*[local-name()='VideoEncoder']"));

        // GetStreamUri
        const std::string reqUri
            = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
              "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\r\n"
              "  <SOAP-ENV:Body><tr2:GetStreamUri><tr2:ProfileToken>ProfileToken_1</tr2:ProfileToken></"
              "tr2:GetStreamUri></SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto resUri = client.Post("/onvif/media2_service", reqUri, "application/soap+xml; charset=utf-8");
        assert(resUri && resUri->status == 200);
        assert(resUri->body.find("rtsp://127.0.0.1:8554/live2") != std::string::npos);

        // GetVideoEncoderConfigurations
        const std::string reqEnc = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body><tr2:GetVideoEncoderConfigurations/></SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resEnc = client.Post("/onvif/media2_service", reqEnc, "application/soap+xml; charset=utf-8");
        assert(resEnc && resEnc->status == 200);
        pugi::xml_document docEnc;
        assert(docEnc.load_string(resEnc->body.c_str()));
        assert(docEnc.select_node("//*[local-name()='Encoding']"));

        // GetServiceCapabilities
        const std::string reqCap = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body><tr2:GetServiceCapabilities/></SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resCap = client.Post("/onvif/media2_service", reqCap, "application/soap+xml; charset=utf-8");
        assert(resCap && resCap->status == 200);
        assert(resCap->body.find("OSD=\"true\"") != std::string::npos);
    }

    // 3. OSD Management (/onvif/media_service & /onvif/media2_service)
    {
        // GetOSDOptions
        const std::string reqOpt = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body><trt:GetOSDOptions/></SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resOpt = client.Post("/onvif/media_service", reqOpt, "application/soap+xml; charset=utf-8");
        assert(resOpt && resOpt->status == 200);
        assert(resOpt->body.find("PositionOption") != std::string::npos);

        // GetOSDs (should include default OSD_1)
        const std::string reqList = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body><trt:GetOSDs/></SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";
        auto resList = client.Post("/onvif/media_service", reqList, "application/soap+xml; charset=utf-8");
        assert(resList && resList->status == 200);
        assert(resList->body.find("OSD_1") != std::string::npos);

        // CreateOSD
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
        auto resCreate = client.Post("/onvif/media_service", reqCreate, "application/soap+xml; charset=utf-8");
        assert(resCreate && resCreate->status == 200);
        assert(resCreate->body.find("OSD_TEST") != std::string::npos);

        // GetOSD
        const std::string reqGet
            = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
              "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">\r\n"
              "  <SOAP-ENV:Body><trt:GetOSD><trt:OSDToken>OSD_TEST</trt:OSDToken></trt:GetOSD></SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto resGet = client.Post("/onvif/media_service", reqGet, "application/soap+xml; charset=utf-8");
        assert(resGet && resGet->status == 200);
        assert(resGet->body.find("East Gate") != std::string::npos);
        assert(resGet->body.find("LowerRight") != std::string::npos);

        // SetOSD via Media2 endpoint
        const std::string reqSet
            = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
              "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
              "  <SOAP-ENV:Body>\r\n"
              "    <tr2:SetOSD>\r\n"
              "      <tr2:OSD token=\"OSD_TEST\">\r\n"
              "        <tt:VideoSourceConfigurationToken>VideoSource_1</tt:VideoSourceConfigurationToken>\r\n"
              "        <tt:Type>Text</tt:Type>\r\n"
              "        <tt:Position><tt:Type>UpperRight</tt:Type></tt:Position>\r\n"
              "        <tt:TextString>\r\n"
              "          <tt:Type>Plain</tt:Type>\r\n"
              "          <tt:PlainText>East Gate - Armed</tt:PlainText>\r\n"
              "          <tt:FontSize>26</tt:FontSize>\r\n"
              "        </tt:TextString>\r\n"
              "      </tr2:OSD>\r\n"
              "    </tr2:SetOSD>\r\n"
              "  </SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto resSet = client.Post("/onvif/media2_service", reqSet, "application/soap+xml; charset=utf-8");
        assert(resSet && resSet->status == 200);

        // DeleteOSD
        const std::string reqDel
            = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
              "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">\r\n"
              "  "
              "<SOAP-ENV:Body><trt:DeleteOSD><trt:OSDToken>OSD_TEST</trt:OSDToken></trt:DeleteOSD></SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto resDel = client.Post("/onvif/media_service", reqDel, "application/soap+xml; charset=utf-8");
        assert(resDel && resDel->status == 200);
    }

    // 4. Analytics Service (/onvif/analytics_service)
    {
        const std::string reqCap = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tan=\"http://www.onvif.org/ver20/analytics/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body><tan:GetServiceCapabilities/></SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resCap = client.Post("/onvif/analytics_service", reqCap, "application/soap+xml; charset=utf-8");
        assert(resCap && resCap->status == 200);
        assert(resCap->body.find("RuleSupport=\"true\"") != std::string::npos);

        const std::string reqRules = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                     "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                     "xmlns:tan=\"http://www.onvif.org/ver20/analytics/wsdl\">\r\n"
                                     "  <SOAP-ENV:Body><tan:GetSupportedRules/></SOAP-ENV:Body>\r\n"
                                     "</SOAP-ENV:Envelope>";
        auto resRules = client.Post("/onvif/analytics_service", reqRules, "application/soap+xml; charset=utf-8");
        assert(resRules && resRules->status == 200);
        assert(resRules->body.find("CellMotionDetector") != std::string::npos);
    }

    // 5. Event Push Subscription (<wsnt:Subscribe>)
    {
        const std::string reqSub
            = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
              "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\" "
              "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\">\r\n"
              "  <SOAP-ENV:Body>\r\n"
              "    <wsnt:Subscribe>\r\n"
              "      <wsnt:ConsumerReference><wsa:Address>http://127.0.0.1:18589/notify</wsa:Address></"
              "wsnt:ConsumerReference>\r\n"
              "    </wsnt:Subscribe>\r\n"
              "  </SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto resSub = client.Post("/onvif/event_service", reqSub, "application/soap+xml; charset=utf-8");
        assert(resSub && resSub->status == 200);
        assert(resSub->body.find("SubscribeResponse") != std::string::npos);
        assert(resSub->body.find("SubscriptionReference") != std::string::npos);

        // Publish event to exercise push path
        OnvifEvent ev;
        ev.topic = "tns1:RuleEngine/CellMotionDetector/Motion";
        ev.dataName = "IsMotion";
        ev.dataValue = "true";
        server.publishEvent(ev);
    }

    server.stop();
    assert(!server.isRunning());
    std::cout << "[PASS] testMedia2OsdAndAnalytics" << std::endl;
}

void testDeviceManagementAndSecurity()
{
    std::cout << "[RUN] testDeviceManagementAndSecurity" << std::endl;

    OnvifServerConfig config;
    config.port = 18591;
    config.deviceName = "DeviceMgmtCamera";

    OnvifServer server(config);
    assert(server.start());
    assert(server.isRunning());

    httplib::Client client("127.0.0.1", config.port);
    client.set_connection_timeout(std::chrono::seconds(2));
    client.set_read_timeout(std::chrono::seconds(2));

    // 1. GetUsers (defaults: admin, operator)
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:GetUsers/>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        pugi::xml_document doc;
        assert(doc.load_string(res->body.c_str()));
        const auto users = doc.select_nodes("//*[local-name()='User']");
        assert(users.size() >= 2);
    }

    // 2. CreateUsers
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                                "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:CreateUsers>\r\n"
                                "      <tds:User>\r\n"
                                "        <tt:Username>guard1</tt:Username>\r\n"
                                "        <tt:Password>guardpass</tt:Password>\r\n"
                                "        <tt:UserLevel>User</tt:UserLevel>\r\n"
                                "      </tds:User>\r\n"
                                "    </tds:CreateUsers>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("CreateUsersResponse") != std::string::npos);
    }

    // 3. Verify user created via GetUsers
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:GetUsers/>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("guard1") != std::string::npos);
    }

    // 4. SetUser (update role)
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                                "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:SetUser>\r\n"
                                "      <tds:User>\r\n"
                                "        <tt:Username>guard1</tt:Username>\r\n"
                                "        <tt:Password>newguardpass</tt:Password>\r\n"
                                "        <tt:UserLevel>Operator</tt:UserLevel>\r\n"
                                "      </tds:User>\r\n"
                                "    </tds:SetUser>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("SetUserResponse") != std::string::npos);
    }

    // 5. DeleteUsers
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:DeleteUsers>\r\n"
                                "      <tds:Username>guard1</tds:Username>\r\n"
                                "    </tds:DeleteUsers>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("DeleteUsersResponse") != std::string::npos);
    }

    // 6. Network Interfaces: Get and Set
    {
        const std::string getReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <tds:GetNetworkInterfaces/>\r\n"
                                   "  </SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", getReq, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("NetworkInterfaces token=\"eth0\"") != std::string::npos);

        const std::string setReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                                   "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <tds:SetNetworkInterfaces>\r\n"
                                   "      <tds:InterfaceToken>eth0</tds:InterfaceToken>\r\n"
                                   "      <tds:NetworkInterface>\r\n"
                                   "        <tt:Enabled>true</tt:Enabled>\r\n"
                                   "        <tt:MTU>1400</tt:MTU>\r\n"
                                   "        <tt:IPv4>\r\n"
                                   "          <tt:Enabled>true</tt:Enabled>\r\n"
                                   "          <tt:Manual>\r\n"
                                   "            <tt:Address>10.0.0.50</tt:Address>\r\n"
                                   "            <tt:PrefixLength>16</tt:PrefixLength>\r\n"
                                   "          </tt:Manual>\r\n"
                                   "          <tt:DHCP>false</tt:DHCP>\r\n"
                                   "        </tt:IPv4>\r\n"
                                   "      </tds:NetworkInterface>\r\n"
                                   "    </tds:SetNetworkInterfaces>\r\n"
                                   "  </SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resSet = client.Post("/onvif/device_service", setReq, "application/soap+xml; charset=utf-8");
        assert(resSet && resSet->status == 200);
        assert(resSet->body.find("SetNetworkInterfacesResponse") != std::string::npos);

        auto resVerify = client.Post("/onvif/device_service", getReq, "application/soap+xml; charset=utf-8");
        assert(resVerify && resVerify->status == 200);
        assert(resVerify->body.find("<tt:MTU>1400</tt:MTU>") != std::string::npos);
        assert(resVerify->body.find("<tt:Address>10.0.0.50</tt:Address>") != std::string::npos);
    }

    // 7. Default Gateway: Get and Set
    {
        const std::string setGwReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                     "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                     "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                                     "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                     "  <SOAP-ENV:Body>\r\n"
                                     "    <tds:SetNetworkDefaultGateway>\r\n"
                                     "      <tds:IPv4Address>10.0.0.1</tds:IPv4Address>\r\n"
                                     "    </tds:SetNetworkDefaultGateway>\r\n"
                                     "  </SOAP-ENV:Body>\r\n"
                                     "</SOAP-ENV:Envelope>";
        auto resGw = client.Post("/onvif/device_service", setGwReq, "application/soap+xml; charset=utf-8");
        assert(resGw && resGw->status == 200);

        const std::string getGwReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                     "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                     "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                     "  <SOAP-ENV:Body>\r\n"
                                     "    <tds:GetNetworkDefaultGateway/>\r\n"
                                     "  </SOAP-ENV:Body>\r\n"
                                     "</SOAP-ENV:Envelope>";
        auto resGet = client.Post("/onvif/device_service", getGwReq, "application/soap+xml; charset=utf-8");
        assert(resGet && resGet->status == 200);
        assert(resGet->body.find("<tt:IPv4Address>10.0.0.1</tt:IPv4Address>") != std::string::npos);
    }

    // 8. DNS & NTP: Get and Set
    {
        const std::string setDns = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <tds:SetDNS>\r\n"
                                   "      <tds:FromDHCP>false</tds:FromDHCP>\r\n"
                                   "      <tds:DNSManual><tt:IPv4Address>9.9.9.9</tt:IPv4Address></tds:DNSManual>\r\n"
                                   "    </tds:SetDNS>\r\n"
                                   "  </SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resDns = client.Post("/onvif/device_service", setDns, "application/soap+xml; charset=utf-8");
        assert(resDns && resDns->status == 200);

        const std::string getDns = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <tds:GetDNS/>\r\n"
                                   "  </SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resGetDns = client.Post("/onvif/device_service", getDns, "application/soap+xml; charset=utf-8");
        assert(resGetDns && resGetDns->status == 200);
        assert(resGetDns->body.find("9.9.9.9") != std::string::npos);

        const std::string setNtp
            = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
              "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
              "  <SOAP-ENV:Body>\r\n"
              "    <tds:SetNTP>\r\n"
              "      <tds:FromDHCP>false</tds:FromDHCP>\r\n"
              "      <tds:NTPManual><tt:DNSname>time.cloudflare.com</tt:DNSname></tds:NTPManual>\r\n"
              "    </tds:SetNTP>\r\n"
              "  </SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto resNtp = client.Post("/onvif/device_service", setNtp, "application/soap+xml; charset=utf-8");
        assert(resNtp && resNtp->status == 200);

        const std::string getNtp = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <tds:GetNTP/>\r\n"
                                   "  </SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resGetNtp = client.Post("/onvif/device_service", getNtp, "application/soap+xml; charset=utf-8");
        assert(resGetNtp && resGetNtp->status == 200);
        assert(resGetNtp->body.find("time.cloudflare.com") != std::string::npos);
    }

    // 9. Hostname: Get and Set
    {
        const std::string setHn = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                  "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                  "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                  "  <SOAP-ENV:Body>\r\n"
                                  "    <tds:SetHostname><tds:Name>PTZ-Camera-West</tds:Name></tds:SetHostname>\r\n"
                                  "  </SOAP-ENV:Body>\r\n"
                                  "</SOAP-ENV:Envelope>";
        auto resHn = client.Post("/onvif/device_service", setHn, "application/soap+xml; charset=utf-8");
        assert(resHn && resHn->status == 200);

        const std::string getHn = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                  "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                  "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                  "  <SOAP-ENV:Body>\r\n"
                                  "    <tds:GetHostname/>\r\n"
                                  "  </SOAP-ENV:Body>\r\n"
                                  "</SOAP-ENV:Envelope>";
        auto resGetHn = client.Post("/onvif/device_service", getHn, "application/soap+xml; charset=utf-8");
        assert(resGetHn && resGetHn->status == 200);
        assert(resGetHn->body.find("PTZ-Camera-West") != std::string::npos);
    }

    // 10. Date & Time: SetSystemDateAndTime
    {
        const std::string setDt = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                  "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                  "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                                  "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                  "  <SOAP-ENV:Body>\r\n"
                                  "    <tds:SetSystemDateAndTime>\r\n"
                                  "      <tds:DateTimeType>Manual</tds:DateTimeType>\r\n"
                                  "      <tds:DaylightSavings>false</tds:DaylightSavings>\r\n"
                                  "      <tds:TimeZone><tt:TZ>UTC</tt:TZ></tds:TimeZone>\r\n"
                                  "      <tds:UTCDateTime>\r\n"
                                  "        <tt:Time><tt:Hour>12</tt:Hour><tt:Minute>30</tt:Minute><tt:Second>0</"
                                  "tt:Second></tt:Time>\r\n"
                                  "        <tt:Date><tt:Year>2026</tt:Year><tt:Month>9</tt:Month><tt:Day>17</tt:Day></"
                                  "tt:Date>\r\n"
                                  "      </tds:UTCDateTime>\r\n"
                                  "    </tds:SetSystemDateAndTime>\r\n"
                                  "  </SOAP-ENV:Body>\r\n"
                                  "</SOAP-ENV:Envelope>";
        auto resDt = client.Post("/onvif/device_service", setDt, "application/soap+xml; charset=utf-8");
        assert(resDt && resDt->status == 200);
        assert(resDt->body.find("SetSystemDateAndTimeResponse") != std::string::npos);
    }

    // 11. Scopes: AddScopes, RemoveScopes, SetScopes
    {
        const std::string addSc = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                  "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                  "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                  "  <SOAP-ENV:Body>\r\n"
                                  "    <tds:AddScopes>\r\n"
                                  "      <tds:ScopeItem>onvif://www.onvif.org/location/Sector4</tds:ScopeItem>\r\n"
                                  "    </tds:AddScopes>\r\n"
                                  "  </SOAP-ENV:Body>\r\n"
                                  "</SOAP-ENV:Envelope>";
        auto resAdd = client.Post("/onvif/device_service", addSc, "application/soap+xml; charset=utf-8");
        assert(resAdd && resAdd->status == 200);

        const std::string getSc = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                  "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                  "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                  "  <SOAP-ENV:Body>\r\n"
                                  "    <tds:GetScopes/>\r\n"
                                  "  </SOAP-ENV:Body>\r\n"
                                  "</SOAP-ENV:Envelope>";
        auto resGet = client.Post("/onvif/device_service", getSc, "application/soap+xml; charset=utf-8");
        assert(resGet && resGet->status == 200);
        assert(resGet->body.find("Sector4") != std::string::npos);

        const std::string remSc = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                  "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                  "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                  "  <SOAP-ENV:Body>\r\n"
                                  "    <tds:RemoveScopes>\r\n"
                                  "      <tds:ScopeItem>onvif://www.onvif.org/location/Sector4</tds:ScopeItem>\r\n"
                                  "    </tds:RemoveScopes>\r\n"
                                  "  </SOAP-ENV:Body>\r\n"
                                  "</SOAP-ENV:Envelope>";
        auto resRem = client.Post("/onvif/device_service", remSc, "application/soap+xml; charset=utf-8");
        assert(resRem && resRem->status == 200);
    }

    // 12. SystemReboot & SetSystemFactoryDefault
    {
        const std::string reboot = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <tds:SystemReboot/>\r\n"
                                   "  </SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resReboot = client.Post("/onvif/device_service", reboot, "application/soap+xml; charset=utf-8");
        assert(resReboot && resReboot->status == 200);
        assert(resReboot->body.find("SystemRebootResponse") != std::string::npos);

        const std::string factory = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body>\r\n"
                                    "    <tds:SetSystemFactoryDefault>"
                                    "      <tds:FactoryDefault>Soft</tds:FactoryDefault>"
                                    "    </tds:SetSystemFactoryDefault>\r\n"
                                    "  </SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";
        auto resFactory = client.Post("/onvif/device_service", factory, "application/soap+xml; charset=utf-8");
        assert(resFactory && resFactory->status == 200);
        assert(resFactory->body.find("SetSystemFactoryDefaultResponse") != std::string::npos);
    }

    server.stop();
    assert(!server.isRunning());
    std::cout << "[PASS] testDeviceManagementAndSecurity" << std::endl;
}

void testImagingExtensionsAndDeviceIo()
{
    std::cout << "[RUN] testImagingExtensionsAndDeviceIo..." << std::endl;

    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, 1U);
    assert(device->start());

    auto adapter = std::make_shared<PelcoDPtzAdapter>(device);

    OnvifServerConfig config;
    config.port = 18595;
    config.bindAddress = "127.0.0.1";
    config.deviceName = "ImagingDeviceIoCamera";

    OnvifServer server(config, adapter, adapter);
    server.setDeviceIoHandler(adapter);
    assert(server.start());
    assert(server.isRunning());

    httplib::Client client("127.0.0.1", config.port);
    client.set_connection_timeout(std::chrono::seconds(2));
    client.set_read_timeout(std::chrono::seconds(2));

    // 1. GetCapabilities: check tt:DeviceIO and tt:Imaging
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tds:GetCapabilities/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("/onvif/imaging_service") != std::string::npos);
        assert(res->body.find("/onvif/deviceio_service") != std::string::npos);
    }

    // 2. GetServices: check deviceIO and imaging
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tds:GetServices/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("http://www.onvif.org/ver10/deviceIO/wsdl") != std::string::npos);
        assert(res->body.find("/onvif/deviceio_service") != std::string::npos);
    }

    // 3. Extended Imaging Service: GetStatus (FocusStatus20)
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <timg:GetStatus>\r\n"
                                "      <timg:VideoSourceToken>VideoSource_1</timg:VideoSourceToken>\r\n"
                                "    </timg:GetStatus>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/imaging_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        pugi::xml_document doc;
        assert(doc.load_string(res->body.c_str()));
        assert(doc.select_node("//*[local-name()='FocusStatus20']"));
        assert(doc.select_node("//*[local-name()='MoveStatus']"));
    }

    // 4. Extended Imaging Service: Move (Continuous, Absolute, Relative) & Stop
    {
        const std::string reqCont = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" "
                                    "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                    "  <SOAP-ENV:Body>\r\n"
                                    "    <timg:Move>\r\n"
                                    "      <timg:VideoSourceToken>VideoSource_1</timg:VideoSourceToken>\r\n"
                                    "      <timg:Focus>\r\n"
                                    "        <tt:Continuous><tt:Speed>0.5</tt:Speed></tt:Continuous>\r\n"
                                    "      </timg:Focus>\r\n"
                                    "    </timg:Move>\r\n"
                                    "  </SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";
        auto resCont = client.Post("/onvif/imaging_service", reqCont, "application/soap+xml; charset=utf-8");
        assert(resCont && resCont->status == 200);

        const std::string reqAbs = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" "
                                   "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <timg:Move>\r\n"
                                   "      <timg:VideoSourceToken>VideoSource_1</timg:VideoSourceToken>\r\n"
                                   "      <timg:Focus>\r\n"
                                   "        <tt:Absolute><tt:Position>0.7</tt:Position></tt:Absolute>\r\n"
                                   "      </timg:Focus>\r\n"
                                   "    </timg:Move>\r\n"
                                   "  </SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resAbs = client.Post("/onvif/imaging_service", reqAbs, "application/soap+xml; charset=utf-8");
        assert(resAbs && resAbs->status == 200);

        const std::string reqStop = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body>\r\n"
                                    "    <timg:Stop>\r\n"
                                    "      <timg:VideoSourceToken>VideoSource_1</timg:VideoSourceToken>\r\n"
                                    "    </timg:Stop>\r\n"
                                    "  </SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";
        auto resStop = client.Post("/onvif/imaging_service", reqStop, "application/soap+xml; charset=utf-8");
        assert(resStop && resStop->status == 200);
    }

    // 5. Extended Imaging Service: GetPresets & SetCurrentPreset
    {
        const std::string reqGet = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <timg:GetPresets>\r\n"
                                   "      <timg:VideoSourceToken>VideoSource_1</timg:VideoSourceToken>\r\n"
                                   "    </timg:GetPresets>\r\n"
                                   "  </SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resGet = client.Post("/onvif/imaging_service", reqGet, "application/soap+xml; charset=utf-8");
        assert(resGet && resGet->status == 200);
        assert(resGet->body.find("Preset_Clear") != std::string::npos);
        assert(resGet->body.find("Preset_BW") != std::string::npos);

        const std::string reqSet = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <timg:SetCurrentPreset>\r\n"
                                   "      <timg:VideoSourceToken>VideoSource_1</timg:VideoSourceToken>\r\n"
                                   "      <timg:PresetToken>Preset_BW</timg:PresetToken>\r\n"
                                   "    </timg:SetCurrentPreset>\r\n"
                                   "  </SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resSet = client.Post("/onvif/imaging_service", reqSet, "application/soap+xml; charset=utf-8");
        assert(resSet && resSet->status == 200);
    }

    // 6. DeviceIO Service: GetRelayOutputs
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tmd:GetRelayOutputs/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/deviceio_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("Relay_1") != std::string::npos);
        assert(res->body.find("Relay_2") != std::string::npos);
    }

    // 7. DeviceIO Service: SetRelayOutputState (active & inactive)
    {
        const std::string reqActive = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                      "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                      "xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\">\r\n"
                                      "  <SOAP-ENV:Body>\r\n"
                                      "    <tmd:SetRelayOutputState>\r\n"
                                      "      <tmd:RelayOutputToken>Relay_1</tmd:RelayOutputToken>\r\n"
                                      "      <tmd:LogicalState>active</tmd:LogicalState>\r\n"
                                      "    </tmd:SetRelayOutputState>\r\n"
                                      "  </SOAP-ENV:Body>\r\n"
                                      "</SOAP-ENV:Envelope>";
        auto resActive = client.Post("/onvif/deviceio_service", reqActive, "application/soap+xml; charset=utf-8");
        assert(resActive && resActive->status == 200);

        const auto relaysAfterActive = adapter->handleGetRelayOutputs();
        assert(!relaysAfterActive.empty() && relaysAfterActive[0].logicalState == RelayLogicalState::Active);

        const std::string reqInactive = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                        "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                        "xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\">\r\n"
                                        "  <SOAP-ENV:Body>\r\n"
                                        "    <tmd:SetRelayOutputState>\r\n"
                                        "      <tmd:RelayOutputToken>Relay_1</tmd:RelayOutputToken>\r\n"
                                        "      <tmd:LogicalState>inactive</tmd:LogicalState>\r\n"
                                        "    </tmd:SetRelayOutputState>\r\n"
                                        "  </SOAP-ENV:Body>\r\n"
                                        "</SOAP-ENV:Envelope>";
        auto resInactive = client.Post("/onvif/deviceio_service", reqInactive, "application/soap+xml; charset=utf-8");
        assert(resInactive && resInactive->status == 200);

        const auto relaysAfterInactive = adapter->handleGetRelayOutputs();
        assert(!relaysAfterInactive.empty() && relaysAfterInactive[0].logicalState == RelayLogicalState::Inactive);
    }

    // 8. DeviceIO Service: SetRelayOutputSettings
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\" "
                                "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tmd:SetRelayOutputSettings>\r\n"
                                "      <tmd:RelayOutputToken>Relay_1</tmd:RelayOutputToken>\r\n"
                                "      <tmd:Properties>\r\n"
                                "        <tt:Mode>Monostable</tt:Mode>\r\n"
                                "        <tt:DelayTime>PT2S</tt:DelayTime>\r\n"
                                "        <tt:IdleState>closed</tt:IdleState>\r\n"
                                "      </tmd:Properties>\r\n"
                                "    </tmd:SetRelayOutputSettings>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/deviceio_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);

        const auto relays = adapter->handleGetRelayOutputs();
        assert(!relays.empty());
        assert(relays[0].mode == RelayMode::Monostable);
        assert(std::abs(relays[0].delayTimeSeconds - 2.0f) < 0.1f);
        assert(relays[0].idleState == RelayIdleState::Closed);
    }

    // 9. DeviceIO Service: GetDigitalInputs
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tmd:GetDigitalInputs/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/deviceio_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("Input_1") != std::string::npos);
    }

    server.stop();
    assert(!server.isRunning());
    device->stop();

    std::cout << "[PASS] testImagingExtensionsAndDeviceIo" << std::endl;
}

void testMetadataStreamsAndMaintenanceExtensions()
{
    std::cout << "[RUN] testMetadataStreamsAndMaintenanceExtensions" << std::endl;

    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, 1U);
    assert(device->start());

    auto adapter = std::make_shared<PelcoDPtzAdapter>(device);

    // Populate analytics objects
    AnalyticsObject carObj {};
    carObj.objectId = 101;
    carObj.className = "Vehicle";
    carObj.confidence = 0.95f;
    carObj.boundingBox = { 0.1f, 0.2f, 0.5f, 0.7f };
    carObj.geoLocation = { 37.7749, -122.4194, 10.0 };
    adapter->addDetectedObject(carObj);

    OnvifServerConfig config;
    config.port = 18596;
    config.bindAddress = "127.0.0.1";
    config.deviceName = "MetadataMaintenanceCamera";

    OnvifServer server(config, adapter, adapter);
    server.setMetadataHandler(adapter);
    server.setDeviceManagementHandler(adapter);
    assert(server.start());
    assert(server.isRunning());

    httplib::Client client("127.0.0.1", config.port);
    client.set_connection_timeout(std::chrono::seconds(2));
    client.set_read_timeout(std::chrono::seconds(2));

    // 1. GetMetadataConfigurations (Media Service)
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><trt:GetMetadataConfigurations/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/media_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("GetMetadataConfigurationsResponse") != std::string::npos);
        assert(res->body.find("MetadataConfig_1") != std::string::npos);
    }

    // 2. SetMetadataConfiguration (Media Service)
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
                                "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <trt:SetMetadataConfiguration>\r\n"
                                "      <trt:Configuration token=\"MetadataConfig_1\">\r\n"
                                "        <tt:Name>UpdatedMetadata</tt:Name>\r\n"
                                "        <tt:UseCount>1</tt:UseCount>\r\n"
                                "        <tt:PTZStatus>\r\n"
                                "          <tt:Status>true</tt:Status>\r\n"
                                "        </tt:PTZStatus>\r\n"
                                "        <tt:Analytics>true</tt:Analytics>\r\n"
                                "        <tt:Events>true</tt:Events>\r\n"
                                "      </trt:Configuration>\r\n"
                                "    </trt:SetMetadataConfiguration>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/media_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("SetMetadataConfigurationResponse") != std::string::npos);

        const auto cfg = adapter->handleGetMetadataConfiguration("MetadataConfig_1");
        assert(cfg.has_value());
        assert(cfg->name == "UpdatedMetadata");
        assert(cfg->eventsEnabled == true);
    }

    // 3. GET /onvif/metadata_stream
    {
        auto res = client.Get("/onvif/metadata_stream");
        assert(res && res->status == 200);
        assert(res->body.find("<tt:MetadataStream") != std::string::npos);
        assert(res->body.find("<tt:PTZStatus>") != std::string::npos);
        assert(res->body.find("<tt:VideoAnalytics>") != std::string::npos);
        assert(res->body.find("ObjectId=\"101\"") != std::string::npos);
        assert(res->body.find("Vehicle") != std::string::npos);
    }

    // 4. Device Management: GetSystemLog (System)
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:GetSystemLog>\r\n"
                                "      <tds:LogType>System</tds:LogType>\r\n"
                                "    </tds:GetSystemLog>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("GetSystemLogResponse") != std::string::npos);
        assert(res->body.find("SystemLog") != std::string::npos);
    }

    // 5. Device Management: GetSystemLog (Access)
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:GetSystemLog>\r\n"
                                "      <tds:LogType>Access</tds:LogType>\r\n"
                                "    </tds:GetSystemLog>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("GetSystemLogResponse") != std::string::npos);
        assert(
            res->body.find("DeviceService:") != std::string::npos || res->body.find("ACCESS LOG") != std::string::npos);
    }

    // 6. Device Management: GetSystemSupportInformation
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tds:GetSystemSupportInformation/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("GetSystemSupportInformationResponse") != std::string::npos);
        assert(res->body.find("SupportInformation") != std::string::npos);
    }

    // 7. Device Management: GetSystemBackup & RestoreSystem
    {
        const std::string reqBackup = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                      "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                      "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                      "  <SOAP-ENV:Body><tds:GetSystemBackup/></SOAP-ENV:Body>\r\n"
                                      "</SOAP-ENV:Envelope>";
        auto resBackup = client.Post("/onvif/device_service", reqBackup, "application/soap+xml; charset=utf-8");
        assert(resBackup && resBackup->status == 200);
        assert(resBackup->body.find("GetSystemBackupResponse") != std::string::npos);
        assert(resBackup->body.find("BackupFiles") != std::string::npos);

        const std::string reqRestore
            = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
              "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
              "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
              "  <SOAP-ENV:Body>\r\n"
              "    <tds:RestoreSystem>\r\n"
              "      <tds:BackupFiles><tt:Data>TEST_RESTORE_BLOB</tt:Data></tds:BackupFiles>\r\n"
              "    </tds:RestoreSystem>\r\n"
              "  </SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto resRestore = client.Post("/onvif/device_service", reqRestore, "application/soap+xml; charset=utf-8");
        assert(resRestore && resRestore->status == 200);
        assert(resRestore->body.find("RestoreSystemResponse") != std::string::npos);
    }

    // 8. Device Management: GetEndpointReference
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tds:GetEndpointReference/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("GetEndpointReferenceResponse") != std::string::npos);
        assert(res->body.find("GUID") != std::string::npos);
    }

    server.stop();
    assert(!server.isRunning());
    device->stop();

    std::cout << "[PASS] testMetadataStreamsAndMaintenanceExtensions" << std::endl;
}

void testProfileGAndPkiCertificates()
{
    std::cout << "[RUN] testProfileGAndPkiCertificates" << std::endl;

    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, 1U);
    assert(device->start());

    auto adapter = std::make_shared<PelcoDPtzAdapter>(device);

    OnvifServerConfig config;
    config.port = 18597;
    config.bindAddress = "127.0.0.1";
    config.deviceName = "ProfileGCam";

    OnvifServer server(config, adapter, adapter);
    server.setDeviceManagementHandler(adapter);
    server.setRecordingHandler(adapter);
    server.setSearchHandler(adapter);
    server.setReplayHandler(adapter);
    assert(server.start());
    assert(server.isRunning());

    httplib::Client client("127.0.0.1", config.port);
    client.set_connection_timeout(std::chrono::seconds(3));
    client.set_read_timeout(std::chrono::seconds(3));

    // 1. Device Management: CreateCertificate
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:CreateCertificate>\r\n"
                                "      <tds:CertificateID>cert_pki_1</tds:CertificateID>\r\n"
                                "      <tds:Subject>CN=ProfileGCam,O=Org</tds:Subject>\r\n"
                                "      <tds:ValidDays>365</tds:ValidDays>\r\n"
                                "    </tds:CreateCertificate>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("CreateCertificateResponse") != std::string::npos);
        pugi::xml_document doc;
        assert(doc.load_string(res->body.c_str()));
        const pugi::xml_node idNode = doc.select_node("//*[local-name()='CertificateID']").node();
        assert(idNode && std::string(idNode.text().as_string()) == "cert_pki_1");
        const pugi::xml_node dataNode = doc.select_node("//*[local-name()='Data']").node();
        assert(dataNode && !std::string(dataNode.text().as_string()).empty());
    }

    // 2. Device Management: GetCertificates
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tds:GetCertificates/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("GetCertificatesResponse") != std::string::npos);
        assert(res->body.find("cert_pki_1") != std::string::npos);
    }

    // 3. Device Management: GetCertificateInformation
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:GetCertificateInformation>\r\n"
                                "      <tds:CertificateID>cert_pki_1</tds:CertificateID>\r\n"
                                "    </tds:GetCertificateInformation>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("GetCertificateInformationResponse") != std::string::npos);
        assert(res->body.find("ProfileGCam") != std::string::npos);
    }

    // 4. Device Management: GetPkcs10Request
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:GetPkcs10Request>\r\n"
                                "      <tds:CertificateID>csr_pki_1</tds:CertificateID>\r\n"
                                "      <tds:Subject>CN=CSRCamera,O=Org</tds:Subject>\r\n"
                                "    </tds:GetPkcs10Request>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("GetPkcs10RequestResponse") != std::string::npos);
        assert(res->body.find("Pkcs10Request") != std::string::npos);
    }

    // 5. Device Management: ClientCertificateMode (Get, Set, Get)
    {
        const std::string reqGet = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body><tds:GetClientCertificateMode/></SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resGet = client.Post("/onvif/device_service", reqGet, "application/soap+xml; charset=utf-8");
        assert(resGet && resGet->status == 200);
        assert(resGet->body.find("GetClientCertificateModeResponse") != std::string::npos);

        const std::string reqSet = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <tds:SetClientCertificateMode>\r\n"
                                   "      <tds:ClientCertificateMode>Optional</tds:ClientCertificateMode>\r\n"
                                   "    </tds:SetClientCertificateMode>\r\n"
                                   "  </SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resSet = client.Post("/onvif/device_service", reqSet, "application/soap+xml; charset=utf-8");
        assert(resSet && resSet->status == 200);
        assert(resSet->body.find("SetClientCertificateModeResponse") != std::string::npos);

        auto resGet2 = client.Post("/onvif/device_service", reqGet, "application/soap+xml; charset=utf-8");
        assert(resGet2 && resGet2->status == 200);
        assert(resGet2->body.find("Optional") != std::string::npos);
    }

    // 6. Device Management: DeleteCertificates
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:DeleteCertificates>\r\n"
                                "      <tds:CertificateID>cert_pki_1</tds:CertificateID>\r\n"
                                "    </tds:DeleteCertificates>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("DeleteCertificatesResponse") != std::string::npos);
    }

    // 7. Recording Service: GetServiceCapabilities & CreateRecording
    std::string recToken;
    {
        const std::string reqCaps = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:trc=\"http://www.onvif.org/ver10/recording/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body><trc:GetServiceCapabilities/></SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";
        auto resCaps = client.Post("/onvif/recording_service", reqCaps, "application/soap+xml; charset=utf-8");
        assert(resCaps && resCaps->status == 200);
        assert(resCaps->body.find("GetServiceCapabilitiesResponse") != std::string::npos);
        assert(resCaps->body.find("DynamicRecordings") != std::string::npos);

        const std::string reqCreate = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                      "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                      "xmlns:trc=\"http://www.onvif.org/ver10/recording/wsdl\" "
                                      "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                      "  <SOAP-ENV:Body>\r\n"
                                      "    <trc:CreateRecording>\r\n"
                                      "      <trc:RecordingConfiguration>\r\n"
                                      "        <tt:Source>\r\n"
                                      "          <tt:SourceId>Profile_1</tt:SourceId>\r\n"
                                      "        </tt:Source>\r\n"
                                      "        <tt:Content>Daily Test</tt:Content>\r\n"
                                      "        <tt:MaximumRetentionTime>P30D</tt:MaximumRetentionTime>\r\n"
                                      "      </trc:RecordingConfiguration>\r\n"
                                      "    </trc:CreateRecording>\r\n"
                                      "  </SOAP-ENV:Body>\r\n"
                                      "</SOAP-ENV:Envelope>";
        auto resCreate = client.Post("/onvif/recording_service", reqCreate, "application/soap+xml; charset=utf-8");
        assert(resCreate && resCreate->status == 200);
        assert(resCreate->body.find("CreateRecordingResponse") != std::string::npos);
        pugi::xml_document doc;
        assert(doc.load_string(resCreate->body.c_str()));
        const pugi::xml_node tNode = doc.select_node("//*[local-name()='RecordingToken']").node();
        assert(tNode);
        recToken = tNode.text().as_string();
        assert(!recToken.empty());
    }

    // 8. Recording Service: GetRecordings & CreateTrack
    std::string trkToken;
    {
        const std::string reqGet = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:trc=\"http://www.onvif.org/ver10/recording/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body><trc:GetRecordings/></SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resGet = client.Post("/onvif/recording_service", reqGet, "application/soap+xml; charset=utf-8");
        assert(resGet && resGet->status == 200);
        assert(resGet->body.find(recToken) != std::string::npos);

        const std::string reqTrk = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:trc=\"http://www.onvif.org/ver10/recording/wsdl\" "
                                   "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <trc:CreateTrack>\r\n"
                                   "      <trc:RecordingToken>"
            + recToken
            + "</trc:RecordingToken>\r\n"
              "      <trc:TrackConfiguration>\r\n"
              "        <tt:TrackType>Video</tt:TrackType>\r\n"
              "        <tt:Description>1080p Stream</tt:Description>\r\n"
              "      </trc:TrackConfiguration>\r\n"
              "    </trc:CreateTrack>\r\n"
              "  </SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto resTrk = client.Post("/onvif/recording_service", reqTrk, "application/soap+xml; charset=utf-8");
        assert(resTrk && resTrk->status == 200);
        assert(resTrk->body.find("CreateTrackResponse") != std::string::npos);
        pugi::xml_document doc;
        assert(doc.load_string(resTrk->body.c_str()));
        const pugi::xml_node tNode = doc.select_node("//*[local-name()='TrackToken']").node();
        assert(tNode);
        trkToken = tNode.text().as_string();
        assert(!trkToken.empty());
    }

    // 9. Recording Service: CreateRecordingJob, GetRecordingJobs, SetRecordingJobMode
    std::string jobToken;
    {
        const std::string reqJob = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:trc=\"http://www.onvif.org/ver10/recording/wsdl\" "
                                   "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <trc:CreateRecordingJob>\r\n"
                                   "      <trc:JobConfiguration>\r\n"
                                   "        <tt:RecordingToken>"
            + recToken
            + "</tt:RecordingToken>\r\n"
              "        <tt:Mode>Active</tt:Mode>\r\n"
              "        <tt:Priority>1</tt:Priority>\r\n"
              "        <tt:SourceToken>Profile_1</tt:SourceToken>\r\n"
              "      </trc:JobConfiguration>\r\n"
              "    </trc:CreateRecordingJob>\r\n"
              "  </SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto resJob = client.Post("/onvif/recording_service", reqJob, "application/soap+xml; charset=utf-8");
        assert(resJob && resJob->status == 200);
        assert(resJob->body.find("CreateRecordingJobResponse") != std::string::npos);
        pugi::xml_document doc;
        assert(doc.load_string(resJob->body.c_str()));
        const pugi::xml_node tNode = doc.select_node("//*[local-name()='JobToken']").node();
        assert(tNode);
        jobToken = tNode.text().as_string();
        assert(!jobToken.empty());

        const std::string reqGet = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:trc=\"http://www.onvif.org/ver10/recording/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body><trc:GetRecordingJobs/></SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resGet = client.Post("/onvif/recording_service", reqGet, "application/soap+xml; charset=utf-8");
        assert(resGet && resGet->status == 200);
        assert(resGet->body.find(jobToken) != std::string::npos);

        const std::string reqMode = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:trc=\"http://www.onvif.org/ver10/recording/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body>\r\n"
                                    "    <trc:SetRecordingJobMode>\r\n"
                                    "      <trc:JobToken>"
            + jobToken
            + "</trc:JobToken>\r\n"
              "      <trc:Mode>Idle</trc:Mode>\r\n"
              "    </trc:SetRecordingJobMode>\r\n"
              "  </SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto resMode = client.Post("/onvif/recording_service", reqMode, "application/soap+xml; charset=utf-8");
        assert(resMode && resMode->status == 200);
        assert(resMode->body.find("SetRecordingJobModeResponse") != std::string::npos);
    }

    // 10. Recording Service: GetRecordingSummary
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:trc=\"http://www.onvif.org/ver10/recording/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><trc:GetRecordingSummary/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/recording_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("GetRecordingSummaryResponse") != std::string::npos);
        assert(res->body.find("NumberRecordings") != std::string::npos);
    }

    // 11. Search Service: GetServiceCapabilities, FindRecordings, GetRecordingSearchResults
    std::string recSearchToken;
    {
        const std::string reqCaps = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:tse=\"http://www.onvif.org/ver10/search/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body><tse:GetServiceCapabilities/></SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";
        auto resCaps = client.Post("/onvif/search_service", reqCaps, "application/soap+xml; charset=utf-8");
        assert(resCaps && resCaps->status == 200);
        assert(resCaps->body.find("GetServiceCapabilitiesResponse") != std::string::npos);

        const std::string reqFind = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:tse=\"http://www.onvif.org/ver10/search/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body>\r\n"
                                    "    <tse:FindRecordings>\r\n"
                                    "      <tse:MaxMatches>10</tse:MaxMatches>\r\n"
                                    "    </tse:FindRecordings>\r\n"
                                    "  </SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";
        auto resFind = client.Post("/onvif/search_service", reqFind, "application/soap+xml; charset=utf-8");
        assert(resFind && resFind->status == 200);
        assert(resFind->body.find("FindRecordingsResponse") != std::string::npos);
        pugi::xml_document doc;
        assert(doc.load_string(resFind->body.c_str()));
        const pugi::xml_node tNode = doc.select_node("//*[local-name()='SearchToken']").node();
        assert(tNode);
        recSearchToken = tNode.text().as_string();
        assert(!recSearchToken.empty());

        const std::string reqResults = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                       "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                       "xmlns:tse=\"http://www.onvif.org/ver10/search/wsdl\">\r\n"
                                       "  <SOAP-ENV:Body>\r\n"
                                       "    <tse:GetRecordingSearchResults>\r\n"
                                       "      <tse:SearchToken>"
            + recSearchToken
            + "</tse:SearchToken>\r\n"
              "    </tse:GetRecordingSearchResults>\r\n"
              "  </SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto resResults = client.Post("/onvif/search_service", reqResults, "application/soap+xml; charset=utf-8");
        assert(resResults && resResults->status == 200);
        assert(resResults->body.find("GetRecordingSearchResultsResponse") != std::string::npos);
        assert(resResults->body.find("ResultList") != std::string::npos);
    }

    // 12. Search Service: FindEvents, GetEventSearchResults, EndSearch
    {
        const std::string reqFind = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:tse=\"http://www.onvif.org/ver10/search/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body>\r\n"
                                    "    <tse:FindEvents>\r\n"
                                    "      <tse:StartPoint>2026-09-01T00:00:00Z</tse:StartPoint>\r\n"
                                    "      <tse:EndPoint>2026-09-17T00:00:00Z</tse:EndPoint>\r\n"
                                    "    </tse:FindEvents>\r\n"
                                    "  </SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";
        auto resFind = client.Post("/onvif/search_service", reqFind, "application/soap+xml; charset=utf-8");
        assert(resFind && resFind->status == 200);
        assert(resFind->body.find("FindEventsResponse") != std::string::npos);
        pugi::xml_document doc;
        assert(doc.load_string(resFind->body.c_str()));
        const pugi::xml_node tNode = doc.select_node("//*[local-name()='SearchToken']").node();
        assert(tNode);
        const std::string evSearchToken = tNode.text().as_string();
        assert(!evSearchToken.empty());

        const std::string reqResults = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                       "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                       "xmlns:tse=\"http://www.onvif.org/ver10/search/wsdl\">\r\n"
                                       "  <SOAP-ENV:Body>\r\n"
                                       "    <tse:GetEventSearchResults>\r\n"
                                       "      <tse:SearchToken>"
            + evSearchToken
            + "</tse:SearchToken>\r\n"
              "    </tse:GetEventSearchResults>\r\n"
              "  </SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto resResults = client.Post("/onvif/search_service", reqResults, "application/soap+xml; charset=utf-8");
        assert(resResults && resResults->status == 200);
        assert(resResults->body.find("GetEventSearchResultsResponse") != std::string::npos);
        assert(resResults->body.find("ResultList") != std::string::npos);

        const std::string reqEnd = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tse=\"http://www.onvif.org/ver10/search/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <tse:EndSearch>\r\n"
                                   "      <tse:SearchToken>"
            + recSearchToken
            + "</tse:SearchToken>\r\n"
              "    </tse:EndSearch>\r\n"
              "  </SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto resEnd = client.Post("/onvif/search_service", reqEnd, "application/soap+xml; charset=utf-8");
        assert(resEnd && resEnd->status == 200);
        assert(resEnd->body.find("EndSearchResponse") != std::string::npos);
    }

    // 13. Replay Service: GetServiceCapabilities, GetReplayUri, Get/Set ReplayConfiguration
    {
        const std::string reqCaps = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:trp=\"http://www.onvif.org/ver10/replay/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body><trp:GetServiceCapabilities/></SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";
        auto resCaps = client.Post("/onvif/replay_service", reqCaps, "application/soap+xml; charset=utf-8");
        assert(resCaps && resCaps->status == 200);
        assert(resCaps->body.find("GetServiceCapabilitiesResponse") != std::string::npos);
        assert(resCaps->body.find("ReversePlayback") != std::string::npos);

        const std::string reqUri = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:trp=\"http://www.onvif.org/ver10/replay/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <trp:GetReplayUri>\r\n"
                                   "      <trp:RecordingToken>"
            + recToken
            + "</trp:RecordingToken>\r\n"
              "    </trp:GetReplayUri>\r\n"
              "  </SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto resUri = client.Post("/onvif/replay_service", reqUri, "application/soap+xml; charset=utf-8");
        assert(resUri && resUri->status == 200);
        assert(resUri->body.find("GetReplayUriResponse") != std::string::npos);
        assert(resUri->body.find("rtsp://") != std::string::npos);

        const std::string reqGetCfg = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                      "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                      "xmlns:trp=\"http://www.onvif.org/ver10/replay/wsdl\">\r\n"
                                      "  <SOAP-ENV:Body><trp:GetReplayConfiguration/></SOAP-ENV:Body>\r\n"
                                      "</SOAP-ENV:Envelope>";
        auto resGetCfg = client.Post("/onvif/replay_service", reqGetCfg, "application/soap+xml; charset=utf-8");
        assert(resGetCfg && resGetCfg->status == 200);
        assert(resGetCfg->body.find("GetReplayConfigurationResponse") != std::string::npos);

        const std::string reqSetCfg = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                      "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                      "xmlns:trp=\"http://www.onvif.org/ver10/replay/wsdl\">\r\n"
                                      "  <SOAP-ENV:Body>\r\n"
                                      "    <trp:SetReplayConfiguration>\r\n"
                                      "      <trp:Configuration>\r\n"
                                      "        <tt:SessionTimeout>PT120S</tt:SessionTimeout>\r\n"
                                      "      </trp:Configuration>\r\n"
                                      "    </trp:SetReplayConfiguration>\r\n"
                                      "  </SOAP-ENV:Body>\r\n"
                                      "</SOAP-ENV:Envelope>";
        auto resSetCfg = client.Post("/onvif/replay_service", reqSetCfg, "application/soap+xml; charset=utf-8");
        assert(resSetCfg && resSetCfg->status == 200);
        assert(resSetCfg->body.find("SetReplayConfigurationResponse") != std::string::npos);

        auto resGetCfg2 = client.Post("/onvif/replay_service", reqGetCfg, "application/soap+xml; charset=utf-8");
        assert(resGetCfg2 && resGetCfg2->status == 200);
        assert(resGetCfg2->body.find("PT120S") != std::string::npos);
    }

    server.stop();
    assert(!server.isRunning());
    device->stop();

    std::cout << "[PASS] testProfileGAndPkiCertificates" << std::endl;
}

void testVideoAnalyticsRuleEngineAndEvaluation()
{
    std::cout << "[RUN] testVideoAnalyticsRuleEngineAndEvaluation" << std::endl;

    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, 1U);
    assert(device->start());
    auto adapter = std::make_shared<PelcoDPtzAdapter>(device);

    OnvifServerConfig config;
    config.bindAddress = "127.0.0.1";
    config.port = 18090;

    OnvifServer server(config, adapter, adapter);
    server.setAnalyticsHandler(adapter);
    assert(server.start());
    assert(server.isRunning());

    httplib::Client client("127.0.0.1", 18090);

    // 1. GetServiceCapabilities
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tan=\"http://www.onvif.org/ver20/analytics/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tan:GetServiceCapabilities/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/analytics_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("RuleSupport=\"true\"") != std::string::npos);
        assert(res->body.find("AnalyticsModuleSupport=\"true\"") != std::string::npos);
    }

    // 2. GetSupportedRules
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tan=\"http://www.onvif.org/ver20/analytics/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tan:GetSupportedRules/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/analytics_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("tt:LineDetector") != std::string::npos);
        assert(res->body.find("tt:FieldDetector") != std::string::npos);
        assert(res->body.find("tt:LoiteringDetector") != std::string::npos);
    }

    // 3. CreateRules (Tripwire LineDetector + FieldDetector)
    {
        const std::string req
            = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
              "xmlns:tan=\"http://www.onvif.org/ver20/analytics/wsdl\" "
              "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
              "  <SOAP-ENV:Body>\r\n"
              "    <tan:CreateRules>\r\n"
              "      <tan:ConfigurationToken>VideoAnalytics_1</tan:ConfigurationToken>\r\n"
              "      <tan:Rule Name=\"PerimeterTripwire\" Type=\"tt:LineDetector\">\r\n"
              "        <tan:Parameters>\r\n"
              "          <tt:SimpleItem Name=\"Direction\" Value=\"LeftToRight\"/>\r\n"
              "          <tt:SimpleItem Name=\"Classes\" Value=\"Human,Vehicle\"/>\r\n"
              "          <tt:SimpleItem Name=\"MinConfidence\" Value=\"0.50\"/>\r\n"
              "          <tt:ElementItem Name=\"Segment\">\r\n"
              "            <tt:Point x=\"0.5000\" y=\"0.0000\"/>\r\n"
              "            <tt:Point x=\"0.5000\" y=\"1.0000\"/>\r\n"
              "          </tt:ElementItem>\r\n"
              "        </tan:Parameters>\r\n"
              "      </tan:Rule>\r\n"
              "      <tan:Rule Name=\"ZoneIntrusion\" Type=\"tt:FieldDetector\">\r\n"
              "        <tan:Parameters>\r\n"
              "          <tt:SimpleItem Name=\"Classes\" Value=\"Human\"/>\r\n"
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
              "    </tan:CreateRules>\r\n"
              "  </SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/analytics_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("CreateRulesResponse") != std::string::npos);
    }

    // 4. GetRules verify persistence
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tan=\"http://www.onvif.org/ver20/analytics/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tan:GetRules/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/analytics_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("PerimeterTripwire") != std::string::npos);
        assert(res->body.find("ZoneIntrusion") != std::string::npos);
    }

    // 5. Test Geometric Rule Evaluation via Adapter & Event Publishing
    std::vector<OnvifEvent> emittedEvents;
    adapter->setEventPublisher([&emittedEvents](const OnvifEvent& ev) {
        emittedEvents.push_back(ev);
    });

    // Frame 1: Object 1 (Human) at x=0.30 (left of vertical line x=0.50, inside zone [0.2, 0.8])
    AnalyticsObject obj1;
    obj1.objectId = 101;
    obj1.className = "Human";
    obj1.confidence = 0.85f;
    obj1.boundingBox = { 0.28f, 0.45f, 0.32f, 0.55f }; // center (0.30, 0.50)

    adapter->setDetectedObjects({ obj1 });

    // Should trigger ZoneIntrusion event because center is inside polygon [0.2, 0.8]
    assert(!emittedEvents.empty());
    bool zoneTriggered = false;
    for (const auto& ev : emittedEvents) {
        if (ev.topic == "tns1:RuleEngine/FieldDetector/ObjectsInside" && ev.sourceValue == "ZoneIntrusion") {
            zoneTriggered = true;
        }
    }
    assert(zoneTriggered);
    emittedEvents.clear();

    // Frame 2: Object 1 moves to x=0.70 (right of vertical line x=0.50) -> Crossing left-to-right!
    AnalyticsObject obj1Moved = obj1;
    obj1Moved.boundingBox = { 0.68f, 0.45f, 0.72f, 0.55f }; // center (0.70, 0.50)

    adapter->setDetectedObjects({ obj1Moved });

    // Should trigger PerimeterTripwire Crossed event
    bool tripwireTriggered = false;
    for (const auto& ev : emittedEvents) {
        if (ev.topic == "tns1:RuleEngine/LineDetector/Crossed" && ev.sourceValue == "PerimeterTripwire") {
            tripwireTriggered = true;
        }
    }
    assert(tripwireTriggered);

    // 6. DeleteRules
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tan=\"http://www.onvif.org/ver20/analytics/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tan:DeleteRules>\r\n"
                                "      <tan:RuleName>PerimeterTripwire</tan:RuleName>\r\n"
                                "    </tan:DeleteRules>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/analytics_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("DeleteRulesResponse") != std::string::npos);

        auto rulesRemaining = adapter->handleGetRules("");
        assert(rulesRemaining.size() == 1U);
        assert(rulesRemaining[0].name == "ZoneIntrusion");
    }

    server.stop();
    assert(!server.isRunning());
    device->stop();

    std::cout << "[PASS] testVideoAnalyticsRuleEngineAndEvaluation" << std::endl;
}

void testPtzGeoMoveAndSphericalSpaces()
{
    std::cout << "[RUN] testPtzGeoMoveAndSphericalSpaces..." << std::endl;

    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, 1U);
    assert(device->start());

    auto adapter = std::make_shared<PelcoDPtzAdapter>(device);

    OnvifServerConfig config;
    config.port = 18605;
    config.bindAddress = "127.0.0.1";
    config.deviceName = "GeoCamera";
    config.defaultLocation.entity = "Device";
    config.defaultLocation.fixed = true;
    config.defaultLocation.location = { 37.9838, 23.7275, 150.0 };
    config.defaultLocation.orientation = { 0.0, 0.0, 0.0 }; // Pointing true North

    OnvifServer server(config);
    adapter->setCameraLocation(config.defaultLocation);
    server.setPtzHandler(adapter);
    server.setDeviceManagementHandler(adapter);
    assert(server.start());
    assert(server.isRunning());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    httplib::Client client("127.0.0.1", config.port);
    client.set_connection_timeout(std::chrono::seconds(2));
    client.set_read_timeout(std::chrono::seconds(2));

    // 1. GetConfigurationOptions - should advertise PositionSphericalSpace
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tptz:GetConfigurationOptions>\r\n"
                                "      <tptz:ConfigurationToken>PTZConfig_1</tptz:ConfigurationToken>\r\n"
                                "    </tptz:GetConfigurationOptions>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("PositionSphericalSpace") != std::string::npos);
    }

    // 2. GetNodes - should advertise <tt:GeoMove>true</tt:GeoMove>
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tptz:GetNodes/>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("<tt:GeoMove>true</tt:GeoMove>") != std::string::npos);
    }

    // 3. GetGeoLocation - initial camera location
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:GetGeoLocation/>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("37.9838") != std::string::npos);
        assert(res->body.find("23.7275") != std::string::npos);
    }

    // 4. SetGeoLocation - update camera location & mounting orientation
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                                "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:SetGeoLocation>\r\n"
                                "      <tds:Location Entity=\"Device\">\r\n"
                                "        <tt:GeoLocation lat=\"38.0000\" lon=\"23.8000\" elevation=\"100.0\"/>\r\n"
                                "        <tt:GeoOrientation yaw=\"90.0\" pitch=\"0.0\" roll=\"0.0\"/>\r\n"
                                "      </tds:Location>\r\n"
                                "    </tds:SetGeoLocation>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("SetGeoLocationResponse") != std::string::npos);

        const auto camLoc = adapter->cameraLocation();
        assert(std::fabs(camLoc.location.latitude - 38.0000) < 0.0001);
        assert(std::fabs(camLoc.location.longitude - 23.8000) < 0.0001);
        assert(std::fabs(camLoc.orientation.yaw - 90.0) < 0.01);
    }

    // 5. GeoMove - Target directly North of camera (lat: 38.01, lon: 23.80)
    // Since camera orientation yaw=90 (mounted facing East), target North is at relative azimuth 270°
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" "
                                "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tptz:GeoMove>\r\n"
                                "      <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                "      <tptz:Target lat=\"38.0100\" lon=\"23.8000\" elevation=\"100.0\"/>\r\n"
                                "      <tptz:AreaWidth>20.0</tptz:AreaWidth>\r\n"
                                "    </tptz:GeoMove>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("GeoMoveResponse") != std::string::npos);

        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        const auto state = mockTransport->getInternalState();
        // 270 deg = 27000 centidegrees
        assert(std::abs(static_cast<int>(state.panCentidegrees) - 27000) < 50);
        assert(state.tiltCentidegrees == 0);
    }

    // 6. AbsoluteMove with PositionSphericalSpace (Azimuth 180°, Elevation 30°)
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" "
                                "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tptz:AbsoluteMove>\r\n"
                                "      <tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>\r\n"
                                "      <tptz:Position>\r\n"
                                "        <tt:PanTilt x=\"180.0\" y=\"30.0\" space=\"http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionSphericalSpace\"/>\r\n"
                                "      </tptz:Position>\r\n"
                                "    </tptz:AbsoluteMove>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("AbsoluteMoveResponse") != std::string::npos);

        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        const auto state = mockTransport->getInternalState();
        assert(state.panCentidegrees == 18000);
        assert(state.tiltCentidegrees == 3000);
    }

    // 7. DeleteGeoLocation
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:DeleteGeoLocation>\r\n"
                                "      <tds:Location Entity=\"Device\"/>\r\n"
                                "    </tds:DeleteGeoLocation>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("DeleteGeoLocationResponse") != std::string::npos);
    }

    server.stop();
    assert(!server.isRunning());
    device->stop();

    std::cout << "[PASS] testPtzGeoMoveAndSphericalSpaces" << std::endl;
}

void testProfileTPrivacyMasksAndVideoSourceModes()
{
    const int port = 18599;
    OnvifServerConfig config;
    config.port = port;
    config.deviceName = "Profile T Privacy & Source Modes Camera";

    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>();
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport);
    assert(device->start());

    auto adapter = std::make_shared<PelcoDPtzAdapter>(device);

    OnvifServer server(config, adapter, adapter);
    server.setMaskHandler(adapter);
    server.setVideoSourceModeHandler(adapter);
    assert(server.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    httplib::Client client("127.0.0.1", port);
    client.set_connection_timeout(std::chrono::seconds(2));
    client.set_read_timeout(std::chrono::seconds(2));

    // 1. Verify Media2 GetServiceCapabilities returns Mask="true"
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tr2:GetServiceCapabilities/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/media2_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("Mask=\"true\"") != std::string::npos);
    }

    // 2. GetMaskOptions
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tr2:GetMaskOptions>\r\n"
                                "      <tr2:ConfigurationToken>VideoSource_1</tr2:ConfigurationToken>\r\n"
                                "    </tr2:GetMaskOptions>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/media2_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("GetMaskOptionsResponse") != std::string::npos);
        assert(res->body.find("MaxMasks") != std::string::npos);
        assert(res->body.find("Rectangle=\"true\"") != std::string::npos);
    }

    // 3. GetMasks - check default mask Mask_1
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tr2:GetMasks>\r\n"
                                "      <tr2:ConfigurationToken>VideoSource_1</tr2:ConfigurationToken>\r\n"
                                "    </tr2:GetMasks>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/media2_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("GetMasksResponse") != std::string::npos);
        assert(res->body.find("Mask_1") != std::string::npos);
    }

    // 4. CreateMask - add a new mask Mask_New
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\" "
                                "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tr2:CreateMask>\r\n"
                                "      <tr2:Mask token=\"Mask_New\">\r\n"
                                "        <tr2:ConfigurationToken>VideoSource_1</tr2:ConfigurationToken>\r\n"
                                "        <tr2:Polygon>\r\n"
                                "          <tt:Point x=\"0.2\" y=\"0.2\"/>\r\n"
                                "          <tt:Point x=\"0.8\" y=\"0.2\"/>\r\n"
                                "          <tt:Point x=\"0.8\" y=\"0.8\"/>\r\n"
                                "          <tt:Point x=\"0.2\" y=\"0.8\"/>\r\n"
                                "        </tr2:Polygon>\r\n"
                                "        <tr2:Type>Blurred</tr2:Type>\r\n"
                                "        <tr2:Enabled>true</tr2:Enabled>\r\n"
                                "      </tr2:Mask>\r\n"
                                "    </tr2:CreateMask>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/media2_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("CreateMaskResponse") != std::string::npos);
        assert(res->body.find("Mask_New") != std::string::npos);
    }

    // 5. GetMask - retrieve Mask_New
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tr2:GetMask>\r\n"
                                "      <tr2:Token>Mask_New</tr2:Token>\r\n"
                                "    </tr2:GetMask>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/media2_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("GetMaskResponse") != std::string::npos);
        assert(res->body.find("Blurred") != std::string::npos);
    }

    // 6. SetMask - modify Mask_New to Pixelated and disabled
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\" "
                                "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tr2:SetMask>\r\n"
                                "      <tr2:Mask token=\"Mask_New\">\r\n"
                                "        <tr2:ConfigurationToken>VideoSource_1</tr2:ConfigurationToken>\r\n"
                                "        <tr2:Polygon>\r\n"
                                "          <tt:Point x=\"0.3\" y=\"0.3\"/>\r\n"
                                "          <tt:Point x=\"0.7\" y=\"0.7\"/>\r\n"
                                "        </tr2:Polygon>\r\n"
                                "        <tr2:Type>Pixelated</tr2:Type>\r\n"
                                "        <tr2:Enabled>false</tr2:Enabled>\r\n"
                                "      </tr2:Mask>\r\n"
                                "    </tr2:SetMask>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/media2_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("SetMaskResponse") != std::string::npos);

        // Verify changes took effect
        const auto maskOpt = adapter->handleGetMask("Mask_New");
        assert(maskOpt.has_value());
        assert(maskOpt->type == MaskType::Pixelated);
        assert(maskOpt->enabled == false);
    }

    // 7. DeleteMask - remove Mask_New
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tr2:DeleteMask>\r\n"
                                "      <tr2:Token>Mask_New</tr2:Token>\r\n"
                                "    </tr2:DeleteMask>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/media2_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("DeleteMaskResponse") != std::string::npos);

        assert(!adapter->handleGetMask("Mask_New").has_value());
    }

    // 8. VideoSourceModes - GetVideoSourceModes on /onvif/media2_service
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tr2:GetVideoSourceModes>\r\n"
                                "      <tr2:VideoSourceToken>VideoSource_1</tr2:VideoSourceToken>\r\n"
                                "    </tr2:GetVideoSourceModes>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/media2_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("GetVideoSourceModesResponse") != std::string::npos);
        assert(res->body.find("Mode_1080p60") != std::string::npos);
        assert(res->body.find("Mode_4k30") != std::string::npos);
    }

    // 9. VideoSourceModes - GetVideoSourceModes on /onvif/device_service
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:GetVideoSourceModes>\r\n"
                                "      <tds:VideoSourceToken>VideoSource_1</tds:VideoSourceToken>\r\n"
                                "    </tds:GetVideoSourceModes>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("GetVideoSourceModesResponse") != std::string::npos);
        assert(res->body.find("Mode_1080p60") != std::string::npos);
    }

    // 10. SetVideoSourceMode - apply Mode_4k30 (triggers reboot flag)
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tr2:SetVideoSourceMode>\r\n"
                                "      <tr2:VideoSourceToken>VideoSource_1</tr2:VideoSourceToken>\r\n"
                                "      <tr2:VideoSourceModeToken>Mode_4k30</tr2:VideoSourceModeToken>\r\n"
                                "    </tr2:SetVideoSourceMode>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/media2_service", req, "application/soap+xml; charset=utf-8");
        assert(res && res->status == 200);
        assert(res->body.find("SetVideoSourceModeResponse") != std::string::npos);
        assert(res->body.find("Reboot>true</") != std::string::npos);

        // Verify active mode in adapter
        const auto modes = adapter->handleGetVideoSourceModes("VideoSource_1");
        for (const auto& m : modes) {
            if (m.token == "Mode_4k30") {
                assert(m.enabled == true);
            } else {
                assert(m.enabled == false);
            }
        }
    }

    // 11. End-to-end OnvifClient calls against the running server
    {
        OnvifClient onvifClient("http://127.0.0.1:" + std::to_string(port) + "/onvif/device_service");
        assert(onvifClient.getCapabilities());

        const auto opts = onvifClient.getMaskOptions("VideoSource_1");
        assert(opts.has_value());
        assert(opts->maxMasks == 8);

        auto masks = onvifClient.getMasks("VideoSource_1");
        assert(masks.size() == 1U);
        assert(masks[0].token == "Mask_1");

        PrivacyMask clientMask {};
        clientMask.token = "Mask_Client";
        clientMask.configurationToken = "VideoSource_1";
        clientMask.type = MaskType::Color;
        clientMask.color = { 0, 255, 0, "RGB" };
        clientMask.enabled = true;
        clientMask.polygon = { { 0.1f, 0.1f }, { 0.5f, 0.5f } };

        const std::string createdTok = onvifClient.createMask(clientMask);
        assert(createdTok == "Mask_Client");

        masks = onvifClient.getMasks("VideoSource_1");
        assert(masks.size() == 2U);

        const auto singleMask = onvifClient.getMask("Mask_Client");
        assert(singleMask.has_value());
        assert(singleMask->token == "Mask_Client");

        clientMask.enabled = false;
        assert(onvifClient.setMask(clientMask));

        assert(onvifClient.deleteMask("Mask_Client"));
        masks = onvifClient.getMasks("VideoSource_1");
        assert(masks.size() == 1U);

        const auto modes = onvifClient.getVideoSourceModes("VideoSource_1");
        assert(modes.size() == 2U);

        assert(onvifClient.setVideoSourceMode("VideoSource_1", "Mode_1080p60"));
    }

    server.stop();
    assert(!server.isRunning());
    device->stop();

    std::cout << "[PASS] testProfileTPrivacyMasksAndVideoSourceModes" << std::endl;
}

int main()
{
    std::cout << "Starting TestOnvifServer test suite..." << std::endl;
    testWsDiscoveryPayloads();
    testHttpSoapEndpoints();
    testProfileTImagingAndEvents();
    testPelcoDPtzAdapter();
    testPresetToursServerAndAdapter();
    testPtzServiceExtensionsServerAndAdapter();
    testMedia2OsdAndAnalytics();
    testDeviceManagementAndSecurity();
    testImagingExtensionsAndDeviceIo();
    testMetadataStreamsAndMaintenanceExtensions();
    testProfileGAndPkiCertificates();
    testVideoAnalyticsRuleEngineAndEvaluation();
    testPtzGeoMoveAndSphericalSpaces();
    testProfileTPrivacyMasksAndVideoSourceModes();
    std::cout << "All TestOnvifServer tests passed successfully!" << std::endl;
    return 0;
}
