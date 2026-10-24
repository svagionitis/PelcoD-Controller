/// @file TestOnvifPtzService.cpp
/// @brief Modular unit tests for OnvifServer PTZ service, presets, tours, and space mappings.

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "OnvifTestFixtures.h"
#include <Onvif/adapters/DeviceSystemAdapter.h>
#include <Onvif/adapters/PelcoDPtzAdapter.h>
#include <PelcoDSim/MockPelcoDDevice.h>

using namespace Onvif;
using namespace Onvif::Test;

TEST(OnvifPtzServiceTest, PelcoDPtzAdapterDirect)
{
    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(std::uint8_t { 1U });
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, std::uint8_t { 1U });
    EXPECT_TRUE(device->start());

    PelcoDPtzAdapter adapter(device);

    // Continuous move right + up
    adapter.handleContinuousMove(1.0f, 0.5f, 0.0f);
    adapter.handleStop(true, true);

    // Absolute move
    adapter.handleAbsoluteMove(90.0f, 45.0f, 0.5f);

    // Presets
    const std::string tok = adapter.handleSetPreset("Preset A", "2");
    EXPECT_EQ(tok, "2");

    auto presets = adapter.handleGetPresets();
    ASSERT_EQ(presets.size(), 1U);
    EXPECT_EQ(presets[0].token, "2");

    std::vector<OnvifEvent> capturedEvents { };
    adapter.setEventPublisher([&capturedEvents](const OnvifEvent& ev) { capturedEvents.push_back(ev); });

    EXPECT_TRUE(adapter.handleGotoPreset("2"));
    ASSERT_FALSE(capturedEvents.empty());
    EXPECT_EQ(capturedEvents.back().topic, "tns1:PTZController/PTZPresets/Reached");
    EXPECT_EQ(capturedEvents.back().sourceValue, "2");

    EXPECT_TRUE(adapter.handleRemovePreset("2"));
    EXPECT_TRUE(adapter.handleGetPresets().empty());

    device->stop();
}

TEST(OnvifPtzServiceTest, PresetToursLifecycle)
{
    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(std::uint8_t { 1U });
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, std::uint8_t { 1U });
    EXPECT_TRUE(device->start());

    auto adapter = std::make_shared<PelcoDPtzAdapter>(device);
    const std::string testDbPath = "test_tours.json";
    static_cast<void>(std::remove(testDbPath.c_str()));
    adapter->setPersistencePath(testDbPath);

    const auto port = getFreeEphemeralPort();
    OnvifServerConfig config { };
    config.bindAddress = "127.0.0.1";
    config.port = static_cast<int>(port);
    config.deviceName = "Preset Tour Test Camera";
    config.defaultUsers = OnvifTest::defaultTestUsers();

    OnvifServer server(config, adapter, adapter);
    ASSERT_TRUE(server.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    OnvifTest::OnvifTestClient client("127.0.0.1", static_cast<int>(port));
    client.set_connection_timeout(std::chrono::seconds(2));
    client.set_read_timeout(std::chrono::seconds(2));

    // 1. GetPresetTourOptions
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                "  "
                                "<SOAP-ENV:Body><tptz:GetPresetTourOptions><tptz:ProfileToken>ProfileToken_1</"
                                "tptz:ProfileToken></tptz:GetPresetTourOptions></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
    }

    // 2. CreatePresetTour
    std::string tourToken { };
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                "  "
                                "<SOAP-ENV:Body><tptz:CreatePresetTour><tptz:ProfileToken>ProfileToken_1</"
                                "tptz:ProfileToken></tptz:CreatePresetTour></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        pugi::xml_document doc { };
        EXPECT_TRUE(doc.load_string(res->body.c_str()));
        const auto tokenNode = doc.select_node("//*[local-name()='PresetTourToken']").node();
        tourToken = tokenNode.text().as_string();
        EXPECT_FALSE(tourToken.empty());
    }

    // 3. ModifyPresetTour
    {
        const std::string req
            = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
              "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
              "  <SOAP-ENV:Body><tptz:ModifyPresetTour><tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>"
              "    <tptz:PresetTour token=\""
            + tourToken
            + "\"><tt:Name>Perimeter Scan</tt:Name>"
              "      <tt:TourSpot><tt:PresetDetail><tt:PresetToken>1</tt:PresetToken></tt:PresetDetail>"
              "        <tt:Speed><tt:PanTilt x=\"0.7\" y=\"0.7\"/></tt:Speed><tt:StayTime>PT5S</tt:StayTime>"
              "      </tt:TourSpot></tptz:PresetTour></tptz:ModifyPresetTour></SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
    }

    // 4. OperatePresetTour (Start, Pause, Stop)
    for (const auto& op : { "Start", "Pause", "Stop" }) {
        const std::string req
            = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
              "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
              "  <SOAP-ENV:Body><tptz:OperatePresetTour><tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>"
              "    <tptz:PresetTourToken>"
            + tourToken
            + "</tptz:PresetTourToken>"
              "    <tptz:Operation>"
            + std::string(op)
            + "</tptz:Operation></tptz:OperatePresetTour></SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
    }

    // 5. RemovePresetTour
    {
        const std::string req
            = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
              "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
              "  <SOAP-ENV:Body><tptz:RemovePresetTour><tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>"
              "    <tptz:PresetTourToken>"
            + tourToken
            + "</tptz:PresetTourToken></tptz:RemovePresetTour></SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
    }

    server.stop();
    device->stop();
    static_cast<void>(std::remove(testDbPath.c_str()));
}

