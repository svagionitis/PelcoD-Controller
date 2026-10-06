/// @file TestQSightlineDevice.cpp
/// @brief Unit tests for QSightlineDevice Qt signal/slot dispatch and lifecycle.

#include "QSightlineDevice.h"
#include <SightlineCore/SightlineProtocolBuilder.h>
#include <SightlineCore/modules/SightlineBlendingBuilder.h>
#include <SightlineCore/modules/SightlineNucBuilder.h>
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

    void clearSentPackets()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sent.clear();
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

/// @brief Verify detection, ROI, snapshot, and classifier slot dispatch in QSightlineDevice.
TEST_F(QSightlineDeviceTest, DetectionCommandSlots)
{
    auto transport = std::make_shared<MockTransportForQt>();
    QSightlineDevice qDevice(transport);
    ASSERT_TRUE(qDevice.start());

    Sightline::MsgSetDetectionParameters detMsg {};
    detMsg.cameraIndex = 0U;
    detMsg.mode = Sightline::DetectionMode::Maritime;
    EXPECT_TRUE(qDevice.setDetection(detMsg));

    Sightline::MsgAdvancedDetectionParameters advMsg {};
    advMsg.cameraIndex = 0U;
    EXPECT_TRUE(qDevice.setAdvancedDetection(advMsg));

    Sightline::MsgDetectionROI roiMsg {};
    roiMsg.cameraIndex = 0U;
    EXPECT_TRUE(qDevice.setDetectionROI(roiMsg));

    Sightline::MsgSetVMTI vmtiMsg {};
    vmtiMsg.cameraIndex = 0U;
    EXPECT_TRUE(qDevice.setVMTI(vmtiMsg));

    EXPECT_TRUE(qDevice.triggerDetectionSnapshot(0U, 1U));

    Sightline::MsgKlvMetricFilters metricMsg {};
    metricMsg.cameraIndex = 0U;
    EXPECT_TRUE(qDevice.setKlvMetricFilters(metricMsg));

    Sightline::MsgClassifierConfig classMsg {};
    classMsg.cameraIndex = 0U;
    EXPECT_TRUE(qDevice.setClassifierConfig(classMsg));

    EXPECT_TRUE(qDevice.setComputeResources(true, true));

    EXPECT_TRUE(qDevice.queryDetectionParams(0U, 0U));
    EXPECT_TRUE(qDevice.queryAdvDetection(0U));
    EXPECT_TRUE(qDevice.queryDetectionROI(0U, 0U));
    EXPECT_TRUE(qDevice.queryVMTI(0U));
    EXPECT_TRUE(qDevice.queryTrackingPixelStats(0U, 1U));
    EXPECT_TRUE(qDevice.queryKlvMetricFilters(0U));
    EXPECT_TRUE(qDevice.queryClassifierConfig(0U));

    const auto sent = transport->getSentPackets();
    EXPECT_GE(sent.size(), 16U);

    qDevice.stop();
}

