/// @file TestOnvifAuthServer.cpp
/// @brief End-to-end authentication/authorization tests against a live OnvifServer (review finding C1).
/// @details Each test starts a server on a unique loopback port, seeds three users with distinct
///          privilege levels and drives it over HTTP. Uses only the public server API so the
///          behavioural red/green state is observable before and after the C1 fix.

#include "OnvifTestClient.h"

#include <Onvif/HttplibInclude.h>
#include <Onvif/OnvifClient.h>
#include <Onvif/OnvifSecurity.h>
#include <Onvif/OnvifServer.h>

#include <gtest/gtest.h>
#include <openssl/evp.h>
#include <pugixml.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace Onvif;
using OnvifTest::makeCreds;
using OnvifTest::OnvifTestClient;

namespace {

constexpr const char* kSoapCt { "application/soap+xml; charset=utf-8" };

/// @brief Next free test port (unique per test to avoid TIME_WAIT collisions).
std::atomic<int> g_nextPort { 18720 };

/// @brief Wraps an operation body in a SOAP 1.2 envelope declaring common ONVIF prefixes.
std::string envelope(const std::string& bodyXml)
{
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
           "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
           "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
           "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
           "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" "
           "xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
           "<s:Body>"
        + bodyXml + "</s:Body></s:Envelope>";
}

/// @brief CreateUsers request body for a single user.
std::string createUserBody(const std::string& name, const std::string& pass, const std::string& level)
{
    return "<tds:CreateUsers><tds:User><tt:Username>" + name + "</tt:Username><tt:Password>" + pass
        + "</tt:Password><tt:UserLevel>" + level + "</tt:UserLevel></tds:User></tds:CreateUsers>";
}

/// @brief Returns true if the response body is a SOAP fault with subcode ter:NotAuthorized.
bool isNotAuthorizedFault(const std::string& body)
{
    pugi::xml_document doc {};
    if (!doc.load_string(body.c_str())) {
        return false;
    }
    const auto sub { doc.select_node("//*[local-name()='Fault']//*[local-name()='Subcode']/*[local-name()='Value']") };
    return sub && std::string(sub.node().text().as_string()).find("NotAuthorized") != std::string::npos;
}

/// @brief Lower-case hex digest (test-side, independent of production code).
std::string hashHex(const EVP_MD* md, const std::string& input)
{
    std::array<unsigned char, EVP_MAX_MD_SIZE> out {};
    unsigned int outLen { 0U };
    EVP_MD_CTX* ctx { EVP_MD_CTX_new() };
    EXPECT_NE(ctx, nullptr);
    EXPECT_EQ(EVP_DigestInit_ex(ctx, md, nullptr), 1);
    EXPECT_EQ(EVP_DigestUpdate(ctx, input.data(), input.size()), 1);
    EXPECT_EQ(EVP_DigestFinal_ex(ctx, out.data(), &outLen), 1);
    EVP_MD_CTX_free(ctx);
    std::ostringstream ss {};
    for (unsigned int i { 0U }; i < outLen; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(out.at(i));
    }
    return ss.str();
}

/// @brief Extracts a parameter from a Digest challenge.
std::string digestParam(const std::string& header, const std::string& name)
{
    const std::string key { name + "=" };
    std::size_t pos { header.find(key) };
    while (pos != std::string::npos && pos > 0U && header.at(pos - 1U) != ' ' && header.at(pos - 1U) != ',') {
        pos = header.find(key, pos + 1U);
    }
    if (pos == std::string::npos) {
        return {};
    }
    pos += key.size();
    if (pos < header.size() && header.at(pos) == '"') {
        const std::size_t end { header.find('"', pos + 1U) };
        return header.substr(pos + 1U, end - pos - 1U);
    }
    const std::size_t end { header.find(',', pos) };
    return header.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
}

/// @brief PTZ handler counting actuation calls (atomic: written by server worker threads).
class CountingPtzHandler : public IPtzHandler {
public:
    void handleContinuousMove(float /*pan*/, float /*tilt*/, float /*zoom*/) override
    {
        moves.fetch_add(1);
    }
    void handleAbsoluteMove(float /*pan*/, float /*tilt*/, float /*zoom*/) override
    {
        moves.fetch_add(1);
    }
    void handleStop(bool /*pt*/, bool /*z*/) override
    {
        moves.fetch_add(1);
    }
    std::string handleSetPreset(const std::string& /*name*/, const std::string& token) override
    {
        return token.empty() ? std::string { "1" } : token;
    }
    bool handleGotoPreset(const std::string& /*token*/) override
    {
        moves.fetch_add(1);
        return true;
    }
    bool handleRemovePreset(const std::string& /*token*/) override
    {
        return true;
    }
    std::vector<PtzPreset> handleGetPresets() override
    {
        return {};
    }
    PtzStatus handleGetStatus() override
    {
        return PtzStatus {};
    }

