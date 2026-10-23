/// @file TestOnvifSoapRouter.cpp
/// @brief Comprehensive unit and concurrency tests for Declarative SoapRouter engine.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include <atomic>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <pugixml.hpp>

#include "AccessPolicy.h"
#include "OnvifAuthTypes.h"
#include "OnvifTypes.h"
#include "SoapRouter.h"

using namespace Onvif;

namespace {

constexpr const char* kDeviceNs = "http://www.onvif.org/ver10/device/wsdl";
constexpr const char* kPtzNs = "http://www.onvif.org/ver20/ptz/wsdl";

SoapContext makeContext(OnvifUserLevel level = OnvifUserLevel::Anonymous)
{
    SoapContext ctx { };
    ctx.principal.username = (level == OnvifUserLevel::Anonymous) ? "" : "testuser";
    ctx.principal.level = level;
    ctx.peerAddress = "127.0.0.1";
    ctx.requestUrl = "/onvif/device_service";
    ctx.serviceName = "Device";
    return ctx;
}

} // namespace

TEST(SoapRouterTest, ExactOpKeyMatch)
{
    SoapRouter router { };
    router.registerRoute(kDeviceNs, "GetSystemDateAndTime", AccessClass::PreAuth,
        [](const SoapContext&) { return SoapHandlerResult::ok("<tds:GetSystemDateAndTimeResponse/>", 200); });

    EXPECT_TRUE(router.hasRoute(kDeviceNs, "GetSystemDateAndTime"));
    EXPECT_FALSE(router.hasRoute(kPtzNs, "GetSystemDateAndTime"));
    EXPECT_EQ(router.routeCount(), 1U);

    const auto ctx = makeContext();
    const auto result = router.dispatch(kDeviceNs, "GetSystemDateAndTime", ctx);

    EXPECT_EQ(result.status, DispatchStatus::Success);
    EXPECT_EQ(result.httpStatusCode, 200);
    EXPECT_NE(result.responseXml.find("<tds:GetSystemDateAndTimeResponse/>"), std::string::npos);
}

TEST(SoapRouterTest, NamespaceMismatchRejection)
{
    SoapRouter router { };
    router.registerRoute(kDeviceNs, "GetSystemDateAndTime", AccessClass::PreAuth,
        [](const SoapContext&) { return SoapHandlerResult::ok("<tds:GetSystemDateAndTimeResponse/>"); });

    const auto ctx = makeContext();
    // Dispatch identical local name with different namespace
    const auto result = router.dispatch(kPtzNs, "GetSystemDateAndTime", ctx);

    EXPECT_EQ(result.status, DispatchStatus::ActionNotSupported);
    EXPECT_EQ(result.httpStatusCode, 500);
    EXPECT_NE(result.responseXml.find("ter:ActionNotSupported"), std::string::npos);
}

TEST(SoapRouterTest, XmlPrefixResolution)
{
    SoapRouter router { };
    router.registerRoute(kDeviceNs, "GetDeviceInformation", AccessClass::ReadSystem,
        [](const SoapContext&) { return SoapHandlerResult::ok("<tds:GetDeviceInformationResponse/>"); });

    const std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                            "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\n"
                            "  <s:Body>\n"
                            "    <tds:GetDeviceInformation/>\n"
                            "  </s:Body>\n"
                            "</s:Envelope>\n";

    pugi::xml_document doc;
    ASSERT_TRUE(doc.load_string(xml.c_str()));
    const pugi::xml_node reqNode = doc.select_node("//*[local-name()='GetDeviceInformation']").node();
    ASSERT_TRUE(reqNode);

    const auto ctx = makeContext(OnvifUserLevel::User);
    const auto result = router.dispatch(reqNode, ctx);

    EXPECT_EQ(result.status, DispatchStatus::Success);
    EXPECT_EQ(result.httpStatusCode, 200);
    EXPECT_NE(result.responseXml.find("<tds:GetDeviceInformationResponse/>"), std::string::npos);
}

TEST(SoapRouterTest, XmlDefaultNamespaceResolution)
{
    SoapRouter router { };
    router.registerRoute(kDeviceNs, "GetScopes", AccessClass::ReadSystem,
        [](const SoapContext&) { return SoapHandlerResult::ok("<tds:GetScopesResponse/>"); });

    const std::string xml = "<GetScopes xmlns=\"http://www.onvif.org/ver10/device/wsdl\"/>";

    pugi::xml_document doc;
    ASSERT_TRUE(doc.load_string(xml.c_str()));
    const pugi::xml_node reqNode = doc.first_child();
    ASSERT_TRUE(reqNode);

    const auto ctx = makeContext(OnvifUserLevel::User);
    const auto result = router.dispatch(reqNode, ctx);

    EXPECT_EQ(result.status, DispatchStatus::Success);
    EXPECT_EQ(result.httpStatusCode, 200);
}

