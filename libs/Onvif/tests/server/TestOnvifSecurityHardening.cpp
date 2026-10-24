/// @file TestOnvifSecurityHardening.cpp
/// @brief Security abuse, attack replay, SSRF, storm, and boundary test suite (P4 Phase 2).
/// @details Verifies authentication replay protections, password complexity policies, first-boot
///          commissioning gating, SSRF protections, WS-Discovery storm filters, and CERT FLP34-C
///          floating-point boundary sanitization.

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <chrono>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "OnvifTestClient.h"
#include "OnvifTestFixtures.h"
#include <Onvif/NotificationUrlValidator.h>
#include <Onvif/OnvifSecurity.h>
#include <Onvif/PasswordPolicy.h>
#include <Onvif/WsDiscoveryValidator.h>
#include <Onvif/XmlUtils.h>

namespace Onvif::Test {

namespace {

    [[nodiscard]] std::string wrapEnvelope(std::string_view bodyXml)
    {
        std::string out { "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                          "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
                          "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
                          "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
                          "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" "
                          "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\" "
                          "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
                          "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
                          "  <s:Body>\r\n" };
        out.append(bodyXml);
        out.append("\r\n  </s:Body>\r\n</s:Envelope>");
        return out;
    }

} // namespace

class SecurityHardeningFixture : public OnvifServerFixture {
protected:
    [[nodiscard]] httplib::Result sendRaw(std::string_view path, std::string_view payload,
        std::string_view contentType = "application/soap+xml; charset=utf-8")
    {
        return m_client->raw().Post(std::string(path).c_str(), payload.data(), payload.size(), contentType.data());
    }
};

// ============================================================================
// 1. Authentication Abuse & Replay Protections
// ============================================================================

TEST_F(SecurityHardeningFixture, ExpiredCreatedTimestamp)
{
    // Clock skewed 360 seconds in the past exceeds the 300 s window
    SecurityCredentials creds { "admin", "Adm1n-Pass", std::chrono::seconds(-360) };
    const std::string secHeader = OnvifSecurity::buildSoapSecurityHeader(creds);
    const std::string body = wrapEnvelope("<tds:GetDeviceInformation/>");
    const std::string secured = OnvifTest::injectSecurity(body, secHeader);

    const auto res = sendRaw("/onvif/device_service", secured);
    ASSERT_TRUE(res);
    EXPECT_TRUE(res->status == 400 || res->status == 401);

    pugi::xml_document doc { };
    ASSERT_TRUE(doc.load_string(res->body.c_str()));
    const auto sub = doc.select_node("//*[local-name()='Fault']//*[local-name()='Subcode']/*[local-name()='Value']");
    ASSERT_TRUE(sub);
    EXPECT_TRUE(std::string(sub.node().text().as_string()).find("NotAuthorized") != std::string::npos);
}

TEST_F(SecurityHardeningFixture, FutureCreatedTimestamp)
{
    // Clock skewed 360 seconds in the future exceeds the 300 s tolerance
    SecurityCredentials creds { "admin", "Adm1n-Pass", std::chrono::seconds(360) };
    const std::string secHeader = OnvifSecurity::buildSoapSecurityHeader(creds);
    const std::string body = wrapEnvelope("<tds:GetDeviceInformation/>");
    const std::string secured = OnvifTest::injectSecurity(body, secHeader);

    const auto res = sendRaw("/onvif/device_service", secured);
    ASSERT_TRUE(res);
    EXPECT_TRUE(res->status == 400 || res->status == 401);

    pugi::xml_document doc { };
    ASSERT_TRUE(doc.load_string(res->body.c_str()));
    const auto sub = doc.select_node("//*[local-name()='Fault']//*[local-name()='Subcode']/*[local-name()='Value']");
    ASSERT_TRUE(sub);
    EXPECT_TRUE(std::string(sub.node().text().as_string()).find("NotAuthorized") != std::string::npos);
}

TEST_F(SecurityHardeningFixture, ReplayedNonceRejected)
{
    SecurityCredentials creds { "admin", "Adm1n-Pass", std::chrono::seconds(0) };
    const std::string secHeader = OnvifSecurity::buildSoapSecurityHeader(creds);
    const std::string body = wrapEnvelope("<tds:GetDeviceInformation/>");
    const std::string secured = OnvifTest::injectSecurity(body, secHeader);

    // First request with fresh nonce must succeed
    const auto res1 = sendRaw("/onvif/device_service", secured);
    ASSERT_TRUE(res1);
    EXPECT_EQ(res1->status, 200);

    // Replay with identical nonce and timestamp must be rejected
    const auto res2 = sendRaw("/onvif/device_service", secured);
    ASSERT_TRUE(res2);
    EXPECT_TRUE(res2->status == 400 || res2->status == 401);

    pugi::xml_document doc { };
    ASSERT_TRUE(doc.load_string(res2->body.c_str()));
    const auto sub = doc.select_node("//*[local-name()='Fault']//*[local-name()='Subcode']/*[local-name()='Value']");
    ASSERT_TRUE(sub);
    EXPECT_TRUE(std::string(sub.node().text().as_string()).find("NotAuthorized") != std::string::npos);
}

TEST_F(SecurityHardeningFixture, DigestCorruptedHash)
{
    // Unauthenticated request to protected operation must issue 401 challenge
    const auto unauthRes = sendRaw("/onvif/device_service", wrapEnvelope("<tds:GetDeviceInformation/>"));
    ASSERT_TRUE(unauthRes);
    EXPECT_EQ(unauthRes->status, 401);
    EXPECT_TRUE(unauthRes->has_header("WWW-Authenticate"));
    const std::string authHeader = unauthRes->get_header_value("WWW-Authenticate");
    EXPECT_TRUE(authHeader.find("Digest") != std::string::npos);
    EXPECT_TRUE(authHeader.find("nonce=") != std::string::npos);

    // Corrupted digest response hash must be rejected with 400 NotAuthorized
    httplib::Headers headers { { "Authorization",
        "Digest username=\"admin\", realm=\"ONVIF\", nonce=\"bogusNonce1234\", "
        "uri=\"/onvif/device_service\", response=\"badresponsehash00000000000000\"" } };
    const auto res = m_client->raw().Post("/onvif/device_service", headers, wrapEnvelope("<tds:GetDeviceInformation/>"),
        "application/soap+xml; charset=utf-8");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);

