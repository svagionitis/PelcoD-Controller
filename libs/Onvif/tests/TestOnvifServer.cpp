#include <Onvif/OnvifClient.h>
#include <Onvif/OnvifServer.h>
#include <Onvif/adapters/PelcoDPtzAdapter.h>
#include <PelcoDSim/MockPelcoDDevice.h>

#include <httplib.h>
#include <pugixml.hpp>

#include <chrono>
#include <cmath>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace Onvif;

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

TEST(OnvifServerTest, WsDiscoveryPayloads)
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

    std::cout << "[PASS] testWsDiscoveryPayloads" << std::endl;
}

TEST(OnvifServerTest, HttpSoapEndpoints)
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

    EXPECT_TRUE(server.start());
    EXPECT_TRUE(server.isRunning());

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
        EXPECT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        EXPECT_TRUE(doc.load_string(res->body.c_str()));
        EXPECT_TRUE(doc.select_node("//*[local-name()='GetSystemDateAndTimeResponse']"));
        EXPECT_TRUE(doc.select_node("//*[local-name()='UTCDateTime']"));
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
        EXPECT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        EXPECT_TRUE(doc.load_string(res->body.c_str()));
        const auto mfgNode = doc.select_node("//*[local-name()='Manufacturer']").node();
        EXPECT_TRUE(mfgNode && std::string(mfgNode.text().as_string()) == "PelcoD-Test");
        const auto modelNode = doc.select_node("//*[local-name()='Model']").node();
        EXPECT_TRUE(modelNode && std::string(modelNode.text().as_string()) == "Model-XYZ");
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
        EXPECT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        EXPECT_TRUE(doc.load_string(res->body.c_str()));
        EXPECT_TRUE(doc.select_node("//*[local-name()='PTZ']/*[local-name()='XAddr']"));
        EXPECT_TRUE(doc.select_node("//*[local-name()='Media']/*[local-name()='XAddr']"));
        EXPECT_TRUE(doc.select_node("//*[local-name()='Imaging']/*[local-name()='XAddr']"));
        EXPECT_TRUE(doc.select_node("//*[local-name()='Events']/*[local-name()='XAddr']"));
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
        EXPECT_TRUE(resProfiles && resProfiles->status == 200);

        pugi::xml_document doc;
        EXPECT_TRUE(doc.load_string(resProfiles->body.c_str()));
        EXPECT_TRUE(doc.select_node("//*[local-name()='Profiles'][@token='ProfileToken_1']"));

        const std::string streamReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                      "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                      "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">\r\n"
                                      "  <SOAP-ENV:Body>\r\n"
                                      "    <trt:GetStreamUri/>\r\n"
                                      "  </SOAP-ENV:Body>\r\n"
                                      "</SOAP-ENV:Envelope>";

        auto resStream = client.Post("/onvif/media_service", streamReq, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resStream && resStream->status == 200);
        EXPECT_TRUE(doc.load_string(resStream->body.c_str()));
        const auto uriNode = doc.select_node("//*[local-name()='Uri']").node();
        EXPECT_TRUE(uriNode && std::string(uriNode.text().as_string()) == "rtsp://127.0.0.1:8554/test_stream");
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
        EXPECT_TRUE(resMove && resMove->status == 200);
        EXPECT_TRUE(mockHandler->moveCount == 1);
        EXPECT_TRUE(std::abs(mockHandler->lastPan - 0.75f) < 0.001f);
        EXPECT_TRUE(std::abs(mockHandler->lastTilt - (-0.5f)) < 0.001f);
        EXPECT_TRUE(std::abs(mockHandler->lastZoom - 0.2f) < 0.001f);

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
        EXPECT_TRUE(resStop && resStop->status == 200);
        EXPECT_TRUE(mockHandler->stopCount == 1);
        EXPECT_TRUE(mockHandler->lastStopPt == true);
        EXPECT_TRUE(mockHandler->lastStopZ == true);
    }

    server.stop();
    EXPECT_TRUE(!server.isRunning());

    std::cout << "[PASS] testHttpSoapEndpoints (Profile S)" << std::endl;
}

TEST(OnvifServerTest, ProfileTImagingAndEvents)
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
    EXPECT_TRUE(server.start());

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
        EXPECT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        EXPECT_TRUE(doc.load_string(res->body.c_str()));
        const auto bNode = doc.select_node("//*[local-name()='Brightness']").node();
        EXPECT_TRUE(bNode && std::abs(bNode.text().as_float() - 60.0f) < 0.1f);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(mockImaging->setSettingsCount == 1);
        EXPECT_TRUE(std::abs(mockImaging->settings.brightness - 85.0f) < 0.1f);
        EXPECT_TRUE(std::abs(mockImaging->settings.contrast - 45.0f) < 0.1f);
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
        EXPECT_TRUE(resMove && resMove->status == 200);
        EXPECT_TRUE(mockImaging->moveFocusCount == 1);
        EXPECT_TRUE(std::abs(mockImaging->lastFocusSpeed - 0.8f) < 0.01f);

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
        EXPECT_TRUE(resStop && resStop->status == 200);
        EXPECT_TRUE(mockImaging->stopFocusCount == 1);
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
        EXPECT_TRUE(resSub && resSub->status == 200);

        pugi::xml_document doc;
        EXPECT_TRUE(doc.load_string(resSub->body.c_str()));
        const auto addrNode = doc.select_node("//*[local-name()='Address']").node();
        EXPECT_TRUE(addrNode);
        const std::string subUrl = addrNode.text().as_string();
        EXPECT_TRUE(subUrl.find("/onvif/events/subscription/") != std::string::npos);

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
        EXPECT_TRUE(resPull && resPull->status == 200);

        pugi::xml_document pullDoc;
        EXPECT_TRUE(pullDoc.load_string(resPull->body.c_str()));
        const auto topicNode = pullDoc.select_node("//*[local-name()='Topic']").node();
        EXPECT_TRUE(
            topicNode && std::string(topicNode.text().as_string()) == "tns1:RuleEngine/CellMotionDetector/Motion");

        const auto dataNode = pullDoc.select_node("//*[local-name()='Data']/*[local-name()='SimpleItem']").node();
        EXPECT_TRUE(dataNode && std::string(dataNode.attribute("Value").as_string()) == "true");

        // Unsubscribe
        const std::string unsubReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                     "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                     "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\">\r\n"
                                     "  <SOAP-ENV:Body>\r\n"
                                     "    <wsnt:Unsubscribe/>\r\n"
                                     "  </SOAP-ENV:Body>\r\n"
                                     "</SOAP-ENV:Envelope>";

        auto resUnsub = client.Post(subPath.c_str(), unsubReq, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resUnsub && resUnsub->status == 200);
    }

    server.stop();
    EXPECT_TRUE(!server.isRunning());
    std::cout << "[PASS] testProfileTImagingAndEvents" << std::endl;
}

