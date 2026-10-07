/// @file TestOnvifSoapFault.cpp
/// @brief Conformance tests for SOAP 1.2 Receiver and ActionNotSupported faults (Finding C3).

#include <Onvif/HttplibInclude.h>
#include <Onvif/OnvifServer.h>
#include <Onvif/SoapFault.h>
#include <Onvif/XmlUtils.h>

#include "OnvifTestClient.h"

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace {

std::atomic<int> g_nextPort { 19350 };

[[nodiscard]] std::string wrapEnvelope(const std::string& faultXml)
{
    return "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
           "xmlns:ter=\"http://www.onvif.org/ver10/error\"><SOAP-ENV:Body>"
        + faultXml + "</SOAP-ENV:Body></SOAP-ENV:Envelope>";
}

[[nodiscard]] std::string makeSoapRequest(const std::string& payloadXml)
{
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
           "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
           "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
           "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
           "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\" "
           "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" "
           "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" "
           "xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\" "
           "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\" "
           "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\" "
           "xmlns:tan=\"http://www.onvif.org/ver20/analytics/wsdl\" "
           "xmlns:trc=\"http://www.onvif.org/ver10/recording/wsdl\" "
           "xmlns:tse=\"http://www.onvif.org/ver10/search/wsdl\" "
           "xmlns:trp=\"http://www.onvif.org/ver10/replay/wsdl\" "
           "xmlns:tth=\"http://www.onvif.org/ver10/thermal/wsdl\">\r\n"
           "  <SOAP-ENV:Body>\r\n"
           "    "
        + payloadXml
        + "\r\n"
          "  </SOAP-ENV:Body>\r\n"
          "</SOAP-ENV:Envelope>\r\n";
}

} // namespace

// ============================================================================
// Unit Tests: SoapFault receiver and actionNotSupported builders
// ============================================================================

TEST(TestOnvifSoapFault, ActionNotSupportedStructure)
{
    const std::string xml = wrapEnvelope(Onvif::SoapFault::actionNotSupported());
    pugi::xml_document doc {};
    ASSERT_TRUE(doc.load_string(xml.c_str()));

    const auto faultNode = doc.select_node("//*[local-name()='Fault']").node();
    ASSERT_FALSE(faultNode.empty());

    const auto codeValNode
        = doc.select_node("//*[local-name()='Fault']/*[local-name()='Code']/*[local-name()='Value']").node();
    ASSERT_FALSE(codeValNode.empty());
    EXPECT_STREQ(codeValNode.text().as_string(), "SOAP-ENV:Receiver");

    const auto subcodeValNode
        = doc.select_node(
                 "//*[local-name()='Fault']/*[local-name()='Code']/*[local-name()='Subcode']/*[local-name()='Value']")
              .node();
    ASSERT_FALSE(subcodeValNode.empty());
    EXPECT_STREQ(subcodeValNode.text().as_string(), "ter:ActionNotSupported");

    const auto reasonNode
        = doc.select_node("//*[local-name()='Fault']/*[local-name()='Reason']/*[local-name()='Text']").node();
    ASSERT_FALSE(reasonNode.empty());
    EXPECT_STREQ(reasonNode.text().as_string(), "Action Not Supported");
    EXPECT_STREQ(reasonNode.attribute("xml:lang").value(), "en");
}

TEST(TestOnvifSoapFault, ReceiverWithSecondarySubcode)
{
    const std::string xml
        = wrapEnvelope(Onvif::SoapFault::receiver("ter:ActionNotSupported", "ter:OptionalSecondary", "Custom Detail"));
    pugi::xml_document doc {};
    ASSERT_TRUE(doc.load_string(xml.c_str()));

    const auto codeValNode
        = doc.select_node("//*[local-name()='Fault']/*[local-name()='Code']/*[local-name()='Value']").node();
    EXPECT_STREQ(codeValNode.text().as_string(), "SOAP-ENV:Receiver");

    const auto priSub
        = doc.select_node(
                 "//*[local-name()='Fault']/*[local-name()='Code']/*[local-name()='Subcode']/*[local-name()='Value']")
              .node();
    EXPECT_STREQ(priSub.text().as_string(), "ter:ActionNotSupported");

    const auto secSub = doc.select_node("//*[local-name()='Fault']/*[local-name()='Code']/*[local-name()='Subcode']/"
                                        "*[local-name()='Subcode']/*[local-name()='Value']")
                            .node();
    EXPECT_STREQ(secSub.text().as_string(), "ter:OptionalSecondary");

    const auto reasonNode
        = doc.select_node("//*[local-name()='Fault']/*[local-name()='Reason']/*[local-name()='Text']").node();
    EXPECT_STREQ(reasonNode.text().as_string(), "Custom Detail");
}

