/// @file TestOnvifExtensions.cpp
/// @brief Modular unit tests for OnvifServer extensions (DeviceIO, PKI, Analytics, Thermal, Privacy Masks).

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "OnvifTestFixtures.h"
#include <Onvif/adapters/DeviceSystemAdapter.h>
#include <Onvif/adapters/PelcoDPtzAdapter.h>
#include <Onvif/adapters/StubAnalyticsAdapter.h>
#include <Onvif/adapters/StubMediaExtensionAdapter.h>
#include <Onvif/adapters/StubRecordingAdapter.h>
#include <Onvif/adapters/StubThermalAdapter.h>
#include <PelcoDSim/MockPelcoDDevice.h>

using namespace Onvif;
using namespace Onvif::Test;

TEST(OnvifExtensionsTest, DeviceIoRelaysAndInputs)
{
    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(std::uint8_t { 1U });
    auto device = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, std::uint8_t { 1U });
    EXPECT_TRUE(device->start());

    auto adapter = std::make_shared<PelcoDPtzAdapter>(device);
    auto devIoAdapter = std::make_shared<DeviceSystemAdapter>();

    const auto port = getFreeEphemeralPort();
    OnvifServerConfig config { };
    config.port = static_cast<int>(port);
    config.bindAddress = "127.0.0.1";
    config.deviceName = "ImagingDeviceIoCamera";
    config.defaultUsers = OnvifTest::defaultTestUsers();

    OnvifServer server(config, adapter, adapter);
    server.setDeviceIoHandler(devIoAdapter);
    ASSERT_TRUE(server.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    OnvifTest::OnvifTestClient client("127.0.0.1", static_cast<int>(port));
    client.set_connection_timeout(std::chrono::seconds(2));
    client.set_read_timeout(std::chrono::seconds(2));

    // 1. GetRelayOutputs
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tmd:GetRelayOutputs/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/deviceio_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_NE(res->body.find("Relay_1"), std::string::npos);
        EXPECT_NE(res->body.find("Relay_2"), std::string::npos);
    }

    // 2. SetRelayOutputState (active & inactive)
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

        const auto relaysActive = devIoAdapter->handleGetRelayOutputs();
        ASSERT_FALSE(relaysActive.empty());
        EXPECT_EQ(relaysActive[0].logicalState, RelayLogicalState::Active);

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

        const auto relaysInactive = devIoAdapter->handleGetRelayOutputs();
        ASSERT_FALSE(relaysInactive.empty());
        EXPECT_EQ(relaysInactive[0].logicalState, RelayLogicalState::Inactive);
    }

    // 3. GetDigitalInputs
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tmd:GetDigitalInputs/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/deviceio_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_NE(res->body.find("Input_1"), std::string::npos);
    }

    server.stop();
    device->stop();
}

