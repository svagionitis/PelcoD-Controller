/// @file TestSightlineVideo.cpp
/// @brief Automated unit test suite for SightlineVideoController and VideoQuickItem.

#include "RecordingFileListModel.h"
#include "SightlineQmlBridge.h"
#include "SightlineVideoController.h"
#include "TrackListModel.h"
#include "VideoQuickItem.h"

#include <LatencyTracker.h>

#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <gtest/gtest.h>

namespace {

class TestableVideoQuickItem : public SightlineApp::VideoQuickItem {
public:
    using SightlineApp::VideoQuickItem::mouseMoveEvent;
    using SightlineApp::VideoQuickItem::mousePressEvent;
    using SightlineApp::VideoQuickItem::mouseReleaseEvent;
};

class SightlineVideoTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (qApp == nullptr) {
            static int argc = 1;
            static char appName[] = "TestSightlineVideo";
            static char* argv[] = { appName, nullptr };
            new QCoreApplication(argc, argv);
            QCoreApplication::setOrganizationName(QStringLiteral("Sightline Intelligence"));
            QCoreApplication::setApplicationName(QStringLiteral("TestSightlineVideo"));
        }
    }
};

TEST_F(SightlineVideoTest, ControllerLifecycleAndDefaultState)
{
    SightlineApp::SightlineVideoController controller {};

    EXPECT_TRUE(controller.isSynthetic());
    EXPECT_EQ(controller.activeCamera(), 0);
    EXPECT_FALSE(controller.availableBackends().isEmpty());

    // Wait briefly for the worker thread to decode at least one frame
    QTest::qWait(120);

    EXPECT_EQ(controller.playbackState(), SightlineApp::SightlineVideoController::PlaybackState::Playing);
    EXPECT_GT(controller.frameWidth(), 0);
    EXPECT_GT(controller.frameHeight(), 0);

    controller.stopStream();
    EXPECT_EQ(controller.playbackState(), SightlineApp::SightlineVideoController::PlaybackState::Idle);
}

TEST_F(SightlineVideoTest, CameraSelectionAndSwitching)
{
    SightlineApp::SightlineVideoController controller {};
    QSignalSpy camSpy(&controller, &SightlineApp::SightlineVideoController::activeCameraChanged);

    controller.selectCamera(1);
    EXPECT_EQ(controller.activeCamera(), 1);
    EXPECT_EQ(camSpy.count(), 1);

    controller.selectCamera(0);
    EXPECT_EQ(controller.activeCamera(), 0);
    EXPECT_EQ(camSpy.count(), 2);

    // Clamping checks
    controller.selectCamera(99);
    EXPECT_EQ(controller.activeCamera(), 0);

    // Verify 4-camera support
    controller.selectCamera(2);
    EXPECT_EQ(controller.activeCamera(), 2);
    controller.selectCamera(3);
    EXPECT_EQ(controller.activeCamera(), 3);
}

TEST_F(SightlineVideoTest, CameraSwitchDoesNotMutateStreamUri)
{
    SightlineApp::SightlineVideoController controller {};
    controller.setSyntheticMode(false);

    // Initial default should be Net0 mount
    EXPECT_EQ(controller.networkChannel(), SightlineApp::SightlineVideoController::NetworkChannel::Net0);
    EXPECT_EQ(controller.sourceUri(), QStringLiteral("rtsp://127.0.0.1:554/net0"));

    // Switching camera must NOT mutate the RTSP mount URI to /net1 or /net2
    controller.selectCamera(1);
    EXPECT_EQ(controller.activeCamera(), 1);
    EXPECT_EQ(controller.sourceUri(), QStringLiteral("rtsp://127.0.0.1:554/net0"));

    controller.selectCamera(2);
    EXPECT_EQ(controller.activeCamera(), 2);
    EXPECT_EQ(controller.sourceUri(), QStringLiteral("rtsp://127.0.0.1:554/net0"));
}

TEST_F(SightlineVideoTest, NetworkChannelMountResolution)
{
    SightlineApp::SightlineVideoController controller {};
    controller.setSyntheticMode(false);

    QSignalSpy channelSpy(&controller, &SightlineApp::SightlineVideoController::networkChannelChanged);

    // Switch to Net1
    controller.selectNetworkChannel(SightlineApp::SightlineVideoController::NetworkChannel::Net1);
    EXPECT_EQ(controller.networkChannel(), SightlineApp::SightlineVideoController::NetworkChannel::Net1);
    EXPECT_EQ(controller.activeNetworkChannel(), 1);
    EXPECT_EQ(controller.sourceUri(), QStringLiteral("rtsp://127.0.0.1:554/net1"));
    EXPECT_EQ(channelSpy.count(), 1);

    // Switch to Legacy 1500-OEM root
    controller.selectNetworkChannel(SightlineApp::SightlineVideoController::NetworkChannel::Legacy);
    EXPECT_EQ(controller.networkChannel(), SightlineApp::SightlineVideoController::NetworkChannel::Legacy);
    EXPECT_EQ(controller.activeNetworkChannel(), 2);
    EXPECT_EQ(controller.sourceUri(), QStringLiteral("rtsp://127.0.0.1:554/"));
    EXPECT_EQ(channelSpy.count(), 2);

    // Switch back to Net0 via integer slot (QML compatibility)
    controller.selectNetworkChannelInt(0);
    EXPECT_EQ(controller.networkChannel(), SightlineApp::SightlineVideoController::NetworkChannel::Net0);
    EXPECT_EQ(controller.activeNetworkChannel(), 0);
    EXPECT_EQ(controller.sourceUri(), QStringLiteral("rtsp://127.0.0.1:554/net0"));
    EXPECT_EQ(channelSpy.count(), 3);
}

