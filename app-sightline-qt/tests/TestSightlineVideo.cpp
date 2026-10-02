/// @file TestSightlineVideo.cpp
/// @brief Automated unit test suite for SightlineVideoController and VideoQuickItem.

#include "SightlineQmlBridge.h"
#include "SightlineVideoController.h"
#include "TrackListModel.h"
#include "VideoQuickItem.h"

#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <gtest/gtest.h>

namespace {

class TestableVideoQuickItem : public SightlineApp::VideoQuickItem {
public:
    using SightlineApp::VideoQuickItem::mousePressEvent;
    using SightlineApp::VideoQuickItem::mouseMoveEvent;
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

    QMouseEvent pressEv(QEvent::MouseButtonPress, QPointF(200.0, 150.0), QPointF(200.0, 150.0), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    item.mousePressEvent(&pressEv);
    EXPECT_TRUE(item.isLassoActive());

    QMouseEvent moveEv(QEvent::MouseMove, QPointF(400.0, 300.0), QPointF(400.0, 300.0), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    item.mouseMoveEvent(&moveEv);
    EXPECT_TRUE(item.isLassoActive());
    EXPECT_FALSE(item.lassoRect().isEmpty());

    QMouseEvent releaseEv(QEvent::MouseButtonRelease, QPointF(400.0, 300.0), QPointF(400.0, 300.0), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
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
    QMouseEvent clickReleaseEv(QEvent::MouseButtonRelease, QPointF(400.0, 300.0), QPointF(400.0, 300.0), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
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

} // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
