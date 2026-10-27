/// @file TestQOnvifIntegration.cpp
/// @brief Unit and integration tests for QOnvifServer and QOnvifDevice.

#include <cstdint>

#include <memory>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QString>

#include <gtest/gtest.h>

#include "MockPelcoDDevice.h"
#include "OnvifServerTypes.h"
#include "OnvifTypes.h"
#include "QOnvifDevice.h"
#include "QOnvifServer.h"
#include "QPelcoDDevice.h"
#include "TestHelpers.h"

namespace {

[[nodiscard]] Onvif::OnvifServerConfig makeTestServerConfig(
    std::uint16_t port, const std::string& bindAddr = "127.0.0.1")
{
    Onvif::OnvifServerConfig cfg {};
    cfg.port = port;
    cfg.bindAddress = bindAddr;
    cfg.discoveryMode = Onvif::DiscoveryMode::NonDiscoverable;
    cfg.auth.enabled = true;
    Onvif::OnvifUser admin {};
    admin.username = "admin";
    admin.password = "Str0ng#Pass2026";
    admin.level = Onvif::OnvifUserLevel::Administrator;
    cfg.defaultUsers.push_back(admin);
    return cfg;
}

} // namespace

TEST(QOnvifIntegrationTest, ServerStartedEmitsNormalizedEndpointUrl)
{
    Onvif::OnvifServerConfig cfg { makeTestServerConfig(18981U, "0.0.0.0") };
    PelcoD::Qt::QOnvifServer server(cfg);

    QString emittedUrl {};
    QObject::connect(
        &server, &PelcoD::Qt::QOnvifServer::serverStarted, [&emittedUrl](const QString& url) { emittedUrl = url; });

    ASSERT_TRUE(server.start());
    EXPECT_EQ(emittedUrl, QStringLiteral("http://127.0.0.1:18981/onvif/device_service"));
    EXPECT_EQ(server.endpointUrl(), QStringLiteral("http://127.0.0.1:18981/onvif/device_service"));
    server.stop();
}

TEST(QOnvifIntegrationTest, ServerWiresAllServiceAdaptersForClient)
{
    Onvif::OnvifServerConfig cfg { makeTestServerConfig(18982U, "127.0.0.1") };
    PelcoD::Qt::QOnvifServer server(cfg);
    ASSERT_TRUE(server.start());

    PelcoD::Qt::QOnvifDevice client;
    client.connectToCamera(server.endpointUrl(), QStringLiteral("admin"), QStringLiteral("Str0ng#Pass2026"));
    ASSERT_TRUE(client.isConnected());

    // Create a privacy mask without specifying configurationToken or with default and verify refreshMasks() includes it
    std::vector<Onvif::PrivacyMask> masks {};
    QObject::connect(&client, &PelcoD::Qt::QOnvifDevice::masksUpdated,
        [&masks](const std::vector<Onvif::PrivacyMask>& m) { masks = m; });
    Onvif::PrivacyMask newMask {};
    newMask.type = Onvif::MaskType::Color;
    newMask.enabled = true;
    newMask.polygon = { { 0.2f, 0.2f }, { 0.4f, 0.2f }, { 0.4f, 0.4f }, { 0.2f, 0.4f } };
    const QString createdToken { client.createMask(newMask) };
    EXPECT_FALSE(createdToken.isEmpty());
    client.refreshMasks();
    bool foundCreatedMask { false };
    for (const auto& m : masks) {
        if (QString::fromStdString(m.token) == createdToken) {
            foundCreatedMask = true;
            break;
        }
    }
    EXPECT_TRUE(foundCreatedMask);

    // Start event subscription & metadata streaming and immediately disconnect to verify clean thread join
    client.startEventSubscription(100);
    EXPECT_TRUE(client.isEventSubscriptionActive());
    client.startMetadataStreaming(100);
    EXPECT_TRUE(client.isMetadataStreamingActive());
    client.disconnectFromCamera();
    EXPECT_FALSE(client.isConnected());
    EXPECT_FALSE(client.isEventSubscriptionActive());
    EXPECT_FALSE(client.isMetadataStreamingActive());

    server.stop();
}

int main(int argc, char* argv[])
{
    PelcoDTest::initTestHarness();
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