TEST_F(SightlineVideoTest, CustomUriPreservationAcrossCameraSwitches)
{
    SightlineApp::SightlineVideoController controller {};
    controller.setSyntheticMode(false);

    const QString customUri { QStringLiteral("udp://@:15004") };
    controller.setSourceUri(customUri);
    EXPECT_EQ(controller.networkChannel(), SightlineApp::SightlineVideoController::NetworkChannel::Custom);
    EXPECT_EQ(controller.sourceUri(), customUri);

    // Camera switching must preserve custom URI
    controller.selectCamera(1);
    EXPECT_EQ(controller.activeCamera(), 1);
    EXPECT_EQ(controller.sourceUri(), customUri);

    controller.selectCamera(0);
    EXPECT_EQ(controller.activeCamera(), 0);
    EXPECT_EQ(controller.sourceUri(), customUri);
}

TEST_F(SightlineVideoTest, TransportModeConfiguration)
{
    SightlineApp::SightlineVideoController controller {};
    QSignalSpy transportSpy(&controller, &SightlineApp::SightlineVideoController::transportModeChanged);

    // Initial default should be Auto
    EXPECT_EQ(controller.transportMode(), SightlineApp::SightlineVideoController::RtspTransport::Auto);
    EXPECT_EQ(controller.activeTransportMode(), 0);

    // Switch to TCP
    controller.selectTransportMode(SightlineApp::SightlineVideoController::RtspTransport::Tcp);
    EXPECT_EQ(controller.transportMode(), SightlineApp::SightlineVideoController::RtspTransport::Tcp);
    EXPECT_EQ(controller.activeTransportMode(), 1);
    EXPECT_EQ(transportSpy.count(), 1);

    // Switch to UDP Unicast
    controller.selectTransportMode(SightlineApp::SightlineVideoController::RtspTransport::Udp);
    EXPECT_EQ(controller.transportMode(), SightlineApp::SightlineVideoController::RtspTransport::Udp);
    EXPECT_EQ(controller.activeTransportMode(), 2);
    EXPECT_EQ(transportSpy.count(), 2);

    // Switch to UDP Multicast
    controller.selectTransportMode(SightlineApp::SightlineVideoController::RtspTransport::Multicast);
    EXPECT_EQ(controller.transportMode(), SightlineApp::SightlineVideoController::RtspTransport::Multicast);
    EXPECT_EQ(controller.activeTransportMode(), 3);
    EXPECT_EQ(transportSpy.count(), 3);

    // Idempotent selection should not re-trigger signal
    controller.selectTransportMode(SightlineApp::SightlineVideoController::RtspTransport::Multicast);
    EXPECT_EQ(transportSpy.count(), 3);

    // Test QML integer slot
    controller.selectTransportModeInt(1);
    EXPECT_EQ(controller.transportMode(), SightlineApp::SightlineVideoController::RtspTransport::Tcp);
    EXPECT_EQ(controller.activeTransportMode(), 1);
    EXPECT_EQ(transportSpy.count(), 4);

    controller.selectTransportModeInt(0);
    EXPECT_EQ(controller.transportMode(), SightlineApp::SightlineVideoController::RtspTransport::Auto);
    EXPECT_EQ(controller.activeTransportMode(), 0);
    EXPECT_EQ(transportSpy.count(), 5);
}

TEST_F(SightlineVideoTest, RtspDigestAuthenticationConfiguration)
{
    SightlineApp::SightlineVideoController controller {};
    QSignalSpy authSpy(&controller, &SightlineApp::SightlineVideoController::rtspAuthChanged);

    EXPECT_FALSE(controller.authEnabled());
    EXPECT_TRUE(controller.rtspUsername().isEmpty());
    EXPECT_TRUE(controller.rtspPassword().isEmpty());

    // Configure credentials
    controller.setRtspCredentials(QStringLiteral("admin"), QStringLiteral("bls_345"));
    EXPECT_TRUE(controller.authEnabled());
    EXPECT_EQ(controller.rtspUsername(), QStringLiteral("admin"));
    EXPECT_EQ(controller.rtspPassword(), QStringLiteral("bls_345"));
    EXPECT_EQ(authSpy.count(), 1);

    // Verify URI sanitization hides password
    controller.setSourceUri(QStringLiteral("rtsp://admin:bls_345@192.168.1.15:554/net0"));
    EXPECT_EQ(controller.sanitizedSourceUri(), QStringLiteral("rtsp://admin:***@192.168.1.15:554/net0"));

    // Clear credentials
    controller.clearRtspCredentials();
    EXPECT_FALSE(controller.authEnabled());
    EXPECT_TRUE(controller.rtspUsername().isEmpty());
    EXPECT_TRUE(controller.rtspPassword().isEmpty());
    EXPECT_EQ(authSpy.count(), 2);
}

