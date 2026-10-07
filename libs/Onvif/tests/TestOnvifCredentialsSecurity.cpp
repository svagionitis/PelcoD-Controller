/// @file TestOnvifCredentialsSecurity.cpp
/// @brief Security unit tests for ONVIF credentials, password policy, vault, and first-boot commissioning (finding C6).

#include "OnvifTestClient.h"

#include <Onvif/CredentialStore.h>
#include <Onvif/CredentialVault.h>
#include <Onvif/HttplibInclude.h>
#include <Onvif/OnvifServer.h>
#include <Onvif/PasswordPolicy.h>
#include <Onvif/SoapFault.h>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace Onvif;
using OnvifTest::makeCreds;
using OnvifTest::OnvifTestClient;

namespace {

std::atomic<int> g_port { 18910 };

constexpr const char* kSoapCt { "application/soap+xml; charset=utf-8" };

std::string envelope(const std::string& bodyXml)
{
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
           "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
           "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
           "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
           "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" "
           "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
           "<s:Body>\r\n"
        + bodyXml + "\r\n</s:Body></s:Envelope>";
}

std::string createUserBody(const std::string& name, const std::string& pass, const std::string& level)
{
    return "<tds:CreateUsers><tds:User><tt:Username>" + name + "</tt:Username><tt:Password>" + pass
        + "</tt:Password><tt:UserLevel>" + level + "</tt:UserLevel></tds:User></tds:CreateUsers>";
}

} // namespace

// ============================================================================
// PasswordPolicy Unit Tests
// ============================================================================

TEST(PasswordPolicyTest, RejectsEmptyPassword)
{
    PasswordPolicyConfig cfg {};
    EXPECT_EQ(PasswordPolicy::check("", "admin", cfg), PasswordCheckResult::EmptyPassword);
}

TEST(PasswordPolicyTest, RejectsShortPassword)
{
    PasswordPolicyConfig cfg {};
    cfg.minLength = 8;
    EXPECT_EQ(PasswordPolicy::check("Sh0rt!", "admin", cfg), PasswordCheckResult::TooShort);
}

TEST(PasswordPolicyTest, RejectsTooLongPassword)
{
    PasswordPolicyConfig cfg {};
    cfg.maxLength = 16;
    const std::string longPass(20, 'A');
    EXPECT_EQ(PasswordPolicy::check(longPass, "admin", cfg), PasswordCheckResult::TooLong);
}

TEST(PasswordPolicyTest, RejectsCommonDefaults)
{
    PasswordPolicyConfig cfg {};
    EXPECT_EQ(PasswordPolicy::check("admin", "admin", cfg), PasswordCheckResult::CommonDefault);
    EXPECT_EQ(PasswordPolicy::check("password", "user", cfg), PasswordCheckResult::CommonDefault);
    EXPECT_EQ(PasswordPolicy::check("123456", "user", cfg), PasswordCheckResult::CommonDefault);
    EXPECT_EQ(PasswordPolicy::check("camera", "operator", cfg), PasswordCheckResult::CommonDefault);
}

TEST(PasswordPolicyTest, RejectsUsernameInPassword)
{
    PasswordPolicyConfig cfg {};
    EXPECT_EQ(PasswordPolicy::check("Adm1n!2026", "admin", cfg), PasswordCheckResult::MatchesUsername);
    EXPECT_EQ(PasswordPolicy::check("super_Admin_123!", "admin", cfg), PasswordCheckResult::MatchesUsername);
}

TEST(PasswordPolicyTest, RejectsInsufficientClasses)
{
    PasswordPolicyConfig cfg {};
    cfg.minClasses = 3;
    EXPECT_EQ(PasswordPolicy::check("alllowercasehere", "user", cfg), PasswordCheckResult::InsufficientClasses);
    EXPECT_EQ(PasswordPolicy::check("ALLUPPERCASEHERE", "user", cfg), PasswordCheckResult::InsufficientClasses);
    EXPECT_EQ(PasswordPolicy::check("12345678901234", "user", cfg), PasswordCheckResult::InsufficientClasses);
}

TEST(PasswordPolicyTest, AcceptsStrongPassword)
{
    PasswordPolicyConfig cfg {};
    EXPECT_EQ(PasswordPolicy::check("Str0ng!P@ssw0rd", "johndoe", cfg), PasswordCheckResult::Valid);
    EXPECT_EQ(PasswordPolicy::check("PelcoD#2026Secure", "operator1", cfg), PasswordCheckResult::Valid);
}

// ============================================================================
// CredentialVault Unit Tests
// ============================================================================