/// @brief Verify detection telemetry signals and cache updates in QSightlineDevice.
TEST_F(QSightlineDeviceTest, DetectionSignalsAndCache)
{
    auto transport = std::make_shared<MockTransportForQt>();
    QSightlineDevice qDevice(transport);
    ASSERT_TRUE(qDevice.start());

    QSignalSpy detSpy(&qDevice, &QSightlineDevice::detectionReceived);
    QSignalSpy advSpy(&qDevice, &QSightlineDevice::advDetectionReceived);
    QSignalSpy roiSpy(&qDevice, &QSightlineDevice::detectionRoiReceived);
    QSignalSpy metricSpy(&qDevice, &QSightlineDevice::klvMetricFiltersReceived);

    // 1. Inject CurrentDetectionParameters (0x54)
    Sightline::MsgSetDetectionParameters detMsg {};
    detMsg.cameraIndex = 0U;
    detMsg.mode = Sightline::DetectionMode::Maritime;
    const auto detPkt = Sightline::SightlineProtocolBuilder::buildSetDetectionParams(detMsg);
    const auto detPayload = Sightline::SightlineFraming::extractPayload(detPkt);
    const auto curDetPkt
        = Sightline::SightlineFraming::buildPacket(Sightline::MessageId::CurrentDetectionParameters, detPayload);
    transport->injectData(curDetPkt);

    // 2. Inject CurrentAdvancedDetectionParameters (0x77)
    Sightline::MsgAdvancedDetectionParameters advMsg {};
    advMsg.cameraIndex = 0U;
    advMsg.updateRate = 42U;
    const auto advPkt = Sightline::SightlineProtocolBuilder::buildSetAdvDetectionParams(advMsg);
    const auto advPayload = Sightline::SightlineFraming::extractPayload(advPkt);
    const auto curAdvPkt = Sightline::SightlineFraming::buildPacket(
        Sightline::MessageId::CurrentAdvancedDetectionParameters, advPayload);
    transport->injectData(curAdvPkt);

    // 3. Inject CurrentDetectionRegionOfInterestParameters (0x7D)
    Sightline::MsgDetectionROI roiMsg {};
    roiMsg.cameraIndex = 0U;
    roiMsg.lineLeftX = 140U;
    const auto roiPkt = Sightline::SightlineProtocolBuilder::buildSetDetectionROI(roiMsg);
    const auto roiPayload = Sightline::SightlineFraming::extractPayload(roiPkt);
    const auto curRoiPkt = Sightline::SightlineFraming::buildPacket(
        Sightline::MessageId::CurrentDetectionRegionOfInterestParameters, roiPayload);
    transport->injectData(curRoiPkt);

    // 4. Inject KlvClassFilters (0xC1)
    Sightline::MsgKlvMetricFilters metricMsg {};
    metricMsg.cameraIndex = 1U;
    metricMsg.minTargetWidthM = 2.4F;
    const auto klvPkt = Sightline::SightlineProtocolBuilder::buildSetKlvMetricFilters(metricMsg);
    transport->injectData(klvPkt);

    QCoreApplication::processEvents();

    EXPECT_EQ(detSpy.count(), 1);
    EXPECT_TRUE(qDevice.lastDetectionParams().has_value());
    EXPECT_EQ(qDevice.lastDetectionParams()->mode, Sightline::DetectionMode::Maritime);

    EXPECT_EQ(advSpy.count(), 1);
    EXPECT_TRUE(qDevice.lastAdvDetection().has_value());
    EXPECT_EQ(qDevice.lastAdvDetection()->updateRate, 42U);

    EXPECT_EQ(roiSpy.count(), 1);
    EXPECT_TRUE(qDevice.lastDetectionROI().has_value());
    EXPECT_EQ(qDevice.lastDetectionROI()->lineLeftX, 140U);

    EXPECT_EQ(metricSpy.count(), 1);
    EXPECT_TRUE(qDevice.lastKlvMetricFilters().has_value());
    EXPECT_NEAR(qDevice.lastKlvMetricFilters()->minTargetWidthM, 2.4F, 1e-4F);

    qDevice.stop();
}