    std::atomic<int> moves { 0 };
};

/// @brief Fixture: live server with admin / operator / user accounts on a fresh loopback port.
class OnvifAuthServerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        port = g_nextPort.fetch_add(1);
        OnvifServerConfig config {};
        config.bindAddress = "127.0.0.1";
        config.port = port;
        config.defaultUsers = { { "admin", "Adm1n-Pass", OnvifUserLevel::Administrator },
            { "op", "Op-Pass", OnvifUserLevel::Operator }, { "viewer", "View-Pass", OnvifUserLevel::User } };
        server = std::make_unique<OnvifServer>(config, ptz);
        ASSERT_TRUE(server->start());
        std::this_thread::sleep_for(std::chrono::milliseconds { 50 });
    }

    void TearDown() override
    {
        server->stop();
    }

    OnvifTestClient client(const std::string& user, const std::string& pass) const
    {
        OnvifTestClient c { "127.0.0.1", port, makeCreds(user, pass) };
        c.set_connection_timeout(std::chrono::seconds { 2 });
        c.set_read_timeout(std::chrono::seconds { 5 });
        return c;
    }

    OnvifTestClient anonymous() const
    {
        return client("", "");
    }

    /// @brief Lists usernames via an authenticated admin GetUsers call.
    std::vector<std::string> usernames() const
    {
        auto admin { client("admin", "Adm1n-Pass") };
        const auto res { admin.Post("/onvif/device_service", envelope("<tds:GetUsers/>"), kSoapCt) };
        std::vector<std::string> names {};
        if (!res || res->status != 200) {
            return names;
        }
        pugi::xml_document doc {};
        if (doc.load_string(res->body.c_str())) {
            for (const auto& n : doc.select_nodes("//*[local-name()='User']/*[local-name()='Username']")) {
                names.emplace_back(n.node().text().as_string());
            }
        }
        return names;
    }

    bool hasUser(const std::string& name) const
    {
        for (const auto& n : usernames()) {
            if (n == name) {
                return true;
            }
        }
        return false;
    }

    int port { 0 };
    std::shared_ptr<CountingPtzHandler> ptz { std::make_shared<CountingPtzHandler>() };
    std::unique_ptr<OnvifServer> server {};
};

} // namespace

// I1
TEST_F(OnvifAuthServerTest, NoCredsPreAuthOpSucceeds)
{
    auto c { anonymous() };
    const auto res { c.Post("/onvif/device_service", envelope("<tds:GetSystemDateAndTime/>"), kSoapCt) };
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    EXPECT_NE(res->body.find("GetSystemDateAndTimeResponse"), std::string::npos);
}

// I2 + I14
TEST_F(OnvifAuthServerTest, NoCredsProtectedOpChallenged)
{
    auto c { anonymous() };
    const auto res { c.Post(
        "/onvif/device_service", envelope(createUserBody("mallory", "Evil-Pass", "Administrator")), kSoapCt) };
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 401);
    EXPECT_EQ(res->get_header_value_count("WWW-Authenticate"), 2U);
    EXPECT_EQ(res->get_header_value("WWW-Authenticate", 0U).rfind("Digest ", 0U), 0U);
    EXPECT_TRUE(isNotAuthorizedFault(res->body)) << res->body;
    EXPECT_FALSE(hasUser("mallory"));
}

// I3
TEST_F(OnvifAuthServerTest, AdminCreateUsersSucceeds)
{
    auto c { client("admin", "Adm1n-Pass") };
    const auto res { c.Post("/onvif/device_service", envelope(createUserBody("newbie", "New-Pass1", "User")), kSoapCt) };
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    EXPECT_TRUE(hasUser("newbie"));
}

