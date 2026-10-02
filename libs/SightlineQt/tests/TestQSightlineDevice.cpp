/// @file TestQSightlineDevice.cpp
/// @brief Unit tests for QSightlineDevice Qt signal/slot dispatch and lifecycle.

#include "QSightlineDevice.h"
#include <SightlineCore/SightlineProtocolBuilder.h>
#include <SightlineCore/modules/SightlineOverlayBuilder.h>
#include <Transport/BaseTransport.h>

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSignalSpy>
#include <mutex>
#include <vector>

namespace {

class MockTransportForQt : public Transport::BaseTransport {
public:
    MockTransportForQt() = default;
    ~MockTransportForQt() override = default;

    bool open() override
    {
        m_isOpen = true;
        return true;
    }

    void close() override
    {
        m_isOpen = false;
    }

    bool isOpen() const noexcept override
    {
        return m_isOpen;
    }

    bool sendData(const std::vector<std::uint8_t>& data) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sent.push_back(data);
        return true;
    }

    void injectData(const std::vector<std::uint8_t>& data)
    {
        invokeDataCallback(data);
    }

    std::vector<std::vector<std::uint8_t>> getSentPackets() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_sent;
    }

private:
    mutable std::mutex m_mutex;
    bool m_isOpen { false };
    std::vector<std::vector<std::uint8_t>> m_sent {};
};

int g_argc = 1;
char g_appName[] = "TestQSightlineDevice";
char* g_argv[] = { g_appName, nullptr };

class QSightlineDeviceTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (QCoreApplication::instance() == nullptr) {
            new QCoreApplication(g_argc, g_argv);
        }
    }
};

/// @brief Verify start and stop lifecycle and connection signal.
TEST_F(QSightlineDeviceTest, LifecycleAndConnectionState)
{
    auto transport = std::make_shared<MockTransportForQt>();
    QSightlineDevice qDevice(transport);

    QSignalSpy connSpy(&qDevice, &QSightlineDevice::connectionStateChanged);
    EXPECT_FALSE(qDevice.isConnected());

    EXPECT_TRUE(qDevice.start());
    EXPECT_TRUE(qDevice.isConnected());
    EXPECT_EQ(connSpy.count(), 1);
    EXPECT_TRUE(connSpy.takeFirst().at(0).toBool());

    qDevice.stop();
    EXPECT_FALSE(qDevice.isConnected());
    EXPECT_EQ(connSpy.count(), 1);
    EXPECT_FALSE(connSpy.takeFirst().at(0).toBool());
}

/// @brief Verify command slots dispatch to underlying transport.
TEST_F(QSightlineDeviceTest, CommandDispatchSlots)
{
    auto transport = std::make_shared<MockTransportForQt>();
    QSightlineDevice qDevice(transport);
    ASSERT_TRUE(qDevice.start());

    EXPECT_TRUE(qDevice.startTracking(0U, 320U, 240U, 64U, 48U, 0x01U));
    EXPECT_TRUE(qDevice.stopTracking(0U, 1U));
    EXPECT_TRUE(qDevice.setStabilization(0U, 1U));
    EXPECT_TRUE(qDevice.sendLensCommand(0U, 1U, 50));
    EXPECT_TRUE(qDevice.queryVersion());

    const auto sent = transport->getSentPackets();
    EXPECT_EQ(sent.size(), 5U);

    qDevice.stop();
}

/// @brief Verify incoming telemetry emits Qt signals.
TEST_F(QSightlineDeviceTest, IncomingTelemetrySignals)
{
    auto transport = std::make_shared<MockTransportForQt>();
    QSightlineDevice qDevice(transport);
    ASSERT_TRUE(qDevice.start());

    QSignalSpy warnSpy(&qDevice, &QSightlineDevice::userWarningReceived);
    QSignalSpy rawSpy(&qDevice, &QSightlineDevice::rawFrameReceived);

    // Inject UserWarning packet
    std::vector<std::uint8_t> warnPayload { 0x07U, 0x00U, 'W', 'a', 'r', 'n' };
    const auto warnPkt
        = Sightline::SightlineProtocolBuilder::buildRawPacket(Sightline::MessageId::UserWarningMessage, warnPayload);
    transport->injectData(warnPkt);

    QCoreApplication::processEvents();

    EXPECT_GE(rawSpy.count(), 1);
    EXPECT_EQ(warnSpy.count(), 1);

    qDevice.stop();
}

