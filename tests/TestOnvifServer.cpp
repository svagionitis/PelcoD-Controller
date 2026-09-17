#include <PelcoDCore/MockPelcoDDevice.h>
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

int main()
{
    std::cout << "Starting TestOnvifServer test suite..." << std::endl;
    testWsDiscoveryPayloads();
    testHttpSoapEndpoints();
    testProfileTImagingAndEvents();
    testPelcoDPtzAdapter();
    testPresetToursServerAndAdapter();
    std::cout << "All TestOnvifServer tests passed successfully!" << std::endl;
    return 0;
}
