/// @file TestSightlineDevice.cpp
/// @brief Unit tests for SightlineDevice lifecycle, command dispatch, and async telemetry.

#include "SightlineDevice.h"
#include "SightlineProtocolBuilder.h"
#include "Transport/BaseTransport.h"

#include <gtest/gtest.h>

#include <atomic>
#include <mutex>
#include <vector>

namespace Sightline {
namespace {

    class MockTestTransport : public Transport::BaseTransport {
    public:
        MockTestTransport() = default;
        ~MockTestTransport() override = default;

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
            m_sentPackets.push_back(data);
            return true;
        }

        void injectData(const std::vector<std::uint8_t>& data)
        {
            invokeDataCallback(data);
        }

        std::vector<std::vector<std::uint8_t>> getSentPackets() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_sentPackets;
        }

    private:
        mutable std::mutex m_mutex;
        bool m_isOpen { false };
        std::vector<std::vector<std::uint8_t>> m_sentPackets {};
    };

    /// @brief Verify lifecycle start, stop, and connection state.
    TEST(TestSightlineDevice, LifecycleStartStop)
    {
        auto transport = std::make_shared<MockTestTransport>();
        SightlineDevice device(transport);

        EXPECT_FALSE(device.isConnected());
        EXPECT_TRUE(device.start());
        EXPECT_TRUE(device.isConnected());

        device.stop();
        EXPECT_FALSE(device.isConnected());
    }

    /// @brief Verify start tracking command dispatch over transport.
    TEST(TestSightlineDevice, StartTrackingDispatch)
    {
        auto transport = std::make_shared<MockTestTransport>();
        SightlineDevice device(transport);
        ASSERT_TRUE(device.start());

        EXPECT_TRUE(device.startTracking(0U, 320U, 240U, 64U, 48U, 0x01U));

        const auto sent = transport->getSentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(sent[0]), MessageId::StartTracking);

        device.stop();
    }

    /// @brief Verify asynchronous tracking coordinate telemetry dispatch to callbacks.
    TEST(TestSightlineDevice, TrackingTelemetryCallback)
    {
        auto transport = std::make_shared<MockTestTransport>();
        SightlineDevice device(transport);
        ASSERT_TRUE(device.start());

        std::atomic<bool> callbackFired { false };
        std::atomic<std::uint8_t> receivedCamera { 0xFFU };

        device.setTrackingCallback([&](const MsgTrackingPositions& trk) {
            callbackFired = true;
            receivedCamera = trk.cameraIndex;
        });

        // Synthesize mock TrackingPositions frame
        std::vector<std::uint8_t> payload {};
        payload.push_back(1U); // cameraIndex = 1
        // timestamp 8 bytes
        for (std::size_t i = 0; i < 8; ++i) {
            payload.push_back(0U);
        }
        // frame number 4 bytes
        for (std::size_t i = 0; i < 4; ++i) {
            payload.push_back(0U);
        }
        // numTracks = 0
        payload.push_back(0U);

        const auto pkt = SightlineProtocolBuilder::buildRawPacket(MessageId::TrackingPositions, payload);
        transport->injectData(pkt);

        EXPECT_TRUE(callbackFired.load());
        EXPECT_EQ(receivedCamera.load(), 1U);
        EXPECT_TRUE(device.lastTrackingPositions().has_value());

        device.stop();
    }

    /// @brief Verify user warning notifications dispatch to callback.
    TEST(TestSightlineDevice, UserWarningCallback)
    {
        auto transport = std::make_shared<MockTestTransport>();
        SightlineDevice device(transport);
        ASSERT_TRUE(device.start());

        std::atomic<std::uint16_t> warningCode { 0U };

        device.setWarningCallback([&](const MsgUserWarningMessage& warn) { warningCode = warn.warningCode; });

        std::vector<std::uint8_t> payload { 0x0A, 0x00, 'T', 'e', 's', 't' };
        const auto pkt = SightlineProtocolBuilder::buildRawPacket(MessageId::UserWarningMessage, payload);
        transport->injectData(pkt);

        EXPECT_EQ(warningCode.load(), 10U);

        device.stop();
    }

    /// @brief Verify extended positions and system status callbacks.
    TEST(TestSightlineDevice, ExtendedPositionsAndStatusCallbacks)
    {
        auto transport = std::make_shared<MockTestTransport>();
        SightlineDevice device(transport);
        ASSERT_TRUE(device.start());

        std::atomic<bool> extFired { false };
        device.setExtendedPositionsCallback([&](const MsgTrackingPositionsExtended& ext) {
            extFired = true;
            EXPECT_EQ(ext.cameraIndex, 0U);
        });

        std::vector<std::uint8_t> extPayload {};
        extPayload.push_back(0U); // cameraIndex
        for (std::size_t i = 0; i < 8; ++i) {
            extPayload.push_back(0U);
        }
        for (std::size_t i = 0; i < 4; ++i) {
            extPayload.push_back(0U);
        }
        extPayload.push_back(0U); // 0 tracks
        const auto extPkt = SightlineProtocolBuilder::buildRawPacket(MessageId::TrackingPositionsExtended, extPayload);
        transport->injectData(extPkt);
        EXPECT_TRUE(extFired.load());

        std::atomic<bool> statusFired { false };
        device.setSystemStatusCallback([&](const MsgSystemStatusMessage& status) {
            statusFired = true;
            EXPECT_EQ(status.cpuLoadPercent, 350U);
        });

        std::vector<std::uint8_t> statPayload {};
        statPayload.push_back(0x5EU);
        statPayload.push_back(0x01U); // 350
        statPayload.push_back(45U); // 45 C
        statPayload.push_back(0U);
        for (std::size_t i = 0; i < 8; ++i) {
            statPayload.push_back(0U);
        }
        const auto statPkt = SightlineProtocolBuilder::buildRawPacket(MessageId::SystemStatusMessage, statPayload);
        transport->injectData(statPkt);
        EXPECT_TRUE(statusFired.load());
        EXPECT_TRUE(device.lastSystemStatus().has_value());

        device.stop();
    }

    /// @brief Verify command dispatch for stabilization, lens, overlay and maintenance.
    TEST(TestSightlineDevice, MultiDomainCommandDispatch)
    {
        auto transport = std::make_shared<MockTestTransport>();
        SightlineDevice device(transport);
        ASSERT_TRUE(device.start());

        EXPECT_TRUE(device.setStabilization(0U, 1U, 1U, 64U));
        EXPECT_TRUE(device.sendLensCommand(0U, 1U, 100));
        EXPECT_TRUE(device.saveParameters(0U));
        EXPECT_TRUE(device.queryVersion());

        MsgSetOverlayMode mode {};
        EXPECT_TRUE(device.setOverlayMode(mode));

        const auto sent = transport->getSentPackets();
        EXPECT_EQ(sent.size(), 5U);

        device.stop();
    }

} // namespace
} // namespace Sightline