TEST_F(SightlineVideoTest, SyntheticModeToggle)
{
    SightlineApp::SightlineVideoController controller {};
    QSignalSpy synthSpy(&controller, &SightlineApp::SightlineVideoController::syntheticChanged);

    controller.setSyntheticMode(false);
    EXPECT_FALSE(controller.isSynthetic());
    EXPECT_EQ(synthSpy.count(), 1);

    controller.setSyntheticMode(true);
    EXPECT_TRUE(controller.isSynthetic());
    EXPECT_EQ(synthSpy.count(), 2);
}

TEST_F(SightlineVideoTest, VideoQuickItemPropertiesAndFrameIngestion)
{
    SightlineApp::VideoQuickItem item {};

    EXPECT_EQ(item.fillMode(), SightlineApp::VideoQuickItem::FillMode::PreserveAspectFit);
    EXPECT_FALSE(item.hasFrame());
    EXPECT_EQ(item.videoWidth(), 0);
    EXPECT_EQ(item.videoHeight(), 0);

    QSignalSpy fillSpy(&item, &SightlineApp::VideoQuickItem::fillModeChanged);
    item.setFillMode(SightlineApp::VideoQuickItem::FillMode::PreserveAspectCrop);
    EXPECT_EQ(item.fillMode(), SightlineApp::VideoQuickItem::FillMode::PreserveAspectCrop);
    EXPECT_EQ(fillSpy.count(), 1);

    QSignalSpy frameSpy(&item, &SightlineApp::VideoQuickItem::hasFrameChanged);
    QSignalSpy sizeSpy(&item, &SightlineApp::VideoQuickItem::videoSizeChanged);

    QImage testFrame(320, 240, QImage::Format_RGB888);
    testFrame.fill(Qt::blue);

    item.updateFrame(testFrame);
    EXPECT_TRUE(item.hasFrame());
    EXPECT_EQ(item.videoWidth(), 320);
    EXPECT_EQ(item.videoHeight(), 240);
    EXPECT_EQ(frameSpy.count(), 1);
    EXPECT_EQ(sizeSpy.count(), 1);

    item.clearFrame();
    EXPECT_FALSE(item.hasFrame());
    EXPECT_EQ(item.videoWidth(), 0);
    EXPECT_EQ(item.videoHeight(), 0);
}

namespace {

    /// @brief Paints the item onto an offscreen canvas and simulates a scene graph swap.
    /// @param[in,out] item Video item under test.
    void presentFrame(SightlineApp::VideoQuickItem& item)
    {
        QImage canvas(320, 240, QImage::Format_RGB32);
        QPainter painter(&canvas);
        item.paint(&painter);
        painter.end();
        item.notifyFrameSwapped();
        item.publishLatency();
    }

} // namespace

TEST_F(SightlineVideoTest, VideoQuickItemMeasuresDisplayLatency)
{
    SightlineApp::VideoQuickItem item {};
    item.setSize(QSizeF(320.0, 240.0));
    EXPECT_DOUBLE_EQ(item.displayLatencyMs(), 0.0);
    EXPECT_EQ(item.presentedFrames(), 0U);

    QImage testFrame(320, 240, QImage::Format_RGB888);
    testFrame.fill(Qt::green);

    constexpr qint64 kFiveMsNs { 5'000'000 };
    const qint64 decodedAt { static_cast<qint64>(Video::steadyNowNs()) - kFiveMsNs };
    item.updateTimedFrame(testFrame, decodedAt);

    QSignalSpy latSpy(&item, &SightlineApp::VideoQuickItem::displayLatencyChanged);
    presentFrame(item);

    EXPECT_EQ(latSpy.count(), 1);
    EXPECT_EQ(item.presentedFrames(), 1U);
    EXPECT_GE(item.displayLatencyLastMs(), 5.0);
    EXPECT_GE(item.displayLatencyMs(), 5.0);
    EXPECT_LE(item.displayLatencyMinMs(), item.displayLatencyMaxMs());

    // Repainting the same frame (e.g. overlay update) must not be counted again
    presentFrame(item);
    EXPECT_EQ(item.presentedFrames(), 1U);

    // Unstamped frames (scrub/PIP path) are displayed but not measured
    item.updateFrame(testFrame);
    presentFrame(item);
    EXPECT_EQ(item.presentedFrames(), 1U);

    item.resetLatency();
    EXPECT_EQ(item.presentedFrames(), 0U);
    EXPECT_DOUBLE_EQ(item.displayLatencyMs(), 0.0);
}

TEST_F(SightlineVideoTest, ControllerFeedsDisplayLatency)
{
    SightlineApp::SightlineVideoController controller {};
    SightlineApp::VideoQuickItem item {};
    item.setSize(QSizeF(320.0, 240.0));
    controller.attachVideoItem(&item);

    for (int i = 0; (i < 20) && (item.presentedFrames() == 0U); ++i) {
        QTest::qWait(30);
        presentFrame(item);
    }

    EXPECT_GT(item.presentedFrames(), 0U);
    EXPECT_GT(item.displayLatencyLastMs(), 0.0);
}

TEST_F(SightlineVideoTest, ControllerAttachVideoItemIntegration)
{
    SightlineApp::SightlineVideoController controller {};
    SightlineApp::VideoQuickItem item {};

    controller.attachVideoItem(&item);

    // Allow worker loop to ingest and deliver frames
    for (int i = 0; i < 20 && !item.hasFrame(); ++i) {
        QTest::qWait(30);
    }

    EXPECT_TRUE(item.hasFrame());
    EXPECT_GT(item.videoWidth(), 0);
    EXPECT_GT(item.videoHeight(), 0);
}