TEST(OnvifServerTest, PelcoDPtzAdapter)
{
    std::cout << "[RUN] testPelcoDPtzAdapter..." << std::endl;

    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, 1U);
    EXPECT_TRUE(device->start());

    PelcoDPtzAdapter adapter(device);

    // Continuous move right + up
    adapter.handleContinuousMove(1.0f, 0.5f, 0.0f);
    // Stop
    adapter.handleStop(true, true);

    // Absolute move
    adapter.handleAbsoluteMove(90.0f, 45.0f, 0.5f);

    // Presets
    const std::string tok = adapter.handleSetPreset("Preset A", "2");
    EXPECT_TRUE(tok == "2");

    auto presets = adapter.handleGetPresets();
    EXPECT_TRUE(presets.size() == 1);
    EXPECT_TRUE(presets[0].token == "2");

    // Test Profile T event emission on preset recall
    std::vector<OnvifEvent> capturedEvents;
    adapter.setEventPublisher([&capturedEvents](const OnvifEvent& ev) { capturedEvents.push_back(ev); });

    EXPECT_TRUE(adapter.handleGotoPreset("2"));
    EXPECT_TRUE(!capturedEvents.empty());
    EXPECT_TRUE(capturedEvents.back().topic == "tns1:PTZController/PTZPresets/Reached");
    EXPECT_TRUE(capturedEvents.back().sourceValue == "2");

    EXPECT_TRUE(adapter.handleRemovePreset("2"));
    EXPECT_TRUE(adapter.handleGetPresets().empty());

    // Optical Focus (Profile T)
    adapter.handleMoveFocus("VideoSource_1", 1.0f);
    adapter.handleStopFocus("VideoSource_1");

    // Imaging Settings
    ImagingSettings imgSettings {};
    imgSettings.brightness = 75.0f;
    imgSettings.backlightCompensation = true;
    EXPECT_TRUE(adapter.handleSetImagingSettings("VideoSource_1", imgSettings));
    const auto readSettings = adapter.handleGetImagingSettings("VideoSource_1");
    EXPECT_TRUE(std::abs(readSettings.brightness - 75.0f) < 0.1f);
    EXPECT_TRUE(readSettings.backlightCompensation == true);

    [[maybe_unused]] const auto status = adapter.handleGetStatus();

    device->stop();
    std::cout << "[PASS] testPelcoDPtzAdapter" << std::endl;
}

TEST(OnvifServerTest, PresetToursServerAndAdapter)
{
    std::cout << "[RUN] testPresetToursServerAndAdapter..." << std::endl;

    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, 1U);
    EXPECT_TRUE(device->start());

    auto adapter = std::make_shared<PelcoDPtzAdapter>(device);
    const std::string testDbPath = "test_tours.json";
    std::remove(testDbPath.c_str());
    adapter->setPersistencePath(testDbPath);

    OnvifServerConfig config;
    config.bindAddress = "127.0.0.1";
    config.port = 18082; // Unique port
    config.deviceName = "Preset Tour Test Camera";

    OnvifServer server(config, adapter, adapter);
    EXPECT_TRUE(server.start());

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
        EXPECT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        EXPECT_TRUE(doc.load_string(res->body.c_str()));
        EXPECT_TRUE(doc.select_node("//*[local-name()='GetPresetTourOptionsResponse']"));
        EXPECT_TRUE(doc.select_node("//*[local-name()='Options']"));
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
        EXPECT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        EXPECT_TRUE(doc.load_string(res->body.c_str()));
        const auto tokenNode = doc.select_node("//*[local-name()='PresetTourToken']").node();
        EXPECT_TRUE(tokenNode);
        tourToken = tokenNode.text().as_string();
        EXPECT_TRUE(!tourToken.empty());
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
        EXPECT_TRUE(res && res->status == 200);
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
        EXPECT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        EXPECT_TRUE(doc.load_string(res->body.c_str()));
        const auto tourNode = doc.select_node("//*[local-name()='PresetTour']").node();
        EXPECT_TRUE(tourNode);
        EXPECT_TRUE(std::string(tourNode.attribute("token").as_string()) == tourToken);
        const auto nameNode = doc.select_node("//*[local-name()='Name']").node();
        EXPECT_TRUE(nameNode && std::string(nameNode.text().as_string()) == "Perimeter Scan");
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
        EXPECT_TRUE(res && res->status == 200);
    }

    // 6. Test Persistence reload
    {
        PelcoDPtzAdapter adapterReloaded(device);
        adapterReloaded.setPersistencePath(testDbPath);
        const auto loadedTours = adapterReloaded.handleGetPresetTours();
        EXPECT_TRUE(loadedTours.size() == 1);
        EXPECT_TRUE(loadedTours[0].token == tourToken);
        EXPECT_TRUE(loadedTours[0].name == "Perimeter Scan");
        EXPECT_TRUE(loadedTours[0].spots.size() == 1);
        EXPECT_TRUE(loadedTours[0].spots[0].presetToken == "1");
        EXPECT_TRUE(loadedTours[0].spots[0].stayTimeSeconds == 5);
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
        EXPECT_TRUE(res && res->status == 200);

        const auto remaining = adapter->handleGetPresetTours();
        EXPECT_TRUE(remaining.empty());
    }

    server.stop();
    EXPECT_TRUE(!server.isRunning());
    device->stop();

    // Clean up test file
    std::remove(testDbPath.c_str());

    std::cout << "[PASS] testPresetToursServerAndAdapter" << std::endl;
}

TEST(OnvifServerTest, PtzServiceExtensionsServerAndAdapter)
{
    std::cout << "[RUN] testPtzServiceExtensionsServerAndAdapter..." << std::endl;

    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, 1U);
    EXPECT_TRUE(device->start());

    auto adapter = std::make_shared<PelcoDPtzAdapter>(device);

    OnvifServerConfig config;
    config.bindAddress = "127.0.0.1";
    config.port = 18083; // Unique port
    config.deviceName = "PTZ Extensions Test Camera";

    OnvifServer server(config, adapter, adapter);
    EXPECT_TRUE(server.start());

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
        EXPECT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        EXPECT_TRUE(doc.load_string(res->body.c_str()));
        const auto homeNode = doc.select_node("//*[local-name()='HomeSupported']").node();
        EXPECT_TRUE(homeNode && homeNode.text().as_bool());
        const auto auxNodes = doc.select_nodes("//*[local-name()='AuxiliaryCommands']");
        EXPECT_TRUE(auxNodes.size() >= 4);
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
        EXPECT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        EXPECT_TRUE(doc.load_string(res->body.c_str()));
        EXPECT_TRUE(doc.select_node("//*[local-name()='GetConfigurationOptionsResponse']"));
        EXPECT_TRUE(doc.select_node("//*[local-name()='PTZConfigurationOptions']"));
        EXPECT_TRUE(doc.select_node("//*[local-name()='Spaces']"));
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
        EXPECT_TRUE(res && res->status == 200);

        pugi::xml_document doc;
        EXPECT_TRUE(doc.load_string(res->body.c_str()));
        EXPECT_TRUE(doc.select_node("//*[local-name()='RelativeMoveResponse']"));
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
        EXPECT_TRUE(resSet && resSet->status == 200);

        pugi::xml_document docSet;
        EXPECT_TRUE(docSet.load_string(resSet->body.c_str()));
        EXPECT_TRUE(docSet.select_node("//*[local-name()='SetHomePositionResponse']"));

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
        EXPECT_TRUE(resGoto && resGoto->status == 200);

        pugi::xml_document docGoto;
        EXPECT_TRUE(docGoto.load_string(resGoto->body.c_str()));
        EXPECT_TRUE(docGoto.select_node("//*[local-name()='GotoHomePositionResponse']"));
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
            EXPECT_TRUE(res && res->status == 200);

            pugi::xml_document doc;
            EXPECT_TRUE(doc.load_string(res->body.c_str()));
            const auto respNode = doc.select_node("//*[local-name()='AuxiliaryResponse']").node();
            EXPECT_TRUE(respNode);
            EXPECT_TRUE(std::string(respNode.text().as_string()) == cmd);
        }
    }

    server.stop();
    EXPECT_TRUE(!server.isRunning());
    device->stop();

    std::cout << "[PASS] testPtzServiceExtensionsServerAndAdapter" << std::endl;
}