TEST(SoapRouterTest, AncestorNamespaceResolution)
{
    SoapRouter router { };
    router.registerRoute(kDeviceNs, "GetSystemDateAndTime", AccessClass::PreAuth,
        [](const SoapContext&) { return SoapHandlerResult::ok("<tds:GetSystemDateAndTimeResponse/>"); });

    const std::string xml = "<root xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\n"
                            "  <parent>\n"
                            "    <tds:GetSystemDateAndTime/>\n"
                            "  </parent>\n"
                            "</root>\n";

    pugi::xml_document doc;
    ASSERT_TRUE(doc.load_string(xml.c_str()));
    const pugi::xml_node reqNode = doc.select_node("//*[local-name()='GetSystemDateAndTime']").node();
    ASSERT_TRUE(reqNode);

    const auto ctx = makeContext(OnvifUserLevel::Anonymous);
    const auto result = router.dispatch(reqNode, ctx);

    EXPECT_EQ(result.status, DispatchStatus::Success);
}

TEST(SoapRouterTest, FallbackNamespaceResolution)
{
    SoapRouter router { };
    router.registerRoute(kDeviceNs, "GetHostname", AccessClass::PreAuth,
        [](const SoapContext&) { return SoapHandlerResult::ok("<tds:GetHostnameResponse/>"); });

    // Node without any explicit xmlns
    const std::string xml = "<GetHostname/>";
    pugi::xml_document doc;
    ASSERT_TRUE(doc.load_string(xml.c_str()));
    const pugi::xml_node reqNode = doc.first_child();
    ASSERT_TRUE(reqNode);

    const auto ctx = makeContext(OnvifUserLevel::Anonymous);
    // Dispatch using fallback namespace for /onvif/device_service
    const auto result = router.dispatch(reqNode, ctx, kDeviceNs);

    EXPECT_EQ(result.status, DispatchStatus::Success);
    EXPECT_EQ(result.httpStatusCode, 200);
}

TEST(SoapRouterTest, AccessPolicyEnforcement)
{
    SoapRouter router { };
    // SetSystemDateAndTime requires WriteSystem access class
    router.registerRoute(kDeviceNs, "SetSystemDateAndTime", AccessClass::WriteSystem,
        [](const SoapContext&) { return SoapHandlerResult::ok("<tds:SetSystemDateAndTimeResponse/>"); });

    // 1. Anonymous caller -> 401 NotAuthorized
    {
        const auto ctx = makeContext(OnvifUserLevel::Anonymous);
        const auto res = router.dispatch(kDeviceNs, "SetSystemDateAndTime", ctx);
        EXPECT_EQ(res.status, DispatchStatus::NotAuthorized);
        EXPECT_EQ(res.httpStatusCode, 401);
        EXPECT_NE(res.responseXml.find("ter:NotAuthorized"), std::string::npos);
    }

    // 2. User level caller -> 400 NotAuthorized (User level cannot WriteSystem)
    {
        const auto ctx = makeContext(OnvifUserLevel::User);
        const auto res = router.dispatch(kDeviceNs, "SetSystemDateAndTime", ctx);
        EXPECT_EQ(res.status, DispatchStatus::NotAuthorized);
        EXPECT_EQ(res.httpStatusCode, 400);
    }

    // 3. Operator level caller -> 400 NotAuthorized (Operator level cannot WriteSystem)
    {
        const auto ctx = makeContext(OnvifUserLevel::Operator);
        const auto res = router.dispatch(kDeviceNs, "SetSystemDateAndTime", ctx);
        EXPECT_EQ(res.status, DispatchStatus::NotAuthorized);
        EXPECT_EQ(res.httpStatusCode, 400);
    }

    // 4. Administrator caller -> 200 OK
    {
        const auto ctx = makeContext(OnvifUserLevel::Administrator);
        const auto res = router.dispatch(kDeviceNs, "SetSystemDateAndTime", ctx);
        EXPECT_EQ(res.status, DispatchStatus::Success);
        EXPECT_EQ(res.httpStatusCode, 200);
    }
}

TEST(SoapRouterTest, AnonymousPreAuthAllowed)
{
    SoapRouter router { };
    router.registerRoute(kDeviceNs, "GetWsdlUrl", AccessClass::PreAuth,
        [](const SoapContext&) { return SoapHandlerResult::ok("<tds:GetWsdlUrlResponse/>"); });

    const auto ctx = makeContext(OnvifUserLevel::Anonymous);
    const auto res = router.dispatch(kDeviceNs, "GetWsdlUrl", ctx);
    EXPECT_EQ(res.status, DispatchStatus::Success);
    EXPECT_EQ(res.httpStatusCode, 200);
}

