/// @file TestOnvifEventSecurity.cpp
/// @brief TDD test suite for ONVIF review finding C5 (SSRF defense & bounded event broker).
/// @details Validates that ConsumerReference URLs in wsnt:Subscribe are protected against SSRF,
///          subscription quotas are strictly enforced, and event dispatch does not exhaust threads.

#include "OnvifTestClient.h"

#include <Onvif/HttplibInclude.h>
#include <Onvif/NotificationDispatcher.h>
#include <Onvif/NotificationUrlValidator.h>
#include <Onvif/OnvifSecurity.h>
#include <Onvif/OnvifServer.h>
#include <Onvif/SubscriptionManager.h>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace Onvif;
using OnvifTest::defaultAdmin;
using OnvifTest::OnvifTestClient;

namespace {

/// @brief Atomic counter to assign unique ports per test fixture.
std::atomic<int> g_eventTestPort { 19200 };

/// @brief Wraps a SOAP operation in a standard SOAP 1.2 envelope.
/// @param[in] bodyXml Inner XML body.
/// @return Complete SOAP envelope string.
std::string makeEnvelope(const std::string& bodyXml)
{
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
           "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
           "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\" "
           "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
           "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\">\r\n"
           "  <s:Body>\r\n"
        + bodyXml
        + "  </s:Body>\r\n"
          "</s:Envelope>";
}

/// @brief Builds a wsnt:Subscribe request payload.
/// @param[in] consumerUrl Target notification consumer reference URL.
/// @param[in] initTerm Optional initial termination duration or time.
/// @return Serialized Subscribe SOAP body.
std::string makeSubscribeBody(const std::string& consumerUrl, const std::string& initTerm = "")
{
    std::string out { "    <wsnt:Subscribe>\r\n" };
    out.append("      <wsnt:ConsumerReference>\r\n")
        .append("        <wsa:Address>")
        .append(consumerUrl)
        .append("</wsa:Address>\r\n")
        .append("      </wsnt:ConsumerReference>\r\n");
    if (!initTerm.empty()) {
        out.append("      <wsnt:InitialTerminationTime>").append(initTerm).append("</wsnt:InitialTerminationTime>\r\n");
    }
    out.append("    </wsnt:Subscribe>\r\n");
    return out;
}

/// @brief Checks if a SOAP response contains a SOAP-ENV:Fault element.
/// @param[in] xml Response payload string.
/// @return True if Fault element is present.
[[nodiscard]] bool hasSoapFault(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return false;
    }
    return !doc.select_node("//*[local-name()='Fault']").node().empty();
}

} // namespace

/// @class OnvifEventSecurityTest
/// @brief Integration fixture testing SSRF and subscription constraints on OnvifServer.
class OnvifEventSecurityTest : public ::testing::Test {
protected:
    int m_port { g_eventTestPort.fetch_add(1) };
    OnvifServerConfig m_config {};
    std::unique_ptr<OnvifServer> m_server {};
    std::unique_ptr<OnvifTestClient> m_client {};

    void SetUp() override
    {
        m_config.bindAddress = "127.0.0.1";
        m_config.port = m_port;
        m_config.auth.enabled = false; // Focus tests on event security logic
        m_config.notification.allowLoopback = false; // Strict SSRF enforcement

        m_server = std::make_unique<OnvifServer>(m_config);
        ASSERT_TRUE(m_server->start());

        m_client = std::make_unique<OnvifTestClient>("127.0.0.1", m_port);
        m_client->set_connection_timeout(2, 0);
        m_client->set_read_timeout(3, 0);
    }

    void TearDown() override
    {
        if (m_server) {
            m_server->stop();
        }
    }
};

