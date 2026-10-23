/// @file EventService.cpp
#include "EventService.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <pugixml.hpp>

#include "NotificationDispatcher.h"
#include "NotificationUrlValidator.h"
#include "SoapFault.h"
#include "SoapRouter.h"
#include "SubscriptionManager.h"
#include "XmlUtils.h"

namespace Onvif {

namespace {
    constexpr std::string_view kEventsNs { "http://www.onvif.org/ver10/events/wsdl" };
    constexpr std::string_view kWsntNs { "http://docs.oasis-open.org/wsn/b-2" };

    std::string buildNotificationMessage(const OnvifEvent& ev)
    {
        std::ostringstream ss;
        const std::string op { ev.propertyOperation.empty() ? "Changed" : ev.propertyOperation };
        ss << "      <wsnt:NotificationMessage>\r\n"
           << "        <wsnt:Topic Dialect=\"http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet\" "
           << "xmlns:tns1=\"http://www.onvif.org/ver10/topics\">" << Xml::escapeXmlText(ev.topic) << "</wsnt:Topic>\r\n"
           << "        <wsnt:Message PropertyOperation=\"" << Xml::escapeXmlAttr(op) << "\">\r\n"
           << "          <tt:Message UtcTime=\"" << Xml::escapeXmlAttr(ev.utcTime) << "\" "
           << "PropertyOperation=\"" << Xml::escapeXmlAttr(op) << "\">\r\n";

        if (!ev.sourceName.empty()) {
            ss << "            <tt:Source>\r\n"
               << "              <tt:SimpleItem Name=\"" << Xml::escapeXmlAttr(ev.sourceName) << "\" Value=\""
               << Xml::escapeXmlAttr(ev.sourceValue) << "\"/>\r\n"
               << "            </tt:Source>\r\n";
        }

        if (!ev.dataName.empty()) {
            ss << "            <tt:Data>\r\n"
               << "              <tt:SimpleItem Name=\"" << Xml::escapeXmlAttr(ev.dataName) << "\" Value=\""
               << Xml::escapeXmlAttr(ev.dataValue) << "\"/>\r\n"
               << "            </tt:Data>\r\n";
        }

        ss << "          </tt:Message>\r\n"
           << "        </wsnt:Message>\r\n"
           << "      </wsnt:NotificationMessage>\r\n";
        return ss.str();
    }
}

EventService::EventService(EventServiceContext ctx)
    : m_ctx(std::move(ctx))
{
}

std::string_view EventService::serviceName() const noexcept
{
    return "Events";
}

void EventService::registerRoutes(SoapRouter& router)
{
    const std::string tevNs { kEventsNs };
    const std::string wsntNs { kWsntNs };

    router.registerRoute(tevNs, "GetServiceCapabilities", AccessClass::PreAuth, [](const SoapContext&) {
        return SoapHandlerResult::ok(
            "    <tev:GetServiceCapabilitiesResponse>\r\n"
            "      <tev:Capabilities WSSubscriptionPolicySupport=\"true\" WSPullPointSupport=\"true\" "
            "MaxPullPoints=\"10\" PersistentNotificationStorage=\"false\"/>\r\n"
            "    </tev:GetServiceCapabilitiesResponse>\r\n");
    });
    router.registerRoute(tevNs, "CreatePullPointSubscription", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleCreatePullPoint(ctx); });
    router.registerRoute(tevNs, "GetEventProperties", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetProperties(ctx); });
    router.registerRoute(tevNs, "SetSynchronizationPoint", AccessClass::Actuate, [this](const SoapContext& ctx) {
        std::string subId;
        const std::string pfx = "/onvif/events/subscription/";
        if (ctx.requestUrl.rfind(pfx, 0) == 0 && ctx.requestUrl.length() > pfx.length()) {
            subId = ctx.requestUrl.substr(pfx.length());
        }
        if (ctx.requestUrl.find("/subscription/") != std::string::npos) {
            if (subId.empty() || !m_ctx.subManager || !m_ctx.subManager->setSynchronizationPoint(subId)) {
                return SoapHandlerResult::fault("wsrf-rw:ResourceUnknownFault", "Subscription does not exist", 400);
            }
        } else if (m_ctx.subManager) {
            static_cast<void>(m_ctx.subManager->setSynchronizationPoint(""));
        }
        return SoapHandlerResult::ok("    <tev:SetSynchronizationPointResponse/>\r\n");
    });
    router.registerRoute(tevNs, "PullMessages", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handlePullMessages(ctx); });
    router.registerRoute(
        tevNs, "Seek", AccessClass::ReadMedia, [this](const SoapContext& ctx) { return handleSeek(ctx); });

    router.registerRoute(
        wsntNs, "Subscribe", AccessClass::ReadMedia, [this](const SoapContext& ctx) { return handleSubscribe(ctx); });
    router.registerRoute(
        wsntNs, "Renew", AccessClass::ReadMedia, [this](const SoapContext& ctx) { return handleRenew(ctx); });
    router.registerRoute(wsntNs, "Unsubscribe", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleUnsubscribe(ctx); });
}