/// @brief Verify Phase 3 tracking telemetry, coasting transitions, and primary track signals.
TEST_F(QSightlineDeviceTest, Phase3TrackingAndCoastingSignals)
{
    auto transport = std::make_shared<MockTransportForQt>();
    QSightlineDevice qDevice(transport);
    ASSERT_TRUE(qDevice.start());

    QSignalSpy coastSpy(&qDevice, &QSightlineDevice::trackCoastingChanged);
    QSignalSpy primaryUpdateSpy(&qDevice, &QSightlineDevice::primaryTrackUpdated);
    QSignalSpy primaryChangeSpy(&qDevice, &QSightlineDevice::primaryTrackChanged);

    // Frame 1: Track 0 is primary and tracking; Track 1 is coasting
    std::vector<std::uint8_t> payload {};
    payload.push_back(0U); // cameraIndex = 0
    payload.push_back(2U); // numTracks = 2
    // Track 0
    payload.push_back(0U); // trackId = 0
    payload.push_back(0x40); payload.push_back(0x01); // col = 320
    payload.push_back(0xF0); payload.push_back(0x00); // row = 240
    payload.push_back(0x40); payload.push_back(0x00); // w = 64
    payload.push_back(0x30); payload.push_back(0x00); // h = 48
    payload.push_back(0x00); payload.push_back(0x00); // velCol = 0
    payload.push_back(0x00); payload.push_back(0x00); // velRow = 0
    payload.push_back(95U);   // byte 13: conf = 95 (bit 7 = 0, not coasting)
    payload.push_back(0x01U); // byte 14: flags = primary (bit 0)
    // Track 1
    payload.push_back(1U); // trackId = 1
    payload.push_back(0x50); payload.push_back(0x01); // col = 336
    payload.push_back(0xFA); payload.push_back(0x00); // row = 250
    payload.push_back(0x20); payload.push_back(0x00); // w = 32
    payload.push_back(0x20); payload.push_back(0x00); // h = 32
    payload.push_back(0x00); payload.push_back(0x00); // velCol = 0
    payload.push_back(0x00); payload.push_back(0x00); // velRow = 0
    payload.push_back(0x80U | 50U); // byte 13: conf = coasting (bit 7 set)
    payload.push_back(0x00U);       // byte 14: flags = non-primary

    const auto pkt1 = Sightline::SightlineFraming::buildPacket(Sightline::MessageId::TrackingPositions, payload);
    transport->injectData(pkt1);
    QCoreApplication::processEvents();

    EXPECT_GE(coastSpy.count(), 2);
    EXPECT_GE(primaryUpdateSpy.count(), 1);
    EXPECT_EQ(primaryChangeSpy.count(), 1);
    EXPECT_EQ(primaryChangeSpy.takeFirst().at(1).toInt(), 0);

    // Frame 2: Track 0 transitions to coasting!
    coastSpy.clear();
    primaryUpdateSpy.clear();
    payload[15U] = 0x80U | 90U; // Track 0 confidence has bit 7 set -> coasting
    const auto pkt2 = Sightline::SightlineFraming::buildPacket(Sightline::MessageId::TrackingPositions, payload);
    transport->injectData(pkt2);
    QCoreApplication::processEvents();

    ASSERT_GE(coastSpy.count(), 1);
    bool foundTrack0Coasting = false;
    for (int i = 0; i < coastSpy.count(); ++i) {
        const auto args = coastSpy.at(i);
        if (args.at(1).toInt() == 0 && args.at(2).toBool() == true) {
            foundTrack0Coasting = true;
            break;
        }
    }
    EXPECT_TRUE(foundTrack0Coasting);

    qDevice.stop();
}

/// @brief Verify Phase 2 tracking command slots dispatch through QSightlineDevice.
TEST_F(QSightlineDeviceTest, Phase3TrackingCommandSlots)
{
    auto transport = std::make_shared<MockTransportForQt>();
    QSightlineDevice qDevice(transport);
    ASSERT_TRUE(qDevice.start());

    // 1. startPrecisionTrack
    transport->clearSentPackets();
    EXPECT_TRUE(qDevice.startPrecisionTrack(0U, 320U, 240U, 64U, 48U, 123456ULL));
    auto sent = transport->getSentPackets();
    ASSERT_EQ(sent.size(), 1U);
    EXPECT_EQ(Sightline::SightlineFraming::identifyMessage(sent[0]), Sightline::MessageId::StartTracking);
    EXPECT_EQ(Sightline::SightlineFraming::extractPayload(sent[0]).size(), 21U);

    // 2. setForcedCoast
    transport->clearSentPackets();
    EXPECT_TRUE(qDevice.setForcedCoast(0U, 2U, Sightline::ForcedCoastingMode::FreezeUpdates));
    sent = transport->getSentPackets();
    ASSERT_EQ(sent.size(), 1U);
    EXPECT_EQ(Sightline::SightlineFraming::identifyMessage(sent[0]), Sightline::MessageId::ModifyTrackIndex);

    // 3. reinitTrack
    transport->clearSentPackets();
    EXPECT_TRUE(qDevice.reinitTrack(0U, 3U));
    sent = transport->getSentPackets();
    ASSERT_EQ(sent.size(), 1U);
    EXPECT_EQ(Sightline::SightlineFraming::identifyMessage(sent[0]), Sightline::MessageId::ModifyTrackIndex);

    // 4. resizeTrack
    transport->clearSentPackets();
    EXPECT_TRUE(qDevice.resizeTrack(0U, 4U, 80U, 60U, true));
    sent = transport->getSentPackets();
    ASSERT_EQ(sent.size(), 1U);
    EXPECT_EQ(Sightline::SightlineFraming::identifyMessage(sent[0]), Sightline::MessageId::ModifyTrackIndex);

    // 5. cueTrackAt
    transport->clearSentPackets();
    EXPECT_TRUE(qDevice.cueTrackAt(0U, 400U, 250U, Sightline::ModifyMode::DesignateNearAsPrimary, 5U));
    sent = transport->getSentPackets();
    ASSERT_EQ(sent.size(), 1U);
    EXPECT_EQ(Sightline::SightlineFraming::identifyMessage(sent[0]), Sightline::MessageId::ModifyTracking);

    // 6. nudgeDisplayTrack
    transport->clearSentPackets();
    EXPECT_TRUE(qDevice.nudgeDisplayTrack(0U, 10, -5));
    sent = transport->getSentPackets();
    ASSERT_EQ(sent.size(), 1U);
    EXPECT_EQ(Sightline::SightlineFraming::identifyMessage(sent[0]), Sightline::MessageId::NudgeTrackingCoordinate);

    qDevice.stop();
}

