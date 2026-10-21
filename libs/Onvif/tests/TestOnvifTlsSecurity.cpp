/// @file TestOnvifTlsSecurity.cpp
/// @brief Unit tests for ONVIF client TLS verification and HTTP authentication policies (Finding H5).

#include <cstddef>
#include <cstdint>

#include <chrono>
#include <string>

#include <gtest/gtest.h>

#include "Onvif/OnvifAuthTypes.h"
#include "Onvif/OnvifClient.h"
#include "Onvif/OnvifHttpClient.h"
#include "Onvif/OnvifSecurity.h"

using namespace Onvif;

TEST(TestOnvifTlsSecurity, DefaultTlsConfigIsStrict)
{
    const TlsConfig config {};
    EXPECT_EQ(config.mode, TlsVerificationMode::Strict);
    EXPECT_TRUE(config.caBundlePath.empty());
    EXPECT_TRUE(config.pinnedPublicKey.empty());
}

TEST(TestOnvifTlsSecurity, HttpClientTlsConfigAccessors)
{
    OnvifHttpClient client {};
    EXPECT_EQ(client.tlsConfig().mode, TlsVerificationMode::Strict);

    TlsConfig customConfig {};
    customConfig.mode = TlsVerificationMode::PinnedCertificate;
    customConfig.caBundlePath = "/etc/ssl/certs/custom-ca.pem";
    customConfig.pinnedPublicKey = "sha256//WoiWRyIOVNa9ihaBciRSC7XHjliYS9VwUGOIud4PB18=";

    client.setTlsConfig(customConfig);
    EXPECT_EQ(client.tlsConfig().mode, TlsVerificationMode::PinnedCertificate);
    EXPECT_EQ(client.tlsConfig().caBundlePath, "/etc/ssl/certs/custom-ca.pem");
    EXPECT_EQ(client.tlsConfig().pinnedPublicKey, "sha256//WoiWRyIOVNa9ihaBciRSC7XHjliYS9VwUGOIud4PB18=");
}

TEST(TestOnvifTlsSecurity, HttpClientHttpCredentials)
{
    OnvifHttpClient client {};
    EXPECT_TRUE(client.httpUsername().empty());

    client.setHttpCredentials("operator_1", "SecretPass123!");
    EXPECT_EQ(client.httpUsername(), "operator_1");
}

TEST(TestOnvifTlsSecurity, OnvifClientTlsConfigIntegration)
{
    TlsConfig tlsConfig {};
    tlsConfig.mode = TlsVerificationMode::InsecureAllowSelfSigned;

    SecurityCredentials credentials {};
    credentials.username = "admin";
    credentials.password = "AdminSecret123!";

    OnvifClient client("http://127.0.0.1:8080/onvif/device_service", credentials, tlsConfig);
    EXPECT_EQ(client.tlsConfig().mode, TlsVerificationMode::InsecureAllowSelfSigned);
    EXPECT_EQ(client.credentials().username, "admin");

    TlsConfig strictConfig {};
    strictConfig.mode = TlsVerificationMode::Strict;
    client.setTlsConfig(strictConfig);
    EXPECT_EQ(client.tlsConfig().mode, TlsVerificationMode::Strict);
}

TEST(TestOnvifTlsSecurity, OnvifClientSynchronizesCredentialsToHttpClient)
{
    OnvifClient client("http://127.0.0.1:8080/onvif/device_service");

    SecurityCredentials credentials {};
    credentials.username = "sec_admin";
    credentials.password = "StrongPassword456#";

    client.setCredentials(credentials);
    EXPECT_EQ(client.credentials().username, "sec_admin");
}

TEST(TestOnvifTlsSecurity, ConnectionFailureReportsErrorMessage)
{
    OnvifHttpClient client {};
    client.setTimeout(std::chrono::milliseconds(200));

    // Non-routable loopback port guaranteed to fail connection
    const HttpResponse res = client.sendGet("http://127.0.0.1:1/nonexistent");
    EXPECT_FALSE(res.isSuccess());
    EXPECT_EQ(res.statusCode, 0);
    EXPECT_FALSE(res.errorMessage.empty());
}