TEST(OnvifServerTest, Media2OsdAndAnalytics)
{
    OnvifServerConfig config;
    config.port = 18588;
    config.bindAddress = "127.0.0.1";
    config.deviceName = "Media2Cam";
    config.model = "ONVIF-M2";
    config.rtspStreamUri = "rtsp://127.0.0.1:8554/live2";

    OnvifServer server(config);
    EXPECT_TRUE(server.start());
    EXPECT_TRUE(server.isRunning());

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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("http://www.onvif.org/ver20/media/wsdl") != std::string::npos);
        EXPECT_TRUE(res->body.find("/onvif/media2_service") != std::string::npos);
        EXPECT_TRUE(res->body.find("http://www.onvif.org/ver20/analytics/wsdl") != std::string::npos);
        EXPECT_TRUE(res->body.find("/onvif/analytics_service") != std::string::npos);
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
        EXPECT_TRUE(resProf && resProf->status == 200);
        pugi::xml_document docProf;
        EXPECT_TRUE(docProf.load_string(resProf->body.c_str()));
        EXPECT_TRUE(docProf.select_node("//*[local-name()='GetProfilesResponse']"));
        EXPECT_TRUE(docProf.select_node("//*[local-name()='Profiles']"));
        EXPECT_TRUE(docProf.select_node("//*[local-name()='VideoEncoder']"));

        // GetStreamUri
        const std::string reqUri
            = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
              "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\r\n"
              "  <SOAP-ENV:Body><tr2:GetStreamUri><tr2:ProfileToken>ProfileToken_1</tr2:ProfileToken></"
              "tr2:GetStreamUri></SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto resUri = client.Post("/onvif/media2_service", reqUri, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resUri && resUri->status == 200);
        EXPECT_TRUE(resUri->body.find("rtsp://127.0.0.1:8554/live2") != std::string::npos);

        // GetVideoEncoderConfigurations
        const std::string reqEnc = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body><tr2:GetVideoEncoderConfigurations/></SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resEnc = client.Post("/onvif/media2_service", reqEnc, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resEnc && resEnc->status == 200);
        pugi::xml_document docEnc;
        EXPECT_TRUE(docEnc.load_string(resEnc->body.c_str()));
        EXPECT_TRUE(docEnc.select_node("//*[local-name()='Encoding']"));

        // GetServiceCapabilities
        const std::string reqCap = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body><tr2:GetServiceCapabilities/></SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resCap = client.Post("/onvif/media2_service", reqCap, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resCap && resCap->status == 200);
        EXPECT_TRUE(resCap->body.find("OSD=\"true\"") != std::string::npos);
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
        EXPECT_TRUE(resOpt && resOpt->status == 200);
        EXPECT_TRUE(resOpt->body.find("PositionOption") != std::string::npos);

        // GetOSDs (should include default OSD_1)
        const std::string reqList = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body><trt:GetOSDs/></SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";
        auto resList = client.Post("/onvif/media_service", reqList, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resList && resList->status == 200);
        EXPECT_TRUE(resList->body.find("OSD_1") != std::string::npos);

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
        EXPECT_TRUE(resCreate && resCreate->status == 200);
        EXPECT_TRUE(resCreate->body.find("OSD_TEST") != std::string::npos);

        // GetOSD
        const std::string reqGet
            = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
              "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">\r\n"
              "  <SOAP-ENV:Body><trt:GetOSD><trt:OSDToken>OSD_TEST</trt:OSDToken></trt:GetOSD></SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto resGet = client.Post("/onvif/media_service", reqGet, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resGet && resGet->status == 200);
        EXPECT_TRUE(resGet->body.find("East Gate") != std::string::npos);
        EXPECT_TRUE(resGet->body.find("LowerRight") != std::string::npos);

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
        EXPECT_TRUE(resSet && resSet->status == 200);

        // DeleteOSD
        const std::string reqDel
            = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
              "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">\r\n"
              "  "
              "<SOAP-ENV:Body><trt:DeleteOSD><trt:OSDToken>OSD_TEST</trt:OSDToken></trt:DeleteOSD></SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto resDel = client.Post("/onvif/media_service", reqDel, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resDel && resDel->status == 200);
    }

    // 4. Analytics Service (/onvif/analytics_service)
    {
        const std::string reqCap = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tan=\"http://www.onvif.org/ver20/analytics/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body><tan:GetServiceCapabilities/></SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resCap = client.Post("/onvif/analytics_service", reqCap, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resCap && resCap->status == 200);
        EXPECT_TRUE(resCap->body.find("RuleSupport=\"true\"") != std::string::npos);

        const std::string reqRules = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                     "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                     "xmlns:tan=\"http://www.onvif.org/ver20/analytics/wsdl\">\r\n"
                                     "  <SOAP-ENV:Body><tan:GetSupportedRules/></SOAP-ENV:Body>\r\n"
                                     "</SOAP-ENV:Envelope>";
        auto resRules = client.Post("/onvif/analytics_service", reqRules, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resRules && resRules->status == 200);
        EXPECT_TRUE(resRules->body.find("CellMotionDetector") != std::string::npos);
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
        EXPECT_TRUE(resSub && resSub->status == 200);
        EXPECT_TRUE(resSub->body.find("SubscribeResponse") != std::string::npos);
        EXPECT_TRUE(resSub->body.find("SubscriptionReference") != std::string::npos);

        // Publish event to exercise push path
        OnvifEvent ev;
        ev.topic = "tns1:RuleEngine/CellMotionDetector/Motion";
        ev.dataName = "IsMotion";
        ev.dataValue = "true";
        server.publishEvent(ev);
    }

    server.stop();
    EXPECT_TRUE(!server.isRunning());
    std::cout << "[PASS] testMedia2OsdAndAnalytics" << std::endl;
}