TEST_F(SightlineVideoTest, SnapshotCapture)
{
    QTemporaryDir tempDir {};
    ASSERT_TRUE(tempDir.isValid());

    SightlineApp::SightlineVideoController controller {};

    // Wait for at least one frame to decode
    for (int i = 0; i < 20; ++i) {
        QTest::qWait(30);
        if (controller.frameWidth() > 0) {
            break;
        }
    }

    const QString targetPath = tempDir.filePath(QStringLiteral("test_snapshot.png"));
    QSignalSpy snapSpy(&controller, &SightlineApp::SightlineVideoController::snapshotTaken);

    const QString savedPath = controller.takeSnapshot(targetPath);
    EXPECT_EQ(savedPath, targetPath);
    EXPECT_TRUE(QFile::exists(targetPath));
    EXPECT_EQ(snapSpy.count(), 1);

    QImage loaded(targetPath);
    EXPECT_FALSE(loaded.isNull());
    EXPECT_EQ(loaded.width(), controller.frameWidth());
    EXPECT_EQ(loaded.height(), controller.frameHeight());
}

TEST_F(SightlineVideoTest, PipModeAndStreamSwapping)
{
    SightlineApp::SightlineVideoController controller {};

    EXPECT_FALSE(controller.pipEnabled());
    EXPECT_EQ(controller.pipCamera(), 1);

    QSignalSpy pipSpy(&controller, &SightlineApp::SightlineVideoController::pipEnabledChanged);
    controller.setPipEnabled(true);
    EXPECT_TRUE(controller.pipEnabled());
    EXPECT_EQ(pipSpy.count(), 1);

    // Swap feeds
    QSignalSpy camSpy(&controller, &SightlineApp::SightlineVideoController::activeCameraChanged);
    QSignalSpy pipCamSpy(&controller, &SightlineApp::SightlineVideoController::pipCameraChanged);

    controller.swapPipFeeds();
    EXPECT_EQ(controller.activeCamera(), 1);
    EXPECT_EQ(controller.pipCamera(), 0);
    EXPECT_EQ(camSpy.count(), 1);
    EXPECT_EQ(pipCamSpy.count(), 1);

    // Attach PIP item and verify reception
    SightlineApp::VideoQuickItem pipItem {};
    controller.attachPipVideoItem(&pipItem);

    for (int i = 0; i < 20 && !pipItem.hasFrame(); ++i) {
        QTest::qWait(30);
    }

    EXPECT_TRUE(pipItem.hasFrame());
    EXPECT_GT(pipItem.videoWidth(), 0);
    EXPECT_GT(pipItem.videoHeight(), 0);
}

TEST_F(SightlineVideoTest, TrackListModelLifecycle)
{
    TrackListModel model {};
    EXPECT_EQ(model.rowCount(), 0);

    Sightline::TrackCoordinate t0 {};
    t0.trackId = 0U;
    t0.centerCol = 640.0;
    t0.centerRow = 360.0;
    t0.width = 60.0;
    t0.height = 60.0;
    t0.confidence = 95U;
    t0.isPrimary = true;

    model.addOrUpdateTrack(t0);
    EXPECT_EQ(model.rowCount(), 1);
    EXPECT_TRUE(model.data(model.index(0, 0), TrackListModel::IsPrimaryRole).toBool());
    EXPECT_DOUBLE_EQ(model.data(model.index(0, 0), TrackListModel::CenterColRole).toDouble(), 640.0);

    Sightline::TrackCoordinate t1 {};
    t1.trackId = 1U;
    t1.centerCol = 800.0;
    t1.centerRow = 450.0;
    t1.width = 40.0;
    t1.height = 40.0;
    t1.confidence = 85U;
    t1.isPrimary = false;

    model.addOrUpdateTrack(t1);
    EXPECT_EQ(model.rowCount(), 2);
    EXPECT_FALSE(model.data(model.index(1, 0), TrackListModel::IsPrimaryRole).toBool());

    // Promote t1 to primary
    model.setPrimaryTrack(1);
    EXPECT_FALSE(model.data(model.index(0, 0), TrackListModel::IsPrimaryRole).toBool());
    EXPECT_TRUE(model.data(model.index(1, 0), TrackListModel::IsPrimaryRole).toBool());

    // Update t1 position
    t1.centerCol = 820.0;
    model.addOrUpdateTrack(t1);
    EXPECT_EQ(model.rowCount(), 2);
    EXPECT_DOUBLE_EQ(model.data(model.index(1, 0), TrackListModel::CenterColRole).toDouble(), 820.0);

    // Remove track 0
    model.removeTrack(0);
    EXPECT_EQ(model.rowCount(), 1);
    EXPECT_EQ(model.data(model.index(0, 0), TrackListModel::TrackIdRole).toInt(), 1);

    // Clear tracks
    model.clearTracks();
    EXPECT_EQ(model.rowCount(), 0);
}

