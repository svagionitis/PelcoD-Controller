/// @file TestSightlineDevice.cpp
/// @brief Unit tests for SightlineDevice lifecycle, command dispatch, and async telemetry.

#include "SightlineDevice.h"
#include "SightlineFraming.h"
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

        void clearSentPackets()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_sentPackets.clear();
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
            EXPECT_EQ(status.cpuLoadPercent, 35U);
            EXPECT_EQ(status.coreTempC, 45);
        });

        std::vector<std::uint8_t> statPayload {};
        // bytes 0..7: errorFlags (u64 LE = 0)
        for (std::size_t i = 0; i < 8; ++i) {
            statPayload.push_back(0U);
        }
        // bytes 8..9: temperatureF (s16 LE = 113 F)
        statPayload.push_back(113U);
        statPayload.push_back(0U);
        // bytes 10..13: load0..3 (load0 = 35%)
        statPayload.push_back(35U);
        statPayload.push_back(20U);
        statPayload.push_back(10U);
        statPayload.push_back(5U);
        // bytes 14..15: temperatureC (s16 LE = 45 C)
        statPayload.push_back(45U);
        statPayload.push_back(0U);
        // bytes 16..19: missedFrames
        for (std::size_t i = 0; i < 4; ++i) {
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

    /// @brief Verify raw traffic callback decouples execution and permits re-entrant callback registration.
    TEST(TestSightlineDevice, RawTrafficReentrancy)
    {
        auto transport = std::make_shared<MockTestTransport>();
        SightlineDevice device(transport);
        ASSERT_TRUE(device.start());

        std::atomic<int> reentrantCalls { 0 };
        device.setRawTrafficCallback([&](bool /*isTx*/, const std::vector<std::uint8_t>& /*pkt*/) {
            // Re-entrant access to m_callbackMutex inside raw traffic callback
            device.setTrackingCallback([&](const MsgTrackingPositions&) {});
            reentrantCalls.fetch_add(1);
        });

        // 1. Test TX path re-entrancy
        EXPECT_TRUE(device.saveParameters(0U));
        EXPECT_EQ(reentrantCalls.load(), 1);

        // 2. Test RX path re-entrancy
        const auto rxPkt = SightlineProtocolBuilder::buildGetVersionNumber();
        transport->injectData(rxPkt);
        EXPECT_EQ(reentrantCalls.load(), 2);

        device.stop();
    }

    /// @brief Verify overlay drawing convenience helpers and batch dispatch over transport.
    TEST(TestSightlineDevice, OverlayDrawingDispatch)
    {
        auto transport = std::make_shared<MockTestTransport>();
        SightlineDevice device(transport);
        ASSERT_TRUE(device.start());

        // 1. drawCross
        EXPECT_TRUE(device.drawCross(0U, 1U, 0, 0, 25U, OverlayPaletteColor::White, 1U, false));
        // 2. drawRectangle
        EXPECT_TRUE(device.drawRectangle(0U, 2U, 10, 20, 100U, 50U, true));
        // 3. drawText
        EXPECT_TRUE(device.drawText(0U, 3U, 10, 80, "HUD ONLINE"));
        // 4. drawKlvField
        EXPECT_TRUE(device.drawKlvField(0U, 4U, 10, 120, KlvFieldTag::UtcTime, KlvFormatType::TimeYmdHms));
        // 5. drawBlackout
        EXPECT_TRUE(device.drawBlackout(0U, 5U, 640U, 480U));
        // 6. destroyOverlay
        EXPECT_TRUE(device.destroyOverlay(0U, 5U));
        // 7. destroyAllOverlays
        EXPECT_TRUE(device.destroyAllOverlays(0U));

        const auto sent = transport->getSentPackets();
        ASSERT_EQ(sent.size(), 7U);
        for (const auto& pkt : sent) {
            EXPECT_EQ(SightlineProtocolParser::identifyMessage(pkt), MessageId::DrawOverlay);
        }

        // Verify first packet is the exact EAN Section 9.1 cross packet
        const std::vector<std::uint8_t> expectedCrossPkt { 0x51U, 0xACU, 0x13U, 0x9CU, 0x00U, 0x01U, 0x01U, 0x04U,
            0x09U, 0x00U, 0x00U, 0x00U, 0x00U, 0x19U, 0x00U, 0x00U, 0x00U, 0x0EU, 0x00U, 0x01U, 0x00U, 0x97U };
        EXPECT_EQ(sent[0U], expectedCrossPkt);

        // 8. drawOverlayBatch
        std::vector<MsgDrawOverlay> batch {};
        batch.push_back(SightlineProtocolBuilder::makeCrossOverlay(0U, 10U, 100, 100, 15U));
        batch.push_back(SightlineProtocolBuilder::makeDestroyOverlay(0U, 11U));
        EXPECT_TRUE(device.drawOverlayBatch(batch));

        const auto sentAfterBatch = transport->getSentPackets();
        EXPECT_EQ(sentAfterBatch.size(), 8U);

        device.stop();
    }

    /// @brief Verify overlay queries, fonts, and logo parameters dispatch over transport.
    TEST(TestSightlineDevice, OverlayQueriesDispatch)
    {
        auto transport = std::make_shared<MockTestTransport>();
        SightlineDevice device(transport);
        ASSERT_TRUE(device.start());

        EXPECT_TRUE(device.getOverlayMode(0U));
        EXPECT_TRUE(device.getLogoParameters(0U));
        EXPECT_TRUE(device.getOverlayObjectsIds(0U));
        EXPECT_TRUE(device.getOverlayObjectParams(42U));
        EXPECT_TRUE(device.setUserFont(1U, "/fonts/Roboto.ttf"));

        MsgLogoParameters logoMsg {};
        logoMsg.cameraIndex = 0U;
        logoMsg.logoOpacity = 128U;
        logoMsg.offsetX = 20U;
        logoMsg.offsetY = 30U;
        EXPECT_TRUE(device.setLogoParameters(logoMsg));

        const auto sent = transport->getSentPackets();
        ASSERT_EQ(sent.size(), 6U);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(sent[0U]), MessageId::GetOverlayMode);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(sent[1U]), MessageId::GetParameters);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(sent[2U]), MessageId::GetParameters);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(sent[3U]), MessageId::GetParameters);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(sent[4U]), MessageId::UserFont);
        EXPECT_EQ(SightlineProtocolParser::identifyMessage(sent[5U]), MessageId::LogoParameters);

        device.stop();
    }

    /// @brief Verify asynchronous overlay telemetry dispatch and cache updates.
    TEST(TestSightlineDevice, OverlayTelemetryCallback)
    {
        auto transport = std::make_shared<MockTestTransport>();
        SightlineDevice device(transport);
        ASSERT_TRUE(device.start());

        std::atomic<bool> modeFired { false };
        std::atomic<bool> idsFired { false };
        std::atomic<bool> paramsFired { false };
        std::atomic<bool> logoFired { false };

        device.setOverlayCallback([&](const MsgSetOverlayMode& mode) {
            EXPECT_EQ(mode.cameraIndex, 0U);
            EXPECT_EQ(mode.primaryReticle, 0x11U);
            modeFired.store(true);
        });

        device.setObjectsIdsCallback([&](const MsgCurrentOverlayObjectsIds& ids) {
            EXPECT_TRUE(ids.isObjectActive(1U));
            idsFired.store(true);
        });

        device.setObjectParamsCallback([&](const MsgCurrentOverlayObjectParameters& params) {
            EXPECT_EQ(params.objectId, 7U);
            paramsFired.store(true);
        });

        device.setLogoCallback([&](const MsgLogoParameters& logo) {
            EXPECT_EQ(logo.logoOpacity, 200U);
            logoFired.store(true);
        });

        // 1. Inject CurrentOverlayMode (0x42)
        const auto modePkt = SightlineProtocolBuilder::buildRawPacket(MessageId::CurrentOverlayMode,
            std::vector<std::uint8_t> { 0x11U, 0x01U, 0x10U, 0x10U, 0x08U, 0x07U, 0x00U });
        transport->injectData(modePkt);
        EXPECT_TRUE(modeFired.load());
        EXPECT_TRUE(device.lastOverlayMode().has_value());

        // 2. Inject CurrentOverlayObjectsIds (0x68)
        std::vector<std::uint8_t> idsPayload(32U, 0U);
        idsPayload[0U] = 0x02U; // Object 1 active
        const auto idsPkt = SightlineProtocolBuilder::buildRawPacket(MessageId::CurrentOverlayObjectsIds, idsPayload);
        transport->injectData(idsPkt);
        EXPECT_TRUE(idsFired.load());
        EXPECT_TRUE(device.lastOverlayObjectsIds().has_value());
        EXPECT_TRUE(device.lastOverlayObjectsIds()->isObjectActive(1U));

        // 3. Inject CurrentOverlayObjectParameters (0x6B)
        std::vector<std::uint8_t> paramPayload {};
        paramPayload.push_back(1U); // type Rectangle
        paramPayload.push_back(7U); // objectId 7
        paramPayload.push_back(4U); // flags
        paramPayload.push_back(1U); // staticObject
        SightlineFraming::appendU16Le(paramPayload, 10U);
        SightlineFraming::appendU16Le(paramPayload, 20U);
        SightlineFraming::appendU16Le(paramPayload, 100U);
        SightlineFraming::appendU16Le(paramPayload, 50U);
        paramPayload.push_back(0x0EU); // color
        const auto paramPkt
            = SightlineProtocolBuilder::buildRawPacket(MessageId::CurrentOverlayObjectParameters, paramPayload);
        transport->injectData(paramPkt);
        EXPECT_TRUE(paramsFired.load());

        // 4. Inject LogoParameters (0x9B)
        MsgLogoParameters logoMsg {};
        logoMsg.cameraIndex = 0U;
        logoMsg.logoOpacity = 200U;
        logoMsg.offsetX = 50U;
        logoMsg.offsetY = 60U;
        const auto logoPkt = SightlineProtocolBuilder::buildSetLogoParameters(logoMsg);
        transport->injectData(logoPkt);
        EXPECT_TRUE(logoFired.load());
        EXPECT_TRUE(device.lastLogoParameters().has_value());
        EXPECT_EQ(device.lastLogoParameters()->logoOpacity, 200U);

        device.stop();
    }

    /// @brief Verify detection, ROI, VMTI, snapshot, and classifier command dispatches.
    TEST(TestSightlineDevice, DetectionCommandDispatch)
    {
        auto transport = std::make_shared<MockTestTransport>();
        SightlineDevice device(transport);
        ASSERT_TRUE(device.start());

        MsgSetDetectionParameters detMsg {};
        detMsg.cameraIndex = 0U;
        detMsg.mode = DetectionMode::Maritime;
        EXPECT_TRUE(device.setDetection(detMsg));

        MsgAdvancedDetectionParameters advMsg {};
        advMsg.cameraIndex = 0U;
        EXPECT_TRUE(device.setAdvancedDetection(advMsg));

        MsgDetectionROI roiMsg {};
        roiMsg.cameraIndex = 0U;
        EXPECT_TRUE(device.setDetectionROI(roiMsg));

        MsgSetVMTI vmtiMsg {};
        vmtiMsg.cameraIndex = 0U;
        EXPECT_TRUE(device.setVMTI(vmtiMsg));

        EXPECT_TRUE(device.triggerDetectionSnapshot(0U, 1U));

        MsgKlvMetricFilters metricMsg {};
        metricMsg.cameraIndex = 0U;
        EXPECT_TRUE(device.setKlvMetricFilters(metricMsg));

        MsgClassifierConfig classMsg {};
        classMsg.cameraIndex = 0U;
        EXPECT_TRUE(device.setClassifierConfig(classMsg));

        EXPECT_TRUE(device.setComputeResources(true, true));

        EXPECT_TRUE(device.queryDetectionParams(0U, 0U));
        EXPECT_TRUE(device.queryAdvDetection(0U));
        EXPECT_TRUE(device.queryDetectionROI(0U, 0U));
        EXPECT_TRUE(device.queryVMTI(0U));
        EXPECT_TRUE(device.queryTrackingPixelStats(0U, 1U));

        const auto sent = transport->getSentPackets();
        EXPECT_GE(sent.size(), 14U);

        device.stop();
    }

    /// @brief Verify inbound detection telemetry callbacks and cached state getters.
    TEST(TestSightlineDevice, DetectionTelemetryCallbacksAndCache)
    {
        auto transport = std::make_shared<MockTestTransport>();
        SightlineDevice device(transport);
        ASSERT_TRUE(device.start());

        std::atomic<bool> detFired { false };
        std::atomic<bool> advFired { false };
        std::atomic<bool> roiFired { false };
        std::atomic<bool> metricFired { false };

        device.setDetectionCallback([&](const MsgSetDetectionParameters& det) {
            EXPECT_EQ(det.cameraIndex, 0U);
            EXPECT_EQ(det.mode, DetectionMode::Maritime);
            detFired.store(true);
        });

        device.setAdvDetectionCallback([&](const MsgAdvancedDetectionParameters& adv) {
            EXPECT_EQ(adv.cameraIndex, 0U);
            EXPECT_EQ(adv.updateRate, 55U);
            advFired.store(true);
        });

        device.setDetectionRoiCallback([&](const MsgDetectionROI& roi) {
            EXPECT_EQ(roi.cameraIndex, 0U);
            EXPECT_EQ(roi.lineLeftX, 120U);
            roiFired.store(true);
        });

        device.setKlvMetricFiltersCb([&](const MsgKlvMetricFilters& filters) {
            EXPECT_EQ(filters.cameraIndex, 1U);
            EXPECT_NEAR(filters.minTargetWidthM, 1.2F, 1e-4F);
            metricFired.store(true);
        });

        // 1. Inject CurrentDetectionParameters (0x54)
        MsgSetDetectionParameters detMsg {};
        detMsg.cameraIndex = 0U;
        detMsg.mode = DetectionMode::Maritime;
        const auto detPkt = SightlineProtocolBuilder::buildSetDetectionParams(detMsg);
        auto detPayload = SightlineFraming::extractPayload(detPkt);
        const auto curDetPkt = SightlineFraming::buildPacket(MessageId::CurrentDetectionParameters, detPayload);
        transport->injectData(curDetPkt);
        EXPECT_TRUE(detFired.load());
        EXPECT_TRUE(device.lastDetectionParams().has_value());
        EXPECT_EQ(device.lastDetectionParams()->mode, DetectionMode::Maritime);

        // 2. Inject CurrentAdvancedDetectionParameters (0x77)
        MsgAdvancedDetectionParameters advMsg {};
        advMsg.cameraIndex = 0U;
        advMsg.updateRate = 55U;
        const auto advPkt = SightlineProtocolBuilder::buildSetAdvDetectionParams(advMsg);
        auto advPayload = SightlineFraming::extractPayload(advPkt);
        const auto curAdvPkt = SightlineFraming::buildPacket(MessageId::CurrentAdvancedDetectionParameters, advPayload);
        transport->injectData(curAdvPkt);
        EXPECT_TRUE(advFired.load());
        EXPECT_TRUE(device.lastAdvDetection().has_value());
        EXPECT_EQ(device.lastAdvDetection()->updateRate, 55U);

        // 3. Inject CurrentDetectionRegionOfInterestParameters (0x7D)
        MsgDetectionROI roiMsg {};
        roiMsg.cameraIndex = 0U;
        roiMsg.lineLeftX = 120U;
        const auto roiPkt = SightlineProtocolBuilder::buildSetDetectionROI(roiMsg);
        auto roiPayload = SightlineFraming::extractPayload(roiPkt);
        const auto curRoiPkt
            = SightlineFraming::buildPacket(MessageId::CurrentDetectionRegionOfInterestParameters, roiPayload);
        transport->injectData(curRoiPkt);
        EXPECT_TRUE(roiFired.load());
        EXPECT_TRUE(device.lastDetectionROI().has_value());
        EXPECT_EQ(device.lastDetectionROI()->lineLeftX, 120U);

        // 4. Inject KlvClassFilters (0xC1)
        MsgKlvMetricFilters metricMsg {};
        metricMsg.cameraIndex = 1U;
        metricMsg.minTargetWidthM = 1.2F;
        const auto klvPkt = SightlineProtocolBuilder::buildSetKlvMetricFilters(metricMsg);
        transport->injectData(klvPkt);
        EXPECT_TRUE(metricFired.load());
        EXPECT_TRUE(device.lastKlvMetricFilters().has_value());
        EXPECT_NEAR(device.lastKlvMetricFilters()->minTargetWidthM, 1.2F, 1e-4F);

        device.stop();
    }

    /// @brief Verify Phase 2 tracking command dispatch through SightlineDevice.
    TEST(TestSightlineDevice, Phase2TrackingCommandDispatch)
    {
        auto transport = std::make_shared<MockTestTransport>();
        SightlineDevice device(transport);
        ASSERT_TRUE(device.start());

        // 1. startPrecisionTrack
        transport->clearSentPackets();
        EXPECT_TRUE(device.startPrecisionTrack(0U, 320U, 240U, 64U, 48U, 123456ULL));
        auto sent = transport->getSentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_EQ(SightlineFraming::identifyMessage(sent[0]), MessageId::StartTracking);
        EXPECT_EQ(SightlineFraming::extractPayload(sent[0]).size(), 21U);

        // 2. setForcedCoast
        transport->clearSentPackets();
        EXPECT_TRUE(device.setForcedCoast(0U, 1U, ForcedCoastingMode::FreezeUpdates));
        sent = transport->getSentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_EQ(SightlineFraming::identifyMessage(sent[0]), MessageId::ModifyTrackIndex);
        const auto coastPayload = SightlineFraming::extractPayload(sent[0]);
        EXPECT_EQ(coastPayload[0], 1U);
        EXPECT_EQ(coastPayload[1], static_cast<std::uint8_t>(TrackIndexAction::CoastFreezeUpdates));

        // 3. reinitTrack
        transport->clearSentPackets();
        EXPECT_TRUE(device.reinitTrack(0U, 2U));
        sent = transport->getSentPackets();
        ASSERT_EQ(sent.size(), 1U);
        const auto reinitPayload = SightlineFraming::extractPayload(sent[0]);
        EXPECT_EQ(reinitPayload[0], 2U);
        EXPECT_EQ(reinitPayload[1], static_cast<std::uint8_t>(TrackIndexAction::Reinitialize));

        // 4. resizeTrack
        transport->clearSentPackets();
        EXPECT_TRUE(device.resizeTrack(0U, 3U, 80U, 60U, true));
        sent = transport->getSentPackets();
        ASSERT_EQ(sent.size(), 1U);
        const auto resizePayload = SightlineFraming::extractPayload(sent[0]);
        EXPECT_EQ(resizePayload[0], 3U);
        EXPECT_EQ(resizePayload[1], static_cast<std::uint8_t>(TrackIndexAction::ResizeWithAcquisitionAssist));

        // 5. cueTrackAt
        transport->clearSentPackets();
        EXPECT_TRUE(device.cueTrackAt(0U, 400U, 250U, ModifyMode::DesignateNearAsPrimary, 5U));
        sent = transport->getSentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_EQ(SightlineFraming::identifyMessage(sent[0]), MessageId::ModifyTracking);
        const auto cuePayload = SightlineFraming::extractPayload(sent[0]);
        ASSERT_EQ(cuePayload.size(), 10U);
        EXPECT_EQ(cuePayload[8], 5U); // trackId
        EXPECT_EQ(cuePayload[9], static_cast<std::uint8_t>(ModifyMode::DesignateNearAsPrimary));

        // 6. nudgeDisplayTrack
        transport->clearSentPackets();
        EXPECT_TRUE(device.nudgeDisplayTrack(0U, -5, 10));
        sent = transport->getSentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_EQ(SightlineFraming::identifyMessage(sent[0]), MessageId::NudgeTrackingCoordinate);
        const auto nudgePayload = SightlineFraming::extractPayload(sent[0]);
        EXPECT_EQ(nudgePayload[2], 1U); // rotate = 1 (DisplayCoordinates)

        device.stop();
    }

} // namespace
} // namespace Sightline