TEST(OnvifServerTest, DeviceManagementAndSecurity)
{
    std::cout << "[RUN] testDeviceManagementAndSecurity" << std::endl;

    OnvifServerConfig config;
    config.port = 18591;
    config.deviceName = "DeviceMgmtCamera";

    OnvifServer server(config);
    EXPECT_TRUE(server.start());
    EXPECT_TRUE(server.isRunning());

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
        EXPECT_TRUE(res && res->status == 200);
        pugi::xml_document doc;
        EXPECT_TRUE(doc.load_string(res->body.c_str()));
        const auto users = doc.select_nodes("//*[local-name()='User']");
        EXPECT_TRUE(users.size() >= 2);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("CreateUsersResponse") != std::string::npos);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("guard1") != std::string::npos);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("SetUserResponse") != std::string::npos);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("DeleteUsersResponse") != std::string::npos);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("NetworkInterfaces token=\"eth0\"") != std::string::npos);

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
        EXPECT_TRUE(resSet && resSet->status == 200);
        EXPECT_TRUE(resSet->body.find("SetNetworkInterfacesResponse") != std::string::npos);

        auto resVerify = client.Post("/onvif/device_service", getReq, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resVerify && resVerify->status == 200);
        EXPECT_TRUE(resVerify->body.find("<tt:MTU>1400</tt:MTU>") != std::string::npos);
        EXPECT_TRUE(resVerify->body.find("<tt:Address>10.0.0.50</tt:Address>") != std::string::npos);
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
        EXPECT_TRUE(resGw && resGw->status == 200);

        const std::string getGwReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                     "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                     "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                     "  <SOAP-ENV:Body>\r\n"
                                     "    <tds:GetNetworkDefaultGateway/>\r\n"
                                     "  </SOAP-ENV:Body>\r\n"
                                     "</SOAP-ENV:Envelope>";
        auto resGet = client.Post("/onvif/device_service", getGwReq, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resGet && resGet->status == 200);
        EXPECT_TRUE(resGet->body.find("<tt:IPv4Address>10.0.0.1</tt:IPv4Address>") != std::string::npos);
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
        EXPECT_TRUE(resDns && resDns->status == 200);

        const std::string getDns = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <tds:GetDNS/>\r\n"
                                   "  </SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resGetDns = client.Post("/onvif/device_service", getDns, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resGetDns && resGetDns->status == 200);
        EXPECT_TRUE(resGetDns->body.find("9.9.9.9") != std::string::npos);

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
        EXPECT_TRUE(resNtp && resNtp->status == 200);

        const std::string getNtp = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <tds:GetNTP/>\r\n"
                                   "  </SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resGetNtp = client.Post("/onvif/device_service", getNtp, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resGetNtp && resGetNtp->status == 200);
        EXPECT_TRUE(resGetNtp->body.find("time.cloudflare.com") != std::string::npos);
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
        EXPECT_TRUE(resHn && resHn->status == 200);

        const std::string getHn = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                  "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                  "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                  "  <SOAP-ENV:Body>\r\n"
                                  "    <tds:GetHostname/>\r\n"
                                  "  </SOAP-ENV:Body>\r\n"
                                  "</SOAP-ENV:Envelope>";
        auto resGetHn = client.Post("/onvif/device_service", getHn, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resGetHn && resGetHn->status == 200);
        EXPECT_TRUE(resGetHn->body.find("PTZ-Camera-West") != std::string::npos);
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
        EXPECT_TRUE(resDt && resDt->status == 200);
        EXPECT_TRUE(resDt->body.find("SetSystemDateAndTimeResponse") != std::string::npos);
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
        EXPECT_TRUE(resAdd && resAdd->status == 200);

        const std::string getSc = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                  "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                  "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                  "  <SOAP-ENV:Body>\r\n"
                                  "    <tds:GetScopes/>\r\n"
                                  "  </SOAP-ENV:Body>\r\n"
                                  "</SOAP-ENV:Envelope>";
        auto resGet = client.Post("/onvif/device_service", getSc, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resGet && resGet->status == 200);
        EXPECT_TRUE(resGet->body.find("Sector4") != std::string::npos);

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
        EXPECT_TRUE(resRem && resRem->status == 200);
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
        EXPECT_TRUE(resReboot && resReboot->status == 200);
        EXPECT_TRUE(resReboot->body.find("SystemRebootResponse") != std::string::npos);

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
        EXPECT_TRUE(resFactory && resFactory->status == 200);
        EXPECT_TRUE(resFactory->body.find("SetSystemFactoryDefaultResponse") != std::string::npos);
    }

    server.stop();
    EXPECT_TRUE(!server.isRunning());
    std::cout << "[PASS] testDeviceManagementAndSecurity" << std::endl;
}

TEST(OnvifServerTest, ImagingExtensionsAndDeviceIo)
{
    std::cout << "[RUN] testImagingExtensionsAndDeviceIo..." << std::endl;

    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, 1U);
    EXPECT_TRUE(device->start());

    auto adapter = std::make_shared<PelcoDPtzAdapter>(device);

    OnvifServerConfig config;
    config.port = 18595;
    config.bindAddress = "127.0.0.1";
    config.deviceName = "ImagingDeviceIoCamera";

    OnvifServer server(config, adapter, adapter);
    server.setDeviceIoHandler(adapter);
    EXPECT_TRUE(server.start());
    EXPECT_TRUE(server.isRunning());

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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("/onvif/imaging_service") != std::string::npos);
        EXPECT_TRUE(res->body.find("/onvif/deviceio_service") != std::string::npos);
    }

    // 2. GetServices: check deviceIO and imaging
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tds:GetServices/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("http://www.onvif.org/ver10/deviceIO/wsdl") != std::string::npos);
        EXPECT_TRUE(res->body.find("/onvif/deviceio_service") != std::string::npos);
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
        EXPECT_TRUE(res && res->status == 200);
        pugi::xml_document doc;
        EXPECT_TRUE(doc.load_string(res->body.c_str()));
        EXPECT_TRUE(doc.select_node("//*[local-name()='FocusStatus20']"));
        EXPECT_TRUE(doc.select_node("//*[local-name()='MoveStatus']"));
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
        EXPECT_TRUE(resCont && resCont->status == 200);

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
        EXPECT_TRUE(resAbs && resAbs->status == 200);

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
        EXPECT_TRUE(resStop && resStop->status == 200);
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
        EXPECT_TRUE(resGet && resGet->status == 200);
        EXPECT_TRUE(resGet->body.find("Preset_Clear") != std::string::npos);
        EXPECT_TRUE(resGet->body.find("Preset_BW") != std::string::npos);

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
        EXPECT_TRUE(resSet && resSet->status == 200);
    }

    // 6. DeviceIO Service: GetRelayOutputs
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tmd:GetRelayOutputs/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/deviceio_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("Relay_1") != std::string::npos);
        EXPECT_TRUE(res->body.find("Relay_2") != std::string::npos);
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
        EXPECT_TRUE(resActive && resActive->status == 200);

        const auto relaysAfterActive = adapter->handleGetRelayOutputs();
        EXPECT_TRUE(!relaysAfterActive.empty() && relaysAfterActive[0].logicalState == RelayLogicalState::Active);

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
        EXPECT_TRUE(resInactive && resInactive->status == 200);

        const auto relaysAfterInactive = adapter->handleGetRelayOutputs();
        EXPECT_TRUE(!relaysAfterInactive.empty() && relaysAfterInactive[0].logicalState == RelayLogicalState::Inactive);
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
        EXPECT_TRUE(res && res->status == 200);

        const auto relays = adapter->handleGetRelayOutputs();
        EXPECT_TRUE(!relays.empty());
        EXPECT_TRUE(relays[0].mode == RelayMode::Monostable);
        EXPECT_TRUE(std::abs(relays[0].delayTimeSeconds - 2.0f) < 0.1f);
        EXPECT_TRUE(relays[0].idleState == RelayIdleState::Closed);
    }

    // 9. DeviceIO Service: GetDigitalInputs
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tmd:GetDigitalInputs/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/deviceio_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("Input_1") != std::string::npos);
    }

    server.stop();
    EXPECT_TRUE(!server.isRunning());
    device->stop();

    std::cout << "[PASS] testImagingExtensionsAndDeviceIo" << std::endl;
}