/// @brief Verify overlay command dispatch slots.
TEST_F(QSightlineDeviceTest, OverlayCommandSlots)
{
    auto transport = std::make_shared<MockTransportForQt>();
    QSightlineDevice qDevice(transport);
    ASSERT_TRUE(qDevice.start());

    Sightline::MsgSetOverlayMode modeMsg {};
    modeMsg.cameraIndex = 0U;
    EXPECT_TRUE(qDevice.setOverlayMode(modeMsg));
    EXPECT_TRUE(qDevice.getOverlayMode(0U));
    EXPECT_TRUE(qDevice.drawCross(0U, 1U, 100, 100, 20U));
    EXPECT_TRUE(qDevice.drawRectangle(0U, 2U, 50, 50, 100U, 80U));
    EXPECT_TRUE(qDevice.drawText(0U, 3U, 10, 10, QStringLiteral("TEST")));
    EXPECT_TRUE(
        qDevice.drawKlvField(0U, 4U, 20, 20, Sightline::KlvFieldTag::UtcTime, Sightline::KlvFormatType::TimeHms));
    EXPECT_TRUE(qDevice.drawBlackout(0U, 5U, 640U, 480U));
    EXPECT_TRUE(qDevice.destroyOverlay(0U, 1U));
    EXPECT_TRUE(qDevice.destroyAllOverlays(0U));

    Sightline::MsgLogoParameters logoMsg {};
    logoMsg.cameraIndex = 0U;
    EXPECT_TRUE(qDevice.setLogoParameters(logoMsg));
    EXPECT_TRUE(qDevice.getLogoParameters(0U));
    EXPECT_TRUE(qDevice.setUserFont(0U, QStringLiteral("font.ttf")));
    EXPECT_TRUE(qDevice.getOverlayObjectsIds(0U));
    EXPECT_TRUE(qDevice.getOverlayObjectParams(1U));

    const auto sent = transport->getSentPackets();
    EXPECT_EQ(sent.size(), 14U);

    qDevice.stop();
}

/// @brief Verify incoming overlay telemetry signals and cache updates.
TEST_F(QSightlineDeviceTest, OverlaySignalsAndCache)
{
    auto transport = std::make_shared<MockTransportForQt>();
    QSightlineDevice qDevice(transport);
    ASSERT_TRUE(qDevice.start());

    QSignalSpy modeSpy(&qDevice, &QSightlineDevice::overlayModeReceived);
    QSignalSpy logoSpy(&qDevice, &QSightlineDevice::logoParametersReceived);

    // Inject SetOverlayMode packet (0x06)
    Sightline::MsgSetOverlayMode modeMsg {};
    modeMsg.cameraIndex = 1U;
    modeMsg.graphics = 0x1010U;
    const auto modePkt = Sightline::SightlineOverlayBuilder::buildSetOverlayMode(modeMsg);
    transport->injectData(modePkt);

    // Inject LogoParameters packet (0x9B)
    Sightline::MsgLogoParameters logoMsg {};
    logoMsg.cameraIndex = 1U;
    logoMsg.logoOpacity = 128U;
    logoMsg.offsetX = 50U;
    logoMsg.offsetY = 30U;
    const auto logoPkt = Sightline::SightlineOverlayBuilder::buildSetLogoParameters(logoMsg);
    transport->injectData(logoPkt);

    QCoreApplication::processEvents();

    EXPECT_EQ(modeSpy.count(), 1);
    EXPECT_TRUE(qDevice.lastOverlayMode().has_value());
    EXPECT_EQ(qDevice.lastOverlayMode()->cameraIndex, 1U);
    EXPECT_EQ(qDevice.lastOverlayMode()->graphics, 0x1010U);

    EXPECT_EQ(logoSpy.count(), 1);
    EXPECT_TRUE(qDevice.lastLogoParameters().has_value());
    EXPECT_EQ(qDevice.lastLogoParameters()->logoOpacity, 128U);
    EXPECT_EQ(qDevice.lastLogoParameters()->offsetX, 50U);

    qDevice.stop();
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