TEST_F(SightlineVideoTest, EnhancementModeUpdatesAndSignals)
{
    SightlineApp::SightlineVideoController controller {};
    QSignalSpy spy(&controller, &SightlineApp::SightlineVideoController::contrastModeChanged);

    EXPECT_EQ(controller.contrastMode(), 0);

    controller.updateEnhancementMode(0, 1, 128, 50, 10, 3);
    EXPECT_EQ(controller.contrastMode(), 1);
    EXPECT_EQ(spy.count(), 1);

    // Update for camera 1 while camera 0 is active - should not emit contrastModeChanged
    controller.updateEnhancementMode(1, 2, 100, 80, 5, 2);
    EXPECT_EQ(controller.contrastMode(), 1);
    EXPECT_EQ(spy.count(), 1);

    // Switch to camera 1
    controller.selectCamera(1);
    EXPECT_EQ(controller.contrastMode(), 2);
}

TEST_F(SightlineVideoTest, FalseColorPaletteSelectionAndUserPalette)
{
    SightlineApp::SightlineVideoController controller {};
    QSignalSpy palSpy(&controller, &SightlineApp::SightlineVideoController::activePaletteChanged);

    EXPECT_EQ(controller.activePalette(), 0);

    // Switch to Rainbow (index 2)
    controller.updateFalseColor(0, 2);
    EXPECT_EQ(controller.activePalette(), 2);
    EXPECT_EQ(palSpy.count(), 1);

    // Upload custom 768-byte palette (256 * 3 bytes)
    const QByteArray samplePalette(768, static_cast<char>(0x80));
    controller.updateUserPalette(0, samplePalette);

    // Switch to User Palette (index 4)
    controller.updateFalseColor(0, 4);
    EXPECT_EQ(controller.activePalette(), 4);
    EXPECT_EQ(palSpy.count(), 2);
}

TEST_F(SightlineVideoTest, EnhancementRoiConfiguration)
{
    SightlineApp::SightlineVideoController controller {};
    QSignalSpy roiSpy(&controller, &SightlineApp::SightlineVideoController::enhancementRoiChanged);

    EXPECT_TRUE(controller.enhancementRoi().isNull());

    controller.updateEnhancementRoi(0, 50, 100, 300, 400);
    EXPECT_EQ(controller.enhancementRoi(), QRect(100, 50, 400, 300));
    EXPECT_EQ(roiSpy.count(), 1);
}

TEST_F(SightlineVideoTest, BridgeEnhancementMethodsAndPresets)
{
    SightlineQmlBridge bridge {};

    QSignalSpy modeSpy(&bridge, &SightlineQmlBridge::enhancementModeChanged);
    QSignalSpy palSpy(&bridge, &SightlineQmlBridge::falseColorPaletteChanged);
    QSignalSpy roiSpy(&bridge, &SightlineQmlBridge::enhancementRoiUpdated);
    QSignalSpy histSpy(&bridge, &SightlineQmlBridge::histogramChanged);

    // 1. setEnhancementMode
    bridge.setEnhancementMode(0, 1, 100, 50, 10, 2);
    EXPECT_EQ(modeSpy.count(), 1);
    EXPECT_EQ(bridge.activeContrastMode(), 1);

    // 2. setFalseColorPalette
    bridge.setFalseColorPalette(0, 3);
    EXPECT_EQ(palSpy.count(), 1);
    EXPECT_EQ(bridge.activePaletteIndex(), 3);

    // 3. setEnhancementRoi
    bridge.setEnhancementRoi(0, 20, 30, 240, 320);
    EXPECT_EQ(roiSpy.count(), 1);
    EXPECT_EQ(bridge.enhancementRoi(), QRect(30, 20, 320, 240));

    // 4. setHistogramControls
    bridge.setHistogramControls(0, true, false, 64, 15, 10, 20);
    EXPECT_EQ(histSpy.count(), 1);

    // 5. Additional bridge methods
    bridge.setDenoiseParameters(0, 5, true, 1);
    bridge.setScintillationMode(0, 1);
    bridge.setGaussianAndLap(0, 2, 5, 10);
    bridge.setCustomConvolution(0, 3, QVariantList { 0, -1, 0, -1, 5, -1, 0, -1, 0 }, false);
    bridge.setLensDistortion(0, 0.05, -0.02, 0.0, 0.0);

    // 6. Presets
    const QVariantMap presetSettings { { QStringLiteral("mode"), 1 }, { QStringLiteral("strength"), 100 },
        { QStringLiteral("blend"), 50 } };
    EXPECT_TRUE(bridge.saveEnhancementPreset(QStringLiteral("TacticalNight"), presetSettings));
    const QStringList presets { bridge.getEnhancementPresets() };
    EXPECT_TRUE(presets.contains(QStringLiteral("TacticalNight")));

    const QVariantMap loadedPreset { bridge.loadEnhancementPreset(QStringLiteral("TacticalNight")) };
    EXPECT_EQ(loadedPreset.value(QStringLiteral("mode")).toInt(), 1);

    // 7. User palette file load and save
    QTemporaryDir tempDir {};
    ASSERT_TRUE(tempDir.isValid());
    const QString palFile { tempDir.filePath(QStringLiteral("test_palette.bin")) };
    const QByteArray sampleYuv(768, static_cast<char>(0x7F));
    bridge.uploadUserPalette(0, sampleYuv);
    EXPECT_TRUE(bridge.saveUserPaletteFile(palFile, sampleYuv));
    EXPECT_TRUE(QFile::exists(palFile));
    EXPECT_EQ(bridge.loadUserPaletteFile(palFile), sampleYuv);
}