TEST(OnvifServerTest, MetadataStreamsAndMaintenanceExtensions)
{
    std::cout << "[RUN] testMetadataStreamsAndMaintenanceExtensions" << std::endl;

    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, 1U);
    EXPECT_TRUE(device->start());

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
    EXPECT_TRUE(server.start());
    EXPECT_TRUE(server.isRunning());

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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("GetMetadataConfigurationsResponse") != std::string::npos);
        EXPECT_TRUE(res->body.find("MetadataConfig_1") != std::string::npos);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("SetMetadataConfigurationResponse") != std::string::npos);

        const auto cfg = adapter->handleGetMetadataConfiguration("MetadataConfig_1");
        EXPECT_TRUE(cfg.has_value());
        EXPECT_TRUE(cfg->name == "UpdatedMetadata");
        EXPECT_TRUE(cfg->eventsEnabled == true);
    }

    // 3. GET /onvif/metadata_stream
    {
        auto res = client.Get("/onvif/metadata_stream");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("<tt:MetadataStream") != std::string::npos);
        EXPECT_TRUE(res->body.find("<tt:PTZStatus>") != std::string::npos);
        EXPECT_TRUE(res->body.find("<tt:VideoAnalytics>") != std::string::npos);
        EXPECT_TRUE(res->body.find("ObjectId=\"101\"") != std::string::npos);
        EXPECT_TRUE(res->body.find("Vehicle") != std::string::npos);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("GetSystemLogResponse") != std::string::npos);
        EXPECT_TRUE(res->body.find("SystemLog") != std::string::npos);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("GetSystemLogResponse") != std::string::npos);
        EXPECT_TRUE(
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("GetSystemSupportInformationResponse") != std::string::npos);
        EXPECT_TRUE(res->body.find("SupportInformation") != std::string::npos);
    }

    // 7. Device Management: GetSystemBackup & RestoreSystem
    {
        const std::string reqBackup = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                      "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                      "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                      "  <SOAP-ENV:Body><tds:GetSystemBackup/></SOAP-ENV:Body>\r\n"
                                      "</SOAP-ENV:Envelope>";
        auto resBackup = client.Post("/onvif/device_service", reqBackup, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resBackup && resBackup->status == 200);
        EXPECT_TRUE(resBackup->body.find("GetSystemBackupResponse") != std::string::npos);
        EXPECT_TRUE(resBackup->body.find("BackupFiles") != std::string::npos);

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
        EXPECT_TRUE(resRestore && resRestore->status == 200);
        EXPECT_TRUE(resRestore->body.find("RestoreSystemResponse") != std::string::npos);
    }

    // 8. Device Management: GetEndpointReference
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tds:GetEndpointReference/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("GetEndpointReferenceResponse") != std::string::npos);
        EXPECT_TRUE(res->body.find("GUID") != std::string::npos);
    }

    server.stop();
    EXPECT_TRUE(!server.isRunning());
    device->stop();

    std::cout << "[PASS] testMetadataStreamsAndMaintenanceExtensions" << std::endl;
}

TEST(OnvifServerTest, ProfileGAndPkiCertificates)
{
    std::cout << "[RUN] testProfileGAndPkiCertificates" << std::endl;

    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, 1U);
    EXPECT_TRUE(device->start());

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
    EXPECT_TRUE(server.start());
    EXPECT_TRUE(server.isRunning());

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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("CreateCertificateResponse") != std::string::npos);
        pugi::xml_document doc;
        EXPECT_TRUE(doc.load_string(res->body.c_str()));
        const pugi::xml_node idNode = doc.select_node("//*[local-name()='CertificateID']").node();
        EXPECT_TRUE(idNode && std::string(idNode.text().as_string()) == "cert_pki_1");
        const pugi::xml_node dataNode = doc.select_node("//*[local-name()='Data']").node();
        EXPECT_TRUE(dataNode && !std::string(dataNode.text().as_string()).empty());
    }

    // 2. Device Management: GetCertificates
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tds:GetCertificates/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("GetCertificatesResponse") != std::string::npos);
        EXPECT_TRUE(res->body.find("cert_pki_1") != std::string::npos);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("GetCertificateInformationResponse") != std::string::npos);
        EXPECT_TRUE(res->body.find("ProfileGCam") != std::string::npos);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("GetPkcs10RequestResponse") != std::string::npos);
        EXPECT_TRUE(res->body.find("Pkcs10Request") != std::string::npos);
    }

    // 5. Device Management: ClientCertificateMode (Get, Set, Get)
    {
        const std::string reqGet = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body><tds:GetClientCertificateMode/></SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resGet = client.Post("/onvif/device_service", reqGet, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resGet && resGet->status == 200);
        EXPECT_TRUE(resGet->body.find("GetClientCertificateModeResponse") != std::string::npos);

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
        EXPECT_TRUE(resSet && resSet->status == 200);
        EXPECT_TRUE(resSet->body.find("SetClientCertificateModeResponse") != std::string::npos);

        auto resGet2 = client.Post("/onvif/device_service", reqGet, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resGet2 && resGet2->status == 200);
        EXPECT_TRUE(resGet2->body.find("Optional") != std::string::npos);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("DeleteCertificatesResponse") != std::string::npos);
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
        EXPECT_TRUE(resCaps && resCaps->status == 200);
        EXPECT_TRUE(resCaps->body.find("GetServiceCapabilitiesResponse") != std::string::npos);
        EXPECT_TRUE(resCaps->body.find("DynamicRecordings") != std::string::npos);

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
        EXPECT_TRUE(resCreate && resCreate->status == 200);
        EXPECT_TRUE(resCreate->body.find("CreateRecordingResponse") != std::string::npos);
        pugi::xml_document doc;
        EXPECT_TRUE(doc.load_string(resCreate->body.c_str()));
        const pugi::xml_node tNode = doc.select_node("//*[local-name()='RecordingToken']").node();
        EXPECT_TRUE(tNode);
        recToken = tNode.text().as_string();
        EXPECT_TRUE(!recToken.empty());
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
        EXPECT_TRUE(resGet && resGet->status == 200);
        EXPECT_TRUE(resGet->body.find(recToken) != std::string::npos);

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
        EXPECT_TRUE(resTrk && resTrk->status == 200);
        EXPECT_TRUE(resTrk->body.find("CreateTrackResponse") != std::string::npos);
        pugi::xml_document doc;
        EXPECT_TRUE(doc.load_string(resTrk->body.c_str()));
        const pugi::xml_node tNode = doc.select_node("//*[local-name()='TrackToken']").node();
        EXPECT_TRUE(tNode);
        trkToken = tNode.text().as_string();
        EXPECT_TRUE(!trkToken.empty());
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
        EXPECT_TRUE(resJob && resJob->status == 200);
        EXPECT_TRUE(resJob->body.find("CreateRecordingJobResponse") != std::string::npos);
        pugi::xml_document doc;
        EXPECT_TRUE(doc.load_string(resJob->body.c_str()));
        const pugi::xml_node tNode = doc.select_node("//*[local-name()='JobToken']").node();
        EXPECT_TRUE(tNode);
        jobToken = tNode.text().as_string();
        EXPECT_TRUE(!jobToken.empty());

        const std::string reqGet = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:trc=\"http://www.onvif.org/ver10/recording/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body><trc:GetRecordingJobs/></SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resGet = client.Post("/onvif/recording_service", reqGet, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resGet && resGet->status == 200);
        EXPECT_TRUE(resGet->body.find(jobToken) != std::string::npos);

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
        EXPECT_TRUE(resMode && resMode->status == 200);
        EXPECT_TRUE(resMode->body.find("SetRecordingJobModeResponse") != std::string::npos);
    }

    // 10. Recording Service: GetRecordingSummary
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:trc=\"http://www.onvif.org/ver10/recording/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><trc:GetRecordingSummary/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/recording_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("GetRecordingSummaryResponse") != std::string::npos);
        EXPECT_TRUE(res->body.find("NumberRecordings") != std::string::npos);
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
        EXPECT_TRUE(resCaps && resCaps->status == 200);
        EXPECT_TRUE(resCaps->body.find("GetServiceCapabilitiesResponse") != std::string::npos);

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
        EXPECT_TRUE(resFind && resFind->status == 200);
        EXPECT_TRUE(resFind->body.find("FindRecordingsResponse") != std::string::npos);
        pugi::xml_document doc;
        EXPECT_TRUE(doc.load_string(resFind->body.c_str()));
        const pugi::xml_node tNode = doc.select_node("//*[local-name()='SearchToken']").node();
        EXPECT_TRUE(tNode);
        recSearchToken = tNode.text().as_string();
        EXPECT_TRUE(!recSearchToken.empty());

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
        EXPECT_TRUE(resResults && resResults->status == 200);
        EXPECT_TRUE(resResults->body.find("GetRecordingSearchResultsResponse") != std::string::npos);
        EXPECT_TRUE(resResults->body.find("ResultList") != std::string::npos);
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
        EXPECT_TRUE(resFind && resFind->status == 200);
        EXPECT_TRUE(resFind->body.find("FindEventsResponse") != std::string::npos);
        pugi::xml_document doc;
        EXPECT_TRUE(doc.load_string(resFind->body.c_str()));
        const pugi::xml_node tNode = doc.select_node("//*[local-name()='SearchToken']").node();
        EXPECT_TRUE(tNode);
        const std::string evSearchToken = tNode.text().as_string();
        EXPECT_TRUE(!evSearchToken.empty());

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
        EXPECT_TRUE(resResults && resResults->status == 200);
        EXPECT_TRUE(resResults->body.find("GetEventSearchResultsResponse") != std::string::npos);
        EXPECT_TRUE(resResults->body.find("ResultList") != std::string::npos);

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
        EXPECT_TRUE(resEnd && resEnd->status == 200);
        EXPECT_TRUE(resEnd->body.find("EndSearchResponse") != std::string::npos);
    }

    // 13. Replay Service: GetServiceCapabilities, GetReplayUri, Get/Set ReplayConfiguration
    {
        const std::string reqCaps = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:trp=\"http://www.onvif.org/ver10/replay/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body><trp:GetServiceCapabilities/></SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";
        auto resCaps = client.Post("/onvif/replay_service", reqCaps, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resCaps && resCaps->status == 200);
        EXPECT_TRUE(resCaps->body.find("GetServiceCapabilitiesResponse") != std::string::npos);
        EXPECT_TRUE(resCaps->body.find("ReversePlayback") != std::string::npos);

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
        EXPECT_TRUE(resUri && resUri->status == 200);
        EXPECT_TRUE(resUri->body.find("GetReplayUriResponse") != std::string::npos);
        EXPECT_TRUE(resUri->body.find("rtsp://") != std::string::npos);

        const std::string reqGetCfg = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                      "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                      "xmlns:trp=\"http://www.onvif.org/ver10/replay/wsdl\">\r\n"
                                      "  <SOAP-ENV:Body><trp:GetReplayConfiguration/></SOAP-ENV:Body>\r\n"
                                      "</SOAP-ENV:Envelope>";
        auto resGetCfg = client.Post("/onvif/replay_service", reqGetCfg, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resGetCfg && resGetCfg->status == 200);
        EXPECT_TRUE(resGetCfg->body.find("GetReplayConfigurationResponse") != std::string::npos);

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
        EXPECT_TRUE(resSetCfg && resSetCfg->status == 200);
        EXPECT_TRUE(resSetCfg->body.find("SetReplayConfigurationResponse") != std::string::npos);

        auto resGetCfg2 = client.Post("/onvif/replay_service", reqGetCfg, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resGetCfg2 && resGetCfg2->status == 200);
        EXPECT_TRUE(resGetCfg2->body.find("PT120S") != std::string::npos);
    }

    server.stop();
    EXPECT_TRUE(!server.isRunning());
    device->stop();

    std::cout << "[PASS] testProfileGAndPkiCertificates" << std::endl;
}

