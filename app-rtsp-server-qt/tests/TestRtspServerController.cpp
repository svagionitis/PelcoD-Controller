#include "RtspServerController.h"

#include <cstddef>
#include <cstdint>

#include <memory>
#include <string>

#include <QCoreApplication>
#include <gtest/gtest.h>

namespace RtspServerApp {
namespace {

    class RtspServerControllerTest : public ::testing::Test {
    protected:
        static void SetUpTestSuite()
        {
            int argc = 1;
            static char appName[] = "RtspTestApp";
            static char* argv[] = { appName, nullptr };
            if (QCoreApplication::instance() == nullptr) {
                static QCoreApplication s_app(argc, argv);
            }
        }
    };

    TEST_F(RtspServerControllerTest, InitialPropertiesAndUrls)
    {
        RtspServerController controller;

        EXPECT_EQ(controller.serverState(), static_cast<int>(RtspServerController::State::Stopped));
        EXPECT_EQ(controller.serverStateString(), QStringLiteral("STOPPED"));
        EXPECT_EQ(controller.port(), 8554);
        EXPECT_EQ(controller.streamPath(), QStringLiteral("live"));
        EXPECT_FALSE(controller.primaryUrl().isEmpty());
        EXPECT_TRUE(controller.primaryUrl().contains(QStringLiteral(":8554/live")));
        EXPECT_EQ(controller.activeClients(), 0);
        EXPECT_EQ(controller.queueDepth(), 0);
    }

    TEST_F(RtspServerControllerTest, PropertyMutationsAndUrlSync)
    {
        RtspServerController controller;

        controller.setPort(9554);
        EXPECT_EQ(controller.port(), 9554);
        EXPECT_TRUE(controller.primaryUrl().contains(QStringLiteral(":9554/")));

        controller.setStreamPath(QStringLiteral("tactical_feed"));
        EXPECT_EQ(controller.streamPath(), QStringLiteral("tactical_feed"));
        EXPECT_TRUE(controller.primaryUrl().endsWith(QStringLiteral("/tactical_feed")));

        controller.setMaxClients(32);
        EXPECT_EQ(controller.maxClients(), 32);

        controller.setQueueCapacity(60);
        EXPECT_EQ(controller.queueCapacity(), 60);

        controller.setAuthEnabled(true);
        EXPECT_TRUE(controller.authEnabled());

        controller.setUsername(QStringLiteral("pilot"));
        EXPECT_EQ(controller.username(), QStringLiteral("pilot"));

        controller.setPassword(QStringLiteral("secret"));
        EXPECT_EQ(controller.password(), QStringLiteral("secret"));

        controller.setAuthEnabled(false);
        EXPECT_FALSE(controller.authEnabled());
        EXPECT_TRUE(controller.username().isEmpty());
        EXPECT_TRUE(controller.password().isEmpty());
    }

    TEST_F(RtspServerControllerTest, StartStopEphemeralLifecycle)
    {
        RtspServerController controller;

        // Use ephemeral port 0 to prevent port collisions during parallel test runs
        controller.setPort(0);
        controller.setStreamPath(QStringLiteral("test_stream"));

        EXPECT_TRUE(controller.startServer());
        EXPECT_EQ(controller.serverState(), static_cast<int>(RtspServerController::State::Running));
        EXPECT_EQ(controller.serverStateString(), QStringLiteral("RUNNING"));

        // Server should be running and local URL populated
        EXPECT_FALSE(controller.localUrl().isEmpty());

        controller.stopServer();
        EXPECT_EQ(controller.serverState(), static_cast<int>(RtspServerController::State::Stopped));
        EXPECT_EQ(controller.serverStateString(), QStringLiteral("STOPPED"));
        EXPECT_EQ(controller.activeClients(), 0);
    }

    TEST_F(RtspServerControllerTest, PresetSaveAndLoad)
    {
        RtspServerController controller;

        const QString presetName = QStringLiteral("TestProfileAlpha");
        controller.setPort(8555);
        controller.setStreamPath(QStringLiteral("alpha"));
        controller.setMaxClients(8);

        EXPECT_TRUE(controller.savePreset(presetName));
        EXPECT_TRUE(controller.presetList().contains(presetName));

        // Change properties
        controller.setPort(7000);
        controller.setStreamPath(QStringLiteral("different"));
        EXPECT_EQ(controller.port(), 7000);

        // Reload preset
        EXPECT_TRUE(controller.loadPreset(presetName));
        EXPECT_EQ(controller.port(), 8555);
        EXPECT_EQ(controller.streamPath(), QStringLiteral("alpha"));
        EXPECT_EQ(controller.maxClients(), 8);

        // Clean up preset
        EXPECT_TRUE(controller.deletePreset(presetName));
        EXPECT_FALSE(controller.presetList().contains(presetName));
    }

} // namespace
} // namespace RtspServerApp
