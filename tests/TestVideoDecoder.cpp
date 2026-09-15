/// @file TestVideoDecoder.cpp
/// @brief Automated unit test suite for the in-tree PelcoDVideo library and components.

#include "AtomicTripleBuffer.h"
#include "DecoderFactory.h"
#include "DecoderTypes.h"
#include "MockVideoDecoder.h"
#include "QVideoStreamWorker.h"

#include <QCoreApplication>
#include <QImage>
#include <QSignalSpy>
#include <QTest>
#include <cassert>
#include <cmath>
#include <iostream>

using namespace PelcoD::Video;

class TestVideoDecoder : public QObject {
    Q_OBJECT

private slots:
    void testMockDecoderLifecycle();
    void testMockDecoderSeeking();
    void testAtomicTripleBuffer();
    void testDecoderFactory();
    void testVideoStreamWorkerMockStream();
    void testLetterboxMath();
    void testCompassHeadingCalculations();
};

void TestVideoDecoder::testMockDecoderLifecycle()
{
    MockVideoDecoder decoder;
    QVERIFY(!decoder.getPerformanceStats().totalDecodedFrames);

    const bool initOk = decoder.initialize("mock://test", PixelFormat::RGB24, 1, DeviceType::CPU);
    QVERIFY(initOk);

    const VideoMetadata meta = decoder.getVideoMetadata();
    QCOMPARE(meta.width, 640);
    QCOMPARE(meta.height, 360);
    QCOMPARE(meta.frameRate, 30.0);
    QCOMPARE(meta.format, PixelFormat::RGB24);

    // Decode 5 consecutive frames
    double lastTimestamp = -1.0;
    for (int i = 0; i < 5; ++i) {
        const bool decoded = decoder.decodeNextFrame();
        QVERIFY(decoded);

        const FrameInfo frame = decoder.getRawFrameData();
        QVERIFY(frame.data != nullptr);
        QCOMPARE(frame.width, 640);
        QCOMPARE(frame.height, 360);
        QCOMPARE(frame.size, static_cast<std::size_t>(640 * 360 * 3));
        QVERIFY(frame.timestamp > lastTimestamp);
        lastTimestamp = frame.timestamp;
    }

    const DecoderPerformanceStats stats = decoder.getPerformanceStats();
    QCOMPARE(stats.totalDecodedFrames, 5ULL);
    QVERIFY(stats.averageDecodeTimeMs >= 0.0);

    decoder.close();
}

void TestVideoDecoder::testMockDecoderSeeking()
{
    MockVideoDecoder decoder;
    QVERIFY(decoder.initialize("mock://test", PixelFormat::RGB24));

    // Seek to 3.5 seconds
    QVERIFY(decoder.seek(3.5));
    QVERIFY(decoder.decodeNextFrame());

    const FrameInfo frame = decoder.getRawFrameData();
    QVERIFY(std::abs(frame.timestamp - 3.5) < 0.1);

    decoder.close();
}

void TestVideoDecoder::testAtomicTripleBuffer()
{
    AtomicTripleBuffer<FrameBufferSlot> buffer;
    QVERIFY(!buffer.hasNewFrame());

    // Producer writes slot
    auto& writeSlot = buffer.getWriteBuffer();
    writeSlot.width = 1920;
    writeSlot.height = 1080;
    writeSlot.timestamp = 1.234;

    // Publish
    buffer.publishWriteBuffer();
    QVERIFY(buffer.hasNewFrame());

    // Consumer swaps
    QVERIFY(buffer.swapReadBuffer());
    const auto& readSlot = buffer.getReadBuffer();
    QCOMPARE(readSlot.width, 1920);
    QCOMPARE(readSlot.height, 1080);
    QCOMPARE(readSlot.timestamp, 1.234);

    // Second swap should return false (no newer frame)
    QVERIFY(!buffer.swapReadBuffer());
}