TEST_F(SightlineVideoTest, BridgeVideoControllerSignalWiring)
{
    SightlineQmlBridge bridge {};
    SightlineApp::SightlineVideoController controller {};

    QObject::connect(&bridge, &SightlineQmlBridge::enhancementModeChanged, &controller,
        &SightlineApp::SightlineVideoController::updateEnhancementMode);
    QObject::connect(&bridge, &SightlineQmlBridge::histogramChanged, &controller,
        &SightlineApp::SightlineVideoController::updateHistogram);
    QObject::connect(&bridge, &SightlineQmlBridge::falseColorPaletteChanged, &controller,
        &SightlineApp::SightlineVideoController::updateFalseColor);
    QObject::connect(&bridge, &SightlineQmlBridge::userPaletteUploaded, &controller,
        &SightlineApp::SightlineVideoController::updateUserPalette);
    QObject::connect(&bridge, &SightlineQmlBridge::enhancementRoiUpdated, &controller,
        &SightlineApp::SightlineVideoController::updateEnhancementRoi);

    // Test signal propagation from bridge to video controller
    bridge.setEnhancementMode(0, 2, 120, 60, 15, 3);
    EXPECT_EQ(controller.contrastMode(), 2);

    bridge.setFalseColorPalette(0, 3);
    EXPECT_EQ(controller.activePalette(), 3);

    bridge.setEnhancementRoi(0, 10, 20, 100, 200);
    EXPECT_EQ(controller.enhancementRoi(), QRect(20, 10, 200, 100));
}

TEST_F(SightlineVideoTest, BridgeDetectionAndClassifierMethods)
{
    SightlineQmlBridge bridge;
    // Without active connection, safe false is returned without crashing
    EXPECT_FALSE(bridge.setDetectionExtended(0, 0, 7, 0, 45, 10, 200, 15, 5, 30));
    EXPECT_FALSE(bridge.setDetectionAdvanced(0, 30, 20, 2, false, 128, 0, 50, true, 0));
    EXPECT_FALSE(bridge.setDetectionRoiLine(0, 0, 0, 50, 200, 590, 200, 1));
    EXPECT_FALSE(bridge.setDetectionRoiGrid(0, 0, 0, 16, 16, "0", "0", "0", "0", true));
    EXPECT_FALSE(bridge.triggerDetectionSnapshot(0, 0));
    EXPECT_FALSE(bridge.setClassifierSettings(0, 1, "", 3, 10, 0, 4, 3));
    EXPECT_FALSE(bridge.setComputeAssignment(true, true));
    EXPECT_FALSE(bridge.setKlvMetricBounds(0, 0.5, 20.0, 0.5, 15.0, true, false, 30.0, 40.0, -120.0, -110.0));
    EXPECT_FALSE(bridge.queryDetection(0, 0));
    EXPECT_FALSE(bridge.queryAdvDetection(0));
    EXPECT_FALSE(bridge.queryDetectionROI(0, 0));
    EXPECT_FALSE(bridge.queryKlvMetricFilters(0));
    EXPECT_FALSE(bridge.queryClassifierConfig(0));
}

TEST_F(SightlineVideoTest, ControllerPauseAndScrubbing)
{
    SightlineApp::SightlineVideoController controller {};

    // Allow worker loop to ingest frames into ring buffer
    for (int i = 0; i < 30 && controller.bufferedFrameCount() < 5; ++i) {
        QTest::qWait(30);
    }

    EXPECT_FALSE(controller.isPaused());
    EXPECT_GT(controller.bufferedFrameCount(), 0);

    // Toggle pause
    controller.togglePause();
    EXPECT_TRUE(controller.isPaused());

    // Check PTS retention
    EXPECT_GT(controller.currentFramePts(), 0ULL);

    // Scrub backward
    controller.setScrubOffset(-2);
    EXPECT_EQ(controller.scrubOffset(), -2);

    // Resume live
    controller.pauseStream(false);
    EXPECT_FALSE(controller.isPaused());
    EXPECT_EQ(controller.scrubOffset(), 0);

    controller.stopStream();
}

