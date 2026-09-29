#include "FlirPfecDevice.h"
#include "NmeaChecksum.h"
#include "Transport/BaseTransport.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Nmea {
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

    TEST(TestFlirPfecDevice, LifecycleAndConnection)
    {
        auto transport = std::make_shared<MockPfecTransport>();
        FlirPfecDevice camera(transport);

        EXPECT_FALSE(camera.isConnected());
        EXPECT_TRUE(camera.start(std::chrono::milliseconds(0)));
        EXPECT_TRUE(camera.isConnected());

        camera.stop();
        EXPECT_FALSE(camera.isConnected());
    }

    TEST(TestFlirPfecDevice, VelocityCommandFraming)
    {
        auto transport = std::make_shared<MockPfecTransport>();
        FlirPfecDevice camera(transport);
        ASSERT_TRUE(camera.start(std::chrono::milliseconds(0)));

        EXPECT_TRUE(camera.setVelocity(0.5f, -0.25f));
        auto sent = transport->sentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_TRUE(NmeaChecksum::validate(sent[0]));
        EXPECT_NE(sent[0].find("$PFEC,GPcmd,p,50,-25"), std::string::npos);

        transport->clearSentPackets();
        EXPECT_TRUE(camera.stopMotion());
        sent = transport->sentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_TRUE(NmeaChecksum::validate(sent[0]));
        EXPECT_NE(sent[0].find("$PFEC,GPcmd,p,0,0"), std::string::npos);
    }

    TEST(TestFlirPfecDevice, AbsoluteAngleSlewFraming)
    {
        auto transport = std::make_shared<MockPfecTransport>();
        FlirPfecDevice camera(transport);
        ASSERT_TRUE(camera.start(std::chrono::milliseconds(0)));

        EXPECT_TRUE(camera.setAbsoluteAngles(145.2, -12.4));
        auto sent = transport->sentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_TRUE(NmeaChecksum::validate(sent[0]));
        EXPECT_NE(sent[0].find("$PFEC,GPcmd,a,145.2,-12.4"), std::string::npos);

        // Pan normalization & Tilt clamping
        transport->clearSentPackets();
        EXPECT_TRUE(camera.setAbsoluteAngles(-10.0, 110.0));
        sent = transport->sentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_NE(sent[0].find("$PFEC,GPcmd,a,350.0,90.0"), std::string::npos);
    }

    TEST(TestFlirPfecDevice, PresetCommands)
    {
        auto transport = std::make_shared<MockPfecTransport>();
        FlirPfecDevice camera(transport);
        ASSERT_TRUE(camera.start(std::chrono::milliseconds(0)));

        EXPECT_TRUE(camera.savePreset(5U));
        auto sent = transport->sentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_TRUE(NmeaChecksum::validate(sent[0]));
        EXPECT_NE(sent[0].find("$PFEC,GPcmd,s,5"), std::string::npos);

        transport->clearSentPackets();
        EXPECT_TRUE(camera.recallPreset(5U));
        sent = transport->sentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_TRUE(NmeaChecksum::validate(sent[0]));
        EXPECT_NE(sent[0].find("$PFEC,GPcmd,g,5"), std::string::npos);
    }

    TEST(TestFlirPfecDevice, OpticsAndSensorControls)
    {
        auto transport = std::make_shared<MockPfecTransport>();
        FlirPfecDevice camera(transport);
        ASSERT_TRUE(camera.start(std::chrono::milliseconds(0)));

        // Zoom
        EXPECT_TRUE(camera.setZoomRate(1.0f));
        auto sent = transport->sentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_TRUE(NmeaChecksum::validate(sent[0]));
        EXPECT_NE(sent[0].find("$PFEC,GPcmd,z,100"), std::string::npos);

        // Sensor Select
        transport->clearSentPackets();
        EXPECT_TRUE(camera.selectSensor(FlirSensorType::ThermalInfrared));
        sent = transport->sentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_NE(sent[0].find("$PFEC,GPcam,c,ir"), std::string::npos);

        // Color Palette
        transport->clearSentPackets();
        EXPECT_TRUE(camera.setColorPalette(FlirColorPalette::Ironbow));
        sent = transport->sentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_NE(sent[0].find("$PFEC,GPcam,p,3"), std::string::npos);

        // NUC
        transport->clearSentPackets();
        EXPECT_TRUE(camera.triggerNuc());
        sent = transport->sentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_NE(sent[0].find("$PFEC,GPcam,nuc"), std::string::npos);
    }

    TEST(TestFlirPfecDevice, GimbalPositionTelemetryReport)
    {
        auto transport = std::make_shared<MockPfecTransport>();
        FlirPfecDevice camera(transport);
        ASSERT_TRUE(camera.start(std::chrono::milliseconds(0)));

        std::atomic<bool> positionCallbackFired { false };
        PfecGimbalPosition reportedPos {};
        const auto subId = camera.addPositionCallback([&](const PfecGimbalPosition& pos) {
            positionCallbackFired = true;
            reportedPos = pos;
        });

        // Frame $PFEC,GPpos,180.5,-15.2 with valid checksum
        const std::string posSentence = NmeaChecksum::frameSentence("PFEC,GPpos,180.5,-15.2");
        transport->injectString(posSentence);

        EXPECT_TRUE(positionCallbackFired.load());
        EXPECT_TRUE(reportedPos.valid);
        EXPECT_NEAR(reportedPos.panDegrees, 180.5, 1e-1);
        EXPECT_NEAR(reportedPos.tiltDegrees, -15.2, 1e-1);

        const auto currPos = camera.currentPosition();
        EXPECT_TRUE(currPos.valid);
        EXPECT_NEAR(currPos.panDegrees, 180.5, 1e-1);
        EXPECT_NEAR(currPos.tiltDegrees, -15.2, 1e-1);

        camera.removePositionCallback(subId);
    }

    TEST(TestFlirPfecDevice, BackgroundTelemetryPolling)
    {
        auto transport = std::make_shared<MockPfecTransport>();
        FlirPfecDevice camera(transport);

        // Start with fast 10ms polling interval
        ASSERT_TRUE(camera.start(std::chrono::milliseconds(10)));

        // Sleep to let polling thread dispatch queries
        std::this_thread::sleep_for(std::chrono::milliseconds(45));
        camera.stop();

        const auto sent = transport->sentPackets();
        EXPECT_GE(sent.size(), 2U);
        for (const auto& pkt : sent) {
            EXPECT_NE(pkt.find("$PFEC,GPpos"), std::string::npos);
            EXPECT_TRUE(NmeaChecksum::validate(pkt));
        }
    }

    TEST(TestFlirPfecDevice, ExtendedCameraControls)
    {
        auto transport = std::make_shared<MockPfecTransport>();
        FlirPfecDevice camera(transport);
        ASSERT_TRUE(camera.start(std::chrono::milliseconds(0)));

        // Digital zoom
        EXPECT_EQ(camera.digitalZoom(), FlirZoomLevel::Zoom1x);
        EXPECT_TRUE(camera.setDigitalZoom(FlirZoomLevel::Zoom4x));
        EXPECT_EQ(camera.digitalZoom(), FlirZoomLevel::Zoom4x);

        auto sent = transport->sentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_NE(sent[0].find("$PFEC,GPcam,z,4.0"), std::string::npos);

        // Gyro stabilization
        transport->clearSentPackets();
        EXPECT_FALSE(camera.isStabilized());
        EXPECT_TRUE(camera.setStabilization(true));
        EXPECT_TRUE(camera.isStabilized());

        sent = transport->sentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_NE(sent[0].find("$PFEC,GPcam,s,on"), std::string::npos);

        // Palette state
        EXPECT_TRUE(camera.setColorPalette(FlirColorPalette::Ironbow));
        EXPECT_EQ(camera.colorPalette(), FlirColorPalette::Ironbow);
    }

} // namespace
} // namespace Nmea