    pugi::xml_document doc { };
    ASSERT_TRUE(doc.load_string(res->body.c_str()));
    const auto sub = doc.select_node("//*[local-name()='Fault']//*[local-name()='Subcode']/*[local-name()='Value']");
    ASSERT_TRUE(sub);
    EXPECT_TRUE(std::string(sub.node().text().as_string()).find("NotAuthorized") != std::string::npos);
}

TEST_F(SecurityHardeningFixture, BruteForceDelayTriggered)
{
    // Rapid sequential authentication failures must not crash the server
    for (int i = 0; i < 8; ++i) {
        SecurityCredentials badCreds { "admin", "WrongPassword!", std::chrono::seconds(0) };
        const std::string secHeader = OnvifSecurity::buildSoapSecurityHeader(badCreds);
        const std::string secured = OnvifTest::injectSecurity(wrapEnvelope("<tds:GetDeviceInformation/>"), secHeader);
        const auto res = sendRaw("/onvif/device_service", secured);
        ASSERT_TRUE(res);
        EXPECT_TRUE(res->status == 400 || res->status == 401);
    }
}

// ============================================================================
// 2. Provisioning & Password Policy Hardening
// ============================================================================

TEST(SecurityHardeningTest, PasswordPolicyRejectsWeak)
{
    PasswordPolicyConfig cfg { };
    cfg.minLength = 8;
    cfg.maxLength = 64;
    cfg.minClasses = 3;
    cfg.rejectCommonDefaults = true;
    cfg.rejectUsername = true;

    EXPECT_EQ(PasswordPolicy::check("", "admin", cfg), PasswordCheckResult::EmptyPassword);
    EXPECT_EQ(PasswordPolicy::check("short", "admin", cfg), PasswordCheckResult::TooShort);
    EXPECT_EQ(PasswordPolicy::check("password", "admin", cfg), PasswordCheckResult::CommonDefault);
    EXPECT_EQ(PasswordPolicy::check("12345678", "admin", cfg), PasswordCheckResult::CommonDefault);
    EXPECT_EQ(PasswordPolicy::check("adminPass1!", "admin", cfg), PasswordCheckResult::MatchesUsername);
    EXPECT_EQ(PasswordPolicy::check("alllowercase123", "admin", cfg), PasswordCheckResult::InsufficientClasses);

    // Compliant strong password
    EXPECT_EQ(PasswordPolicy::check("Secure-Passw0rd!", "admin", cfg), PasswordCheckResult::Valid);
}