TEST(OnvifExtensionsTest, PkiCertificatesAndProfileG)
{
    const auto port = getFreeEphemeralPort();
    OnvifServerConfig config { };
    config.port = static_cast<int>(port);
    config.bindAddress = "127.0.0.1";
    config.deviceName = "ProfileGCam";
    config.defaultUsers = OnvifTest::defaultTestUsers();

    auto devSysAdapter = std::make_shared<DeviceSystemAdapter>();
    auto recordingAdapter = std::make_shared<StubRecordingAdapter>();

    OnvifServer server(config);
    server.setDeviceManagementHandler(devSysAdapter);
    server.setRecordingHandler(recordingAdapter);
    ASSERT_TRUE(server.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    OnvifTest::OnvifTestClient client("127.0.0.1", static_cast<int>(port));
    client.set_connection_timeout(std::chrono::seconds(2));
    client.set_read_timeout(std::chrono::seconds(2));

    // 1. CreateCertificate
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
        EXPECT_NE(res->body.find("CreateCertificateResponse"), std::string::npos);
    }

    // 2. GetCertificates
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
                                "  <SOAP-ENV:Body><tds:GetCertificates/></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/device_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_NE(res->body.find("cert_pki_1"), std::string::npos);
    }

    // 3. Recording Service: GetServiceCapabilities & CreateRecording
    {
        const std::string reqCaps = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                    "xmlns:trc=\"http://www.onvif.org/ver10/recording/wsdl\">\r\n"
                                    "  <SOAP-ENV:Body><trc:GetServiceCapabilities/></SOAP-ENV:Body>\r\n"
                                    "</SOAP-ENV:Envelope>";
        auto resCaps = client.Post("/onvif/recording_service", reqCaps, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resCaps && resCaps->status == 200);
        EXPECT_NE(resCaps->body.find("DynamicRecordings"), std::string::npos);

        const std::string reqCreate = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                      "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                      "xmlns:trc=\"http://www.onvif.org/ver10/recording/wsdl\" "
                                      "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                                      "  <SOAP-ENV:Body>\r\n"
                                      "    <trc:CreateRecording>\r\n"
                                      "      <trc:RecordingConfiguration>\r\n"
                                      "        <tt:Source><tt:SourceId>Profile_1</tt:SourceId></tt:Source>\r\n"
                                      "        <tt:Content>Daily Test</tt:Content>\r\n"
                                      "        <tt:MaximumRetentionTime>P30D</tt:MaximumRetentionTime>\r\n"
                                      "      </trc:RecordingConfiguration>\r\n"
                                      "    </trc:CreateRecording>\r\n"
                                      "  </SOAP-ENV:Body>\r\n"
                                      "</SOAP-ENV:Envelope>";
        auto resCreate = client.Post("/onvif/recording_service", reqCreate, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(resCreate && resCreate->status == 200);
        EXPECT_NE(resCreate->body.find("CreateRecordingResponse"), std::string::npos);
    }

    server.stop();
}

TEST(OnvifExtensionsTest, ThermalAndPrivacyMasks)
{
    const auto port = getFreeEphemeralPort();
    OnvifServerConfig config { };
    config.port = static_cast<int>(port);
    config.bindAddress = "127.0.0.1";
    config.deviceName = "ThermalPrivacyCamera";
    config.defaultUsers = OnvifTest::defaultTestUsers();

    auto thermalAdapter = std::make_shared<StubThermalAdapter>();
    auto mediaExtAdapter = std::make_shared<StubMediaExtensionAdapter>();

    OnvifServer server(config);
    server.setThermalHandler(thermalAdapter);
    server.setMaskHandler(mediaExtAdapter);
    ASSERT_TRUE(server.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    OnvifTest::OnvifTestClient client("127.0.0.1", static_cast<int>(port));
    client.set_connection_timeout(std::chrono::seconds(2));
    client.set_read_timeout(std::chrono::seconds(2));

    // 1. Thermal: GetColorPalettes
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tth=\"http://www.onvif.org/ver10/thermal/wsdl\">\r\n"
                                "  "
                                "<SOAP-ENV:Body><tth:GetColorPalettes><tth:VideoSourceToken>VideoSource_1</"
                                "tth:VideoSourceToken></tth:GetColorPalettes></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/thermal_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_NE(res->body.find("WhiteHot"), std::string::npos);
    }

    // 2. Privacy Masks: GetMasks
    {
        const std::string req = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\">\r\n"
                                "  "
                                "<SOAP-ENV:Body><tr2:GetMasks><tr2:ConfigurationToken>VideoSource_1</"
                                "tr2:ConfigurationToken></tr2:GetMasks></SOAP-ENV:Body>\r\n"
                                "</SOAP-ENV:Envelope>";
        auto res = client.Post("/onvif/media2_service", req, "application/soap+xml; charset=utf-8");
        EXPECT_TRUE(res && res->status == 200);
        EXPECT_NE(res->body.find("GetMasksResponse"), std::string::npos);
    }

    server.stop();
}
