/// @file TestOnvifAuth.cpp
/// @brief Unit tests for ONVIF server authentication and authorization (review finding C1).
/// @details Covers NonceCache, CredentialStore, UsernameTokenValidator, DigestValidator,
///          AccessPolicy, Authenticator back-off, SoapFault and the OnvifServer start-up guards.
///          No sockets are used except by the start-up guard tests.

#include <Onvif/AccessPolicy.h>
#include <Onvif/Authenticator.h>
#include <Onvif/CredentialStore.h>
#include <Onvif/HttpDigest.h>
#include <Onvif/NonceCache.h>
#include <Onvif/OnvifSecurity.h>
#include <Onvif/OnvifServer.h>
#include <Onvif/SoapFault.h>
#include <Onvif/UsernameToken.h>

#include <gtest/gtest.h>
#include <openssl/evp.h>
#include <pugixml.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace Onvif;

namespace {

constexpr const char* kDigestType
    = "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest";
constexpr const char* kTextType
    = "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordText";

/// @brief Formats a system time point as an ISO-8601 UTC string.
std::string isoUtc(std::chrono::system_clock::time_point tp)
{
    const std::time_t t { std::chrono::system_clock::to_time_t(tp) };
    std::tm tmUtc {};
#if defined(_WIN32)
    static_cast<void>(gmtime_s(&tmUtc, &t));
#else
    static_cast<void>(gmtime_r(&t, &tmUtc));
#endif
    std::ostringstream ss {};
    ss << std::put_time(&tmUtc, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

/// @brief Standard three-user seed used by most tests.
std::vector<OnvifUser> seedUsers()
{
    return { { "admin", "Adm1n-Pass", OnvifUserLevel::Administrator },
        { "op", "Op-Pass", OnvifUserLevel::Operator }, { "viewer", "View-Pass", OnvifUserLevel::User } };
}

/// @brief Builds a serialized wsse:Security element.
std::string makeSecurityXml(const std::string& user, const std::string& passwordValue, const std::string& created,
    const std::string& nonceB64, const std::string& passwordType)
{
    std::ostringstream ss {};
    ss << "<wsse:Security "
          "xmlns:wsse=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\" "
          "xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">"
       << "<wsse:UsernameToken><wsse:Username>" << user << "</wsse:Username>";
    if (passwordType.empty()) {
        ss << "<wsse:Password>" << passwordValue << "</wsse:Password>";
    } else {
        ss << "<wsse:Password Type=\"" << passwordType << "\">" << passwordValue << "</wsse:Password>";
    }
    ss << "<wsse:Nonce>" << nonceB64 << "</wsse:Nonce>"
       << "<wsu:Created>" << created << "</wsu:Created>"
       << "</wsse:UsernameToken></wsse:Security>";
    return ss.str();
}

/// @brief Builds a valid PasswordDigest Security element for the given credentials and time.
std::string makeDigestSecurity(const std::string& user, const std::string& password,
    std::chrono::system_clock::time_point created, const std::vector<std::uint8_t>& nonce)
{
    const std::string createdStr { isoUtc(created) };
    const std::string digest { OnvifSecurity::computePasswordDigest(nonce, createdStr, password) };
    return makeSecurityXml(user, digest, createdStr, OnvifSecurity::base64Encode(nonce), kDigestType);
}

/// @brief Fixture wiring a CredentialStore, NonceCache and UsernameTokenValidator together.
class UsernameTokenTest : public ::testing::Test {
protected:
    std::shared_ptr<CredentialStore> store { std::make_shared<CredentialStore>(seedUsers()) };
    std::shared_ptr<NonceCache> cache { std::make_shared<NonceCache>(64U, std::chrono::seconds { 600 }) };
    OnvifAuthConfig cfg {};
    UsernameTokenValidator validator { store, cache, cfg };
    std::chrono::system_clock::time_point now { std::chrono::system_clock::now() };
    std::vector<std::uint8_t> nonce { OnvifSecurity::generateNonce() };

    AuthResult run(const std::string& securityXml)
    {
        pugi::xml_document doc {};
        EXPECT_TRUE(doc.load_string(securityXml.c_str()));
        return validator.validate(doc.document_element(), now);
    }
};

/// @brief Lower-case hex digest helper for Digest tests (independent of production code).
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

/// @brief Extracts a quoted or token parameter value from a Digest challenge/credential string.
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

/// @brief Builds a client Authorization header for RFC 7616 Digest.
std::string makeDigestAuth(const std::string& challenge, const EVP_MD* md, const std::string& algName,
    const std::string& user, const std::string& password, const std::string& method, const std::string& uri,
    const std::string& nc, const std::string& cnonce)
{
    const std::string realm { digestParam(challenge, "realm") };
    const std::string nonce { digestParam(challenge, "nonce") };
    const std::string ha1 { hashHex(md, user + ":" + realm + ":" + password) };
    const std::string ha2 { hashHex(md, method + ":" + uri) };
    const std::string resp { hashHex(md, ha1 + ":" + nonce + ":" + nc + ":" + cnonce + ":auth:" + ha2) };
    std::ostringstream ss {};
    ss << "Digest username=\"" << user << "\", realm=\"" << realm << "\", nonce=\"" << nonce << "\", uri=\"" << uri
       << "\", algorithm=" << algName << ", qop=auth, nc=" << nc << ", cnonce=\"" << cnonce << "\", response=\""
       << resp << "\"";
    return ss.str();
}

/// @brief Fixture for Digest validator tests.
class DigestTest : public ::testing::Test {
protected:
    std::shared_ptr<CredentialStore> store { std::make_shared<CredentialStore>(seedUsers()) };
    std::shared_ptr<NonceCache> cache { std::make_shared<NonceCache>(64U, std::chrono::seconds { 600 }) };
    OnvifAuthConfig cfg {};
    DigestValidator validator { store, cache, cfg };
    std::chrono::system_clock::time_point now { std::chrono::system_clock::now() };

    std::string challengeFor(const std::string& algorithm) const
    {
        for (const auto& c : validator.makeChallenges(false, now)) {
            if (digestParam(c, "algorithm") == algorithm) {
                return c;
            }
        }
        return {};
    }
};

} // namespace

// ============================================================================
// NonceCache (U1-U3)
// ============================================================================

TEST(NonceCacheTest, RejectsReplay)
{
    NonceCache cache { 16U, std::chrono::seconds { 60 } };
    const auto t0 { std::chrono::steady_clock::now() };
    EXPECT_TRUE(cache.insert("nonce-a", t0));
    EXPECT_FALSE(cache.insert("nonce-a", t0 + std::chrono::seconds { 1 }));
    EXPECT_TRUE(cache.insert("nonce-b", t0));
}

TEST(NonceCacheTest, EvictsByTtl)
{
    NonceCache cache { 16U, std::chrono::seconds { 60 } };
    const auto t0 { std::chrono::steady_clock::now() };
    EXPECT_TRUE(cache.insert("nonce-a", t0));
    EXPECT_TRUE(cache.insert("nonce-a", t0 + std::chrono::seconds { 61 }));
}

TEST(NonceCacheTest, BoundedCapacity)
{
    NonceCache cache { 8U, std::chrono::seconds { 600 } };
    const auto t0 { std::chrono::steady_clock::now() };
    for (int i { 0 }; i < 100; ++i) {
        EXPECT_TRUE(cache.insert("n" + std::to_string(i), t0));
        EXPECT_LE(cache.size(), 8U);
    }
}

// ============================================================================
// CredentialStore (U18-U19)
// ============================================================================

TEST(CredentialStoreTest, KeepsLastAdmin)
{
    CredentialStore store { seedUsers() };
    EXPECT_EQ(store.adminCount(), 1U);
    EXPECT_FALSE(store.erase("admin"));
    EXPECT_FALSE(store.upsert({ "admin", "x-Pass-1", OnvifUserLevel::User }));
    EXPECT_TRUE(store.upsert({ "admin2", "Adm2-Pass", OnvifUserLevel::Administrator }));
    EXPECT_TRUE(store.erase("admin"));
    EXPECT_FALSE(store.find("admin").has_value());
}

TEST(CredentialStoreTest, ListHidesPasswords)
{
    const CredentialStore store { seedUsers() };
    const auto users { store.list() };
    ASSERT_EQ(users.size(), 3U);
    for (const auto& u : users) {
        EXPECT_TRUE(u.password.empty()) << u.username;
    }
    const auto admin { store.find("admin") };
    ASSERT_TRUE(admin.has_value());
    EXPECT_EQ(admin->password, "Adm1n-Pass");
}

TEST(CredentialStoreTest, RejectsInvalidUsernames)
{
    CredentialStore store { seedUsers() };
    EXPECT_FALSE(store.upsert({ "", "Pass-123", OnvifUserLevel::User }));
    EXPECT_FALSE(store.upsert({ "bad name", "Pass-123", OnvifUserLevel::User }));
    EXPECT_FALSE(store.upsert({ "<script>", "Pass-123", OnvifUserLevel::User }));
    EXPECT_FALSE(store.upsert({ std::string(33U, 'a'), "Pass-123", OnvifUserLevel::User }));
    EXPECT_TRUE(store.upsert({ "cam.ops-1_x", "Pass-123", OnvifUserLevel::User }));
}

// ============================================================================
// UsernameTokenValidator (U4-U10)
// ============================================================================

TEST_F(UsernameTokenTest, ValidDigestSucceeds)
{
    const auto result { run(makeDigestSecurity("op", "Op-Pass", now, nonce)) };
    EXPECT_EQ(result.outcome, AuthOutcome::Success);
    EXPECT_EQ(result.principal.username, "op");
    EXPECT_EQ(result.principal.level, OnvifUserLevel::Operator);
}

TEST_F(UsernameTokenTest, WrongPasswordFails)
{
    const auto result { run(makeDigestSecurity("op", "wrong", now, nonce)) };
    EXPECT_EQ(result.outcome, AuthOutcome::InvalidCredentials);
}

TEST_F(UsernameTokenTest, UnknownUserSameOutcome)
{
    const auto result { run(makeDigestSecurity("ghost", "Op-Pass", now, nonce)) };
    EXPECT_EQ(result.outcome, AuthOutcome::InvalidCredentials);
}

TEST_F(UsernameTokenTest, StaleCreatedFails)
{
    const auto past { run(makeDigestSecurity("op", "Op-Pass", now - std::chrono::seconds { 301 }, nonce)) };
    EXPECT_EQ(past.outcome, AuthOutcome::Stale);
    const auto future { run(
        makeDigestSecurity("op", "Op-Pass", now + std::chrono::seconds { 301 }, OnvifSecurity::generateNonce())) };
    EXPECT_EQ(future.outcome, AuthOutcome::Stale);
    const auto inside { run(
        makeDigestSecurity("op", "Op-Pass", now - std::chrono::seconds { 299 }, OnvifSecurity::generateNonce())) };
    EXPECT_EQ(inside.outcome, AuthOutcome::Success);
}

TEST_F(UsernameTokenTest, ReplayedNonceFails)
{
    const std::string xml { makeDigestSecurity("op", "Op-Pass", now, nonce) };
    EXPECT_EQ(run(xml).outcome, AuthOutcome::Success);
    EXPECT_EQ(run(xml).outcome, AuthOutcome::Replay);
}

TEST_F(UsernameTokenTest, PasswordTextRejected)
{
    const std::string created { isoUtc(now) };
    const std::string nonceB64 { OnvifSecurity::base64Encode(nonce) };
    EXPECT_EQ(run(makeSecurityXml("op", "Op-Pass", created, nonceB64, kTextType)).outcome, AuthOutcome::Unsupported);
    EXPECT_EQ(run(makeSecurityXml("op", "Op-Pass", created, OnvifSecurity::base64Encode(OnvifSecurity::generateNonce()),
                      ""))
                  .outcome,
        AuthOutcome::Unsupported);
}

TEST_F(UsernameTokenTest, BadBase64NonceFails)
{
    const std::string created { isoUtc(now) };
    const std::string digest { OnvifSecurity::computePasswordDigest(nonce, created, "Op-Pass") };
    EXPECT_EQ(run(makeSecurityXml("op", digest, created, "!!not*base64!!", kDigestType)).outcome,
        AuthOutcome::InvalidCredentials);
    EXPECT_EQ(run(makeSecurityXml("op", digest, created, "", kDigestType)).outcome, AuthOutcome::InvalidCredentials);
}

TEST_F(UsernameTokenTest, MalformedCreatedFails)
{
    const std::string nonceB64 { OnvifSecurity::base64Encode(nonce) };
    const std::string digest { OnvifSecurity::computePasswordDigest(nonce, "yesterday", "Op-Pass") };
    EXPECT_EQ(run(makeSecurityXml("op", digest, "yesterday", nonceB64, kDigestType)).outcome,
        AuthOutcome::InvalidCredentials);
}

TEST_F(UsernameTokenTest, EmptySecurityIsNoCredentials)
{
    EXPECT_EQ(validator.validate(pugi::xml_node {}, now).outcome, AuthOutcome::NoCredentials);
}

TEST(UsernameTokenLocateTest, TokenInBodyIgnored)
{
    const std::string sec { makeDigestSecurity(
        "op", "Op-Pass", std::chrono::system_clock::now(), OnvifSecurity::generateNonce()) };
    const std::string inBody { "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\"><s:Body>" + sec
        + "</s:Body></s:Envelope>" };
    pugi::xml_document bodyDoc {};
    ASSERT_TRUE(bodyDoc.load_string(inBody.c_str()));
    EXPECT_FALSE(findSecurityHeader(bodyDoc));

    const std::string inHeader { "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\"><s:Header>" + sec
        + "</s:Header><s:Body/></s:Envelope>" };
    pugi::xml_document headerDoc {};
    ASSERT_TRUE(headerDoc.load_string(inHeader.c_str()));
    EXPECT_TRUE(findSecurityHeader(headerDoc));
}

// ============================================================================
// DigestValidator (U11-U15)
// ============================================================================

TEST_F(DigestTest, ChallengesOfferSha256AndMd5)
{
    const auto challenges { validator.makeChallenges(false, now) };
    ASSERT_EQ(challenges.size(), 2U);
    EXPECT_EQ(digestParam(challenges.at(0U), "algorithm"), "SHA-256");
    EXPECT_EQ(digestParam(challenges.at(1U), "algorithm"), "MD5");
    for (const auto& c : challenges) {
        EXPECT_EQ(c.rfind("Digest ", 0U), 0U);
        EXPECT_EQ(digestParam(c, "realm"), "ONVIF");
        EXPECT_EQ(digestParam(c, "qop"), "auth");
        EXPECT_FALSE(digestParam(c, "nonce").empty());
    }
}

TEST_F(DigestTest, Sha256ValidSucceeds)
{
    const std::string auth { makeDigestAuth(challengeFor("SHA-256"), EVP_sha256(), "SHA-256", "admin", "Adm1n-Pass",
        "POST", "/onvif/device_service", "00000001", "c1") };
    const auto result { validator.validate(auth, "POST", "/onvif/device_service", now) };
    EXPECT_EQ(result.outcome, AuthOutcome::Success);
    EXPECT_EQ(result.principal.level, OnvifUserLevel::Administrator);
}

TEST_F(DigestTest, Md5ValidSucceeds)
{
    const std::string auth { makeDigestAuth(challengeFor("MD5"), EVP_md5(), "MD5", "viewer", "View-Pass", "GET",
        "/onvif/metadata_stream", "00000001", "c2") };
    const auto result { validator.validate(auth, "GET", "/onvif/metadata_stream", now) };
    EXPECT_EQ(result.outcome, AuthOutcome::Success);
    EXPECT_EQ(result.principal.level, OnvifUserLevel::User);
}

TEST_F(DigestTest, WrongPasswordFails)
{
    const std::string auth { makeDigestAuth(challengeFor("SHA-256"), EVP_sha256(), "SHA-256", "admin", "nope",
        "POST", "/onvif/device_service", "00000001", "c3") };
    EXPECT_EQ(validator.validate(auth, "POST", "/onvif/device_service", now).outcome,
        AuthOutcome::InvalidCredentials);
}

TEST_F(DigestTest, ExpiredNonceIsStale)
{
    const std::string auth { makeDigestAuth(challengeFor("SHA-256"), EVP_sha256(), "SHA-256", "admin", "Adm1n-Pass",
        "POST", "/onvif/device_service", "00000001", "c4") };
    const auto later { now + cfg.digestNonceTtl + std::chrono::seconds { 1 } };
    EXPECT_EQ(validator.validate(auth, "POST", "/onvif/device_service", later).outcome, AuthOutcome::Stale);
}

TEST_F(DigestTest, ForgedNonceFails)
{
    std::string challenge { challengeFor("SHA-256") };
    const std::string nonce { digestParam(challenge, "nonce") };
    challenge.replace(challenge.find(nonce), nonce.size(), OnvifSecurity::base64Encode(std::string("forged-nonce")));
    const std::string auth { makeDigestAuth(challenge, EVP_sha256(), "SHA-256", "admin", "Adm1n-Pass", "POST",
        "/onvif/device_service", "00000001", "c5") };
    EXPECT_EQ(validator.validate(auth, "POST", "/onvif/device_service", now).outcome,
        AuthOutcome::InvalidCredentials);
}

TEST_F(DigestTest, UriMismatchFails)
{
    const std::string auth { makeDigestAuth(challengeFor("SHA-256"), EVP_sha256(), "SHA-256", "admin", "Adm1n-Pass",
        "POST", "/onvif/media_service", "00000001", "c6") };
    EXPECT_EQ(validator.validate(auth, "POST", "/onvif/device_service", now).outcome,
        AuthOutcome::InvalidCredentials);
}

TEST_F(DigestTest, NcReplayFails)
{
    const std::string auth { makeDigestAuth(challengeFor("SHA-256"), EVP_sha256(), "SHA-256", "admin", "Adm1n-Pass",
        "POST", "/onvif/device_service", "00000001", "c7") };
    EXPECT_EQ(validator.validate(auth, "POST", "/onvif/device_service", now).outcome, AuthOutcome::Success);
    EXPECT_EQ(validator.validate(auth, "POST", "/onvif/device_service", now).outcome, AuthOutcome::Replay);
}

TEST_F(DigestTest, MalformedHeaderNoCrash)
{
    const std::vector<std::string> junk { "", "Digest", "Digest ,,,,", "Basic YWRtaW46YWRtaW4=",
        "Digest username=\"admin", "Digest username=admin, response=\"\"", std::string(8192U, 'A'),
        "Digest username=\"a\\\"b\", realm=\"ONVIF\"" };
    for (const auto& h : junk) {
        const auto outcome { validator.validate(h, "POST", "/onvif/device_service", now).outcome };
        EXPECT_TRUE(outcome == AuthOutcome::InvalidCredentials || outcome == AuthOutcome::NoCredentials) << h;
    }
}

// ============================================================================
// AccessPolicy (U16-U17)
// ============================================================================

TEST(AccessPolicyTest, MatrixMatchesSpec)
{
    using L = OnvifUserLevel;
    using C = AccessClass;
    struct Row {
        C cls;
        bool admin;
        bool oper;
        bool user;
        bool anon;
    };
    const std::array<Row, 8U> rows { { { C::PreAuth, true, true, true, true },
        { C::ReadSystem, true, true, true, false }, { C::ReadSystemSensitive, true, true, false, false },
        { C::ReadSystemSecret, true, false, false, false }, { C::WriteSystem, true, false, false, false },
        { C::Unrecoverable, true, false, false, false }, { C::ReadMedia, true, true, true, false },
        { C::Actuate, true, true, false, false } } };
    for (const auto& r : rows) {
        EXPECT_EQ(AccessPolicy::permits(L::Administrator, r.cls), r.admin);
        EXPECT_EQ(AccessPolicy::permits(L::Operator, r.cls), r.oper);
        EXPECT_EQ(AccessPolicy::permits(L::User, r.cls), r.user);
        EXPECT_EQ(AccessPolicy::permits(L::Anonymous, r.cls), r.anon);
        EXPECT_FALSE(AccessPolicy::permits(L::Extended, r.cls) && r.cls != C::PreAuth);
    }
}

TEST(AccessPolicyTest, ClassifiesRepresentativeOps)
{
    EXPECT_EQ(AccessPolicy::classify("Device", "GetSystemDateAndTime"), AccessClass::PreAuth);
    EXPECT_EQ(AccessPolicy::classify("Device", "tds:GetCapabilities"), AccessClass::PreAuth);
    EXPECT_EQ(AccessPolicy::classify("Device", "GetDeviceInformation"), AccessClass::ReadSystem);
    EXPECT_EQ(AccessPolicy::classify("Device", "GetUsers"), AccessClass::ReadSystemSensitive);
    EXPECT_EQ(AccessPolicy::classify("Device", "GetSystemBackup"), AccessClass::ReadSystemSecret);
    EXPECT_EQ(AccessPolicy::classify("Device", "CreateUsers"), AccessClass::WriteSystem);
    EXPECT_EQ(AccessPolicy::classify("Device", "SystemReboot"), AccessClass::Unrecoverable);
    EXPECT_EQ(AccessPolicy::classify("Media", "GetStreamUri"), AccessClass::ReadMedia);
    EXPECT_EQ(AccessPolicy::classify("PTZ", "tptz:ContinuousMove"), AccessClass::Actuate);
    EXPECT_EQ(AccessPolicy::classify("PTZ", "SetPreset"), AccessClass::Actuate);
    EXPECT_EQ(AccessPolicy::classify("Imaging", "Stop"), AccessClass::Actuate);
    EXPECT_EQ(AccessPolicy::classify("PullPoint", "PullMessages"), AccessClass::ReadMedia);
    EXPECT_EQ(AccessPolicy::classify("Thermal", "TriggerNUC"), AccessClass::Actuate);
}

TEST(AccessPolicyTest, UnknownOpAdminOnly)
{
    const AccessClass cls { AccessPolicy::classify("Device", "SomeFutureOperation") };
    EXPECT_EQ(cls, AccessClass::Unrecoverable);
    EXPECT_FALSE(AccessPolicy::permits(OnvifUserLevel::Operator, cls));
    EXPECT_EQ(AccessPolicy::classify("NoSuchService", "GetSystemDateAndTime"), AccessClass::Unrecoverable);
    EXPECT_EQ(AccessPolicy::classify("Device", ""), AccessClass::Unrecoverable);
}

// ============================================================================
// Authenticator (routing + back-off)
// ============================================================================

TEST(AuthenticatorTest, NoCredentialsReported)
{
    Authenticator auth { std::make_shared<CredentialStore>(seedUsers()), OnvifAuthConfig {} };
    AuthInput in {};
    in.method = "POST";
    in.uri = "/onvif/device_service";
    EXPECT_EQ(auth.authenticate(in).outcome, AuthOutcome::NoCredentials);
    EXPECT_EQ(auth.challenges(false).size(), 2U);
}

TEST(AuthenticatorTest, UsernameTokenRouted)
{
    Authenticator auth { std::make_shared<CredentialStore>(seedUsers()), OnvifAuthConfig {} };
    pugi::xml_document doc {};
    ASSERT_TRUE(doc.load_string(makeDigestSecurity(
                                    "admin", "Adm1n-Pass", std::chrono::system_clock::now(), OnvifSecurity::generateNonce())
                                    .c_str()));
    AuthInput in {};
    in.method = "POST";
    in.uri = "/onvif/device_service";
    in.securityHeader = doc.document_element();
    const auto result { auth.authenticate(in) };
    EXPECT_EQ(result.outcome, AuthOutcome::Success);
    EXPECT_EQ(result.principal.username, "admin");
}

TEST(AuthenticatorTest, DisabledMechanismsIgnored)
{
    OnvifAuthConfig cfg {};
    cfg.allowUsernameToken = false;
    Authenticator auth { std::make_shared<CredentialStore>(seedUsers()), cfg };
    pugi::xml_document doc {};
    ASSERT_TRUE(doc.load_string(makeDigestSecurity(
                                    "admin", "Adm1n-Pass", std::chrono::system_clock::now(), OnvifSecurity::generateNonce())
                                    .c_str()));
    AuthInput in {};
    in.securityHeader = doc.document_element();
    EXPECT_EQ(auth.authenticate(in).outcome, AuthOutcome::NoCredentials);
}

TEST(AuthenticatorTest, BackoffAfterRepeatedFailures)
{
    Authenticator auth { std::make_shared<CredentialStore>(seedUsers()), OnvifAuthConfig {} };
    const auto t0 { std::chrono::steady_clock::now() };
    for (int i { 0 }; i < 5; ++i) {
        auth.recordFailure("10.0.0.9", t0);
    }
    EXPECT_EQ(auth.penalty("10.0.0.9", t0), std::chrono::milliseconds { 0 });
    auth.recordFailure("10.0.0.9", t0);
    EXPECT_GT(auth.penalty("10.0.0.9", t0), std::chrono::milliseconds { 0 });
    EXPECT_LE(auth.penalty("10.0.0.9", t0), std::chrono::milliseconds { 2000 });
    EXPECT_EQ(auth.penalty("10.0.0.10", t0), std::chrono::milliseconds { 0 });
    EXPECT_EQ(auth.penalty("10.0.0.9", t0 + std::chrono::seconds { 61 }), std::chrono::milliseconds { 0 });
}

// ============================================================================
// SoapFault
// ============================================================================

TEST(SoapFaultTest, NotAuthorizedIsWellFormed)
{
    const std::string xml { "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                            "xmlns:ter=\"http://www.onvif.org/ver10/error\"><SOAP-ENV:Body>"
        + SoapFault::notAuthorized() + "</SOAP-ENV:Body></SOAP-ENV:Envelope>" };
    pugi::xml_document doc {};
    ASSERT_TRUE(doc.load_string(xml.c_str()));
    const auto code { doc.select_node("//*[local-name()='Fault']/*[local-name()='Code']/*[local-name()='Value']") };
    ASSERT_TRUE(code);
    EXPECT_EQ(std::string(code.node().text().as_string()), "SOAP-ENV:Sender");
    const auto sub { doc.select_node("//*[local-name()='Subcode']/*[local-name()='Value']") };
    ASSERT_TRUE(sub);
    EXPECT_EQ(std::string(sub.node().text().as_string()), "ter:NotAuthorized");
}

// ============================================================================
// OnvifServer start-up guards (I12 + Q2 interim guard)
// ============================================================================

TEST(OnvifServerAuthGuardTest, AuthDisabledNonLoopbackStartFails)
{
    OnvifServerConfig config {};
    config.bindAddress = "0.0.0.0";
    config.port = 18711;
    config.defaultUsers = seedUsers();
    config.auth.enabled = false;
    OnvifServer server { config };
    EXPECT_FALSE(server.start());
    EXPECT_FALSE(server.isRunning());
}

TEST(OnvifServerAuthGuardTest, DefaultPasswordNonLoopbackStartFails)
{
    OnvifServerConfig config {};
    config.bindAddress = "0.0.0.0";
    config.port = 18712;
    OnvifServer server { config };
    EXPECT_FALSE(server.start());
}

TEST(OnvifServerAuthGuardTest, DefaultPasswordAllowedExplicitly)
{
    OnvifServerConfig config {};
    config.bindAddress = "0.0.0.0";
    config.port = 18713;
    config.auth.allowDefaultPassword = true;
    OnvifServer server { config };
    EXPECT_TRUE(server.start());
    server.stop();
}

TEST(OnvifServerAuthGuardTest, LoopbackWithDefaultsStarts)
{
    OnvifServerConfig config {};
    config.bindAddress = "127.0.0.1";
    config.port = 18714;
    OnvifServer server { config };
    EXPECT_TRUE(server.start());
    server.stop();
}

TEST(OnvifServerAuthGuardTest, PortInUseStartFails)
{
    OnvifServerConfig config {};
    config.bindAddress = "127.0.0.1";
    config.port = 18715;
    OnvifServer first { config };
    ASSERT_TRUE(first.start());
    OnvifServer second { config };
    EXPECT_FALSE(second.start());
    first.stop();
}