/// @brief Verify SetTrackingParameters slot dispatch and signal reception through QSightlineDevice.
TEST_F(QSightlineDeviceTest, SetTrackingParametersSlotAndSignal)
{
    auto transport = std::make_shared<MockTransportForQt>();
    QSightlineDevice qDevice(transport);
    ASSERT_TRUE(qDevice.start());

    // 1. Dispatch setTrackingParameters slot
    Sightline::MsgSetTrackingParameters params {};
    params.cameraIndex = 1U;
    params.mode = 5U; // Drone
    params.flags = 0x05U; // AcqAssist | Color
    params.maxMisses = 20U;
    params.zoomSmoothing = 10U;
    params.rollSmoothing = 5U;
    params.maxPauseTime = 12U;
    params.acquisitionSearchCol = 640U;
    params.acquisitionSearchRow = 480U;

    transport->clearSentPackets();
    EXPECT_TRUE(qDevice.setTrackingParameters(params));

    auto sent = transport->getSentPackets();
    ASSERT_EQ(sent.size(), 1U);
    EXPECT_EQ(Sightline::SightlineFraming::identifyMessage(sent[0]), Sightline::MessageId::SetTrackingParameters);

    // Verify lastTrackingParameters was updated
    const auto lastParams = qDevice.lastTrackingParameters();
    ASSERT_TRUE(lastParams.has_value());
    EXPECT_EQ(lastParams->cameraIndex, 1U);
    EXPECT_EQ(lastParams->mode, 5U);
    EXPECT_EQ(lastParams->flags, 0x05U);
    EXPECT_EQ(lastParams->maxMisses, 20U);
    EXPECT_EQ(lastParams->zoomSmoothing, 10U);
    EXPECT_EQ(lastParams->rollSmoothing, 5U);
    EXPECT_EQ(lastParams->maxPauseTime, 12U);
    EXPECT_EQ(lastParams->acquisitionSearchCol, 640U);
    EXPECT_EQ(lastParams->acquisitionSearchRow, 480U);

    // 2. Test signal reception by injecting a 0x0C response packet
    QSignalSpy paramSpy(&qDevice, &QSightlineDevice::trackingParametersReceived);

    std::vector<std::uint8_t> payload {
        32U,   // 0: objectSize
        6U,    // 1: mode (Person)
        0U,    // 2: mode2
        30U,   // 3: maxMisses
        0x00U, 0x00U, // 4-5: nearVal
        32U,   // 6: objectHeight
        2U,    // 7: cameraIndex
        15U,   // 8: zoomSmoothing
        8U,    // 9: rollSmoothing
        10U,   // 10: maxTracks
        0x80U, 0x01U, // 11-12: acquisitionSearchCol (384)
        0x20U, 0x01U, // 13-14: acquisitionSearchRow (288)
        0x12U, // 15: flags (IntelAssist | Unique)
        5U     // 16: maxPauseTime
    };
    const auto packet = Sightline::SightlineFraming::buildPacket(Sightline::MessageId::CurrentTrackingParameters, payload);
    transport->injectData(packet);

    QCoreApplication::processEvents();

    EXPECT_EQ(paramSpy.count(), 1);
    const auto receivedParams = qDevice.lastTrackingParameters();
    ASSERT_TRUE(receivedParams.has_value());
    EXPECT_EQ(receivedParams->cameraIndex, 2U);
    EXPECT_EQ(receivedParams->mode, 6U);
    EXPECT_EQ(receivedParams->flags, 0x12U);
    EXPECT_EQ(receivedParams->maxMisses, 30U);
    EXPECT_EQ(receivedParams->zoomSmoothing, 15U);
    EXPECT_EQ(receivedParams->rollSmoothing, 8U);
    EXPECT_EQ(receivedParams->maxPauseTime, 5U);
    EXPECT_EQ(receivedParams->acquisitionSearchCol, 384U);
    EXPECT_EQ(receivedParams->acquisitionSearchRow, 288U);

    qDevice.stop();
}

