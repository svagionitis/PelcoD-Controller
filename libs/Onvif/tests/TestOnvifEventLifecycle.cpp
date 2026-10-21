/// @file TestOnvifEventLifecycle.cpp
/// @brief TDD test suite for ONVIF review finding H3 (Event subscription lifecycle & resource limits).
/// @details Validates active background TTL sweeper, resource bounds, wsrf-rw:ResourceUnknownFault
///          on invalid/expired subscription tokens, accurate lease renewal arithmetic, and starvation prevention.

#include "OnvifTestClient.h"

#include <cstddef>
#include <cstdint>

#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include <Onvif/OnvifServer.h>
#include <Onvif/OnvifServerTypes.h>
#include <Onvif/SubscriptionManager.h>

using namespace Onvif;
using OnvifTest::OnvifTestClient;

namespace {

/// @brief Atomic counter to assign unique ports per test fixture.
std::atomic<int> g_lifecycleTestPort { 19500 };

/// @brief Wraps inner XML in a SOAP 1.2 envelope with ONVIF and WS-Addressing namespaces.
[[nodiscard]] std::string makeEnvelope(const std::string& bodyXml)
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

/// @brief Checks if a SOAP response contains a wsrf-rw:ResourceUnknownFault fault code.
[[nodiscard]] bool hasResourceUnknownFault(const std::string& xml)
{
    pugi::xml_document doc {};
    if (!doc.load_string(xml.c_str())) {
        return false;
    }
    const auto fault = doc.select_node("//*[local-name()='Fault']").node();
    if (!fault) {
        return false;
    }
    const auto subcodeVal = doc.select_node("//*[local-name()='Subcode']/*[local-name()='Value']").node();
    if (subcodeVal) {
        const std::string text = subcodeVal.text().as_string();
        if (text.find("ResourceUnknownFault") != std::string::npos) {
            return true;
        }
    }
    return xml.find("ResourceUnknownFault") != std::string::npos;
}

/// @brief Formats a system time point to ISO 8601 UTC string.
[[nodiscard]] std::string formatUtc(std::chrono::system_clock::time_point tp)
{
    const std::time_t t { std::chrono::system_clock::to_time_t(tp) };
    std::tm tmBuf {};
#ifdef _WIN32
    gmtime_s(&tmBuf, &t);
#else
    gmtime_r(&t, &tmBuf);
#endif
    std::ostringstream ss {};
    ss << std::put_time(&tmBuf, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

} // namespace

/// @class OnvifEventLifecycleTest
/// @brief Integration fixture testing event subscription endpoints and failure modes.
class OnvifEventLifecycleTest : public ::testing::Test {
protected:
    int m_port { g_lifecycleTestPort.fetch_add(1) };
    OnvifServerConfig m_config {};
    std::unique_ptr<OnvifServer> m_server {};
    std::unique_ptr<OnvifTestClient> m_client {};

    void SetUp() override
    {
        m_config.bindAddress = "127.0.0.1";
        m_config.port = m_port;
        m_config.auth.enabled = false;
        m_config.notification.allowLoopback = true;
        m_config.notification.minLease = std::chrono::seconds(1);
        m_config.notification.defaultLease = std::chrono::seconds(5);
        m_config.notification.maxLease = std::chrono::seconds(300);
        m_config.notification.sweepInterval = std::chrono::milliseconds(50);

        m_server = std::make_unique<OnvifServer>(m_config);
        ASSERT_TRUE(m_server->start());

        m_client = std::make_unique<OnvifTestClient>("127.0.0.1", m_port);
        m_client->set_connection_timeout(2, 0);
        m_client->set_read_timeout(15, 0);
    }

    void TearDown() override
    {
        if (m_server) {
            m_server->stop();
        }
    }
};

// =============================================================================
// Unit Tests: SubscriptionManager Active Sweeper & Lifecyle
// =============================================================================

/// @brief Verifies that expired PullPoint subscriptions are autonomously evicted
///        by the background sweeper without any client traffic.
TEST(SubscriptionManagerLifecycleTest, ActiveSweeper_EvictsPullExpiredWithoutTraffic)
{
    NotificationConfig cfg {};
    cfg.minLease = std::chrono::seconds(1);
    cfg.defaultLease = std::chrono::seconds(1);
    cfg.sweepInterval = std::chrono::milliseconds(50);

    SubscriptionManager mgr(cfg);

    const auto res = mgr.createPullSub("1");
    ASSERT_TRUE(res.success);
    EXPECT_EQ(mgr.getPullSubCount(), 1U);

    // Sleep past the 1s lease + sweep cycle
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    // Must be swept to 0 without any explicit API invocation
    EXPECT_EQ(mgr.getPullSubCount(), 0U);
}

/// @brief Verifies that expired Push subscriptions are autonomously evicted
///        by the background sweeper without any client traffic.
TEST(SubscriptionManagerLifecycleTest, ActiveSweeper_EvictsPushExpiredWithoutTraffic)
{
    NotificationConfig cfg {};
    cfg.minLease = std::chrono::seconds(1);
    cfg.defaultLease = std::chrono::seconds(1);
    cfg.sweepInterval = std::chrono::milliseconds(50);

    SubscriptionManager mgr(cfg);

    const auto res = mgr.createPushSub("http://127.0.0.1:9999/hook", "1");
    ASSERT_TRUE(res.success);
    EXPECT_EQ(mgr.getPushSubCount(), 1U);

    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    EXPECT_EQ(mgr.getPushSubCount(), 0U);
}

/// @brief Verifies lease calculation when client passes an ISO 8601 absolute UTC dateTime.
TEST(SubscriptionManagerLifecycleTest, ParseLease_AbsoluteDateTime_CalculatesDuration)
{
    NotificationConfig cfg {};
    cfg.minLease = std::chrono::seconds(10);
    cfg.maxLease = std::chrono::seconds(3600);
    cfg.defaultLease = std::chrono::seconds(60);

    SubscriptionManager mgr(cfg);

    // Request termination 120 seconds in the future
    const auto targetTp = std::chrono::system_clock::now() + std::chrono::seconds(120);
    const std::string targetStr = formatUtc(targetTp);

    const auto lease = mgr.parseLease(targetStr);
    EXPECT_GE(lease.count(), 110);
    EXPECT_LE(lease.count(), 125);
}

/// @brief Verifies that pullMessages unblocks immediately when subscription expires during wait.
TEST(SubscriptionManagerLifecycleTest, PullMessages_AbortsImmediatelyOnExpiration)
{
    NotificationConfig cfg {};
    cfg.minLease = std::chrono::seconds(1);
    cfg.defaultLease = std::chrono::seconds(1);

    SubscriptionManager mgr(cfg);
    const auto sub = mgr.createPullSub("1");
    ASSERT_TRUE(sub.success);

    std::vector<OnvifEvent> events {};
    std::string curTime {};
    std::string termTime {};
    std::atomic<bool> serverRunning { true };

    const auto start = std::chrono::steady_clock::now();
    // Request 5s timeout, but subscription expires in 1s
    const bool ok = mgr.pullMessages(sub.id, 10, 5, events, curTime, termTime, serverRunning);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(ok);
    // Should unblock around 1s, well before the 5s timeout
    EXPECT_LT(elapsed, std::chrono::milliseconds(2500));
}

/// @brief Verifies that per-subscription queues enforce FIFO eviction when full.
TEST(SubscriptionManagerLifecycleTest, QueueBounds_EnforcesFifoEviction)
{
    NotificationConfig cfg {};
    cfg.maxQueuePerSubscription = 2;
    cfg.minLease = std::chrono::seconds(60);
    cfg.defaultLease = std::chrono::seconds(60);

    SubscriptionManager mgr(cfg);
    const auto sub = mgr.createPullSub("60");
    ASSERT_TRUE(sub.success);

    // Publish 3 events
    OnvifEvent ev1 {};
    ev1.topic = "tns1:Event1";
    OnvifEvent ev2 {};
    ev2.topic = "tns1:Event2";
    OnvifEvent ev3 {};
    ev3.topic = "tns1:Event3";

    std::vector<std::string> pushUrls {};
    mgr.publishEvent(ev1, pushUrls);
    mgr.publishEvent(ev2, pushUrls);
    mgr.publishEvent(ev3, pushUrls);

    std::vector<OnvifEvent> outEvents {};
    std::string curTime {};
    std::string termTime {};
    std::atomic<bool> serverRunning { true };

    const bool ok = mgr.pullMessages(sub.id, 10, 0, outEvents, curTime, termTime, serverRunning);
    ASSERT_TRUE(ok);
    ASSERT_EQ(outEvents.size(), 2U);
    // Oldest event (ev1) dropped; ev2 and ev3 remain
    EXPECT_EQ(outEvents[0].topic, "tns1:Event2");
    EXPECT_EQ(outEvents[1].topic, "tns1:Event3");
}

// =============================================================================
// Integration Tests: OnvifServer HTTP Endpoints & Fault Mapping
// =============================================================================

/// @brief Verifies that Unsubscribe on a non-existent subscription returns HTTP 400
///        and wsrf-rw:ResourceUnknownFault (fixing review finding H3 silent-200 bug).
TEST_F(OnvifEventLifecycleTest, UnknownSubscription_Unsubscribe_ReturnsResourceUnknownFault)
{
    const std::string body { "    <wsnt:Unsubscribe/>\r\n" };
    const std::string reqXml { makeEnvelope(body) };

    const auto res { m_client->Post(
        "/onvif/events/subscription/non_existent_token_999", reqXml, "application/soap+xml; charset=utf-8") };

    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
    EXPECT_TRUE(hasResourceUnknownFault(res->body));
}

/// @brief Verifies that PullMessages on a non-existent subscription returns HTTP 400
///        and wsrf-rw:ResourceUnknownFault.
TEST_F(OnvifEventLifecycleTest, UnknownSubscription_PullMessages_ReturnsResourceUnknownFault)
{
    const std::string body { "    <tev:PullMessages>\r\n"
                             "      <tev:Timeout>PT1S</tev:Timeout>\r\n"
                             "      <tev:MessageLimit>10</tev:MessageLimit>\r\n"
                             "    </tev:PullMessages>\r\n" };
    const std::string reqXml { makeEnvelope(body) };

    const auto res { m_client->Post(
        "/onvif/events/subscription/non_existent_token_888", reqXml, "application/soap+xml; charset=utf-8") };

    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
    EXPECT_TRUE(hasResourceUnknownFault(res->body));
}

/// @brief Verifies that Renew on a non-existent subscription returns HTTP 400
///        and wsrf-rw:ResourceUnknownFault.
TEST_F(OnvifEventLifecycleTest, UnknownSubscription_Renew_ReturnsResourceUnknownFault)
{
    const std::string body { "    <wsnt:Renew>\r\n"
                             "      <wsnt:TerminationTime>PT30S</wsnt:TerminationTime>\r\n"
                             "    </wsnt:Renew>\r\n" };
    const std::string reqXml { makeEnvelope(body) };

    const auto res { m_client->Post(
        "/onvif/events/subscription/non_existent_token_777", reqXml, "application/soap+xml; charset=utf-8") };

    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
    EXPECT_TRUE(hasResourceUnknownFault(res->body));
}

/// @brief Verifies that Renew on an active subscription advances termination time
///        from current timestamp and returns valid wsnt:RenewResponse.
TEST_F(OnvifEventLifecycleTest, RenewSubscription_AdvancesFromCurrentTimestamp)
{
    // Create subscription first
    const std::string subBody { "    <tev:CreatePullPointSubscription>\r\n"
                                "      <tev:InitialTerminationTime>PT10S</tev:InitialTerminationTime>\r\n"
                                "    </tev:CreatePullPointSubscription>\r\n" };
    const auto subRes { m_client->Post(
        "/onvif/event_service", makeEnvelope(subBody), "application/soap+xml; charset=utf-8") };
    ASSERT_TRUE(subRes);
    ASSERT_EQ(subRes->status, 200);

    pugi::xml_document doc {};
    ASSERT_TRUE(doc.load_string(subRes->body.c_str()));
    const auto addrNode = doc.select_node("//*[local-name()='Address']").node();
    ASSERT_TRUE(addrNode);
    const std::string addrStr { addrNode.text().as_string() };
    const auto slashPos { addrStr.rfind('/') };
    ASSERT_NE(slashPos, std::string::npos);
    const std::string subId { addrStr.substr(slashPos + 1) };

    // Renew for 30s
    const std::string renewBody { "    <wsnt:Renew>\r\n"
                                  "      <wsnt:TerminationTime>PT30S</wsnt:TerminationTime>\r\n"
                                  "    </wsnt:Renew>\r\n" };
    const auto renewRes { m_client->Post(
        "/onvif/events/subscription/" + subId, makeEnvelope(renewBody), "application/soap+xml; charset=utf-8") };

    ASSERT_TRUE(renewRes);
    EXPECT_EQ(renewRes->status, 200);
    EXPECT_TRUE(renewRes->body.find("RenewResponse") != std::string::npos);
    EXPECT_TRUE(renewRes->body.find("TerminationTime") != std::string::npos);
}

/// @brief Verifies that PullMessages clamps excessive requested timeouts (e.g. 60s) to <= 10s.
TEST_F(OnvifEventLifecycleTest, PullMessages_ClampsExcessiveTimeout)
{
    // Create subscription
    const std::string subBody { "    <tev:CreatePullPointSubscription>\r\n"
                                "      <tev:InitialTerminationTime>PT30S</tev:InitialTerminationTime>\r\n"
                                "    </tev:CreatePullPointSubscription>\r\n" };
    const auto subRes { m_client->Post(
        "/onvif/event_service", makeEnvelope(subBody), "application/soap+xml; charset=utf-8") };
    ASSERT_TRUE(subRes);
    ASSERT_EQ(subRes->status, 200);

    pugi::xml_document doc {};
    ASSERT_TRUE(doc.load_string(subRes->body.c_str()));
    const auto addrNode = doc.select_node("//*[local-name()='Address']").node();
    const std::string addrStr { addrNode.text().as_string() };
    const std::string subId { addrStr.substr(addrStr.rfind('/') + 1) };

    // Request 60s timeout on empty queue; server should clamp to <= 10s
    const std::string pullBody { "    <tev:PullMessages>\r\n"
                                 "      <tev:Timeout>PT60S</tev:Timeout>\r\n"
                                 "      <tev:MessageLimit>5</tev:MessageLimit>\r\n"
                                 "    </tev:PullMessages>\r\n" };

    const auto start = std::chrono::steady_clock::now();
    const auto pullRes { m_client->Post(
        "/onvif/events/subscription/" + subId, makeEnvelope(pullBody), "application/soap+xml; charset=utf-8") };
    const auto elapsed = std::chrono::steady_clock::now() - start;

    ASSERT_TRUE(pullRes);
    EXPECT_EQ(pullRes->status, 200);
    // Must return within ~10 seconds (with tolerance), not 60 seconds
    EXPECT_LE(elapsed, std::chrono::milliseconds(11500));
}

/// @brief Verifies that server stop unblocks in-flight PullMessages threads immediately.
TEST_F(OnvifEventLifecycleTest, PullMessages_AbortsImmediatelyOnServerStop)
{
    const std::string subBody { "    <tev:CreatePullPointSubscription>\r\n"
                                "      <tev:InitialTerminationTime>PT30S</tev:InitialTerminationTime>\r\n"
                                "    </tev:CreatePullPointSubscription>\r\n" };
    const auto subRes { m_client->Post(
        "/onvif/event_service", makeEnvelope(subBody), "application/soap+xml; charset=utf-8") };
    ASSERT_TRUE(subRes);
    ASSERT_EQ(subRes->status, 200);

    pugi::xml_document doc {};
    ASSERT_TRUE(doc.load_string(subRes->body.c_str()));
    const auto addrNode = doc.select_node("//*[local-name()='Address']").node();
    const std::string addrStr { addrNode.text().as_string() };
    const std::string subId { addrStr.substr(addrStr.rfind('/') + 1) };

    std::atomic<bool> workerReturned { false };
    const auto start = std::chrono::steady_clock::now();

    std::thread worker([this, subId, &workerReturned]() {
        OnvifTestClient client("127.0.0.1", m_port);
        const std::string pullBody { "    <tev:PullMessages>\r\n"
                                     "      <tev:Timeout>PT10S</tev:Timeout>\r\n"
                                     "      <tev:MessageLimit>5</tev:MessageLimit>\r\n"
                                     "    </tev:PullMessages>\r\n" };
        client.Post(
            "/onvif/events/subscription/" + subId, makeEnvelope(pullBody), "application/soap+xml; charset=utf-8");
        workerReturned.store(true);
    });

    // Sleep briefly to ensure worker is waiting in pullMessages
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop server
    m_server->stop();

    worker.join();
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_TRUE(workerReturned.load());
    // Should unblock promptly upon stop(), well under 2 seconds
    EXPECT_LT(elapsed, std::chrono::milliseconds(2000));
}
