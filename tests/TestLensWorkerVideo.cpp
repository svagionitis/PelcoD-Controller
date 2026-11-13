/// @file TestLensWorkerVideo.cpp
/// @brief Unit and concurrency tests for LensWorker video ingestion and triple-buffer publishing.

#include <cstdint>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QThread>

#include <gtest/gtest.h>

#include "AtomicTripleBuffer.h"
#include "LensFramePacket.h"
#include "LensWorker.h"

namespace LensApp {
namespace {

    class TestLensWorkerVideo : public ::testing::Test {
    protected:
        static void SetUpTestSuite()
        {
            if (qApp == nullptr) {
                static int argc = 1;
                static char appName[] = "TestLensWorkerVideo";
                static char* argv[] = { appName, nullptr };
                s_app = new QCoreApplication(argc, argv);
            }
        }

        static void TearDownTestSuite()
        {
            delete s_app;
            s_app = nullptr;
        }

    private:
        static QCoreApplication* s_app;
    };

    QCoreApplication* TestLensWorkerVideo::s_app = nullptr;

    TEST_F(TestLensWorkerVideo, TripleBufferPreallocationAndSwap)
    {
        Video::AtomicTripleBuffer<LensFramePacket> buffer;

        auto& writeSlot = buffer.getWriteBuffer();
        writeSlot.preallocate(640U, 480U);
        EXPECT_EQ(writeSlot.width, 640U);
        EXPECT_EQ(writeSlot.height, 480U);
        EXPECT_EQ(writeSlot.rgbBuffer.size(), 640U * 480U * 3U);
        EXPECT_EQ(writeSlot.lumaBuffer.size(), 640U * 480U);

        writeSlot.frameId = 42U;
        buffer.publishWriteBuffer();

        EXPECT_TRUE(buffer.swapReadBuffer());
        const auto& readSlot = buffer.getReadBuffer();
        EXPECT_EQ(readSlot.frameId, 42U);
        EXPECT_EQ(readSlot.width, 640U);
    }

    TEST_F(TestLensWorkerVideo, WorkerSyntheticIngestionCycle)
    {
        Video::AtomicTripleBuffer<LensFramePacket> buffer;
        LensWorker worker(buffer);

        QSignalSpy openedSpy(&worker, &LensWorker::sourceOpened);
        QSignalSpy frameSpy(&worker, &LensWorker::frameReady);

        worker.initWorker();
        worker.pause();

        worker.openSource(QStringLiteral("synthetic://siemens-star"));
        worker.pause();

        EXPECT_GE(openedSpy.count(), 1);

        // Step frame manually
        worker.stepFrame();
        EXPECT_GE(frameSpy.count(), 1);

        // Verify published frame in buffer
        EXPECT_TRUE(buffer.swapReadBuffer());
        const auto& packet = buffer.getReadBuffer();
        EXPECT_TRUE(packet.isSynthetic);
        EXPECT_EQ(packet.width, 1280U);
        EXPECT_EQ(packet.height, 720U);
        EXPECT_GT(packet.frameId, 0U);

        worker.stop();
    }

} // namespace
} // namespace LensApp
