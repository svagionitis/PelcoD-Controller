#include "Nmea/FlirPfecDevice.h"
#include "Nmea/NmeaChecksum.h"
#include "Transport/BaseTransport.h"
#include "adapters/FlirPfecPtzAdapter.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace PayloadHal {
namespace {

    class MockPfecTransport : public Transport::BaseTransport {
    public:
        MockPfecTransport() = default;
        ~MockPfecTransport() override = default;

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
            std::lock_guard<std::mutex> lock(m_txMutex);
            m_sentPackets.push_back(std::string(data.begin(), data.end()));
            return true;
        }

        void injectString(std::string_view text)
        {
            const std::vector<std::uint8_t> data(text.begin(), text.end());
            invokeDataCallback(data);
        }

        std::vector<std::string> sentPackets() const
        {
            std::lock_guard<std::mutex> lock(m_txMutex);
            return m_sentPackets;
        }

        void clearSentPackets()
        {
            std::lock_guard<std::mutex> lock(m_txMutex);
            m_sentPackets.clear();
        }

    private:
        std::atomic<bool> m_isOpen { false };
        mutable std::mutex m_txMutex {};
        std::vector<std::string> m_sentPackets {};
    };

    TEST(TestFlirPfecPtzAdapter, LifecycleAndDeviceState)
    {
        auto transport = std::make_shared<MockPfecTransport>();
        auto flirDevice = std::make_shared<Nmea::FlirPfecDevice>(transport);
        FlirPfecPtzAdapter adapter(flirDevice);

        EXPECT_FALSE(adapter.isConnected());
        EXPECT_EQ(adapter.state(), DeviceState::Disconnected);

        ASSERT_TRUE(adapter.connect());
        EXPECT_TRUE(adapter.isConnected());
        EXPECT_EQ(adapter.state(), DeviceState::Ready);

        const auto info = adapter.info();
        EXPECT_EQ(info.manufacturer, "FLIR");
        EXPECT_NE(info.model.find("FLIR"), std::string::npos);

        adapter.disconnect();
        EXPECT_FALSE(adapter.isConnected());
        EXPECT_EQ(adapter.state(), DeviceState::Disconnected);
    }

    TEST(TestFlirPfecPtzAdapter, MotionAndAngles)
    {
        auto transport = std::make_shared<MockPfecTransport>();
        auto flirDevice = std::make_shared<Nmea::FlirPfecDevice>(transport);
        FlirPfecPtzAdapter adapter(flirDevice);
        ASSERT_TRUE(adapter.connect());

        // Velocity
        EXPECT_TRUE(adapter.setNormalizedVelocity(0.5f, -0.25f));
        auto sent = transport->sentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_NE(sent[0].find("$PFEC,GPcmd,p,50,-25"), std::string::npos);

        // Rate
        transport->clearSentPackets();
        EXPECT_TRUE(adapter.setRate(30.0, -15.0)); // 30/60 = 0.5, -15/30 = -0.5
        sent = transport->sentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_NE(sent[0].find("$PFEC,GPcmd,p,50,-50"), std::string::npos);

        // Absolute Angles
        transport->clearSentPackets();
        EXPECT_TRUE(adapter.setAbsoluteAngles(120.0, -15.0));
        sent = transport->sentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_NE(sent[0].find("$PFEC,GPcmd,a,120.0,-15.0"), std::string::npos);

        // Stop
        transport->clearSentPackets();
        EXPECT_TRUE(adapter.stopMotion());
        sent = transport->sentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_NE(sent[0].find("$PFEC,GPcmd,p,0,0"), std::string::npos);
    }

    TEST(TestFlirPfecPtzAdapter, PresetsAndLimits)
    {
        auto transport = std::make_shared<MockPfecTransport>();
        auto flirDevice = std::make_shared<Nmea::FlirPfecDevice>(transport);
        FlirPfecPtzAdapter adapter(flirDevice);
        ASSERT_TRUE(adapter.connect());

        double minPan { 0.0 };
        double maxPan { 0.0 };
        double minTilt { 0.0 };
        double maxTilt { 0.0 };
        EXPECT_TRUE(adapter.getLimits(minPan, maxPan, minTilt, maxTilt));
        EXPECT_EQ(minPan, -180.0);
        EXPECT_EQ(maxPan, 180.0);
        EXPECT_EQ(minTilt, -90.0);
        EXPECT_EQ(maxTilt, 90.0);

        EXPECT_TRUE(adapter.savePreset(4U));
        auto sent = transport->sentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_NE(sent[0].find("$PFEC,GPcmd,s,4"), std::string::npos);

        transport->clearSentPackets();
        EXPECT_TRUE(adapter.recallPreset(4U));
        sent = transport->sentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_NE(sent[0].find("$PFEC,GPcmd,g,4"), std::string::npos);
    }

    TEST(TestFlirPfecPtzAdapter, TelemetryCallback)
    {
        auto transport = std::make_shared<MockPfecTransport>();
        auto flirDevice = std::make_shared<Nmea::FlirPfecDevice>(transport);
        FlirPfecPtzAdapter adapter(flirDevice);
        ASSERT_TRUE(adapter.connect());

        std::atomic<bool> callbackFired { false };
        GimbalTelemetry lastTelem {};
        adapter.registerTelemetryCallback([&](const GimbalTelemetry& telem) {
            callbackFired = true;
            lastTelem = telem;
        });

        const std::string posSentence = Nmea::NmeaChecksum::frameSentence("PFEC,GPpos,145.2,-10.5");
        transport->injectString(posSentence);

        EXPECT_TRUE(callbackFired.load());
        EXPECT_NEAR(lastTelem.panAngleDeg, 145.2, 1e-1);
        EXPECT_NEAR(lastTelem.tiltAngleDeg, -10.5, 1e-1);

        const auto curr = adapter.currentTelemetry();
        EXPECT_NEAR(curr.panAngleDeg, 145.2, 1e-1);
        EXPECT_NEAR(curr.tiltAngleDeg, -10.5, 1e-1);
    }

} // namespace
} // namespace PayloadHal
