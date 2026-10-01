/// @file TestSightlineVideo.cpp
/// @brief Automated unit test suite for SightlineVideoController and VideoQuickItem.

#include "SightlineVideoController.h"
#include "TrackListModel.h"
#include "VideoQuickItem.h"

#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <gtest/gtest.h>

namespace {

class SightlineVideoTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (qApp == nullptr) {
            static int argc = 1;
            static char appName[] = "TestSightlineVideo";
            static char* argv[] = { appName, nullptr };
            new QCoreApplication(argc, argv);
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

} // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