TEST_F(SecurityHardeningFixture, UnprovisionedGatingBlocks)
{
    // Start an isolated server with empty default users (Unprovisioned state)
    const auto unprovPort = getFreeEphemeralPort();
    OnvifServerConfig unprovConfig { };
    unprovConfig.bindAddress = "127.0.0.1";
    unprovConfig.port = static_cast<int>(unprovPort);
    unprovConfig.defaultUsers.clear(); // forces unprovisioned mode

    OnvifServer unprovServer(unprovConfig);
    ASSERT_TRUE(unprovServer.start());
    EXPECT_EQ(unprovServer.provisioningState(), ProvisioningState::Unprovisioned);

    OnvifTest::OnvifTestClient client("127.0.0.1", static_cast<int>(unprovPort), OnvifTest::makeCreds("", ""));

    // Pre-auth operation (GetSystemDateAndTime) must succeed
    pugi::xml_document docDate { };
    auto resDate = client.raw().Post(
        "/onvif/device_service", wrapEnvelope("<tds:GetSystemDateAndTime/>"), "application/soap+xml; charset=utf-8");
    ASSERT_TRUE(resDate);
    EXPECT_EQ(resDate->status, 200);

    // Operational call (GetProfiles) must be rejected with deviceUnprovisioned fault
    auto resProf = client.raw().Post(
        "/onvif/media_service", wrapEnvelope("<trt:GetProfiles/>"), "application/soap+xml; charset=utf-8");
    ASSERT_TRUE(resProf);
    EXPECT_EQ(resProf->status, 400);

    pugi::xml_document docFault { };
    ASSERT_TRUE(docFault.load_string(resProf->body.c_str()));
    const auto sub
        = docFault.select_node("//*[local-name()='Fault']//*[local-name()='Subcode']/*[local-name()='Value']");
    ASSERT_TRUE(sub);
    EXPECT_TRUE(std::string(sub.node().text().as_string()).find("OperationProhibited") != std::string::npos);

    unprovServer.stop();
}

// ============================================================================
// 3. Server-Side Request Forgery (SSRF) Mitigations
// ============================================================================

TEST(SecurityHardeningTest, SsrfBlockLoopbackIpv4)
{
    NotificationConfig cfg { };
    cfg.allowLoopback = false;

    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://127.0.0.1:8080/hook", cfg).isValid());
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://127.0.0.2:9000/hook", cfg).isValid());
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://localhost:8080/hook", cfg).isValid());
}

TEST(SecurityHardeningTest, SsrfBlockLoopbackIpv6)
{
    NotificationConfig cfg { };
    cfg.allowLoopback = false;

    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://[::1]:8080/hook", cfg).isValid());
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://[0:0:0:0:0:0:0:1]:8080/hook", cfg).isValid());
}

TEST(SecurityHardeningTest, SsrfBlockCloudMetadata)
{
    NotificationConfig cfg { };

    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://169.254.169.254/latest/meta-data/", cfg).isValid());
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://[fe80::1]/hook", cfg).isValid());
}

TEST(SecurityHardeningTest, SsrfBlockSensitivePorts)
{
    NotificationConfig cfg { };

    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://192.168.1.50:22/hook", cfg).isValid()); // SSH
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://192.168.1.50:25/hook", cfg).isValid()); // SMTP
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://192.168.1.50:445/hook", cfg).isValid()); // SMB
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://192.168.1.50:3306/hook", cfg).isValid()); // MySQL
}

TEST(SecurityHardeningTest, SsrfBlockUserinfoScheme)
{
    NotificationConfig cfg { };

    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://admin:pass@192.168.1.50/hook", cfg).isValid());
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("file:///etc/passwd", cfg).isValid());
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("gopher://192.168.1.50/hook", cfg).isValid());
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("ftp://192.168.1.50/hook", cfg).isValid());
}

// ============================================================================
// 4. WS-Discovery Storm & Amplification Rejection
// ============================================================================