/// @brief Verify blending command dispatch slots.
TEST_F(QSightlineDeviceTest, BlendingCommandSlots)
{
    auto transport = std::make_shared<MockTransportForQt>();
    QSightlineDevice qDevice(transport);
    ASSERT_TRUE(qDevice.start());

    Sightline::MsgSetBlendParameters blendMsg {};
    blendMsg.warpIndex = 1U;
    blendMsg.fixedIndex = 0U;
    blendMsg.mode = Sightline::BlendMode::FrameBlendWarpEo;
    blendMsg.amt = 128U;
    EXPECT_TRUE(qDevice.setBlend(blendMsg));
    EXPECT_TRUE(qDevice.getBlendParameters());

    Sightline::MsgFourAlignPoints ptsMsg {};
    ptsMsg.index = 2U;
    EXPECT_TRUE(qDevice.setFourAlignPoints(ptsMsg));
    EXPECT_TRUE(qDevice.getFourAlignPoints(2U));

    Sightline::MsgBlendAlign alignMsg {};
    alignMsg.index = 1U;
    alignMsg.zoom = 4096U;
    EXPECT_TRUE(qDevice.setBlendAlign(alignMsg));
    EXPECT_TRUE(qDevice.getBlendAlign(1U));

    Sightline::MsgSetMultipleAlignment multiMsg {};
    EXPECT_TRUE(qDevice.setMultipleAlignment(multiMsg));
    EXPECT_TRUE(qDevice.getMultipleAlignment());

    const auto sent = transport->getSentPackets();
    EXPECT_EQ(sent.size(), 8U);

    qDevice.stop();
}

/// @brief Verify incoming blending telemetry signals and cache updates.
TEST_F(QSightlineDeviceTest, BlendingSignalsAndCache)
{
    auto transport = std::make_shared<MockTransportForQt>();
    QSightlineDevice qDevice(transport);
    ASSERT_TRUE(qDevice.start());

    QSignalSpy blendSpy(&qDevice, &QSightlineDevice::blendParametersReceived);
    QSignalSpy curBlendSpy(&qDevice, &QSightlineDevice::currentBlendParamsReceived);
    QSignalSpy ptsSpy(&qDevice, &QSightlineDevice::fourAlignPointsReceived);
    QSignalSpy alignSpy(&qDevice, &QSightlineDevice::blendAlignReceived);
    QSignalSpy multiSpy(&qDevice, &QSightlineDevice::multipleAlignmentReceived);

    // 1. Inject MsgSetBlendParameters (0x2F)
    Sightline::MsgSetBlendParameters blendMsg {};
    blendMsg.warpIndex = 1U;
    blendMsg.fixedIndex = 0U;
    blendMsg.mode = Sightline::BlendMode::ThermalBlendWarpEo;
    blendMsg.amt = 200U;
    transport->injectData(Sightline::SightlineBlendingBuilder::buildSetBlendParameters(blendMsg));

    // 2. Inject MsgCurrentBlendParameters (0x4D)
    const std::vector<std::uint8_t> curBlendPayload {
        0x01U, 10U, 25U, 2U, 5U, 0U, 128U,
        static_cast<std::uint8_t>(Sightline::BlendMode::FrameBlendWarpEo),
        180U, 0U, 0x01U, 0U, 0U, 1U, 1U, 0U, 128U, 10U, 50U
    };
    transport->injectData(Sightline::SightlineFraming::buildPacket(Sightline::MessageId::CurrentBlendParameters, curBlendPayload));

    // 3. Inject MsgFourAlignPoints (0x95)
    Sightline::MsgFourAlignPoints ptsMsg {};
    ptsMsg.index = 1U;
    ptsMsg.points[0].leftCol = 10;
    ptsMsg.points[0].leftRow = 20;
    transport->injectData(Sightline::SightlineBlendingBuilder::buildFourAlignPoints(ptsMsg));

    // 4. Inject MsgBlendAlign (0xB9)
    Sightline::MsgBlendAlign alignMsg {};
    alignMsg.index = 1U;
    alignMsg.rotate = 45U * 128U;
    alignMsg.zoom = 4096U;
    transport->injectData(Sightline::SightlineBlendingBuilder::buildSetBlendAlign(alignMsg));

    // 5. Inject MsgSetMultipleAlignment (0x74)
    Sightline::MsgSetMultipleAlignment multiMsg {};
    multiMsg.nAlignments = 1U;
    multiMsg.alignment[0].horizontal = 5U;
    multiMsg.alignment[0].vertical = 10U;
    transport->injectData(Sightline::SightlineBlendingBuilder::buildSetMultipleAlignment(multiMsg));

    QCoreApplication::processEvents();

    EXPECT_EQ(blendSpy.count(), 2);
    EXPECT_EQ(curBlendSpy.count(), 1);
    EXPECT_EQ(ptsSpy.count(), 1);
    EXPECT_EQ(alignSpy.count(), 1);
    EXPECT_EQ(multiSpy.count(), 1);

    ASSERT_TRUE(qDevice.lastBlendParams().has_value());
    EXPECT_EQ(qDevice.lastBlendParams()->mode, Sightline::BlendMode::FrameBlendWarpEo);

    ASSERT_TRUE(qDevice.lastCurrentBlendParams().has_value());
    EXPECT_EQ(qDevice.lastCurrentBlendParams()->amt, 180U);

    ASSERT_TRUE(qDevice.lastFourAlignPoints().has_value());
    EXPECT_EQ(qDevice.lastFourAlignPoints()->index, 1U);
    EXPECT_EQ(qDevice.lastFourAlignPoints()->points[0].leftCol, 10);

    ASSERT_TRUE(qDevice.lastBlendAlign().has_value());
    EXPECT_EQ(qDevice.lastBlendAlign()->index, 1U);
    EXPECT_EQ(qDevice.lastBlendAlign()->rotate, 45U * 128U);

    ASSERT_TRUE(qDevice.lastMultipleAlignment().has_value());
    EXPECT_EQ(qDevice.lastMultipleAlignment()->nAlignments, 1U);
    EXPECT_EQ(qDevice.lastMultipleAlignment()->alignment[0].horizontal, 5U);

    qDevice.stop();
}