TEST_F(SightlineVideoTest, VideoQuickItemCoordinateMappingAndLasso)
{
    TestableVideoQuickItem item {};
    item.setSize(QSizeF(800.0, 600.0));

    QImage testFrame(640, 480, QImage::Format_RGB888);
    testFrame.fill(Qt::black);
    item.updateFrame(testFrame);

    EXPECT_TRUE(item.hasFrame());
    EXPECT_EQ(item.videoWidth(), 640);
    EXPECT_EQ(item.videoHeight(), 480);

    const QRectF content = item.contentRect();
    EXPECT_DOUBLE_EQ(content.width(), 800.0);
    EXPECT_DOUBLE_EQ(content.height(), 600.0);

    const QPointF mappedPoint = item.mapToVideo(QPointF(400.0, 300.0));
    EXPECT_NEAR(mappedPoint.x(), 320.0, 1.0);
    EXPECT_NEAR(mappedPoint.y(), 240.0, 1.0);

    const QRectF mappedRect = item.mapToVideoRect(QRectF(200.0, 150.0, 400.0, 300.0));
    EXPECT_NEAR(mappedRect.x(), 160.0, 1.0);
    EXPECT_NEAR(mappedRect.y(), 120.0, 1.0);
    EXPECT_NEAR(mappedRect.width(), 320.0, 1.0);
    EXPECT_NEAR(mappedRect.height(), 240.0, 1.0);

    // Test InteractionMode property
    EXPECT_EQ(item.interactionMode(), SightlineApp::VideoQuickItem::InteractionMode::None);
    item.setInteractionMode(SightlineApp::VideoQuickItem::InteractionMode::LassoAcquire);
    EXPECT_EQ(item.interactionMode(), SightlineApp::VideoQuickItem::InteractionMode::LassoAcquire);

    // Test simulated mouse lasso drag: press at (200, 150), move to (400, 300), release
    QSignalSpy targetSpy(&item, &SightlineApp::VideoQuickItem::targetAcquired);

    QMouseEvent pressEv(QEvent::MouseButtonPress, QPointF(200.0, 150.0), QPointF(200.0, 150.0), Qt::LeftButton,
        Qt::LeftButton, Qt::NoModifier);
    item.mousePressEvent(&pressEv);
    EXPECT_TRUE(item.isLassoActive());

    QMouseEvent moveEv(QEvent::MouseMove, QPointF(400.0, 300.0), QPointF(400.0, 300.0), Qt::LeftButton, Qt::LeftButton,
        Qt::NoModifier);
    item.mouseMoveEvent(&moveEv);
    EXPECT_TRUE(item.isLassoActive());
    EXPECT_FALSE(item.lassoRect().isEmpty());

    QMouseEvent releaseEv(QEvent::MouseButtonRelease, QPointF(400.0, 300.0), QPointF(400.0, 300.0), Qt::LeftButton,
        Qt::LeftButton, Qt::NoModifier);
    item.mouseReleaseEvent(&releaseEv);
    EXPECT_FALSE(item.isLassoActive());

    EXPECT_EQ(targetSpy.count(), 1);
    const auto args = targetSpy.takeFirst();
    const int col = args.at(0).toInt();
    const int row = args.at(1).toInt();
    const int w = args.at(2).toInt();
    const int h = args.at(3).toInt();

    // Box was (200, 150) to (400, 300) -> in video coords: (160, 120) to (320, 240)
    // Width = 160, Height = 120, Center = (240, 180)
    EXPECT_NEAR(col, 240, 2);
    EXPECT_NEAR(row, 180, 2);
    EXPECT_NEAR(w, 160, 2);
    EXPECT_NEAR(h, 120, 2);

    // Test ClickToTrack mode
    item.setInteractionMode(SightlineApp::VideoQuickItem::InteractionMode::ClickToTrack);
    QSignalSpy clickSpy(&item, &SightlineApp::VideoQuickItem::targetAcquired);
    QMouseEvent clickReleaseEv(QEvent::MouseButtonRelease, QPointF(400.0, 300.0), QPointF(400.0, 300.0), Qt::LeftButton,
        Qt::LeftButton, Qt::NoModifier);
    item.mouseReleaseEvent(&clickReleaseEv);
    EXPECT_EQ(clickSpy.count(), 1);
    const auto clickArgs = clickSpy.takeFirst();
    EXPECT_NEAR(clickArgs.at(0).toInt(), 320, 2);
    EXPECT_NEAR(clickArgs.at(1).toInt(), 240, 2);
}

TEST_F(SightlineVideoTest, BridgeTrackingParameters)
{
    SightlineQmlBridge bridge {};
    EXPECT_FALSE(bridge.setTrackingParameters(0, 0, 0, 15, 0, 0, 0, 0, 0));
    EXPECT_FALSE(bridge.queryTrackingParameters(0));
}