void TestVideoDecoder::testDecoderFactory()
{
    const auto backends = DecoderFactory::availableBackends();
    QVERIFY(!backends.empty());

    // Mock is always present
    QVERIFY(std::find(backends.begin(), backends.end(), BackendType::Mock) != backends.end());

    auto mock = DecoderFactory::create(BackendType::Mock);
    QVERIFY(mock != nullptr);
    QVERIFY(mock->initialize("mock://test"));
    QCOMPARE(mock->getVideoMetadata().width, 640);

#if defined(PELCOD_HAS_FFMPEG)
    QVERIFY(std::find(backends.begin(), backends.end(), BackendType::FFmpeg) != backends.end());
    auto ffmpeg = DecoderFactory::create(BackendType::FFmpeg);
    QVERIFY(ffmpeg != nullptr);
#endif

#if defined(PELCOD_HAS_GSTREAMER)
    QVERIFY(std::find(backends.begin(), backends.end(), BackendType::GStreamer) != backends.end());
    auto gst = DecoderFactory::create(BackendType::GStreamer);
    QVERIFY(gst != nullptr);
#endif
}

void TestVideoDecoder::testVideoStreamWorkerMockStream()
{
    QVideoStreamWorker worker;
    QSignalSpy spyStatus(&worker, &QVideoStreamWorker::streamStatusChanged);
    QSignalSpy spyMetadata(&worker, &QVideoStreamWorker::streamMetadataReady);
    QSignalSpy spyFrames(&worker, &QVideoStreamWorker::frameReady);

    worker.openStream("mock://test", BackendType::Mock);

    // Wait for worker to connect and start streaming
    QVERIFY(spyStatus.wait(2000));
    QVERIFY(worker.isStreaming() || worker.streamState() == StreamState::Connecting);

    // Wait for at least 3 decoded frames
    const bool gotFrames = QTest::qWaitFor([&]() {
        return spyFrames.count() >= 3;
    }, 2000);

    QVERIFY(gotFrames);
    QVERIFY(!spyMetadata.isEmpty());

    // Inspect emitted QImage
    const auto frameArgs = spyFrames.first();
    const auto image = frameArgs.at(0).value<QImage>();
    QCOMPARE(image.width(), 640);
    QCOMPARE(image.height(), 360);
    QCOMPARE(image.format(), QImage::Format_RGB888);

    // Test pause / resume
    worker.pausePlayback();
    QCOMPARE(worker.streamState(), StreamState::Paused);

    worker.resumePlayback();
    QCOMPARE(worker.streamState(), StreamState::Streaming);

    // Test clean stop
    worker.stopPlayback();
    QCOMPARE(worker.streamState(), StreamState::Disconnected);
}

void TestVideoDecoder::testLetterboxMath()
{
    // 16:9 video frame inside 4:3 display (800x600) -> should be letterboxed vertically
    const double videoW = 1920.0;
    const double videoH = 1080.0;
    const double canvasW = 800.0;
    const double canvasH = 600.0;

    const double scale = std::min(canvasW / videoW, canvasH / videoH);
    const double renderedW = videoW * scale;
    const double renderedH = videoH * scale;

    QCOMPARE(renderedW, 800.0);
    QVERIFY(renderedH < 600.0); // Vertical letterboxing bars present

    const double offsetY = (canvasH - renderedH) / 2.0;
    QVERIFY(offsetY > 0.0);
}

void TestVideoDecoder::testCompassHeadingCalculations()
{
    auto headingToCardinal = [](double deg) -> const char* {
        while (deg < 0.0) deg += 360.0;
        while (deg >= 360.0) deg -= 360.0;
        static const char* kCardinals[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
        const int idx = static_cast<int>(std::floor((deg + 22.5) / 45.0)) % 8;
        return kCardinals[idx];
    };

    QCOMPARE(headingToCardinal(0.0), "N");
    QCOMPARE(headingToCardinal(45.0), "NE");
    QCOMPARE(headingToCardinal(90.0), "E");
    QCOMPARE(headingToCardinal(180.0), "S");
    QCOMPARE(headingToCardinal(270.0), "W");
    QCOMPARE(headingToCardinal(359.9), "N");
}

QTEST_MAIN(TestVideoDecoder)
#include "TestVideoDecoder.moc"