TEST_F(OnvifAuthServerTest, WrongPasswordRejected)
{
    auto c { client("admin", "not-the-password") };
    const auto res { c.Post("/onvif/device_service", envelope(createUserBody("eve", "Eve-Pass1", "User")), kSoapCt) };
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
    EXPECT_TRUE(isNotAuthorizedFault(res->body)) << res->body;
    EXPECT_FALSE(hasUser("eve"));
}

// I4
TEST_F(OnvifAuthServerTest, OperatorCannotCreateUsers)
{
    auto c { client("op", "Op-Pass") };
    const auto res { c.Post(
        "/onvif/device_service", envelope(createUserBody("escalate", "Esc-Pass1", "Administrator")), kSoapCt) };
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
    EXPECT_TRUE(isNotAuthorizedFault(res->body)) << res->body;
    EXPECT_FALSE(hasUser("escalate"));
}

// I5
TEST_F(OnvifAuthServerTest, OperatorMayActuateUserMayNot)
{
    const std::string move { envelope("<tptz:ContinuousMove><tptz:ProfileToken>ProfileToken_1</tptz:ProfileToken>"
                                      "<tptz:Velocity><tt:PanTilt x=\"0.5\" y=\"0\"/></tptz:Velocity>"
                                      "</tptz:ContinuousMove>") };
    auto viewer { client("viewer", "View-Pass") };
    const auto denied { viewer.Post("/onvif/ptz_service", move, kSoapCt) };
    ASSERT_TRUE(denied);
    EXPECT_EQ(denied->status, 400);
    EXPECT_EQ(ptz->moves.load(), 0);

    auto op { client("op", "Op-Pass") };
    const auto allowed { op.Post("/onvif/ptz_service", move, kSoapCt) };
    ASSERT_TRUE(allowed);
    EXPECT_EQ(allowed->status, 200);
    EXPECT_EQ(ptz->moves.load(), 1);
}

// I6
TEST_F(OnvifAuthServerTest, UserMayReadMedia)
{
    auto viewer { client("viewer", "View-Pass") };
    const auto res { viewer.Post("/onvif/media_service", envelope("<trt:GetProfiles/>"), kSoapCt) };
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    EXPECT_NE(res->body.find("GetProfilesResponse"), std::string::npos);
}

// I7
TEST_F(OnvifAuthServerTest, HttpDigestEndToEnd)
{
    httplib::Client raw { "127.0.0.1", port };
    raw.set_read_timeout(std::chrono::seconds { 5 });
    const std::string body { envelope("<tds:GetDeviceInformation/>") };

    const auto first { raw.Post("/onvif/device_service", body, kSoapCt) };
    ASSERT_TRUE(first);
    ASSERT_EQ(first->status, 401);
    const std::string challenge { first->get_header_value("WWW-Authenticate", 0U) };
    ASSERT_EQ(digestParam(challenge, "algorithm"), "SHA-256");

    const std::string realm { digestParam(challenge, "realm") };
    const std::string nonce { digestParam(challenge, "nonce") };
    const std::string uri { "/onvif/device_service" };
    const std::string ha1 { hashHex(EVP_sha256(), "admin:" + realm + ":Adm1n-Pass") };
    const std::string ha2 { hashHex(EVP_sha256(), "POST:" + uri) };
    const std::string response { hashHex(EVP_sha256(), ha1 + ":" + nonce + ":00000001:abc123:auth:" + ha2) };
    const std::string auth { "Digest username=\"admin\", realm=\"" + realm + "\", nonce=\"" + nonce + "\", uri=\""
        + uri + "\", algorithm=SHA-256, qop=auth, nc=00000001, cnonce=\"abc123\", response=\"" + response + "\"" };

    const httplib::Headers headers { { "Authorization", auth } };
    const auto second { raw.Post("/onvif/device_service", headers, body, kSoapCt) };
    ASSERT_TRUE(second);
    EXPECT_EQ(second->status, 200);
    EXPECT_NE(second->body.find("GetDeviceInformationResponse"), std::string::npos);
}

