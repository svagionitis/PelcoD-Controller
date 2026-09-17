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
    std::cout << "[RUN] testHttpSoapEndpoints..." << std::endl;

    OnvifServerConfig config;
    config.bindAddress = "127.0.0.1";
    config.port = 18080; // High test port
    config.deviceName = "Test Bridge Camera";
    config.manufacturer = "PelcoD-Test";
    config.model = "Model-XYZ";
    config.rtspStreamUri = "rtsp://127.0.0.1:8554/test_stream";

    auto mockHandler = std::make_shared<MockPtzHandler>();
    OnvifServer server(config, mockHandler);

    assert(server.start());
    assert(server.isRunning());

    // Give server thread a moment to bind and listen
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

    // 3. GetCapabilities
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

    // 6. PTZ Presets & Status
    {
        const std::string setPresetReq
            = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
              "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
              "  <SOAP-ENV:Body>\r\n"
              "    <tptz:SetPreset>\r\n"
              "      <tptz:PresetName>Gate View</tptz:PresetName>\r\n"
              "      <tptz:PresetToken>5</tptz:PresetToken>\r\n"
              "    </tptz:SetPreset>\r\n"
              "  </SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";

        auto resSet = client.Post("/onvif/ptz_service", setPresetReq, "application/soap+xml; charset=utf-8");
        assert(resSet && resSet->status == 200);
        assert(mockHandler->presets.size() == 1);
        assert(mockHandler->presets[0].token == "5");

        const std::string getPresetsReq
            = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
              "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
              "  <SOAP-ENV:Body>\r\n"
              "    <tptz:GetPresets/>\r\n"
              "  </SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";

        auto resGet = client.Post("/onvif/ptz_service", getPresetsReq, "application/soap+xml; charset=utf-8");
        assert(resGet && resGet->status == 200);
        pugi::xml_document doc;
        assert(doc.load_string(resGet->body.c_str()));
        assert(doc.select_node("//*[local-name()='Preset'][@token='5']"));

        const std::string statusReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                      "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                      "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                      "  <SOAP-ENV:Body>\r\n"
                                      "    <tptz:GetStatus/>\r\n"
                                      "  </SOAP-ENV:Body>\r\n"
                                      "</SOAP-ENV:Envelope>";

        auto resStatus = client.Post("/onvif/ptz_service", statusReq, "application/soap+xml; charset=utf-8");
        assert(resStatus && resStatus->status == 200);
        assert(doc.load_string(resStatus->body.c_str()));
        assert(doc.select_node("//*[local-name()='PTZStatus']"));
    }

    server.stop();
    assert(!server.isRunning());

    std::cout << "[PASS] testHttpSoapEndpoints" << std::endl;
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

    assert(adapter.handleGotoPreset("2"));
    assert(adapter.handleRemovePreset("2"));
    assert(adapter.handleGetPresets().empty());

    const auto status = adapter.handleGetStatus();
    (void)status;

    device->stop();
    std::cout << "[PASS] testPelcoDPtzAdapter" << std::endl;
}

int main()
{
    std::cout << "Starting TestOnvifServer test suite..." << std::endl;
    testWsDiscoveryPayloads();
    testHttpSoapEndpoints();
    testPelcoDPtzAdapter();
    std::cout << "All TestOnvifServer tests passed successfully!" << std::endl;
    return 0;
}