TEST(OnvifPtzServiceTest, PtzNodesAndRelativeMove)
{
    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(std::uint8_t { 1U });
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, std::uint8_t { 1U });
    EXPECT_TRUE(device->start());

    auto adapter = std::make_shared<PelcoDPtzAdapter>(device);

    const auto port = getFreeEphemeralPort();
    OnvifServerConfig config { };
    config.bindAddress = "127.0.0.1";
    config.port = static_cast<int>(port);
    config.deviceName = "PTZ Extensions Camera";
    config.defaultUsers = OnvifTest::defaultTestUsers();

    OnvifServer server(config, adapter, adapter);
    ASSERT_TRUE(server.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    OnvifTest::OnvifTestClient client("127.0.0.1", static_cast<int>(port));
    client.set_connection_timeout(std::chrono::seconds(2));
    client.set_read_timeout(std::chrono::seconds(2));

    // GetNodes
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tptz:GetNodes/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        pugi::xml_document doc { };
        EXPECT_TRUE(doc.load_string(res->body.c_str()));
        const auto homeNode = doc.select_node("//*[local-name()='HomeSupported']").node();
        EXPECT_TRUE(homeNode && homeNode.text().as_bool());
    }

    // RelativeMove
    {
        const std::string req
            = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
              "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
              "  <SOAP-ENV:Body><tptz:RelativeMove><tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>"
              "    <tptz:Translation><tt:PanTilt x=\"10.0\" y=\"5.0\"/><tt:Zoom x=\"0.1\"/></tptz:Translation>"
              "  </tptz:RelativeMove></SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
    }

    // SetHomePosition and GotoHomePosition
    {
        const std::string reqSet = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                   "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                   "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                   "  "
                                   "<SOAP-ENV:Body><tptz:SetHomePosition><tptz:ProfileToken>ProfileToken_1</"
                                   "tptz:ProfileToken></tptz:SetHomePosition></SOAP-ENV:Body>\r\n"
                                   "</SOAP-ENV:Envelope>";
        auto resSet = client.Post("/onvif/ptz_service", reqSet, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resSet && resSet->status == 200);

        const std::string reqGoto = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
                                    "  "
                                    "<SOAP-ENV:Body><tptz:GotoHomePosition><tptz:ProfileToken>ProfileToken_1</"
                                    "tptz:ProfileToken></tptz:GotoHomePosition></SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";
        auto resGoto = client.Post("/onvif/ptz_service", reqGoto, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resGoto && resGoto->status == 200);
    }

    // SendAuxiliaryCommand
    {
        const std::string req
            = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
              "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
              "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
              "  <SOAP-ENV:Body><tptz:SendAuxiliaryCommand><tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>"
              "    <tptz:AuxiliaryData>tt:Wiper|On</tptz:AuxiliaryData></tptz:SendAuxiliaryCommand></SOAP-ENV:Body>\r\n"
              "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
    }

    server.stop();
    device->stop();
}

TEST(OnvifPtzServiceTest, PtzGeoMoveSpherical)
{
    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(std::uint8_t { 1U });
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, std::uint8_t { 1U });
    EXPECT_TRUE(device->start());

    auto adapter = std::make_shared<PelcoDPtzAdapter>(device);
    auto devSysAdapter = std::make_shared<DeviceSystemAdapter>();
    devSysAdapter->setLocationCallback([adapter](const LocationEntity& loc) { adapter->setCameraLocation(loc); });

    const auto port = getFreeEphemeralPort();
    OnvifServerConfig config { };
    config.port = static_cast<int>(port);
    config.bindAddress = "127.0.0.1";
    config.deviceName = "GeoCamera";
    config.defaultLocation.entity = "Device";
    config.defaultLocation.fixed = true;
    config.defaultLocation.location = { 37.9838, 23.7275, 150.0 };
    config.defaultLocation.orientation = { 0.0, 0.0, 0.0 };
    config.defaultUsers = OnvifTest::defaultTestUsers();

    OnvifServer server(config);
    adapter->setCameraLocation(config.defaultLocation);
    devSysAdapter->setCameraLocation(config.defaultLocation);
    server.setPtzHandler(adapter);
    server.setDeviceManagementHandler(devSysAdapter);
    ASSERT_TRUE(server.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    OnvifTest::OnvifTestClient client("127.0.0.1", static_cast<int>(port));
    client.set_connection_timeout(std::chrono::seconds(2));
    client.set_read_timeout(std::chrono::seconds(2));

    const std::string req
        = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
          "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
          "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
          "  <SOAP-ENV:Body><tptz:GeoMove><tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>"
          "    <tptz:Target><tt:GeoLocation lat=\"37.9850\" lon=\"23.7290\" elevation=\"155.0\"/></tptz:Target>"
          "  </tptz:GeoMove></SOAP-ENV:Body>\r\n"
          "</SOAP-ENV:Envelope>";

    auto res = client.Post("/onvif/ptz_service", req, "application/soap+xml; charset=utf-8");
    EXPECT_TRUE(res && res->status == 200);

    server.stop();
    device->stop();
}