/// @brief Verifies that Subscribe rejects cloud metadata service addresses (169.254.169.254).
TEST_F(OnvifEventSecurityTest, SsrfCloudMetadataAddressRejected)
{
    const std::string reqXml { makeEnvelope(
        makeSubscribeBody("http://169.254.169.254/latest/meta-data/iam/security-credentials/")) };
    const auto res { m_client->Post("/onvif/event_service", reqXml, "application/soap+xml; charset=utf-8") };

    ASSERT_TRUE(res);
    EXPECT_NE(res->status, 200);
    EXPECT_TRUE(hasSoapFault(res->body));
    EXPECT_TRUE(res->body.find("SubscribeResponse") == std::string::npos);
}

/// @brief Verifies that Subscribe rejects loopback URLs when allowLoopback is false.
TEST_F(OnvifEventSecurityTest, SsrfLoopbackAddressRejectedByDefault)
{
    const std::string reqXml { makeEnvelope(makeSubscribeBody("http://127.0.0.1:8080/internal/admin")) };
    const auto res { m_client->Post("/onvif/event_service", reqXml, "application/soap+xml; charset=utf-8") };

    ASSERT_TRUE(res);
    EXPECT_NE(res->status, 200);
    EXPECT_TRUE(hasSoapFault(res->body));
    EXPECT_TRUE(res->body.find("SubscribeResponse") == std::string::npos);
}

/// @brief Verifies that Subscribe rejects sensitive/privileged service ports (SSH 22).
TEST_F(OnvifEventSecurityTest, SsrfSensitivePortRejected)
{
    const std::string reqXml { makeEnvelope(makeSubscribeBody("http://192.168.1.100:22/probe")) };
    const auto res { m_client->Post("/onvif/event_service", reqXml, "application/soap+xml; charset=utf-8") };

    ASSERT_TRUE(res);
    EXPECT_NE(res->status, 200);
    EXPECT_TRUE(hasSoapFault(res->body));
}

/// @brief Verifies that Subscribe rejects non-HTTP/HTTPS schemes (e.g. file, gopher, ftp).
TEST_F(OnvifEventSecurityTest, SsrfDangerousSchemeRejected)
{
    const std::string reqXml { makeEnvelope(makeSubscribeBody("file:///etc/shadow")) };
    const auto res { m_client->Post("/onvif/event_service", reqXml, "application/soap+xml; charset=utf-8") };

    ASSERT_TRUE(res);
    EXPECT_NE(res->status, 200);
    EXPECT_TRUE(hasSoapFault(res->body));
}

/// @brief Verifies that Subscribe rejects URLs with embedded userinfo (credentials).
TEST_F(OnvifEventSecurityTest, SsrfUserinfoCredentialsRejected)
{
    const std::string reqXml { makeEnvelope(makeSubscribeBody("http://admin:secret@192.168.1.50:8080/hook")) };
    const auto res { m_client->Post("/onvif/event_service", reqXml, "application/soap+xml; charset=utf-8") };

    ASSERT_TRUE(res);
    EXPECT_NE(res->status, 200);
    EXPECT_TRUE(hasSoapFault(res->body));
}

/// @brief Verifies that exceeding maxPushSubscriptions rejects further subscriptions.
TEST_F(OnvifEventSecurityTest, PushSubscriptionQuotaEnforced)
{
    // Reconfigure server with small quota and loopback enabled for quota testing
    m_server->stop();
    m_config.notification.allowLoopback = true;
    m_config.notification.maxPushSubscriptions = 2;
    m_server = std::make_unique<OnvifServer>(m_config);
    ASSERT_TRUE(m_server->start());

    const std::string req1 { makeEnvelope(makeSubscribeBody("http://127.0.0.1:19999/sub1")) };
    const auto res1 { m_client->Post("/onvif/event_service", req1, "application/soap+xml; charset=utf-8") };
    ASSERT_TRUE(res1);
    EXPECT_EQ(res1->status, 200);

    const std::string req2 { makeEnvelope(makeSubscribeBody("http://127.0.0.1:19999/sub2")) };
    const auto res2 { m_client->Post("/onvif/event_service", req2, "application/soap+xml; charset=utf-8") };
    ASSERT_TRUE(res2);
    EXPECT_EQ(res2->status, 200);

    // Third subscription exceeds quota
    const std::string req3 { makeEnvelope(makeSubscribeBody("http://127.0.0.1:19999/sub3")) };
    const auto res3 { m_client->Post("/onvif/event_service", req3, "application/soap+xml; charset=utf-8") };
    ASSERT_TRUE(res3);
    EXPECT_NE(res3->status, 200);
    EXPECT_TRUE(hasSoapFault(res3->body));
}

