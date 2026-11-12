/// @file TestStabilizerWorker.cpp
/// @brief Integration tests for StabilizerWorker and StabilizerPlayerController.

#include <cstdint>
#include <memory>

#include <QGuiApplication>
#include <QSignalSpy>
#include <QTest>
#include <gtest/gtest.h>

#include "AtomicTripleBuffer.h"
#include "StabilizerFramePacket.h"
#include "StabilizerPlayerController.h"
#include "StabilizerWorker.h"

namespace StabilizerApp {

class StabilizerWorkerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (qApp == nullptr) {
            static int argc = 1;
            static char appName[] = "TestStabilizerWorker";
            static char* argv[] = { appName, nullptr };
            static auto app = std::make_unique<QCoreApplication>(argc, argv);
        }
    }
};

TEST_F(StabilizerWorkerTest, WorkerSyntheticStreamProcessing)
{
    Video::AtomicTripleBuffer<StabilizerFramePacket> tripleBuf;
    StabilizerWorker worker(tripleBuf);

    QSignalSpy openSpy(&worker, &StabilizerWorker::sourceOpened);
    QSignalSpy frameSpy(&worker, &StabilizerWorker::frameReady);

    worker.openSource(QStringLiteral("synthetic://shaker"));

    EXPECT_EQ(openSpy.count(), 1);
    EXPECT_EQ(worker.frameWidth(), 1280U);
    EXPECT_EQ(worker.frameHeight(), 720U);

    // Process a tick
    worker.processTick();

    EXPECT_GE(frameSpy.count(), 1);
    EXPECT_TRUE(tripleBuf.swapReadBuffer());

    const auto& readSlot = tripleBuf.getReadBuffer();
    EXPECT_EQ(readSlot.width, 1280U);
    EXPECT_EQ(readSlot.height, 720U);
    EXPECT_FALSE(readSlot.rawRgb.empty());
    EXPECT_FALSE(readSlot.stabilizedRgb.empty());

    worker.stop();
    EXPECT_FALSE(worker.isRunning());
}

TEST_F(StabilizerWorkerTest, WorkerStepFrameAdvancesOnce)
{
    Video::AtomicTripleBuffer<StabilizerFramePacket> tripleBuf;
    StabilizerWorker worker(tripleBuf);

    worker.openSource(QStringLiteral("synthetic://shaker"));
    worker.pause();

    QSignalSpy frameSpy(&worker, &StabilizerWorker::frameReady);

    worker.stepFrame();
    EXPECT_EQ(frameSpy.count(), 1);

    worker.stepFrame();
    EXPECT_EQ(frameSpy.count(), 2);

    worker.stop();
}

TEST_F(StabilizerWorkerTest, ControllerIntegrationLifecycle)
{
    StabilizerPlayerController controller;

    QSignalSpy openSpy(&controller, &StabilizerPlayerController::videoDimensionsChanged);
    QSignalSpy frameSpy(&controller, &StabilizerPlayerController::frameReady);

    controller.setSourceUri(QStringLiteral("synthetic://shaker"));

    // Wait for worker thread to open source and process initial frames
    ASSERT_TRUE(openSpy.wait(2000));
    EXPECT_EQ(controller.videoWidth(), 1280);
    EXPECT_EQ(controller.videoHeight(), 720);

    // Verify frames are flowing
    ASSERT_TRUE(frameSpy.wait(2000));
    EXPECT_GT(frameSpy.count(), 0);

    EXPECT_EQ(controller.playbackState(), StabilizerPlayerController::PlaybackState::Playing);

    controller.pause();
    EXPECT_EQ(controller.playbackState(), StabilizerPlayerController::PlaybackState::Paused);

    controller.stop();
    EXPECT_EQ(controller.playbackState(), StabilizerPlayerController::PlaybackState::Idle);
}

} // namespace StabilizerApp

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    int fakeArgc = 1;
    char fakeName[] = "TestStabilizerWorker";
    char* fakeArgv[] = { fakeName, nullptr };
    QGuiApplication app(fakeArgc, fakeArgv);
    return RUN_ALL_TESTS();
}
