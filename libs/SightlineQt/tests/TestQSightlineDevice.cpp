/// @file TestQSightlineDevice.cpp
/// @brief Unit tests for QSightlineDevice Qt signal/slot dispatch and lifecycle.

#include "QSightlineDevice.h"
#include <SightlineCore/SightlineProtocolBuilder.h>
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

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