/// @brief Verifies that publishing rapid bursts of events does not exhaust threads or hang stop().
TEST_F(OnvifEventSecurityTest, BurstEventPublishDoesNotExhaustThreads)
{
    // Register one push subscription pointing to non-responding endpoint
    m_server->stop();
    m_config.notification.allowLoopback = true;
    m_server = std::make_unique<OnvifServer>(m_config);
    ASSERT_TRUE(m_server->start());

    const std::string reqSub { makeEnvelope(makeSubscribeBody("http://127.0.0.1:19998/blackhole")) };
    const auto resSub { m_client->Post("/onvif/event_service", reqSub, "application/soap+xml; charset=utf-8") };
    ASSERT_TRUE(resSub);
    EXPECT_EQ(resSub->status, 200);

    // Rapid burst of 200 events
    for (int i { 0 }; i < 200; ++i) {
        OnvifEvent ev {};
        ev.topic = "tns1:RuleEngine/CellMotionDetector/Motion";
        ev.dataName = "IsMotion";
        ev.dataValue = (i % 2 == 0) ? "true" : "false";
        m_server->publishEvent(ev);
    }

    // Server stop must terminate cleanly without waiting for 200 detached threads
    const auto startStop { std::chrono::steady_clock::now() };
    m_server->stop();
    const auto elapsed { std::chrono::steady_clock::now() - startStop };
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 3000);
}

/// @brief Verifies that PullMessages with non-existent or expired subscription returns a SOAP Fault.
TEST_F(OnvifEventSecurityTest, PullMessagesUnknownSubIdFault)
{
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

    // Request non-existent subscription ID
    const auto res { m_client->Post(
        "/onvif/events/subscription/nonexistent_id", pullReq, "application/soap+xml; charset=utf-8") };
    ASSERT_TRUE(res);
    EXPECT_NE(res->status, 200);
    EXPECT_TRUE(hasSoapFault(res->body));
    EXPECT_TRUE(res->body.find("ResourceUnknownFault") != std::string::npos);
}

/// @brief Verifies that Renew on active subscription extends lease, and Renew on unknown subId faults.
TEST_F(OnvifEventSecurityTest, RenewLifecycleEnforced)
{
    // Create valid PullPoint subscription
    const std::string subReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                               "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\">\r\n"
                               "  <SOAP-ENV:Body>\r\n"
                               "    <tev:CreatePullPointSubscription/>\r\n"
                               "  </SOAP-ENV:Body>\r\n"
                               "</SOAP-ENV:Envelope>";
    const auto resSub { m_client->Post("/onvif/event_service", subReq, "application/soap+xml; charset=utf-8") };
    ASSERT_TRUE(resSub && resSub->status == 200);

    pugi::xml_document doc {};
    ASSERT_TRUE(doc.load_string(resSub->body.c_str()));
    const auto addrNode = doc.select_node("//*[local-name()='Address']").node();
    ASSERT_TRUE(addrNode);
    const std::string subUrl = addrNode.text().as_string();
    const auto slashPos = subUrl.find("/onvif/events/subscription/");
    ASSERT_NE(slashPos, std::string::npos);
    const std::string subPath = subUrl.substr(slashPos);

    // Renew valid subscription
    const std::string renewReq = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                                 "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                                 "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\">\r\n"
                                 "  <SOAP-ENV:Body>\r\n"
                                 "    <wsnt:Renew>\r\n"
                                 "      <wsnt:TerminationTime>PT30M</wsnt:TerminationTime>\r\n"
                                 "    </wsnt:Renew>\r\n"
                                 "  </SOAP-ENV:Body>\r\n"
                                 "</SOAP-ENV:Envelope>";
    const auto resRenew { m_client->Post(subPath.c_str(), renewReq, "application/soap+xml; charset=utf-8") };
    ASSERT_TRUE(resRenew);
    EXPECT_EQ(resRenew->status, 200);
    EXPECT_TRUE(resRenew->body.find("RenewResponse") != std::string::npos);

    // Renew unknown subscription returns fault
    const auto resRenewBogus { m_client->Post(
        "/onvif/events/subscription/99999", renewReq, "application/soap+xml; charset=utf-8") };
    ASSERT_TRUE(resRenewBogus);
    EXPECT_NE(resRenewBogus->status, 200);
    EXPECT_TRUE(hasSoapFault(resRenewBogus->body));
}