TEST(OnvifServerTest, VideoAnalyticsRuleEngineAndEvaluation)
{
    std::cout << "[RUN] testVideoAnalyticsRuleEngineAndEvaluation" << std::endl;

    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, 1U);
    EXPECT_TRUE(device->start());
    auto adapter = std::make_shared<PelcoDPtzAdapter>(device);

    OnvifServerConfig config;
    config.bindAddress = "127.0.0.1";
    config.port = 18090;

    OnvifServer server(config, adapter, adapter);
    server.setAnalyticsHandler(adapter);
    EXPECT_TRUE(server.start());
    EXPECT_TRUE(server.isRunning());

    httplib::Client client("127.0.0.1", 18090);

    // 1. GetServiceCapabilities
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tan=\"http://www.onvif.org/ver20/analytics/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tan:GetServiceCapabilities/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/analytics_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("RuleSupport=\"true\"") != std::string::npos);
        EXPECT_TRUE(res->body.find("AnalyticsModuleSupport=\"true\"") != std::string::npos);
    }

    // 2. GetSupportedRules
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tan=\"http://www.onvif.org/ver20/analytics/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tan:GetSupportedRules/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/analytics_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("tt:LineDetector") != std::string::npos);
        EXPECT_TRUE(res->body.find("tt:FieldDetector") != std::string::npos);
        EXPECT_TRUE(res->body.find("tt:LoiteringDetector") != std::string::npos);
    }

    // 3. CreateRules (Tripwire LineDetector + FieldDetector)
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("CreateRulesResponse") != std::string::npos);
    }

    // 4. GetRules verify persistence
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tan=\"http://www.onvif.org/ver20/analytics/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tan:GetRules/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/analytics_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("PerimeterTripwire") != std::string::npos);
        EXPECT_TRUE(res->body.find("ZoneIntrusion") != std::string::npos);
    }

    // 5. Test Geometric Rule Evaluation via Adapter & Event Publishing
    std::vector<OnvifEvent> emittedEvents;
    adapter->setEventPublisher([&emittedEvents](const OnvifEvent& ev) { emittedEvents.push_back(ev); });

    // Frame 1: Object 1 (Human) at x=0.30 (left of vertical line x=0.50, inside zone [0.2, 0.8])
    AnalyticsObject obj1;
    obj1.objectId = 101;
    obj1.className = "Human";
    obj1.confidence = 0.85f;
    obj1.boundingBox = { 0.28f, 0.45f, 0.32f, 0.55f }; // center (0.30, 0.50)

    adapter->setDetectedObjects({ obj1 });

    // Should trigger ZoneIntrusion event because center is inside polygon [0.2, 0.8]
    EXPECT_TRUE(!emittedEvents.empty());
    bool zoneTriggered = false;
    for (const auto& ev : emittedEvents) {
        if (ev.topic == "tns1:RuleEngine/FieldDetector/ObjectsInside" && ev.sourceValue == "ZoneIntrusion") {
            zoneTriggered = true;
        }
    }
    EXPECT_TRUE(zoneTriggered);
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
    EXPECT_TRUE(tripwireTriggered);

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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("DeleteRulesResponse") != std::string::npos);

        auto rulesRemaining = adapter->handleGetRules("");
        EXPECT_TRUE(rulesRemaining.size() == 1U);
        EXPECT_TRUE(rulesRemaining[0].name == "ZoneIntrusion");
    }

    server.stop();
    EXPECT_TRUE(!server.isRunning());
    device->stop();

    std::cout << "[PASS] testVideoAnalyticsRuleEngineAndEvaluation" << std::endl;
}

TEST(OnvifServerTest, PtzGeoMoveAndSphericalSpaces)
{
    std::cout << "[RUN] testPtzGeoMoveAndSphericalSpaces..." << std::endl;

    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, 1U);
    EXPECT_TRUE(device->start());

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
    EXPECT_TRUE(server.start());
    EXPECT_TRUE(server.isRunning());

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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("PositionSphericalSpace") != std::string::npos);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("<tt:GeoMove>true</tt:GeoMove>") != std::string::npos);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("37.9838") != std::string::npos);
        EXPECT_TRUE(res->body.find("23.7275") != std::string::npos);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("SetGeoLocationResponse") != std::string::npos);

        const auto camLoc = adapter->cameraLocation();
        EXPECT_TRUE(std::fabs(camLoc.location.latitude - 38.0000) < 0.0001);
        EXPECT_TRUE(std::fabs(camLoc.location.longitude - 23.8000) < 0.0001);
        EXPECT_TRUE(std::fabs(camLoc.orientation.yaw - 90.0) < 0.01);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("GeoMoveResponse") != std::string::npos);

        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        const auto state = mockTransport->getInternalState();
        // 270 deg = 27000 centidegrees
        EXPECT_TRUE(std::abs(static_cast<int>(state.panCentidegrees) - 27000) < 50);
        EXPECT_TRUE(state.tiltCentidegrees == 0);
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
                                "        <tt:PanTilt x=\"180.0\" y=\"30.0\" "
                                "space=\"http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionSphericalSpace\"/>\r\n"
                                "      </tptz:Position>\r\n"
                                "    </tptz:AbsoluteMove>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";

        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("AbsoluteMoveResponse") != std::string::npos);

        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        const auto state = mockTransport->getInternalState();
        EXPECT_TRUE(state.panCentidegrees == 18000);
        EXPECT_TRUE(state.tiltCentidegrees == 3000);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("DeleteGeoLocationResponse") != std::string::npos);
    }

    server.stop();
    EXPECT_TRUE(!server.isRunning());
    device->stop();

    std::cout << "[PASS] testPtzGeoMoveAndSphericalSpaces" << std::endl;
}