TEST(SoapRouterTest, ActionNotSupportedFault)
{
    SoapRouter router { };
    const auto ctx = makeContext();
    const auto res = router.dispatch(kDeviceNs, "NonExistentOperation", ctx);

    EXPECT_EQ(res.status, DispatchStatus::ActionNotSupported);
    EXPECT_EQ(res.httpStatusCode, 500);
    EXPECT_NE(res.responseXml.find("SOAP-ENV:Receiver"), std::string::npos);
    EXPECT_NE(res.responseXml.find("ter:ActionNotSupported"), std::string::npos);
}

TEST(SoapRouterTest, HandlerFaultResponse)
{
    SoapRouter router { };
    router.registerRoute(kDeviceNs, "SetScopes", AccessClass::WriteSystem, [](const SoapContext&) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "Invalid scope URI syntax", 400);
    });

    const auto ctx = makeContext(OnvifUserLevel::Administrator);
    const auto res = router.dispatch(kDeviceNs, "SetScopes", ctx);

    EXPECT_EQ(res.status, DispatchStatus::BadRequest);
    EXPECT_EQ(res.httpStatusCode, 400);
    EXPECT_NE(res.responseXml.find("ter:InvalidArgVal"), std::string::npos);
    EXPECT_NE(res.responseXml.find("Invalid scope URI syntax"), std::string::npos);
}

TEST(SoapRouterTest, HandlerExceptionContainment)
{
    SoapRouter router { };
    router.registerRoute(kDeviceNs, "ExplodingOp", AccessClass::PreAuth,
        [](const SoapContext&) -> SoapHandlerResult { throw std::runtime_error("Simulated internal exception"); });

    const auto ctx = makeContext();
    const auto res = router.dispatch(kDeviceNs, "ExplodingOp", ctx);

    EXPECT_EQ(res.status, DispatchStatus::InternalFault);
    EXPECT_EQ(res.httpStatusCode, 500);
    EXPECT_NE(res.responseXml.find("Simulated internal exception"), std::string::npos);
}

TEST(SoapRouterTest, RouteCountAndClear)
{
    SoapRouter router { };
    EXPECT_EQ(router.routeCount(), 0U);

    router.registerRoute(
        kDeviceNs, "Op1", AccessClass::PreAuth, [](const SoapContext&) { return SoapHandlerResult::ok(""); });
    router.registerRoute(
        kDeviceNs, "Op2", AccessClass::PreAuth, [](const SoapContext&) { return SoapHandlerResult::ok(""); });
    router.registerRoute(
        kPtzNs, "Op1", AccessClass::Actuate, [](const SoapContext&) { return SoapHandlerResult::ok(""); });

    EXPECT_EQ(router.routeCount(), 3U);
    EXPECT_TRUE(router.hasRoute(kDeviceNs, "Op1"));
    EXPECT_TRUE(router.hasRoute(kPtzNs, "Op1"));

    router.clearRoutes();
    EXPECT_EQ(router.routeCount(), 0U);
    EXPECT_FALSE(router.hasRoute(kDeviceNs, "Op1"));
}

TEST(SoapRouterTest, ConcurrentDispatchStress)
{
    SoapRouter router { };
    router.registerRoute(kDeviceNs, "GetSystemDateAndTime", AccessClass::PreAuth,
        [](const SoapContext&) { return SoapHandlerResult::ok("<tds:GetSystemDateAndTimeResponse/>"); });
    router.registerRoute(kDeviceNs, "GetDeviceInformation", AccessClass::ReadSystem,
        [](const SoapContext&) { return SoapHandlerResult::ok("<tds:GetDeviceInformationResponse/>"); });
    router.registerRoute(kPtzNs, "ContinuousMove", AccessClass::Actuate,
        [](const SoapContext&) { return SoapHandlerResult::ok("<tptz:ContinuousMoveResponse/>"); });

    constexpr std::size_t kThreads = 8U;
    constexpr std::size_t kIterations = 500U;
    std::vector<std::thread> workers;
    workers.reserve(kThreads);

    std::atomic<std::size_t> successCount { 0U };

    for (std::size_t i = 0U; i < kThreads; ++i) {
        workers.emplace_back([&router, &successCount]() {
            const auto ctxAdmin = makeContext(OnvifUserLevel::Administrator);
            for (std::size_t j = 0U; j < kIterations; ++j) {
                const auto res1 = router.dispatch(kDeviceNs, "GetSystemDateAndTime", ctxAdmin);
                if (res1.status == DispatchStatus::Success) {
                    successCount.fetch_add(1U, std::memory_order_relaxed);
                }
                const auto res2 = router.dispatch(kDeviceNs, "GetDeviceInformation", ctxAdmin);
                if (res2.status == DispatchStatus::Success) {
                    successCount.fetch_add(1U, std::memory_order_relaxed);
                }
                const auto res3 = router.dispatch(kPtzNs, "ContinuousMove", ctxAdmin);
                if (res3.status == DispatchStatus::Success) {
                    successCount.fetch_add(1U, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    EXPECT_EQ(successCount.load(), kThreads * kIterations * 3U);
}