TEST(TestOnvifSoapFault, ActionNotSupportedXmlEscaping)
{
    const std::string attackReason = "Action <script>alert(1)</script> & \"quotes\" 'single'";
    const std::string xml = wrapEnvelope(Onvif::SoapFault::actionNotSupported(attackReason));
    pugi::xml_document doc {};
    ASSERT_TRUE(doc.load_string(xml.c_str()));

    const auto reasonNode
        = doc.select_node("//*[local-name()='Fault']/*[local-name()='Reason']/*[local-name()='Text']").node();
    ASSERT_FALSE(reasonNode.empty());
    EXPECT_STREQ(reasonNode.text().as_string(), attackReason.c_str());
}

// ============================================================================
// Integration Tests: Live server returns SOAP 1.2 Receiver Fault on unknown op
// ============================================================================

class OnvifSoapFaultServerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_port = g_nextPort.fetch_add(1);
        Onvif::OnvifServerConfig config {};
        config.bindAddress = "127.0.0.1";
        config.port = m_port;

        Onvif::OnvifUser admin {};
        admin.username = "admin";
        admin.password = "Admin_Test_1234!";
        admin.level = Onvif::OnvifUserLevel::Administrator;
        config.defaultUsers = { admin };
        config.auth.allowDefaultPassword = true;

        m_server = std::make_unique<Onvif::OnvifServer>(config);
        ASSERT_TRUE(m_server->start());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        m_client = std::make_unique<OnvifTest::OnvifTestClient>(
            "127.0.0.1", m_port, OnvifTest::makeCreds("admin", "Admin_Test_1234!"));
    }

    void TearDown() override
    {
        if (m_server) {
            m_server->stop();
        }
    }

    void assertActionNotSupported(
        const std::string& path, const std::string& payloadXml, const std::string& expectedOpName)
    {
        const std::string reqBody = makeSoapRequest(payloadXml);
        const auto res = m_client->Post(path, reqBody, "application/soap+xml; charset=utf-8");
        ASSERT_TRUE(res) << "Failed HTTP POST to " << path;
        EXPECT_EQ(res->status, 500) << "Expected HTTP 500 on " << path << ", got " << res->status
                                    << "\nBody: " << res->body;

        pugi::xml_document doc {};
        ASSERT_TRUE(doc.load_string(res->body.c_str())) << "Response body is not well-formed XML:\n" << res->body;

        // Verify no malformed double colon tags or bogus echoed response tags
        EXPECT_EQ(res->body.find("tds:tds:"), std::string::npos);
        EXPECT_EQ(res->body.find("trt:trt:"), std::string::npos);
        EXPECT_EQ(res->body.find("tptz:tptz:"), std::string::npos);
        EXPECT_EQ(res->body.find("<tds:" + expectedOpName + "Response"), std::string::npos);
        EXPECT_EQ(res->body.find("<trt:" + expectedOpName + "Response"), std::string::npos);
        EXPECT_EQ(res->body.find("<tptz:" + expectedOpName + "Response"), std::string::npos);

        // Verify SOAP 1.2 Receiver fault structure
        const auto fault = doc.select_node("//*[local-name()='Fault']").node();
        EXPECT_FALSE(fault.empty()) << "Missing Fault element on " << path << "\nBody: " << res->body;

        const auto codeVal
            = doc.select_node("//*[local-name()='Fault']/*[local-name()='Code']/*[local-name()='Value']").node();
        EXPECT_STREQ(codeVal.text().as_string(), "SOAP-ENV:Receiver");

        const auto subcodeVal = doc.select_node("//*[local-name()='Fault']/*[local-name()='Code']/"
                                                "*[local-name()='Subcode']/*[local-name()='Value']")
                                    .node();
        EXPECT_STREQ(subcodeVal.text().as_string(), "ter:ActionNotSupported");
    }

    int m_port { 0 };
    std::unique_ptr<Onvif::OnvifServer> m_server {};
    std::unique_ptr<OnvifTest::OnvifTestClient> m_client {};
};