SoapHandlerResult EventService::handleGetProperties(const SoapContext&)
{
    std::ostringstream ss;
    ss << "    <tev:GetEventPropertiesResponse>\r\n"
       << "      "
          "<tev:TopicNamespaceLocation>http://www.onvif.org/onvif/ver10/topics/topicns.xml</"
          "tev:TopicNamespaceLocation>\r\n"
       << "      <wsnt:FixedTopicSet>true</wsnt:FixedTopicSet>\r\n"
       << "      <wstop:TopicSet xmlns:wstop=\"http://docs.oasis-open.org/wsn/t-1\">\r\n"
       << "        <tns1:RuleEngine xmlns:tns1=\"http://www.onvif.org/ver10/topics\">\r\n"
       << "          <tns1:CellMotionDetector><tns1:Motion/></tns1:CellMotionDetector>\r\n"
       << "        </tns1:RuleEngine>\r\n"
       << "        <tns1:PTZController xmlns:tns1=\"http://www.onvif.org/ver10/topics\">\r\n"
       << "          <tns1:PTZPresets><tns1:Reached/></tns1:PTZPresets>\r\n"
       << "        </tns1:PTZController>\r\n"
       << "        <tns1:Device xmlns:tns1=\"http://www.onvif.org/ver10/topics\">\r\n"
       << "          <tns1:Trigger><tns1:DigitalInput/></tns1:Trigger>\r\n"
       << "        </tns1:Device>\r\n"
       << "      </wstop:TopicSet>\r\n"
       << "    </tev:GetEventPropertiesResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult EventService::handleCreatePullPoint(const SoapContext& ctx)
{
    const auto initTermNode = ctx.reqNode.select_node(".//*[local-name()='InitialTerminationTime']").node();
    const std::string initTermStr = initTermNode ? initTermNode.text().as_string() : "";
    const auto topicNode = ctx.reqNode.select_node(".//*[local-name()='TopicExpression']").node();
    const std::string topicFilter = topicNode ? topicNode.text().as_string() : "";

    if (!m_ctx.subManager) {
        return SoapHandlerResult::fault("ter:ActionNotSupported", "Event service unavailable", 500);
    }

    const auto result = m_ctx.subManager->createPullSub(initTermStr, topicFilter);
    if (!result.success) {
        return SoapHandlerResult::fault("wsnt:SubscribeCreationFailedFault", result.errorReason, 500);
    }

    const std::string host = m_ctx.resolveHost ? m_ctx.resolveHost(ctx) : "127.0.0.1";
    std::ostringstream ss;
    ss << "    <tev:CreatePullPointSubscriptionResponse>\r\n"
       << "      <tev:SubscriptionReference>\r\n"
       << "        <wsa:Address>http://" << Xml::escapeXmlText(host) << ":" << m_ctx.config.port
       << "/onvif/events/subscription/" << Xml::escapeXmlText(result.id) << "</wsa:Address>\r\n"
       << "      </tev:SubscriptionReference>\r\n"
       << "      <wsnt:CurrentTime>" << result.terminationTimeUtc << "</wsnt:CurrentTime>\r\n"
       << "      <wsnt:TerminationTime>" << result.terminationTimeUtc << "</wsnt:TerminationTime>\r\n"
       << "    </tev:CreatePullPointSubscriptionResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult EventService::handlePullMessages(const SoapContext& ctx)
{
    if (!m_ctx.subManager) {
        return SoapHandlerResult::fault("wsrf-rw:ResourceUnknownFault", "Event service unavailable", 500);
    }

    std::string subId;
    const std::string pfx = "/onvif/events/subscription/";
    if (ctx.requestUrl.rfind(pfx, 0) == 0 && ctx.requestUrl.length() > pfx.length()) {
        subId = ctx.requestUrl.substr(pfx.length());
    }
    if (subId.empty()) {
        const auto subIdNode = ctx.reqNode.select_node(".//*[local-name()='SubscriptionId']").node();
        if (subIdNode) {
            subId = subIdNode.text().as_string();
        }
    }

    const auto toNode = ctx.reqNode.select_node(".//*[local-name()='Timeout']").node();
    const std::string toStr = toNode ? toNode.text().as_string() : "PT5S";
    int timeoutSec = 5;
    if (!toStr.empty()) {
        const auto ms = Xml::parseIsoDurationMs(toStr);
        if (ms.has_value()) {
            timeoutSec = static_cast<int>(std::clamp(ms->count() / 1000LL, 1LL, 60LL));
        }
    }
    const auto limNode = ctx.reqNode.select_node(".//*[local-name()='MessageLimit']").node();
    const int limit = limNode ? std::clamp(limNode.text().as_int(10), 1, 100) : 10;

    std::vector<OnvifEvent> pulledEvents;
    std::string curTime;
    std::string termTime;
    static std::atomic<bool> defaultRunningFlag { true };
    const std::atomic<bool>& runningFlag = m_ctx.runningFlag ? *m_ctx.runningFlag : defaultRunningFlag;
    const bool ok
        = m_ctx.subManager->pullMessages(subId, limit, timeoutSec, pulledEvents, curTime, termTime, runningFlag);
    if (!ok) {
        return SoapHandlerResult::fault("wsrf-rw:ResourceUnknownFault", "Subscription not found or expired", 400);
    }

    std::ostringstream ss;
    ss << "    <tev:PullMessagesResponse>\r\n"
       << "      <wsnt:CurrentTime>" << curTime << "</wsnt:CurrentTime>\r\n"
       << "      <wsnt:TerminationTime>" << termTime << "</wsnt:TerminationTime>\r\n";
    for (const auto& ev : pulledEvents) {
        ss << buildNotificationMessage(ev);
    }
    ss << "    </tev:PullMessagesResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult EventService::handleRenew(const SoapContext& ctx)
{
    if (!m_ctx.subManager) {
        return SoapHandlerResult::fault("wsrf-rw:ResourceUnknownFault", "Event service unavailable", 500);
    }
    std::string subId;
    const std::string pfx = "/onvif/events/subscription/";
    if (ctx.requestUrl.rfind(pfx, 0) == 0 && ctx.requestUrl.length() > pfx.length()) {
        subId = ctx.requestUrl.substr(pfx.length());
    }
    if (subId.empty()) {
        const auto subIdNode = ctx.reqNode.select_node(".//*[local-name()='SubscriptionId']").node();
        if (subIdNode) {
            subId = subIdNode.text().as_string();
        }
    }

    const auto termNode = ctx.reqNode.select_node(".//*[local-name()='TerminationTime']").node();
    const std::string termStr = termNode ? termNode.text().as_string() : "PT300S";

    std::string newTerm;
    const bool renewed = m_ctx.subManager->renewSub(subId, termStr, newTerm);
    if (!renewed) {
        return SoapHandlerResult::fault("wsrf-rw:ResourceUnknownFault", "Subscription not found or expired", 400);
    }
    std::ostringstream ss;
    ss << "    <wsnt:RenewResponse>\r\n"
       << "      <wsnt:TerminationTime>" << newTerm << "</wsnt:TerminationTime>\r\n"
       << "      <wsnt:CurrentTime>" << newTerm << "</wsnt:CurrentTime>\r\n"
       << "    </wsnt:RenewResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult EventService::handleUnsubscribe(const SoapContext& ctx)
{
    if (!m_ctx.subManager) {
        return SoapHandlerResult::fault("wsrf-rw:ResourceUnknownFault", "Event service unavailable", 500);
    }
    std::string subId;
    const std::string pfx = "/onvif/events/subscription/";
    if (ctx.requestUrl.rfind(pfx, 0) == 0 && ctx.requestUrl.length() > pfx.length()) {
        subId = ctx.requestUrl.substr(pfx.length());
    }
    if (subId.empty()) {
        const auto subIdNode = ctx.reqNode.select_node(".//*[local-name()='SubscriptionId']").node();
        if (subIdNode) {
            subId = subIdNode.text().as_string();
        }
    }
    if (subId.empty() || !m_ctx.subManager->unsubscribe(subId)) {
        return SoapHandlerResult::fault("wsrf-rw:ResourceUnknownFault", "Subscription not found or expired", 400);
    }
    return SoapHandlerResult::ok("    <wsnt:UnsubscribeResponse/>\r\n");
}

SoapHandlerResult EventService::handleSubscribe(const SoapContext& ctx)
{
    const auto consumerNode
        = ctx.reqNode.select_node(".//*[local-name()='ConsumerReference']/*[local-name()='Address']").node();
    const std::string consumerUrl = consumerNode ? consumerNode.text().as_string() : "";

    const bool loopback = (ctx.host == "127.0.0.1" || ctx.host == "localhost" || ctx.host == "::1");
    const auto check = NotificationUrlValidator::validateUrl(consumerUrl, m_ctx.config.notification, loopback);
    if (!check.isValid()) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", check.reason, 400, "wsnt:InvalidConsumerReferenceFault");
    }

    const auto initTermNode = ctx.reqNode.select_node(".//*[local-name()='InitialTerminationTime']").node();
    const std::string initTermStr = initTermNode ? initTermNode.text().as_string() : "";
    const auto topicNode = ctx.reqNode.select_node(".//*[local-name()='TopicExpression']").node();
    const std::string topicFilter = topicNode ? topicNode.text().as_string() : "";

    if (!m_ctx.subManager) {
        return SoapHandlerResult::fault("wsnt:SubscribeCreationFailedFault", "Event service unavailable", 500);
    }
    const auto result = m_ctx.subManager->createPushSub(consumerUrl, initTermStr, topicFilter);
    if (!result.success) {
        return SoapHandlerResult::fault("wsnt:SubscribeCreationFailedFault", result.errorReason, 500);
    }

    const std::string host = m_ctx.resolveHost ? m_ctx.resolveHost(ctx) : "127.0.0.1";
    std::ostringstream ss;
    ss << "    <wsnt:SubscribeResponse>\r\n"
       << "      <wsnt:SubscriptionReference>\r\n"
       << "        <wsa:Address>http://" << Xml::escapeXmlText(host) << ":" << m_ctx.config.port
       << "/onvif/events/subscription/" << Xml::escapeXmlText(result.id) << "</wsa:Address>\r\n"
       << "      </wsnt:SubscriptionReference>\r\n"
       << "      <wsnt:CurrentTime>" << result.terminationTimeUtc << "</wsnt:CurrentTime>\r\n"
       << "      <wsnt:TerminationTime>" << result.terminationTimeUtc << "</wsnt:TerminationTime>\r\n"
       << "    </wsnt:SubscribeResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult EventService::handleSeek(const SoapContext&)
{
    return SoapHandlerResult::ok("    <tev:SeekResponse/>\r\n");
}

} // namespace Onvif
