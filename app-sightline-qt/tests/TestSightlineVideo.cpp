/// @file TestSightlineVideo.cpp
/// @brief Automated unit test suite for SightlineVideoController and VideoQuickItem.

#include "SightlineVideoController.h"
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

} // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