TEST(CredentialVaultTest, RoundTripEncryptionAndDecryption)
{
    const std::string key { CredentialVault::deriveKey("master-secret", "random-salt-12345") };
    ASSERT_EQ(key.size(), 32U);

    const std::vector<OnvifUser> users { { "admin", "Adm1n#Secure2026", OnvifUserLevel::Administrator },
        { "operator", "Oper4tor!Pass2026", OnvifUserLevel::Operator } };

    const std::string envelope { CredentialVault::encrypt(users, key) };
    ASSERT_FALSE(envelope.empty());

    // Verify plaintext is not visible in base64 envelope
    EXPECT_EQ(envelope.find("Adm1n#Secure2026"), std::string::npos);
    EXPECT_EQ(envelope.find("Oper4tor!Pass2026"), std::string::npos);

    const auto decrypted { CredentialVault::decrypt(envelope, key) };
    ASSERT_TRUE(decrypted.has_value());
    ASSERT_EQ(decrypted->size(), 2U);
    EXPECT_EQ(decrypted->at(0).username, "admin");
    EXPECT_EQ(decrypted->at(0).password, "Adm1n#Secure2026");
    EXPECT_EQ(decrypted->at(0).level, OnvifUserLevel::Administrator);
    EXPECT_EQ(decrypted->at(1).username, "operator");
    EXPECT_EQ(decrypted->at(1).password, "Oper4tor!Pass2026");
    EXPECT_EQ(decrypted->at(1).level, OnvifUserLevel::Operator);
}

TEST(CredentialVaultTest, DecryptionFailsWithWrongKey)
{
    const std::string key1 { CredentialVault::deriveKey("secret-1", "salt") };
    const std::string key2 { CredentialVault::deriveKey("secret-2", "salt") };

    const std::vector<OnvifUser> users { { "admin", "P@ssword123!", OnvifUserLevel::Administrator } };
    const std::string envelope { CredentialVault::encrypt(users, key1) };
    ASSERT_FALSE(envelope.empty());

    const auto decrypted { CredentialVault::decrypt(envelope, key2) };
    EXPECT_FALSE(decrypted.has_value());
}

TEST(CredentialVaultTest, TamperedCiphertextFailsAuthTag)
{
    const std::string key { CredentialVault::deriveKey("secret", "salt") };
    const std::vector<OnvifUser> users { { "admin", "P@ssword123!", OnvifUserLevel::Administrator } };
    std::string envelope { CredentialVault::encrypt(users, key) };
    ASSERT_GE(envelope.size(), 10U);

    // Tamper single base64 character in payload
    envelope[envelope.size() - 5] = (envelope[envelope.size() - 5] == 'A') ? 'B' : 'A';
    const auto decrypted { CredentialVault::decrypt(envelope, key) };
    EXPECT_FALSE(decrypted.has_value());
}

TEST(CredentialVaultTest, SaveAndLoadFromFile)
{
    const std::string key { CredentialVault::deriveKey("vault-pass", "vault-salt") };
    const std::string tmpPath { "/tmp/test_onvif_vault.bin" };

    const std::vector<OnvifUser> users { { "sysadmin", "Sup3r#Secr3t!", OnvifUserLevel::Administrator } };

    EXPECT_TRUE(CredentialVault::saveToFile(tmpPath, users, key));
    const auto loaded { CredentialVault::loadFromFile(tmpPath, key) };
    ASSERT_TRUE(loaded.has_value());
    ASSERT_EQ(loaded->size(), 1U);
    EXPECT_EQ(loaded->at(0).username, "sysadmin");
    EXPECT_EQ(loaded->at(0).password, "Sup3r#Secr3t!");

    std::filesystem::remove(tmpPath);
}

// ============================================================================
// OnvifServer Provisioning & First-Boot Integration Tests
// ============================================================================

TEST(OnvifServerProvisioningTest, EmptyDefaultUsersStartsInUnprovisionedMode)
{
    const int port { g_port.fetch_add(1) };
    OnvifServerConfig config {};
    config.bindAddress = "127.0.0.1";
    config.port = port;
    // defaultUsers is empty by default
    ASSERT_TRUE(config.defaultUsers.empty());

    OnvifServer server { config };
    EXPECT_EQ(server.provisioningState(), ProvisioningState::Unprovisioned);
    ASSERT_TRUE(server.start());
    EXPECT_TRUE(server.isRunning());
    server.stop();
}

TEST(OnvifServerProvisioningTest, UnprovisionedAllowsDiscoveryPreAuth)
{
    const int port { g_port.fetch_add(1) };
    OnvifServerConfig config {};
    config.bindAddress = "127.0.0.1";
    config.port = port;

    OnvifServer server { config };
    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    OnvifTestClient client { "127.0.0.1", port }; // anonymous
    const std::string devInfoReq { envelope("<tds:GetDeviceInformation/>") };
    const auto res { client.Post("/onvif/device_service", devInfoReq, kSoapCt) };
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_NE(res->body.find("GetDeviceInformationResponse"), std::string::npos);

    const std::string dateReq { envelope("<tds:GetSystemDateAndTime/>") };
    const auto dateRes { client.Post("/onvif/device_service", dateReq, kSoapCt) };
    ASSERT_NE(dateRes, nullptr);
    EXPECT_EQ(dateRes->status, 200);

    server.stop();
}