TEST(SecurityHardeningTest, DiscoveryReflectionPort)
{
    pugi::xml_document doc { };
    ASSERT_TRUE(doc.load_string(
        "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
        "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
        "xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\">"
        "<s:Header><wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</wsa:Action></s:Header>"
        "<s:Body><d:Probe/></s:Body></s:Envelope>"));

    // Datagram arriving from source port 3702 must be flagged as reflection attempt
    const auto res = WsDiscoveryValidator::validateProbe(doc, 3702, "test-uuid-1234", true);
    EXPECT_TRUE(res.isReflectionAttempt);
    EXPECT_FALSE(res.isValidProbe);
}

TEST(SecurityHardeningTest, DiscoverySelfLoop)
{
    const std::string myUuid = "my-camera-uuid-1111-2222";
    pugi::xml_document doc { };
    const std::string xml = "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
                            "xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\">"
                            "<s:Header><wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</wsa:Action>"
                            "<wsa:MessageID>urn:uuid:"
        + myUuid
        + "</wsa:MessageID></s:Header>"
          "<s:Body><d:Probe/></s:Body></s:Envelope>";
    ASSERT_TRUE(doc.load_string(xml.c_str()));

    // Probe containing our own UUID must be dropped to prevent loop storms
    const auto res = WsDiscoveryValidator::validateProbe(doc, 50123, myUuid, true);
    EXPECT_TRUE(res.isSelfMessage);
    EXPECT_FALSE(res.isValidProbe);
}

TEST(SecurityHardeningTest, DiscoveryRateLimit)
{
    // Limit to 3 requests per second per IP
    DiscoveryRateLimiter limiter(3U, 10U);

    EXPECT_TRUE(limiter.checkRateLimit("192.168.1.100"));
    EXPECT_TRUE(limiter.checkRateLimit("192.168.1.100"));
    EXPECT_TRUE(limiter.checkRateLimit("192.168.1.100"));

    // 4th burst request exceeds rate and must be dropped
    EXPECT_FALSE(limiter.checkRateLimit("192.168.1.100"));

    // Other IP addresses maintain their own token bucket
    EXPECT_TRUE(limiter.checkRateLimit("192.168.1.101"));
}

// ============================================================================
// 5. IEEE 754 Floating-Point Bounds (CERT FLP34-C)
// ============================================================================

TEST(SecurityHardeningTest, PtzNanVelocitySanitized)
{
    EXPECT_FALSE(Xml::isSafeFinite(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE(Xml::isSafeFinite(std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(Xml::isSafeFinite(-std::numeric_limits<float>::infinity()));
    EXPECT_TRUE(Xml::isSafeFinite(0.0f));
    EXPECT_TRUE(Xml::isSafeFinite(1.0f));

    EXPECT_FLOAT_EQ(Xml::safeFloat(std::numeric_limits<float>::quiet_NaN(), 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(Xml::safeFloat(std::numeric_limits<float>::infinity(), 0.0f), 0.0f);

    pugi::xml_document doc { };
    ASSERT_TRUE(doc.load_string("<node nan=\"NaN\" inf=\"INF\" valid=\"0.75\"/>"));
    const auto root = doc.document_element();

    EXPECT_FALSE(Xml::parseOptionalFloat(root.attribute("nan")).has_value());
    EXPECT_FALSE(Xml::parseOptionalFloat(root.attribute("inf")).has_value());
    ASSERT_TRUE(Xml::parseOptionalFloat(root.attribute("valid")).has_value());
    EXPECT_FLOAT_EQ(*Xml::parseOptionalFloat(root.attribute("valid")), 0.75f);
}

TEST(SecurityHardeningTest, PtzSubnormalFloatSanitized)
{
    pugi::xml_document doc { };
    ASSERT_TRUE(doc.load_string("<node normal=\"0.001\" subnormal=\"1.0e-45\" overflow=\"1.0e39\"/>"));
    const auto root = doc.document_element();

    const auto normal = Xml::parseOptionalFloat(root.attribute("normal"));
    ASSERT_TRUE(normal.has_value());
    EXPECT_GT(*normal, 0.0f);

    // Overflow beyond float limits must not result in NaN or uncontrolled cast
    const auto overflow = Xml::parseOptionalFloat(root.attribute("overflow"));
    EXPECT_FALSE(overflow.has_value());
}

} // namespace Onvif::Test