// I8
TEST_F(OnvifAuthServerTest, ReplayedRequestRejected)
{
    const std::string secured { OnvifTest::injectSecurity(envelope("<tds:GetDeviceInformation/>"),
        OnvifSecurity::buildSoapSecurityHeader(makeCreds("admin", "Adm1n-Pass"))) };
    httplib::Client raw { "127.0.0.1", port };
    raw.set_read_timeout(std::chrono::seconds { 5 });
    const auto first { raw.Post("/onvif/device_service", secured, kSoapCt) };
    ASSERT_TRUE(first);
    EXPECT_EQ(first->status, 200);
    const auto replay { raw.Post("/onvif/device_service", secured, kSoapCt) };
    ASSERT_TRUE(replay);
    EXPECT_EQ(replay->status, 400);
    EXPECT_TRUE(isNotAuthorizedFault(replay->body)) << replay->body;
}

// I9
TEST_F(OnvifAuthServerTest, MetadataStreamRequiresAuth)
{
    auto c { anonymous() };
    const auto res { c.Get("/onvif/metadata_stream") };
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 401);
    EXPECT_GE(res->get_header_value_count("WWW-Authenticate"), 1U);
}

// I10
TEST_F(OnvifAuthServerTest, SetUserPasswordTakesEffect)
{
    auto admin { client("admin", "Adm1n-Pass") };
    const auto set { admin.Post("/onvif/device_service",
        envelope("<tds:SetUser><tds:User><tt:Username>viewer</tt:Username><tt:Password>Rotated-Pass</tt:Password>"
                 "<tt:UserLevel>User</tt:UserLevel></tds:User></tds:SetUser>"),
        kSoapCt) };
    ASSERT_TRUE(set);
    ASSERT_EQ(set->status, 200);

    auto oldCreds { client("viewer", "View-Pass") };
    const auto oldRes { oldCreds.Post("/onvif/media_service", envelope("<trt:GetProfiles/>"), kSoapCt) };
    ASSERT_TRUE(oldRes);
    EXPECT_EQ(oldRes->status, 400);

    auto newCreds { client("viewer", "Rotated-Pass") };
    const auto newRes { newCreds.Post("/onvif/media_service", envelope("<trt:GetProfiles/>"), kSoapCt) };
    ASSERT_TRUE(newRes);
    EXPECT_EQ(newRes->status, 200);
}

// I11
TEST_F(OnvifAuthServerTest, DeleteUserRevokesAccess)
{
    auto admin { client("admin", "Adm1n-Pass") };
    const auto del { admin.Post("/onvif/device_service",
        envelope("<tds:DeleteUsers><tds:Username>viewer</tds:Username></tds:DeleteUsers>"), kSoapCt) };
    ASSERT_TRUE(del);
    ASSERT_EQ(del->status, 200);

    auto viewer { client("viewer", "View-Pass") };
    const auto res { viewer.Post("/onvif/media_service", envelope("<trt:GetProfiles/>"), kSoapCt) };
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

TEST_F(OnvifAuthServerTest, GetUsersNeverReturnsPasswords)
{
    auto admin { client("admin", "Adm1n-Pass") };
    const auto res { admin.Post("/onvif/device_service", envelope("<tds:GetUsers/>"), kSoapCt) };
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200);
    EXPECT_EQ(res->body.find("Adm1n-Pass"), std::string::npos);
    EXPECT_EQ(res->body.find("<tt:Password>"), std::string::npos);
}

TEST_F(OnvifAuthServerTest, LastAdminCannotBeDeleted)
{
    auto admin { client("admin", "Adm1n-Pass") };
    const auto del { admin.Post("/onvif/device_service",
        envelope("<tds:DeleteUsers><tds:Username>admin</tds:Username></tds:DeleteUsers>"), kSoapCt) };
    ASSERT_TRUE(del);
    EXPECT_TRUE(hasUser("admin"));
}

// I13
TEST_F(OnvifAuthServerTest, OnvifClientInterop)
{
    OnvifClient onvif { "http://127.0.0.1:" + std::to_string(port) + "/onvif/device_service",
        makeCreds("admin", "Adm1n-Pass") };
    onvif.setTimeout(std::chrono::milliseconds { 3000 });
    const auto info { onvif.getDeviceInformation() };
    ASSERT_TRUE(info.has_value());
    EXPECT_FALSE(info->manufacturer.empty());

    OnvifClient wrong { "http://127.0.0.1:" + std::to_string(port) + "/onvif/device_service",
        makeCreds("admin", "bad") };
    wrong.setTimeout(std::chrono::milliseconds { 3000 });
    EXPECT_FALSE(wrong.getDeviceInformation().has_value());
}
