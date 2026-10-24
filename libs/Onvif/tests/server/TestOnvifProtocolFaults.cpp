/// @file TestOnvifProtocolFaults.cpp
/// @brief Negative and boundary protocol fault test suite for OnvifServer (P4 Phase 2).
/// @details Verifies that malformed payloads, structural violations, mismatched namespaces,
///          non-existent tokens, and out-of-range parameters return standard SOAP 1.2 faults.

#include <cstddef>
#include <cstdint>

#include <memory>
#include <string>
#include <string_view>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "OnvifTestClient.h"
#include "OnvifTestFixtures.h"

namespace Onvif::Test {

namespace {

    [[nodiscard]] std::string wrapEnvelope(std::string_view bodyXml)
    {
        std::string out { "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                          "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
                          "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                          "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
                          "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" "
                          "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                          "  <s:Body>\r\n" };
        out.append(bodyXml);
        out.append("\r\n  </s:Body>\r\n</s:Envelope>");
        return out;
    }

    void assertSoapFault(const pugi::xml_document& doc, std::string_view expectedCode, std::string_view expectedSubcode,
        std::string_view expectedSecondary = "")
    {
        const auto faultNode = doc.select_node("//*[local-name()='Fault']").node();
        ASSERT_FALSE(faultNode.empty()) << "Missing SOAP Fault element";

        const auto codeValNode
            = doc.select_node("//*[local-name()='Fault']/*[local-name()='Code']/*[local-name()='Value']").node();
        ASSERT_FALSE(codeValNode.empty()) << "Missing Fault Code Value";
        EXPECT_STREQ(codeValNode.text().as_string(), expectedCode.data());

        const auto subcodeValNode = doc.select_node("//*[local-name()='Fault']/*[local-name()='Code']/"
                                                    "*[local-name()='Subcode']/*[local-name()='Value']")
                                        .node();
        ASSERT_FALSE(subcodeValNode.empty()) << "Missing Fault Subcode Value";
        EXPECT_STREQ(subcodeValNode.text().as_string(), expectedSubcode.data());

        if (!expectedSecondary.empty()) {
            const auto secNode
                = doc.select_node("//*[local-name()='Fault']/*[local-name()='Code']/*[local-name()='Subcode']/"
                                  "*[local-name()='Subcode']/*[local-name()='Value']")
                      .node();
            ASSERT_FALSE(secNode.empty()) << "Missing Secondary Subcode Value";
            EXPECT_STREQ(secNode.text().as_string(), expectedSecondary.data());
        }
    }

} // namespace

class ProtocolFaultFixture : public OnvifServerFixture {
protected:
    [[nodiscard]] httplib::Result sendRaw(std::string_view path, std::string_view payload,
        std::string_view contentType = "application/soap+xml; charset=utf-8")
    {
        return m_client->raw().Post(std::string(path).c_str(), payload.data(), payload.size(), contentType.data());
    }
};

TEST_F(ProtocolFaultFixture, NonXmlBodyReturns400)
{
    const auto res = sendRaw("/onvif/device_service", "Plain non-XML binary payload \x01\x02\x03\xFF");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

TEST_F(ProtocolFaultFixture, MalformedXmlReturns400)
{
    const std::string malformed = "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\"><s:Body><Unclosed>";
    const auto res = sendRaw("/onvif/device_service", malformed);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

TEST_F(ProtocolFaultFixture, MissingSoapBodyFault)
{
    const std::string noBody
        = "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\"><s:Header/></s:Envelope>";
    const auto res = sendRaw("/onvif/device_service", noBody);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);

    pugi::xml_document doc { };
    ASSERT_TRUE(doc.load_string(res->body.c_str()));
    assertSoapFault(doc, "SOAP-ENV:Sender", "ter:InvalidArgVal");
}

TEST_F(ProtocolFaultFixture, EmptySoapBodyFault)
{
    const std::string emptyBody
        = "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\"><s:Body/></s:Envelope>";
    const auto res = sendRaw("/onvif/device_service", emptyBody);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);

    pugi::xml_document doc { };
    ASSERT_TRUE(doc.load_string(res->body.c_str()));
    assertSoapFault(doc, "SOAP-ENV:Sender", "ter:InvalidArgVal");
}

TEST_F(ProtocolFaultFixture, UnknownOpReturnsFault)
{
    const std::string req = wrapEnvelope("<tds:NonExistentDeviceAction/>");
    pugi::xml_document doc { };
    const int status = sendSoapReq("/onvif/device_service", req, doc);
    EXPECT_EQ(status, 500);
    assertSoapFault(doc, "SOAP-ENV:Receiver", "ter:ActionNotSupported");
}

TEST_F(ProtocolFaultFixture, NamespaceMismatchFault)
{
    // ContinuousMove belongs to PTZ, sending it to device_service must fail
    const std::string req
        = wrapEnvelope("<tptz:ContinuousMove><tptz:ProfileToken>Profile1</tptz:ProfileToken></tptz:ContinuousMove>");
    pugi::xml_document doc { };
    const int status = sendSoapReq("/onvif/device_service", req, doc);
    EXPECT_EQ(status, 500);
    assertSoapFault(doc, "SOAP-ENV:Receiver", "ter:ActionNotSupported");
}

TEST_F(ProtocolFaultFixture, NonExistentProfileFault)
{
    const std::string req
        = wrapEnvelope("<trt:GetProfile><trt:ProfileToken>InvalidProfileToken</trt:ProfileToken></trt:GetProfile>");
    pugi::xml_document doc { };
    const int status = sendSoapReq("/onvif/media_service", req, doc);
    EXPECT_TRUE(status == 400 || status == 500);
    assertSoapFault(doc, "SOAP-ENV:Sender", "ter:InvalidArgVal", "ter:NoProfile");
}

TEST_F(ProtocolFaultFixture, NonExistentConfigFault)
{
    const std::string req = wrapEnvelope("<tptz:GetConfiguration><tptz:PTZConfigurationToken>BadToken</"
                                         "tptz:PTZConfigurationToken></tptz:GetConfiguration>");
    pugi::xml_document doc { };
    const int status = sendSoapReq("/onvif/ptz_service", req, doc);
    EXPECT_TRUE(status == 400 || status == 500);
    assertSoapFault(doc, "SOAP-ENV:Sender", "ter:InvalidArgVal", "ter:NoToken");
}

TEST_F(ProtocolFaultFixture, NonExistentPresetFault)
{
    const std::string req = wrapEnvelope("<tptz:GotoPreset><tptz:ProfileToken>Profile1</tptz:ProfileToken>"
                                         "<tptz:PresetToken>NoSuchPreset</tptz:PresetToken></tptz:GotoPreset>");
    pugi::xml_document doc { };
    const int status = sendSoapReq("/onvif/ptz_service", req, doc);
    EXPECT_TRUE(status == 400 || status == 500);
    assertSoapFault(doc, "SOAP-ENV:Sender", "ter:InvalidArgVal", "ter:NoToken");
}

TEST_F(ProtocolFaultFixture, VelocityOutOfRangeFault)
{
    // Allowed range for PanTilt velocity is [-1.0, 1.0]; 5.0 is invalid
    const std::string req
        = wrapEnvelope("<tptz:ContinuousMove><tptz:ProfileToken>Profile1</tptz:ProfileToken>"
                       "<tptz:Velocity><tt:PanTilt x=\"5.0\" y=\"0.0\"/></tptz:Velocity></tptz:ContinuousMove>");
    pugi::xml_document doc { };
    const int status = sendSoapReq("/onvif/ptz_service", req, doc);
    EXPECT_TRUE(status == 400 || status == 500);
    assertSoapFault(doc, "SOAP-ENV:Sender", "ter:InvalidArgVal", "ter:OutofRange");
}

TEST_F(ProtocolFaultFixture, MissingMandatoryArgsFault)
{
    // SetSystemDateAndTime requires DateTimeType; omitting it must return fault
    const std::string req = wrapEnvelope(
        "<tds:SetSystemDateAndTime><tds:DaylightSavings>false</tds:DaylightSavings></tds:SetSystemDateAndTime>");
    pugi::xml_document doc { };
    const int status = sendSoapReq("/onvif/device_service", req, doc);
    EXPECT_TRUE(status == 400 || status == 500);
    assertSoapFault(doc, "SOAP-ENV:Sender", "ter:InvalidArgVal");
}

TEST_F(ProtocolFaultFixture, InvalidIsoDurationFault)
{
    const std::string req = wrapEnvelope("<tptz:ContinuousMove><tptz:ProfileToken>Profile1</tptz:ProfileToken>"
                                         "<tptz:Velocity><tt:PanTilt x=\"0.5\" y=\"0.0\"/></tptz:Velocity>"
                                         "<tptz:Timeout>NOT_A_VALID_ISO_DURATION</tptz:Timeout></tptz:ContinuousMove>");
    pugi::xml_document doc { };
    const int status = sendSoapReq("/onvif/ptz_service", req, doc);
    EXPECT_TRUE(status == 400 || status == 500);
    assertSoapFault(doc, "SOAP-ENV:Sender", "ter:InvalidArgVal");
}

} // namespace Onvif::Test