TEST(OnvifServerProvisioningTest, UnprovisionedRejectsOperationalRequests)
{
    const int port { g_port.fetch_add(1) };
    OnvifServerConfig config {};
    config.bindAddress = "127.0.0.1";
    config.port = port;

    OnvifServer server { config };
    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    OnvifTestClient client { "127.0.0.1", port };
    // Send PTZ move while unprovisioned
    const std::string ptzReq { envelope(
        "<tptz:ContinuousMove><tptz:ProfileToken>Profile_1</tptz:ProfileToken>"
        "<tptz:Velocity><tt:PanTilt x=\"0.5\" y=\"0.0\"/></tptz:Velocity></tptz:ContinuousMove>") };

    const auto res { client.Post("/onvif/ptz_service", ptzReq, kSoapCt) };
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 400);
    EXPECT_NE(res->body.find("DeviceUnprovisioned"), std::string::npos);

    server.stop();
}

TEST(OnvifServerProvisioningTest, CreateUsersWithWeakPasswordFails)
{
    const int port { g_port.fetch_add(1) };
    OnvifServerConfig config {};
    config.bindAddress = "127.0.0.1";
    config.port = port;

    OnvifServer server { config };
    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    OnvifTestClient client { "127.0.0.1", port };
    // Attempt to set weak/trivial default password "admin"
    const std::string createReq { envelope(createUserBody("admin", "admin", "Administrator")) };
    const auto res { client.Post("/onvif/device_service", createReq, kSoapCt) };
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 400);
    EXPECT_NE(res->body.find("PasswordTooWeak"), std::string::npos);
    EXPECT_EQ(server.provisioningState(), ProvisioningState::Unprovisioned);

    server.stop();
}

TEST(OnvifServerProvisioningTest, CreateUsersWithStrongPasswordTransitionsToProvisioned)
{
    const int port { g_port.fetch_add(1) };
    OnvifServerConfig config {};
    config.bindAddress = "127.0.0.1";
    config.port = port;

    OnvifServer server { config };
    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    OnvifTestClient unauthClient { "127.0.0.1", port };
    // Create initial admin with compliant password
    const std::string strongPass { "PelcoD#2026Secure" };
    const std::string createReq { envelope(createUserBody("admin", strongPass, "Administrator")) };
    const auto res { unauthClient.Post("/onvif/device_service", createReq, kSoapCt) };
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_NE(res->body.find("CreateUsersResponse"), std::string::npos);

    // Verify transition to Provisioned
    EXPECT_EQ(server.provisioningState(), ProvisioningState::Provisioned);

    // Now authenticated operational requests should succeed
    OnvifTestClient authClient { "127.0.0.1", port, makeCreds("admin", strongPass) };
    const std::string ptzReq { envelope(
        "<tptz:ContinuousMove><tptz:ProfileToken>Profile_1</tptz:ProfileToken>"
        "<tptz:Velocity><tt:PanTilt x=\"0.0\" y=\"0.0\"/></tptz:Velocity></tptz:ContinuousMove>") };
    const auto ptzRes { authClient.Post("/onvif/ptz_service", ptzReq, kSoapCt) };
    ASSERT_NE(ptzRes, nullptr);
    EXPECT_EQ(ptzRes->status, 200);

    server.stop();
}

TEST(OnvifServerProvisioningTest, FactoryDefaultResetsToUnprovisioned)
{
    const int port { g_port.fetch_add(1) };
    OnvifServerConfig config {};
    config.bindAddress = "127.0.0.1";
    config.port = port;

    OnvifServer server { config };
    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    OnvifTestClient setupClient { "127.0.0.1", port };
    const std::string strongPass { "PelcoD#2026Secure" };
    const std::string createReq { envelope(createUserBody("admin", strongPass, "Administrator")) };
    ASSERT_EQ(setupClient.Post("/onvif/device_service", createReq, kSoapCt)->status, 200);
    EXPECT_EQ(server.provisioningState(), ProvisioningState::Provisioned);

    // Now call SetSystemFactoryDefault
    OnvifTestClient adminClient { "127.0.0.1", port, makeCreds("admin", strongPass) };
    const std::string factoryResetReq { envelope(
        "<tds:SetSystemFactoryDefault><tds:FactoryDefault>Hard</tds:FactoryDefault></tds:SetSystemFactoryDefault>") };
    const auto resetRes { adminClient.Post("/onvif/device_service", factoryResetReq, kSoapCt) };
    ASSERT_NE(resetRes, nullptr);
    EXPECT_EQ(resetRes->status, 200);

    // Verify state has returned to Unprovisioned
    EXPECT_EQ(server.provisioningState(), ProvisioningState::Unprovisioned);

    server.stop();
}