TEST(OnvifServerTest, ProfileTPrivacyMasksAndVideoSourceModes)
{
    const int port = 18599;
    OnvifServerConfig config;
    config.port = port;
    config.deviceName = "Profile T Privacy & Source Modes Camera";

    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>();
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport);
    EXPECT_TRUE(device->start());

    auto adapter = std::make_shared<PelcoDPtzAdapter>(device);

    OnvifServer server(config, adapter, adapter);
    server.setMaskHandler(adapter);
    server.setVideoSourceModeHandler(adapter);
    EXPECT_TRUE(server.start());

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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("Mask=\"true\"") != std::string::npos);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("GetMaskOptionsResponse") != std::string::npos);
        EXPECT_TRUE(res->body.find("MaxMasks") != std::string::npos);
        EXPECT_TRUE(res->body.find("Rectangle=\"true\"") != std::string::npos);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("GetMasksResponse") != std::string::npos);
        EXPECT_TRUE(res->body.find("Mask_1") != std::string::npos);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("CreateMaskResponse") != std::string::npos);
        EXPECT_TRUE(res->body.find("Mask_New") != std::string::npos);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("GetMaskResponse") != std::string::npos);
        EXPECT_TRUE(res->body.find("Blurred") != std::string::npos);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("SetMaskResponse") != std::string::npos);

        // Verify changes took effect
        const auto maskOpt = adapter->handleGetMask("Mask_New");
        EXPECT_TRUE(maskOpt.has_value());
        EXPECT_TRUE(maskOpt->type == MaskType::Pixelated);
        EXPECT_TRUE(maskOpt->enabled == false);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("DeleteMaskResponse") != std::string::npos);

        EXPECT_TRUE(!adapter->handleGetMask("Mask_New").has_value());
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("GetVideoSourceModesResponse") != std::string::npos);
        EXPECT_TRUE(res->body.find("Mode_1080p60") != std::string::npos);
        EXPECT_TRUE(res->body.find("Mode_4k30") != std::string::npos);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("GetVideoSourceModesResponse") != std::string::npos);
        EXPECT_TRUE(res->body.find("Mode_1080p60") != std::string::npos);
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
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("SetVideoSourceModeResponse") != std::string::npos);
        EXPECT_TRUE(res->body.find("Reboot>true</") != std::string::npos);

        // Verify active mode in adapter
        const auto modes = adapter->handleGetVideoSourceModes("VideoSource_1");
        for (const auto& m : modes) {
            if (m.token == "Mode_4k30") {
                EXPECT_TRUE(m.enabled == true);
            } else {
                EXPECT_TRUE(m.enabled == false);
            }
        }
    }

    // 11. End-to-end OnvifClient calls against the running server
    {
        OnvifClient onvifClient("http://127.0.0.1:" + std::to_string(port) + "/onvif/device_service");
        EXPECT_TRUE(onvifClient.getCapabilities());

        const auto opts = onvifClient.getMaskOptions("VideoSource_1");
        EXPECT_TRUE(opts.has_value());
        EXPECT_TRUE(opts->maxMasks == 8);

        auto masks = onvifClient.getMasks("VideoSource_1");
        EXPECT_TRUE(masks.size() == 1U);
        EXPECT_TRUE(masks[0].token == "Mask_1");

        PrivacyMask clientMask {};
        clientMask.token = "Mask_Client";
        clientMask.configurationToken = "VideoSource_1";
        clientMask.type = MaskType::Color;
        clientMask.color = { 0, 255, 0, "RGB" };
        clientMask.enabled = true;
        clientMask.polygon = { { 0.1f, 0.1f }, { 0.5f, 0.5f } };

        const std::string createdTok = onvifClient.createMask(clientMask);
        EXPECT_TRUE(createdTok == "Mask_Client");

        masks = onvifClient.getMasks("VideoSource_1");
        EXPECT_TRUE(masks.size() == 2U);

        const auto singleMask = onvifClient.getMask("Mask_Client");
        EXPECT_TRUE(singleMask.has_value());
        EXPECT_TRUE(singleMask->token == "Mask_Client");

        clientMask.enabled = false;
        EXPECT_TRUE(onvifClient.setMask(clientMask));

        EXPECT_TRUE(onvifClient.deleteMask("Mask_Client"));
        masks = onvifClient.getMasks("VideoSource_1");
        EXPECT_TRUE(masks.size() == 1U);

        const auto modes = onvifClient.getVideoSourceModes("VideoSource_1");
        EXPECT_TRUE(modes.size() == 2U);

        EXPECT_TRUE(onvifClient.setVideoSourceMode("VideoSource_1", "Mode_1080p60"));
    }

    server.stop();
    EXPECT_TRUE(!server.isRunning());
    device->stop();

    std::cout << "[PASS] testProfileTPrivacyMasksAndVideoSourceModes" << std::endl;
}