TEST_F(OnvifSoapFaultServerTest, DeviceServiceUnknownOpReturnsFault)
{
    assertActionNotSupported("/onvif/device_service", "<tds:UnknownDeviceOperation/>", "UnknownDeviceOperation");
    assertActionNotSupported("/onvif/device_service", "<NonExistentAction/>", "NonExistentAction");
}

TEST_F(OnvifSoapFaultServerTest, MediaServiceUnknownOpReturnsFault)
{
    assertActionNotSupported("/onvif/media_service", "<trt:UnsupportedMediaAction/>", "UnsupportedMediaAction");
}

TEST_F(OnvifSoapFaultServerTest, Media2ServiceUnknownOpReturnsFault)
{
    assertActionNotSupported("/onvif/media2_service", "<tr2:UnsupportedMedia2Action/>", "UnsupportedMedia2Action");
}

TEST_F(OnvifSoapFaultServerTest, PtzServiceUnknownOpReturnsFault)
{
    assertActionNotSupported("/onvif/ptz_service", "<tptz:UnsupportedPtzAction/>", "UnsupportedPtzAction");
}

TEST_F(OnvifSoapFaultServerTest, ImagingServiceUnknownOpReturnsFault)
{
    assertActionNotSupported("/onvif/imaging_service", "<timg:UnsupportedImagingAction/>", "UnsupportedImagingAction");
}

TEST_F(OnvifSoapFaultServerTest, DeviceIoServiceUnknownOpReturnsFault)
{
    assertActionNotSupported(
        "/onvif/deviceio_service", "<tmd:UnsupportedDeviceIoAction/>", "UnsupportedDeviceIoAction");
}

TEST_F(OnvifSoapFaultServerTest, EventServiceUnknownOpReturnsFault)
{
    assertActionNotSupported("/onvif/event_service", "<tev:UnsupportedEventAction/>", "UnsupportedEventAction");
}

TEST_F(OnvifSoapFaultServerTest, SubscriptionServiceUnknownOpReturnsFault)
{
    assertActionNotSupported("/onvif/events/subscription", "<wsnt:UnsupportedSubAction/>", "UnsupportedSubAction");
}

TEST_F(OnvifSoapFaultServerTest, AnalyticsServiceUnknownOpReturnsFault)
{
    assertActionNotSupported(
        "/onvif/analytics_service", "<tan:UnsupportedAnalyticsAction/>", "UnsupportedAnalyticsAction");
}

TEST_F(OnvifSoapFaultServerTest, RecordingServiceUnknownOpReturnsFault)
{
    assertActionNotSupported(
        "/onvif/recording_service", "<trc:UnsupportedRecordingAction/>", "UnsupportedRecordingAction");
}

TEST_F(OnvifSoapFaultServerTest, SearchServiceUnknownOpReturnsFault)
{
    assertActionNotSupported("/onvif/search_service", "<tse:UnsupportedSearchAction/>", "UnsupportedSearchAction");
}

TEST_F(OnvifSoapFaultServerTest, ReplayServiceUnknownOpReturnsFault)
{
    assertActionNotSupported("/onvif/replay_service", "<trp:UnsupportedReplayAction/>", "UnsupportedReplayAction");
}

TEST_F(OnvifSoapFaultServerTest, ThermalServiceUnknownOpReturnsFault)
{
    assertActionNotSupported("/onvif/thermal_service", "<tth:UnsupportedThermalAction/>", "UnsupportedThermalAction");
}

TEST_F(OnvifSoapFaultServerTest, ValidOperationsStillSucceed)
{
    const std::string validReq = makeSoapRequest("<tds:GetSystemDateAndTime/>");
    const auto res = m_client->Post("/onvif/device_service", validReq, "application/soap+xml; charset=utf-8");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);

    pugi::xml_document doc {};
    ASSERT_TRUE(doc.load_string(res->body.c_str()));
    const auto dateNode = doc.select_node("//*[local-name()='GetSystemDateAndTimeResponse']").node();
    EXPECT_FALSE(dateNode.empty());
}