/// @brief Unit tests for NotificationUrlValidator.
TEST(NotificationUrlValidatorTest, UnitValidations)
{
    NotificationConfig cfg {};
    cfg.allowLoopback = false;

    // Loopback
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://127.0.0.1:80/hook", cfg).isValid());
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://localhost:80/hook", cfg).isValid());
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://[::1]:80/hook", cfg).isValid());

    // Cloud metadata
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://169.254.169.254/meta", cfg).isValid());
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://169.254.1.1/meta", cfg).isValid());

    // Schemes
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("ftp://192.168.1.1/hook", cfg).isValid());
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("gopher://192.168.1.1/hook", cfg).isValid());
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("javascript:alert(1)", cfg).isValid());

    // Userinfo
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://user:pass@192.168.1.50/hook", cfg).isValid());

    // Ports
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://192.168.1.50:22/hook", cfg).isValid());
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://192.168.1.50:25/hook", cfg).isValid());
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://192.168.1.50:3306/hook", cfg).isValid());

    // Allowed LAN IP
    EXPECT_TRUE(NotificationUrlValidator::validateUrl("http://192.168.1.50:8080/hook", cfg).isValid());
    EXPECT_TRUE(NotificationUrlValidator::validateUrl("https://192.168.1.50:8443/hook", cfg).isValid());

    // Whitelist
    cfg.allowedHosts = { "vms.local" };
    EXPECT_TRUE(NotificationUrlValidator::validateUrl("http://vms.local:8080/hook", cfg).isValid());
    EXPECT_FALSE(NotificationUrlValidator::validateUrl("http://other.local:8080/hook", cfg).isValid());
}

/// @brief Unit tests for SubscriptionManager lease parsing and clamping.
TEST(SubscriptionManagerTest, LeaseClampingAndQuotas)
{
    NotificationConfig cfg {};
    cfg.maxSubscriptions = 2;
    cfg.minLease = std::chrono::seconds(10);
    cfg.maxLease = std::chrono::seconds(100);
    cfg.defaultLease = std::chrono::seconds(30);

    SubscriptionManager mgr(cfg);

    // Clamps minimum
    EXPECT_EQ(mgr.parseLease("PT1S").count(), 10);
    // Clamps maximum
    EXPECT_EQ(mgr.parseLease("PT10000S").count(), 100);
    // Default when empty
    EXPECT_EQ(mgr.parseLease("").count(), 30);

    // Quotas
    const auto sub1 = mgr.createPullSub("PT30S");
    EXPECT_TRUE(sub1.success);
    const auto sub2 = mgr.createPullSub("PT30S");
    EXPECT_TRUE(sub2.success);
    const auto sub3 = mgr.createPullSub("PT30S");
    EXPECT_FALSE(sub3.success);
}