/// @brief NUC replies (0x35 / 0x36 / 0xA1 / 0xAF) emit queued signals; sendFramed transmits.
TEST_F(QSightlineDeviceTest, NucSignalsAndFramedSend)
{
    auto transport = std::make_shared<MockTransportForQt>();
    QSightlineDevice qDevice(transport);
    ASSERT_TRUE(qDevice.start());

    QSignalSpy nucSpy(&qDevice, &QSightlineDevice::nucParamsReceived);
    QSignalSpy tableSpy(&qDevice, &QSightlineDevice::nucTableReceived);
    QSignalSpy statsSpy(&qDevice, &QSightlineDevice::deadStatsReceived);
    QSignalSpy noiseSpy(&qDevice, &QSightlineDevice::noiseStatsReceived);

    transport->injectData(Sightline::SightlineNucBuilder::buildNucParameters(Sightline::MsgNucParameters {}));
    Sightline::MsgReadWriteNuc table {};
    table.fileOp = Sightline::NucFileOp::LoadNuc;
    table.fileName = "zoom1";
    transport->injectData(Sightline::SightlineNucBuilder::buildReadWriteNuc(table));
    transport->injectData(Sightline::SightlineFraming::buildPacket(
        Sightline::MessageId::DeadPixelStats, std::vector<std::uint8_t>(33U, 0U)));
    transport->injectData(Sightline::SightlineFraming::buildPacket(
        Sightline::MessageId::Noise3D, std::vector<std::uint8_t>(17U, 0U)));

    QCoreApplication::processEvents();

    EXPECT_EQ(nucSpy.count(), 1);
    EXPECT_EQ(tableSpy.count(), 1);
    EXPECT_EQ(statsSpy.count(), 1);
    EXPECT_EQ(noiseSpy.count(), 1);

    transport->clearSentPackets();
    const auto query { Sightline::SightlineNucBuilder::buildGetNucParameters(0U) };
    EXPECT_TRUE(qDevice.sendFramed(query));
    const auto sent { transport->getSentPackets() };
    ASSERT_EQ(sent.size(), 1U);
    EXPECT_EQ(sent[0U], query);

    qDevice.stop();
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