TEST(OnvifServerTest, ThermalServiceAndRadiometry)
{
    const int port = 18585;
    OnvifServerConfig config;
    config.port = port;
    config.deviceName = "Thermal Test Camera";

    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>();
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport);
    EXPECT_TRUE(device->start());

    auto adapter = std::make_shared<PelcoDPtzAdapter>(device);

    std::vector<OnvifEvent> publishedEvents;
    adapter->setEventPublisher([&](const OnvifEvent& ev) { publishedEvents.push_back(ev); });

    OnvifServer server(config, adapter, adapter);
    server.setThermalHandler(adapter);

    EXPECT_TRUE(server.start());
    EXPECT_TRUE(server.isRunning());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    httplib::Client client("127.0.0.1", port);
    client.set_connection_timeout(5, 0);
    client.set_read_timeout(5, 0);

    // 1. GetCapabilities on /onvif/device_service - verify Thermal extension
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:GetCapabilities/>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("GetCapabilitiesResponse") != std::string::npos);
        EXPECT_TRUE(res->body.find("<tt:Thermal>") != std::string::npos);
        EXPECT_TRUE(res->body.find("/onvif/thermal_service") != std::string::npos);
    }

    // 2. GetServices on /onvif/device_service - verify Thermal WSDL
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tds:GetServices/>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("http://www.onvif.org/ver10/thermal/wsdl") != std::string::npos);
    }

    // 3. Thermal Service - GetServiceCapabilities
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tth=\"http://www.onvif.org/ver10/thermal/wsdl\">\r\n"
                                "  <SOAP-ENV:Body>\r\n"
                                "    <tth:GetServiceCapabilities/>\r\n"
                                "  </SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/thermal_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("GetServiceCapabilitiesResponse") != std::string::npos);
        EXPECT_TRUE(res->body.find("Radiometry=\"true\"") != std::string::npos);
        EXPECT_TRUE(res->body.find("ColorPalette=\"true\"") != std::string::npos);
        EXPECT_TRUE(res->body.find("NUC=\"true\"") != std::string::npos);
    }

    // 4. Thermal Service - GetRadiometryConfiguration & SetRadiometryConfiguration
    {
        const std::string getReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tth=\"http://www.onvif.org/ver10/thermal/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <tth:GetRadiometryConfiguration>\r\n"
                                   "      <tth:VideoSourceToken>VideoSource_1</tth:VideoSourceToken>\r\n"
                                   "    </tth:GetRadiometryConfiguration>\r\n"
                                   "  </SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/thermal_service", getReq, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("GetRadiometryConfigurationResponse") != std::string::npos);
        EXPECT_TRUE(res->body.find("<tth:Emissivity>") != std::string::npos);

        const std::string setReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tth=\"http://www.onvif.org/ver10/thermal/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <tth:SetRadiometryConfiguration>\r\n"
                                   "      <tth:VideoSourceToken>VideoSource_1</tth:VideoSourceToken>\r\n"
                                   "      <tth:Configuration>\r\n"
                                   "        <tth:Emissivity>0.96</tth:Emissivity>\r\n"
                                   "        <tth:Distance>12.5</tth:Distance>\r\n"
                                   "        <tth:ReflectedTemperature>24.0</tth:ReflectedTemperature>\r\n"
                                   "        <tth:AtmosphericTemperature>23.0</tth:AtmosphericTemperature>\r\n"
                                   "        <tth:RelativeHumidity>55.0</tth:RelativeHumidity>\r\n"
                                   "        <tth:WindowTransmission>0.95</tth:WindowTransmission>\r\n"
                                   "      </tth:Configuration>\r\n"
                                   "    </tth:SetRadiometryConfiguration>\r\n"
                                   "  </SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto setRes = client.Post("/onvif/thermal_service", setReq, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(setRes && setRes->status == 200);
        EXPECT_TRUE(setRes->body.find("SetRadiometryConfigurationResponse") != std::string::npos);

        const auto cfg = adapter->handleGetRadiometryConfiguration("VideoSource_1");
        EXPECT_TRUE(std::fabs(cfg.emissivity - 0.96f) < 0.001f);
        EXPECT_TRUE(std::fabs(cfg.distance - 12.5f) < 0.001f);
    }

    // 5. Thermal Service - GetColorPalettes & SetColorPalette
    {
        const std::string getReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tth=\"http://www.onvif.org/ver10/thermal/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <tth:GetColorPalettes>\r\n"
                                   "      <tth:VideoSourceToken>VideoSource_1</tth:VideoSourceToken>\r\n"
                                   "    </tth:GetColorPalettes>\r\n"
                                   "  </SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/thermal_service", getReq, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("GetColorPalettesResponse") != std::string::npos);
        EXPECT_TRUE(res->body.find("WhiteHot") != std::string::npos);
        EXPECT_TRUE(res->body.find("Ironbow") != std::string::npos);

        const std::string setReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tth=\"http://www.onvif.org/ver10/thermal/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <tth:SetColorPalette>\r\n"
                                   "      <tth:VideoSourceToken>VideoSource_1</tth:VideoSourceToken>\r\n"
                                   "      <tth:PaletteToken>Ironbow</tth:PaletteToken>\r\n"
                                   "    </tth:SetColorPalette>\r\n"
                                   "  </SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto setRes = client.Post("/onvif/thermal_service", setReq, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(setRes && setRes->status == 200);
        EXPECT_TRUE(setRes->body.find("SetColorPaletteResponse") != std::string::npos);
    }

    // 6. Thermal Service - TriggerNUC
    {
        const std::string nucReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tth=\"http://www.onvif.org/ver10/thermal/wsdl\">\r\n"
                                   "  <SOAP-ENV:Body>\r\n"
                                   "    <tth:TriggerNUC>\r\n"
                                   "      <tth:VideoSourceToken>VideoSource_1</tth:VideoSourceToken>\r\n"
                                   "    </tth:TriggerNUC>\r\n"
                                   "  </SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/thermal_service", nucReq, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_TRUE(res->body.find("TriggerNUCResponse") != std::string::npos);
    }

    // 7. Radiometry Alarm Threshold Event verification via SetRadiometryBoxes
    {
        RadiometryBox testBox {};
        testBox.token = "Box_1";
        testBox.label = "Overheat Zone";
        testBox.minTemperature = 30.0f;
        testBox.maxTemperature = 85.0f; // Above 50.0 default threshold
        testBox.avgTemperature = 65.0f;
        testBox.topLeft = { 0.1f, 0.1f };
        testBox.bottomRight = { 0.5f, 0.5f };

        publishedEvents.clear();
        EXPECT_TRUE(adapter->handleSetRadiometryBoxes("VideoSource_1", { testBox }));
        // Expect an alarm event was emitted because 85.0 >= 70.0
        EXPECT_TRUE(!publishedEvents.empty());
        EXPECT_TRUE(publishedEvents.back().topic.find("Thermal/Radiometry/HighTemperatureAlarm") != std::string::npos);
    }

    // 8. End-to-end OnvifClient calls against running server
    {
        OnvifClient onvifClient("http://127.0.0.1:" + std::to_string(port) + "/onvif/device_service");
        const auto caps = onvifClient.getCapabilities();
        EXPECT_TRUE(caps.has_value());
        EXPECT_TRUE(!caps->thermalXAddr.empty());

        // Radiometry config
        const auto radCfg = onvifClient.getRadiometryConfiguration("VideoSource_1");
        EXPECT_TRUE(radCfg.has_value());
        EXPECT_TRUE(std::fabs(radCfg->emissivity - 0.96f) < 0.001f);

        RadiometryConfig newCfg = *radCfg;
        newCfg.emissivity = 0.88f;
        EXPECT_TRUE(onvifClient.setRadiometryConfiguration("VideoSource_1", newCfg));

        // Color palettes
        const auto palettes = onvifClient.getColorPalettes("VideoSource_1");
        EXPECT_TRUE(!palettes.empty());
        EXPECT_TRUE(onvifClient.setColorPalette("VideoSource_1", "Rainbow"));

        // Spots
        RadiometrySpot spot1 {};
        spot1.token = "Spot_Client1";
        spot1.label = "Bearing";
        spot1.position = { 0.4f, 0.6f };
        spot1.temperature = 41.5f;
        EXPECT_TRUE(onvifClient.setRadiometrySpots("VideoSource_1", { spot1 }));

        const auto spots = onvifClient.getRadiometrySpots("VideoSource_1");
        EXPECT_TRUE(spots.size() == 1U);
        EXPECT_TRUE(spots[0].token == "Spot_Client1");
        EXPECT_TRUE(std::fabs(spots[0].temperature - 41.5f) < 0.001f);

        // Boxes
        RadiometryBox box1 {};
        box1.token = "Box_Client1";
        box1.label = "Transformer";
        box1.topLeft = { 0.2f, 0.2f };
        box1.bottomRight = { 0.8f, 0.8f };
        box1.minTemperature = 28.0f;
        box1.maxTemperature = 62.0f;
        box1.avgTemperature = 48.0f;
        EXPECT_TRUE(onvifClient.setRadiometryBoxes("VideoSource_1", { box1 }));

        const auto boxes = onvifClient.getRadiometryBoxes("VideoSource_1");
        EXPECT_TRUE(boxes.size() == 1U);
        EXPECT_TRUE(boxes[0].token == "Box_Client1");

        // Trigger NUC
        EXPECT_TRUE(onvifClient.triggerNuc("VideoSource_1"));
    }

    server.stop();
    EXPECT_TRUE(!server.isRunning());
    device->stop();

    std::cout << "[PASS] testThermalServiceAndRadiometry" << std::endl;
}