TEST_F(SightlineVideoTest, RecordingFileListModelLifecycle)
{
    RecordingFileListModel model {};
    EXPECT_EQ(model.rowCount(), 0);

    // Formatter helpers
    EXPECT_EQ(RecordingFileListModel::formatFileSize(500ULL), QStringLiteral("500 B"));
    EXPECT_EQ(RecordingFileListModel::formatFileSize(2048ULL), QStringLiteral("2.0 KB"));
    EXPECT_EQ(RecordingFileListModel::formatFileSize(10485760ULL), QStringLiteral("10.0 MB"));
    EXPECT_EQ(RecordingFileListModel::formatFileSize(1073741824ULL), QStringLiteral("1.00 GB"));

    EXPECT_EQ(RecordingFileListModel::formatTypeName(0U), QStringLiteral("MPEG-TS"));
    EXPECT_EQ(RecordingFileListModel::formatTypeName(1U), QStringLiteral("MP4 (fMP4)"));
    EXPECT_EQ(RecordingFileListModel::formatTypeName(2U), QStringLiteral("JPEG Still"));

    EXPECT_EQ(RecordingFileListModel::formatTimestamp(0ULL), QStringLiteral("---"));
    EXPECT_NE(RecordingFileListModel::formatTimestamp(1609459200000000ULL), QStringLiteral("---"));

    // Populate entries
    std::vector<Sightline::DirListEntry> entries {};
    Sightline::DirListEntry e1 {};
    e1.filename = "flight_0001.ts";
    e1.fileSizeBytes = 104857600ULL;
    e1.timestampUs = 1609459200000000ULL;
    e1.isPinned = false;
    e1.formatType = 0U;

    Sightline::DirListEntry e2 {};
    e2.filename = "snap_0001.jpg";
    e2.fileSizeBytes = 204800ULL;
    e2.timestampUs = 1609459300000000ULL;
    e2.isPinned = true;
    e2.formatType = 2U;

    entries.push_back(e1);
    entries.push_back(e2);

    model.updateEntries(entries);
    EXPECT_EQ(model.rowCount(), 2);

    const QModelIndex idx0 = model.index(0, 0);
    EXPECT_EQ(model.data(idx0, RecordingFileListModel::FilenameRole).toString(), QStringLiteral("flight_0001.ts"));
    EXPECT_EQ(model.data(idx0, RecordingFileListModel::FileSizeBytesRole).toULongLong(), 104857600ULL);
    EXPECT_FALSE(model.data(idx0, RecordingFileListModel::IsPinnedRole).toBool());
    EXPECT_EQ(model.data(idx0, RecordingFileListModel::FormatTypeRole).toInt(), 0);
    EXPECT_EQ(model.data(idx0, RecordingFileListModel::FormatStringRole).toString(), QStringLiteral("MPEG-TS"));

    // Pinning
    model.setFilePinned(QStringLiteral("flight_0001.ts"), true);
    EXPECT_TRUE(model.data(idx0, RecordingFileListModel::IsPinnedRole).toBool());

    // Removal
    model.removeEntry(QStringLiteral("flight_0001.ts"));
    EXPECT_EQ(model.rowCount(), 1);
    EXPECT_EQ(model.data(model.index(0, 0), RecordingFileListModel::FilenameRole).toString(),
        QStringLiteral("snap_0001.jpg"));

    // Append
    model.appendEntries({ e1 });
    EXPECT_EQ(model.rowCount(), 2);

    model.clear();
    EXPECT_EQ(model.rowCount(), 0);
}

TEST_F(SightlineVideoTest, BridgeRecordingPropertiesAndValidation)
{
    SightlineQmlBridge bridge {};
    EXPECT_FALSE(bridge.isRecordingActive());
    EXPECT_EQ(bridge.freeStorageMB(), 0);
    EXPECT_EQ(bridge.usedStorageMB(), 0);
    EXPECT_DOUBLE_EQ(bridge.storageUsagePercent(), 0.0);
    EXPECT_EQ(bridge.currentBitrateKbps(), 0);
    EXPECT_EQ(bridge.droppedFrames(), 0);
    EXPECT_EQ(bridge.elapsedRecordingSec(), 0);
    EXPECT_TRUE(bridge.currentFilename().isEmpty());
    EXPECT_TRUE(bridge.lastRecordingEvent().isEmpty());
    EXPECT_TRUE(bridge.lastAckStatus().isEmpty());
    ASSERT_NE(bridge.recordingFileListModel(), nullptr);

    // Filename validation
    const auto validRes = bridge.validateFilename(QStringLiteral("mission_flight"));
    EXPECT_TRUE(validRes[QStringLiteral("valid")].toBool());
    EXPECT_TRUE(validRes[QStringLiteral("error")].toString().isEmpty());

    // Ending with digit 0-9 violates Sightline rollover convention
    const auto invalidDigit = bridge.validateFilename(QStringLiteral("mission_flight01"));
    EXPECT_FALSE(invalidDigit[QStringLiteral("valid")].toBool());
    EXPECT_FALSE(invalidDigit[QStringLiteral("error")].toString().isEmpty());

    // Disconnected operations should safely return false without crashing
    EXPECT_FALSE(bridge.startRecordingV2(0, QStringLiteral("test"), 0, 0, 0, 0, true));
    EXPECT_FALSE(bridge.stopRecordingV2(0));
    EXPECT_FALSE(bridge.captureSnapshotV2(0, QStringLiteral("snap"), 0, 85, true));
    EXPECT_FALSE(bridge.requestDirectoryListing(0, 0, 20, QStringLiteral("")));
    EXPECT_FALSE(bridge.pinStorageFile(0, QStringLiteral("file.ts"), true));
    EXPECT_FALSE(bridge.deleteStorageFile(0, QStringLiteral("file.ts")));
}

TEST_F(SightlineVideoTest, CooperativeStopStreamRapidCycling)
{
    SightlineApp::SightlineVideoController controller {};

    // Rapid start/stop cycling to ensure no thread leaks, deadlocks, or termination crashes
    for (int i = 0; i < 10; ++i) {
        controller.startStream();
        QTest::qWait(20);
        controller.stopStream();
        EXPECT_EQ(controller.playbackState(), SightlineApp::SightlineVideoController::PlaybackState::Idle);
    }
}

TEST_F(SightlineVideoTest, StopStreamResponsiveDuringActiveStream)
{
    SightlineApp::SightlineVideoController controller {};
    QTest::qWait(100);
    EXPECT_EQ(controller.playbackState(), SightlineApp::SightlineVideoController::PlaybackState::Playing);

    const auto start = std::chrono::steady_clock::now();
    controller.stopStream();
    const auto elapsed
        = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

    EXPECT_EQ(controller.playbackState(), SightlineApp::SightlineVideoController::PlaybackState::Idle);
    EXPECT_LT(elapsed, 2500);
}

} // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
