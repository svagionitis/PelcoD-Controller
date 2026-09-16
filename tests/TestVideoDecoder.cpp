/// @file TestVideoDecoder.cpp
/// @brief Automated unit test suite for the in-tree PelcoDVideo library and components.

#include "AtomicTripleBuffer.h"
#include "BrailleRenderer.h"
#include "DecoderFactory.h"
#include "DecoderTypes.h"
#include "DeviceEnumerator.h"
#include "MockVideoDecoder.h"
#include "QVideoStreamWorker.h"
#if defined(PELCOD_HAS_FILTERS)
#include "VideoFilters.h"
#endif

#include <QCoreApplication>
#include <QImage>
#include <QSignalSpy>
#include <QTest>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <iostream>
#include <thread>

using namespace PelcoD::Video;

class TestVideoDecoder : public QObject {
    Q_OBJECT

private slots:
    void testMockDecoderLifecycle();
    void testMockDecoderSeeking();
    void testAtomicTripleBuffer();
    void testDecoderFactory();
    void testVideoStreamWorkerMockStream();
    void testSourceTypeDetection();
    void testDeviceEnumeration();
    void testLoopPlaybackControl();
    void testLetterboxMath();
    void testCompassHeadingCalculations();
    void testBrailleRendererUtf8();
    void testBrailleRendererLumaAndPalette();
    void testBrailleRendererGridRasterization();
    void testFrameProcessorPipeline();
#if defined(PELCOD_HAS_FILTERS)
    void testVideoFiltersNullSafety();
    void testFalseColorThermalPalettes();
    void testLocalAreaProcessingAndClahe();
    void testTemporalDenoise();
    void testDarkChannelDehaze();
    void testImageStabilizationEIS();
    void testAutoWhiteBalance();
    void testChromaticAberrationCorrection();
    void testIsothermFilter();
    void testHotspotTrackerFilter();
    void testMovingTargetIndicatorFilter();
    void testTacticalReticleOverlayFilter();
    void testOpticalFlowFieldFilter();
    void testCentroidTargetTrackerFilter();
    void testPerimeterTripwireFilter();
    void testMotionHeatmapFilter();
    void testPrivacyMaskFilter();
    void testTimestampWatermarkFilter();
    void testTelemetryOsdFilter();
    void testPictureInPictureFilter();
    void testVideoFiltersPipelineIntegration();
    void testConcurrentProcessorReconfiguration();
#endif
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
    const bool gotFrames = QTest::qWaitFor([&]() { return spyFrames.count() >= 3; }, 2000);

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

void TestVideoDecoder::testSourceTypeDetection()
{
    // Test pattern URIs
    QCOMPARE(detectSourceType("mock://smpte-bars"), SourceType::MockPattern);
    QCOMPARE(detectSourceType("mock://test"), SourceType::MockPattern);

    // Network / RTSP feeds
    QCOMPARE(detectSourceType("rtsp://192.168.1.100:554/live"), SourceType::Rtsp);
    QCOMPARE(detectSourceType("rtmp://stream.example.com/live/feed"), SourceType::Rtsp);
    QCOMPARE(detectSourceType("http://192.168.1.100/video.mjpg"), SourceType::Rtsp);
    QCOMPARE(detectSourceType("udp://239.255.0.1:1234"), SourceType::Rtsp);
    QCOMPARE(detectSourceType("tcp://127.0.0.1:8000"), SourceType::Rtsp);

    // Hardware capture devices
    QCOMPARE(detectSourceType("video=Integrated Camera"), SourceType::Device);
    QCOMPARE(detectSourceType("video:0"), SourceType::Device);
    QCOMPARE(detectSourceType("device://default"), SourceType::Device);
    QCOMPARE(detectSourceType("dshow:video=USB Webcam"), SourceType::Device);
    QCOMPARE(detectSourceType("/dev/video0"), SourceType::Device);
    QCOMPARE(detectSourceType("/dev/video1"), SourceType::Device);

    // Local multimedia files
    QCOMPARE(detectSourceType("sample.mp4"), SourceType::File);
    QCOMPARE(detectSourceType("C:/Videos/recording.mkv"), SourceType::File);
    QCOMPARE(detectSourceType("test_clip.avi"), SourceType::File);
    QCOMPARE(detectSourceType("/var/media/camera_dump.ts"), SourceType::File);
}

void TestVideoDecoder::testDeviceEnumeration()
{
    // Ensure hardware device enumeration runs safely without throwing or crashing
    const auto devices = PelcoD::Video::DeviceEnumerator::enumerateDevices();
    for (const auto& dev : devices) {
        QVERIFY(!dev.name.empty());
        QVERIFY(!dev.path.empty());
    }
}

void TestVideoDecoder::testLoopPlaybackControl()
{
    PelcoDQt::QVideoStreamWorker worker;
    // Verify default is loop = true
    QVERIFY(worker.isLoopPlayback());

    worker.setLoopPlayback(false);
    QVERIFY(!worker.isLoopPlayback());

    worker.setLoopPlayback(true);
    QVERIFY(worker.isLoopPlayback());
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
        while (deg < 0.0)
            deg += 360.0;
        while (deg >= 360.0)
            deg -= 360.0;
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

void TestVideoDecoder::testBrailleRendererUtf8()
{
    using videodecoder::BrailleRenderer;

    // Dot mask 0 should produce empty Braille pattern U+2800 (\xE2\xA0\x80)
    const std::string emptyBraille = BrailleRenderer::utf8BrailleChar(0x00);
    QCOMPARE(emptyBraille, std::string("\xE2\xA0\x80"));

    // Dot mask 0xFF should produce full 8-dot Braille pattern U+28FF (\xE2\xA3\xBF)
    const std::string fullBraille = BrailleRenderer::utf8BrailleChar(0xFF);
    QCOMPARE(fullBraille, std::string("\xE2\xA3\xBF"));

    // Dot 1 (bit 0) -> U+2801 (\xE2\xA0\x81)
    QCOMPARE(BrailleRenderer::utf8BrailleChar(0x01), std::string("\xE2\xA0\x81"));

    // Dot 8 (bit 7) -> U+2880 (\xE2\xA2\x80)
    QCOMPARE(BrailleRenderer::utf8BrailleChar(0x80), std::string("\xE2\xA2\x80"));
}

void TestVideoDecoder::testBrailleRendererLumaAndPalette()
{
    using videodecoder::BrailleRenderer;
    using videodecoder::TuiColorPalette;

    // ITU-R BT.601 luminance checks
    QCOMPARE(BrailleRenderer::calculateLuma(0, 0, 0), 0);
    QCOMPARE(BrailleRenderer::calculateLuma(255, 255, 255), 255);
    QCOMPARE(BrailleRenderer::calculateLuma(255, 0, 0), 76);
    QCOMPARE(BrailleRenderer::calculateLuma(0, 255, 0), 149);
    QCOMPARE(BrailleRenderer::calculateLuma(0, 0, 255), 29);

    // Color palette simulation
    std::uint8_t outR = 0, outG = 0, outB = 0;

    // TrueColor pass-through
    BrailleRenderer::applyPalette(TuiColorPalette::TrueColor, 100, 150, 200, outR, outG, outB);
    QCOMPARE(outR, 100);
    QCOMPARE(outG, 150);
    QCOMPARE(outB, 200);

    // Amber phosphor tint
    BrailleRenderer::applyPalette(TuiColorPalette::Amber, 255, 255, 255, outR, outG, outB);
    QCOMPARE(outR, 255);
    QCOMPARE(outG, 176);
    QCOMPARE(outB, 0);

    // Cyan HUD tint
    BrailleRenderer::applyPalette(TuiColorPalette::CyanHud, 255, 255, 255, outR, outG, outB);
    QCOMPARE(outR, 0);
    QCOMPARE(outG, 230);
    QCOMPARE(outB, 255);
}

void TestVideoDecoder::testBrailleRendererGridRasterization()
{
    using videodecoder::BrailleRenderer;
    using videodecoder::BrailleRenderOptions;
    using videodecoder::DitherAlgorithm;
    using videodecoder::TerminalPixelCell;
    using videodecoder::TuiRenderMode;

    // Create synthetic 20x20 test image (left half black, right half white)
    std::vector<std::uint8_t> rgbData(20 * 20 * 3, 0);
    for (int y = 0; y < 20; ++y) {
        for (int x = 10; x < 20; ++x) {
            const std::size_t idx = static_cast<std::size_t>((y * 20 + x) * 3);
            rgbData[idx] = 255;
            rgbData[idx + 1U] = 255;
            rgbData[idx + 2U] = 255;
        }
    }

    std::vector<TerminalPixelCell> cells;

    // Test with Bayer 4x4
    BrailleRenderOptions opts;
    opts.mode = TuiRenderMode::Braille;
    opts.dither = DitherAlgorithm::Bayer4x4;

    BrailleRenderer::renderFrame(rgbData.data(), 20, 20, 10, 5, opts, cells);
    QCOMPARE(static_cast<int>(cells.size()), 50);

    // Leftmost cell (pure black) should be empty Braille
    QCOMPARE(cells[0].utf8Text, std::string("\xE2\xA0\x80"));
    // Rightmost cell (pure white) should be full Braille
    QCOMPARE(cells[9].utf8Text, std::string("\xE2\xA3\xBF"));

    // Test with Half-Block mode
    opts.mode = TuiRenderMode::HalfBlock;
    BrailleRenderer::renderFrame(rgbData.data(), 20, 20, 10, 5, opts, cells);
    QCOMPARE(static_cast<int>(cells.size()), 50);
    QVERIFY(cells[0].hasBg);
    QCOMPARE(cells[0].utf8Text, std::string("\xE2\x96\x80"));
}

namespace {

class InvertColorTestProcessor : public IFrameProcessor {
public:
    void process(std::uint8_t* data, int width, int height, PixelFormat /*format*/) override
    {
        const std::size_t totalBytes = static_cast<std::size_t>(width * height * 3);
        for (std::size_t i = 0U; i < totalBytes; ++i) {
            data[i] = static_cast<std::uint8_t>(255U - data[i]);
        }
    }
};

class BrightnessOffsetTestProcessor : public IFrameProcessor {
public:
    explicit BrightnessOffsetTestProcessor(int offset)
        : m_offset(offset)
    {
    }

    void process(std::uint8_t* data, int width, int height, PixelFormat /*format*/) override
    {
        const std::size_t totalBytes = static_cast<std::size_t>(width * height * 3);
        for (std::size_t i = 0U; i < totalBytes; ++i) {
            const int val = static_cast<int>(data[i]) + m_offset;
            data[i] = static_cast<std::uint8_t>(std::clamp(val, 0, 255));
        }
    }

private:
    int m_offset { 0 };
};

} // namespace

void TestVideoDecoder::testFrameProcessorPipeline()
{
    MockVideoDecoder decoder;
    QVERIFY(decoder.initialize("mock://test", PixelFormat::RGB24));

    // Decode baseline frame 0 without processors
    decoder.seek(0.0);
    QVERIFY(decoder.decodeNextFrame());
    const FrameInfo raw0 = decoder.getRawFrameData();
    const std::uint8_t origR = raw0.data[0];
    const std::uint8_t origG = raw0.data[1];
    const std::uint8_t origB = raw0.data[2];

    // Register invert color processor and re-decode frame 0
    auto invertProc = std::make_shared<InvertColorTestProcessor>();
    decoder.addFrameProcessor(invertProc);

    decoder.seek(0.0);
    QVERIFY(decoder.decodeNextFrame());
    const FrameInfo raw1 = decoder.getRawFrameData();
    QCOMPARE(raw1.data[0], static_cast<std::uint8_t>(255U - origR));
    QCOMPARE(raw1.data[1], static_cast<std::uint8_t>(255U - origG));
    QCOMPARE(raw1.data[2], static_cast<std::uint8_t>(255U - origB));

    // Test sequential chaining: add brightness offset +10 and re-decode frame 0
    auto brightProc = std::make_shared<BrightnessOffsetTestProcessor>(10);
    decoder.addFrameProcessor(brightProc);

    decoder.seek(0.0);
    QVERIFY(decoder.decodeNextFrame());
    const FrameInfo raw2 = decoder.getRawFrameData();
    const std::uint8_t expectedR = static_cast<std::uint8_t>(std::clamp(static_cast<int>(255U - origR) + 10, 0, 255));
    QCOMPARE(raw2.data[0], expectedR);

    // Clear processors and verify restoration of original values on frame 0
    decoder.clearFrameProcessors();
    decoder.seek(0.0);
    QVERIFY(decoder.decodeNextFrame());
    const FrameInfo raw3 = decoder.getRawFrameData();
    QCOMPARE(raw3.data[0], origR);
    QCOMPARE(raw3.data[1], origG);
    QCOMPARE(raw3.data[2], origB);

    // Verify integration with QVideoStreamWorker
    QVideoStreamWorker worker;
    worker.addFrameProcessor(invertProc);
    QSignalSpy spyFrames(&worker, &QVideoStreamWorker::frameReady);
    worker.openStream("mock://test", BackendType::Mock);

    QVERIFY(spyFrames.wait(2000));
    QVERIFY(spyFrames.count() >= 1);
    worker.stopPlayback();
}

#if defined(PELCOD_HAS_FILTERS)
void TestVideoDecoder::testVideoFiltersNullSafety()
{
    BrightnessContrastFilter bcFilter;
    GaussianBlurFilter blurFilter;
    EdgeDetectionFilter edgeFilter;
    ClaheFilter claheFilter;
    FalseColorFilter falseColorFilter;
    LocalAreaProcessingFilter lapFilter;
    HistogramEqualizationFilter histFilter;
    TemporalDenoiseFilter denoiseFilter;
    LensDistortionFilter lensFilter;
    DarkChannelDehazeFilter dehazeFilter;
    ImageStabilizationFilter stabFilter;
    WhiteBalanceFilter wbFilter;
    ChromaticAberrationFilter caFilter;

    // Verify null data pointers do not crash
    bcFilter.process(nullptr, 0, 0, PixelFormat::RGB24);
    blurFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    edgeFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    claheFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    falseColorFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    lapFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    histFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    denoiseFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    lensFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    dehazeFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    stabFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    wbFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    caFilter.process(nullptr, 640, 360, PixelFormat::RGB24);

    IsothermFilter isoFilter;
    HotspotTrackerFilter spotFilter;
    MovingTargetIndicatorFilter mtiFilter;
    TacticalReticleOverlayFilter reticleFilter;
    OpticalFlowFieldFilter flowFilter;
    CentroidTargetTrackerFilter trackerFilter;
    PerimeterTripwireFilter tripwireFilter;
    MotionHeatmapFilter heatmapFilter;
    PrivacyMaskFilter privacyFilter;
    TimestampWatermarkFilter watermarkFilter;
    TelemetryOsdFilter telemetryFilter;
    PictureInPictureFilter pipFilter;

    isoFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    spotFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    mtiFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    reticleFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    flowFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    trackerFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    tripwireFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    heatmapFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    privacyFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    watermarkFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    telemetryFilter.process(nullptr, 640, 360, PixelFormat::RGB24);
    pipFilter.process(nullptr, 640, 360, PixelFormat::RGB24);

    // Verify non-positive dimensions do not crash
    std::vector<std::uint8_t> dummy(1024U, 128U);
    bcFilter.process(dummy.data(), -1, 10, PixelFormat::RGB24);
    blurFilter.process(dummy.data(), 10, -1, PixelFormat::RGB24);
    edgeFilter.process(dummy.data(), 0, 0, PixelFormat::RGB24);
    claheFilter.process(dummy.data(), -5, -5, PixelFormat::RGB24);
    falseColorFilter.process(dummy.data(), 0, 10, PixelFormat::RGB24);
    lapFilter.process(dummy.data(), 10, 0, PixelFormat::RGB24);
    histFilter.process(dummy.data(), -1, -1, PixelFormat::RGB24);
    denoiseFilter.process(dummy.data(), 0, 0, PixelFormat::RGB24);
    lensFilter.process(dummy.data(), 0, 0, PixelFormat::RGB24);
    dehazeFilter.process(dummy.data(), 0, 0, PixelFormat::RGB24);
    stabFilter.process(dummy.data(), -1, 0, PixelFormat::RGB24);
    wbFilter.process(dummy.data(), 0, -1, PixelFormat::RGB24);
    caFilter.process(dummy.data(), -1, -1, PixelFormat::RGB24);
    isoFilter.process(dummy.data(), -1, 0, PixelFormat::RGB24);
    spotFilter.process(dummy.data(), 0, -1, PixelFormat::RGB24);
    mtiFilter.process(dummy.data(), -1, -1, PixelFormat::RGB24);
    reticleFilter.process(dummy.data(), 0, 0, PixelFormat::RGB24);
    flowFilter.process(dummy.data(), -1, -1, PixelFormat::RGB24);
    trackerFilter.process(dummy.data(), 0, -1, PixelFormat::RGB24);
    tripwireFilter.process(dummy.data(), -1, 0, PixelFormat::RGB24);
    heatmapFilter.process(dummy.data(), 0, 0, PixelFormat::RGB24);
    privacyFilter.process(dummy.data(), -1, 0, PixelFormat::RGB24);
    watermarkFilter.process(dummy.data(), 0, -1, PixelFormat::RGB24);
    telemetryFilter.process(dummy.data(), -1, -1, PixelFormat::RGB24);
    pipFilter.process(dummy.data(), 0, 0, PixelFormat::RGB24);

    QVERIFY(true);
}

void TestVideoDecoder::testFalseColorThermalPalettes()
{
    // Generate a 256x1 synthetic gray ramp (R=i, G=i, B=i)
    std::vector<std::uint8_t> origBuf(256U * 3U);
    for (std::size_t i = 0U; i < 256U; ++i) {
        origBuf[i * 3U + 0U] = static_cast<std::uint8_t>(i);
        origBuf[i * 3U + 1U] = static_cast<std::uint8_t>(i);
        origBuf[i * 3U + 2U] = static_cast<std::uint8_t>(i);
    }

    // 1. WhiteHot (grayscale preservation)
    {
        std::vector<std::uint8_t> buf = origBuf;
        FalseColorFilter filter(FalseColorPalette::WhiteHot);
        filter.process(buf.data(), 256, 1, PixelFormat::RGB24);
        QCOMPARE(buf[0], static_cast<std::uint8_t>(0));
        QCOMPARE(buf[255U * 3U], static_cast<std::uint8_t>(255));
    }

    // 2. BlackHot (grayscale inversion: 0 becomes 255, 255 becomes 0)
    {
        std::vector<std::uint8_t> buf = origBuf;
        FalseColorFilter filter(FalseColorPalette::BlackHot);
        filter.process(buf.data(), 256, 1, PixelFormat::RGB24);
        QCOMPARE(buf[0], static_cast<std::uint8_t>(255));
        QCOMPARE(buf[255U * 3U], static_cast<std::uint8_t>(0));
    }

    // 3. Iron256 colormap verification
    {
        std::vector<std::uint8_t> buf = origBuf;
        FalseColorFilter filter(FalseColorPalette::Iron256);
        filter.process(buf.data(), 256, 1, PixelFormat::RGB24);
        const bool differentColors
            = (buf[0] != buf[255U * 3U] || buf[1] != buf[255U * 3U + 1U] || buf[2] != buf[255U * 3U + 2U]);
        QVERIFY(differentColors);
    }

    // 4. Custom User Palette via interpolation
    {
        std::vector<std::uint8_t> buf = origBuf;
        FalseColorFilter filter(FalseColorPalette::UserPalette);

        std::map<uint8_t, std::vector<uint8_t>> controlPoints;
        controlPoints[0] = { 0, 0, 255 }; // Blue cold
        controlPoints[128] = { 0, 255, 0 }; // Green mid
        controlPoints[255] = { 255, 0, 0 }; // Red hot
        filter.generateInterpolatedPalette(controlPoints, false);

        filter.process(buf.data(), 256, 1, PixelFormat::RGB24);
        // Pixel 0 should be blue
        QCOMPARE(buf[0], static_cast<std::uint8_t>(0));
        QCOMPARE(buf[1], static_cast<std::uint8_t>(0));
        QCOMPARE(buf[2], static_cast<std::uint8_t>(255));
        // Pixel 255 should be red
        QCOMPARE(buf[255U * 3U + 0U], static_cast<std::uint8_t>(255));
        QCOMPARE(buf[255U * 3U + 1U], static_cast<std::uint8_t>(0));
        QCOMPARE(buf[255U * 3U + 2U], static_cast<std::uint8_t>(0));
    }
}

void TestVideoDecoder::testLocalAreaProcessingAndClahe()
{
    // Generate low-contrast 64x64 synthetic image (values between 100 and 110)
    const int w = 64;
    const int h = 64;
    std::vector<std::uint8_t> lowContrast(static_cast<std::size_t>(w * h * 3));
    for (std::size_t i = 0U; i < lowContrast.size(); i += 3U) {
        const std::uint8_t val = static_cast<std::uint8_t>(100U + static_cast<unsigned int>((i / 3U) % 11U));
        lowContrast[i] = val;
        lowContrast[i + 1U] = val;
        lowContrast[i + 2U] = val;
    }

    // Test CLAHE: contrast should expand
    std::vector<std::uint8_t> claheBuf = lowContrast;
    ClaheFilter clahe(4.0, 8, 1.0);
    clahe.process(claheBuf.data(), w, h, PixelFormat::RGB24);

    std::uint8_t minVal = 255;
    std::uint8_t maxVal = 0;
    for (std::size_t i = 0U; i < claheBuf.size(); i += 3U) {
        minVal = std::min(minVal, claheBuf[i]);
        maxVal = std::max(maxVal, claheBuf[i]);
    }
    // Dynamic range should be wider than original [100, 110]
    QVERIFY(minVal < 100 || maxVal > 110);

    // Test LAP: Local Area Processing executes cleanly
    std::vector<std::uint8_t> lapBuf = lowContrast;
    LocalAreaProcessingFilter lap(3, 0.7, 2.0);
    lap.process(lapBuf.data(), w, h, PixelFormat::RGB24);
    QVERIFY(!lapBuf.empty());
}

void TestVideoDecoder::testTemporalDenoise()
{
    const int w = 32;
    const int h = 32;
    const std::size_t numBytes = static_cast<std::size_t>(w * h * 3);

    // Frame 1: uniform 128
    std::vector<std::uint8_t> frame1(numBytes, 128U);
    TemporalDenoiseFilter denoise(0.5, 30.0);
    denoise.process(frame1.data(), w, h, PixelFormat::RGB24);
    // History initialized to 128
    QCOMPARE(frame1[0], static_cast<std::uint8_t>(128));

    // Frame 2: small noise on pixel 0 (value 136, diff = 8 < threshold 30)
    std::vector<std::uint8_t> frame2(numBytes, 128U);
    frame2[0] = 136U;
    denoise.process(frame2.data(), w, h, PixelFormat::RGB24);
    // Should be averaged towards history: 0.5 * 128 + 0.5 * 136 = 132
    QCOMPARE(frame2[0], static_cast<std::uint8_t>(132));

    // Frame 3: large motion on pixel 0 (value 230, diff = 98 > threshold 30 across channels)
    std::vector<std::uint8_t> frame3(numBytes, 128U);
    frame3[0] = 230U;
    frame3[1] = 230U;
    frame3[2] = 230U;
    denoise.process(frame3.data(), w, h, PixelFormat::RGB24);
    // Motion thresholding keeps the new value without temporal blur
    QCOMPARE(frame3[0], static_cast<std::uint8_t>(230));
    QCOMPARE(frame3[1], static_cast<std::uint8_t>(230));
    QCOMPARE(frame3[2], static_cast<std::uint8_t>(230));

    // Test reset
    denoise.reset();
}

void TestVideoDecoder::testDarkChannelDehaze()
{
    DarkChannelDehazeFilter dehaze(0.90, 5, 0.15);
    QVERIFY(qFuzzyCompare(dehaze.getOmega(), 0.90));
    QCOMPARE(dehaze.getPatchSize(), 5);
    QVERIFY(qFuzzyCompare(dehaze.getT0(), 0.15));

    dehaze.setOmega(0.85);
    QVERIFY(qFuzzyCompare(dehaze.getOmega(), 0.85));
    dehaze.setPatchSize(7);
    QCOMPARE(dehaze.getPatchSize(), 7);
    dehaze.setT0(0.10);
    QVERIFY(qFuzzyCompare(dehaze.getT0(), 0.10));

    // Create a foggy synthetic image (high minimum luma, low contrast: values between 180 and 220)
    const int w = 64;
    const int h = 64;
    const std::size_t numBytes = static_cast<std::size_t>(w * h * 3);
    std::vector<std::uint8_t> foggy(numBytes);
    for (std::size_t i = 0U; i < numBytes; ++i) {
        foggy[i] = static_cast<std::uint8_t>(180U + (i % 41U));
    }

    dehaze.process(foggy.data(), w, h, PixelFormat::RGB24);

    std::uint8_t minVal = 255U;
    std::uint8_t maxVal = 0U;
    for (std::size_t i = 0U; i < numBytes; ++i) {
        minVal = std::min(minVal, foggy[i]);
        maxVal = std::max(maxVal, foggy[i]);
    }

    // Baseline minimum was 180; after dehazing minVal should drop significantly penetrating haze
    QVERIFY(minVal < 160U);
    QVERIFY(maxVal >= 180U);
}

void TestVideoDecoder::testImageStabilizationEIS()
{
    ImageStabilizationFilter stab(0.85, 25.0, 0.05);
    QVERIFY(qFuzzyCompare(stab.getSmoothingFactor(), 0.85));
    QVERIFY(qFuzzyCompare(stab.getMaxJitterPixels(), 25.0));
    QVERIFY(qFuzzyCompare(stab.getCropMarginPercent(), 0.05));

    stab.setSmoothingFactor(0.75);
    QVERIFY(qFuzzyCompare(stab.getSmoothingFactor(), 0.75));
    stab.setMaxJitterPixels(40.0);
    QVERIFY(qFuzzyCompare(stab.getMaxJitterPixels(), 40.0));
    stab.setCropMarginPercent(0.06);
    QVERIFY(qFuzzyCompare(stab.getCropMarginPercent(), 0.06));

    const int w = 128;
    const int h = 128;

    auto createPattern = [w, h](int offsetX, int offsetY) -> std::vector<std::uint8_t> {
        std::vector<std::uint8_t> img(static_cast<std::size_t>(w * h * 3), 0U);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const int px = (x + offsetX) / 16;
                const int py = (y + offsetY) / 16;
                const std::uint8_t val = ((px + py) % 2 == 0) ? 240U : 20U;
                const std::size_t idx = static_cast<std::size_t>((y * w + x) * 3);
                img[idx + 0U] = val;
                img[idx + 1U] = val;
                img[idx + 2U] = val;
            }
        }
        return img;
    };

    std::vector<std::uint8_t> f1 = createPattern(0, 0);
    stab.process(f1.data(), w, h, PixelFormat::RGB24);

    std::vector<std::uint8_t> f2 = createPattern(3, 2);
    stab.process(f2.data(), w, h, PixelFormat::RGB24);

    std::vector<std::uint8_t> f3 = createPattern(-2, -1);
    stab.process(f3.data(), w, h, PixelFormat::RGB24);

    QVERIFY(!f3.empty());
    stab.reset();
}

void TestVideoDecoder::testAutoWhiteBalance()
{
    WhiteBalanceFilter wb(WhiteBalanceFilter::Mode::GrayWorld, 1.0);
    QCOMPARE(wb.getMode(), WhiteBalanceFilter::Mode::GrayWorld);
    QVERIFY(qFuzzyCompare(wb.getStrength(), 1.0));

    wb.setMode(WhiteBalanceFilter::Mode::WhitePatch);
    QCOMPARE(wb.getMode(), WhiteBalanceFilter::Mode::WhitePatch);
    wb.setStrength(0.85);
    QVERIFY(qFuzzyCompare(wb.getStrength(), 0.85));

    // Test GrayWorld with warm color tint (R=200, G=100, B=50)
    wb.setMode(WhiteBalanceFilter::Mode::GrayWorld);
    wb.setStrength(1.0);
    const int w = 32;
    const int h = 32;
    const std::size_t numBytes = static_cast<std::size_t>(w * h * 3);
    std::vector<std::uint8_t> tinted(numBytes);
    for (std::size_t i = 0U; i < numBytes; i += 3U) {
        tinted[i + 0U] = 200U;
        tinted[i + 1U] = 100U;
        tinted[i + 2U] = 50U;
    }

    wb.process(tinted.data(), w, h, PixelFormat::RGB24);

    // After GrayWorld, R should decrease and B should increase
    QVERIFY(tinted[0] < 200U);
    QVERIFY(tinted[2] > 50U);

    // Test WhitePatch mode
    wb.setMode(WhiteBalanceFilter::Mode::WhitePatch);
    std::vector<std::uint8_t> patchImg(numBytes, 50U);
    for (std::size_t i = 0U; i < 30U; i += 3U) {
        patchImg[i + 0U] = 240U;
        patchImg[i + 1U] = 180U;
        patchImg[i + 2U] = 120U;
    }
    wb.process(patchImg.data(), w, h, PixelFormat::RGB24);
    QVERIFY(!patchImg.empty());
}

void TestVideoDecoder::testChromaticAberrationCorrection()
{
    ChromaticAberrationFilter ca(0.008, -0.008, 0.05, -0.05);
    QVERIFY(qFuzzyCompare(ca.getRedCoeff(), 0.008));
    QVERIFY(qFuzzyCompare(ca.getBlueCoeff(), -0.008));
    QVERIFY(qFuzzyCompare(ca.getCenterOffsetX(), 0.05));
    QVERIFY(qFuzzyCompare(ca.getCenterOffsetY(), -0.05));

    ca.setParameters(0.003, -0.003, 0.0, 0.0);
    QVERIFY(qFuzzyCompare(ca.getRedCoeff(), 0.003));
    QVERIFY(qFuzzyCompare(ca.getBlueCoeff(), -0.003));
    QVERIFY(qFuzzyCompare(ca.getCenterOffsetX(), 0.0));
    QVERIFY(qFuzzyCompare(ca.getCenterOffsetY(), 0.0));

    // Process a 64x64 checkerboard image
    const int w = 64;
    const int h = 64;
    const std::size_t numBytes = static_cast<std::size_t>(w * h * 3);
    std::vector<std::uint8_t> img(numBytes);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::uint8_t val = ((x / 8 + y / 8) % 2 == 0) ? 255U : 0U;
            const std::size_t idx = static_cast<std::size_t>((y * w + x) * 3);
            img[idx + 0U] = val;
            img[idx + 1U] = val;
            img[idx + 2U] = val;
        }
    }

    ca.setParameters(0.01, -0.01);
    ca.process(img.data(), w, h, PixelFormat::RGB24);

    QVERIFY(!img.empty());
}

void TestVideoDecoder::testIsothermFilter()
{
    IsothermFilter iso(140, 180, IsothermFilter::HighlightColor::Red, true);
    QCOMPARE(iso.getLowThreshold(), 140);
    QCOMPARE(iso.getHighThreshold(), 180);
    QCOMPARE(iso.getHighlightColor(), IsothermFilter::HighlightColor::Red);
    QVERIFY(iso.isWhiteHotBackground());

    iso.setPreset(IsothermFilter::Preset::HumanBody);
    QCOMPARE(iso.getPreset(), IsothermFilter::Preset::HumanBody);
    QCOMPARE(iso.getLowThreshold(), 140);
    QCOMPARE(iso.getHighThreshold(), 180);
    QCOMPARE(iso.getHighlightColor(), IsothermFilter::HighlightColor::Amber);

    iso.setPreset(IsothermFilter::Preset::HighHeat);
    QCOMPARE(iso.getPreset(), IsothermFilter::Preset::HighHeat);
    QCOMPARE(iso.getLowThreshold(), 200);
    QCOMPARE(iso.getHighThreshold(), 255);
    QCOMPARE(iso.getHighlightColor(), IsothermFilter::HighlightColor::Red);

    iso.setThresholds(100, 150);
    QCOMPARE(iso.getPreset(), IsothermFilter::Preset::Custom);
    QCOMPARE(iso.getLowThreshold(), 100);
    QCOMPARE(iso.getHighThreshold(), 150);
    iso.setHighlightColor(IsothermFilter::HighlightColor::Cyan);
    QCOMPARE(iso.getHighlightColor(), IsothermFilter::HighlightColor::Cyan);
    iso.setWhiteHotBackground(false);
    QVERIFY(!iso.isWhiteHotBackground());

    // Process a 2x1 image: pixel 0 inside isotherm (luma 120), pixel 1 outside (luma 50)
    iso.setWhiteHotBackground(true);
    iso.setHighlightColor(IsothermFilter::HighlightColor::Red);
    std::vector<std::uint8_t> frame = {
        120U, 120U, 120U, // Pixel 0 (inside 100-150)
        50U, 50U, 50U // Pixel 1 (outside)
    };

    iso.process(frame.data(), 2, 1, PixelFormat::RGB24);
    // Pixel 0 should be Red alert
    QCOMPARE(frame[0], static_cast<std::uint8_t>(255));
    QCOMPARE(frame[1], static_cast<std::uint8_t>(0));
    QCOMPARE(frame[2], static_cast<std::uint8_t>(0));
    // Pixel 1 should be monochrome luma 50
    QCOMPARE(frame[3], static_cast<std::uint8_t>(50));
    QCOMPARE(frame[4], static_cast<std::uint8_t>(50));
    QCOMPARE(frame[5], static_cast<std::uint8_t>(50));
}

void TestVideoDecoder::testHotspotTrackerFilter()
{
    HotspotTrackerFilter tracker(true, 16);
    QVERIFY(tracker.getShowOverlay());
    QCOMPARE(tracker.getCenterBoxSize(), 16);

    tracker.setShowOverlay(false);
    QVERIFY(!tracker.getShowOverlay());
    tracker.setCenterBoxSize(20);
    QCOMPARE(tracker.getCenterBoxSize(), 20);

    // Create 64x64 test image with uniform luma 100
    const int w = 64;
    const int h = 64;
    std::vector<std::uint8_t> img(static_cast<std::size_t>(w * h * 3), 100U);

    // Hotspot at (12, 18)
    const std::size_t hotIdx = static_cast<std::size_t>((18 * w + 12) * 3);
    img[hotIdx + 0U] = 250U;
    img[hotIdx + 1U] = 250U;
    img[hotIdx + 2U] = 250U;

    // Coldspot at (45, 52)
    const std::size_t coldIdx = static_cast<std::size_t>((52 * w + 45) * 3);
    img[coldIdx + 0U] = 10U;
    img[coldIdx + 1U] = 10U;
    img[coldIdx + 2U] = 10U;

    tracker.process(img.data(), w, h, PixelFormat::RGB24);

    const auto stats = tracker.getStats();
    QCOMPARE(stats.hotX, 12);
    QCOMPARE(stats.hotY, 18);
    QCOMPARE(stats.hotVal, static_cast<std::uint8_t>(250));
    QCOMPARE(stats.coldX, 45);
    QCOMPARE(stats.coldY, 52);
    QCOMPARE(stats.coldVal, static_cast<std::uint8_t>(10));
}

void TestVideoDecoder::testMovingTargetIndicatorFilter()
{
    MovingTargetIndicatorFilter mti(50, 10000, 8);
    QCOMPARE(mti.getMinArea(), 50);
    QCOMPARE(mti.getMaxArea(), 10000);
    QCOMPARE(mti.getMaxTargets(), 8);

    mti.setMinArea(40);
    QCOMPARE(mti.getMinArea(), 40);
    mti.setMaxArea(8000);
    QCOMPARE(mti.getMaxArea(), 8000);
    mti.setMaxTargets(12);
    QCOMPARE(mti.getMaxTargets(), 12);

    const int w = 128;
    const int h = 128;
    std::vector<std::uint8_t> staticFrame(static_cast<std::size_t>(w * h * 3), 120U);

    // Feed 5 static frames to build MOG2 background model
    for (int i = 0; i < 5; ++i) {
        std::vector<std::uint8_t> f = staticFrame;
        mti.process(f.data(), w, h, PixelFormat::RGB24);
    }

    // Frame 6: introduce large moving object (20x20 block) at (40, 40)
    std::vector<std::uint8_t> movingFrame = staticFrame;
    for (int y = 40; y < 60; ++y) {
        for (int x = 40; x < 60; ++x) {
            const std::size_t idx = static_cast<std::size_t>((y * w + x) * 3);
            movingFrame[idx + 0U] = 255U;
            movingFrame[idx + 1U] = 255U;
            movingFrame[idx + 2U] = 255U;
        }
    }

    mti.process(movingFrame.data(), w, h, PixelFormat::RGB24);
    QVERIFY(mti.getTargetCount() >= 1U);
    const auto targets = mti.getTargets();
    QVERIFY(!targets.empty());
    QVERIFY(targets[0].width > 0);
    QVERIFY(targets[0].height > 0);

    mti.reset();
    QCOMPARE(mti.getTargetCount(), 0ULL);
}

void TestVideoDecoder::testTacticalReticleOverlayFilter()
{
    TacticalReticleOverlayFilter reticle(
        TacticalReticleOverlayFilter::Style::Crosshair, TacticalReticleOverlayFilter::Color::TacticalGreen, 1, 12);

    QCOMPARE(reticle.getStyle(), TacticalReticleOverlayFilter::Style::Crosshair);
    QCOMPARE(reticle.getColor(), TacticalReticleOverlayFilter::Color::TacticalGreen);
    QCOMPARE(reticle.getLineThickness(), 1);
    QCOMPARE(reticle.getDeadbandGap(), 12);

    reticle.setStyle(TacticalReticleOverlayFilter::Style::MilDot);
    QCOMPARE(reticle.getStyle(), TacticalReticleOverlayFilter::Style::MilDot);
    reticle.setColor(TacticalReticleOverlayFilter::Color::Red);
    QCOMPARE(reticle.getColor(), TacticalReticleOverlayFilter::Color::Red);
    reticle.setLineThickness(2);
    QCOMPARE(reticle.getLineThickness(), 2);
    reticle.setDeadbandGap(14);
    QCOMPARE(reticle.getDeadbandGap(), 14);

    const int w = 64;
    const int h = 64;

    // Test all styles for rendering stability
    const auto styles = { TacticalReticleOverlayFilter::Style::Crosshair, TacticalReticleOverlayFilter::Style::MilDot,
        TacticalReticleOverlayFilter::Style::Stadiametric, TacticalReticleOverlayFilter::Style::CornerBrackets };

    for (const auto s : styles) {
        std::vector<std::uint8_t> frame(static_cast<std::size_t>(w * h * 3), 0U);
        reticle.setStyle(s);
        reticle.process(frame.data(), w, h, PixelFormat::RGB24);

        // Verify reticle drew non-zero lines
        bool hasPixels = false;
        for (const auto b : frame) {
            if (b > 0U) {
                hasPixels = true;
                break;
            }
        }
        QVERIFY(hasPixels);
    }
}

void TestVideoDecoder::testOpticalFlowFieldFilter()
{
    OpticalFlowFieldFilter flow(OpticalFlowFieldFilter::DisplayMode::VectorArrows, 16, 1.5, 2.0);
    QCOMPARE(flow.getDisplayMode(), OpticalFlowFieldFilter::DisplayMode::VectorArrows);
    QCOMPARE(flow.getGridStep(), 16);
    QVERIFY(qFuzzyCompare(flow.getMinVelocity(), 1.5));
    QVERIFY(qFuzzyCompare(flow.getArrowScale(), 2.0));

    flow.setDisplayMode(OpticalFlowFieldFilter::DisplayMode::ColorFlow);
    QCOMPARE(flow.getDisplayMode(), OpticalFlowFieldFilter::DisplayMode::ColorFlow);
    flow.setGridStep(20);
    QCOMPARE(flow.getGridStep(), 20);
    flow.setMinVelocity(2.0);
    QVERIFY(qFuzzyCompare(flow.getMinVelocity(), 2.0));
    flow.setArrowScale(1.5);
    QVERIFY(qFuzzyCompare(flow.getArrowScale(), 1.5));

    const int w = 64;
    const int h = 64;
    auto createMovingPattern = [w, h](int shiftX) {
        std::vector<std::uint8_t> frame(static_cast<std::size_t>(w * h * 3), 0U);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const std::uint8_t val = ((x + shiftX) % 16 < 8) ? 200U : 40U;
                const std::size_t idx = static_cast<std::size_t>((y * w + x) * 3);
                frame[idx + 0U] = val;
                frame[idx + 1U] = val;
                frame[idx + 2U] = val;
            }
        }
        return frame;
    };

    auto f1 = createMovingPattern(0);
    flow.process(f1.data(), w, h, PixelFormat::RGB24);

    auto f2 = createMovingPattern(4);
    flow.process(f2.data(), w, h, PixelFormat::RGB24);

    flow.setDisplayMode(OpticalFlowFieldFilter::DisplayMode::VectorArrows);
    auto f3 = createMovingPattern(8);
    flow.process(f3.data(), w, h, PixelFormat::RGB24);

    QVERIFY(!f3.empty());
    flow.reset();
}

void TestVideoDecoder::testCentroidTargetTrackerFilter()
{
    CentroidTargetTrackerFilter tracker(true, 40, 40);
    QVERIFY(tracker.isAutoAcquire());
    tracker.setAutoAcquire(false);
    QVERIFY(!tracker.isAutoAcquire());

    tracker.acquireTarget(20, 20, 30, 30);
    QVERIFY(tracker.isTargetLocked());
    auto state = tracker.getTargetState();
    QCOMPARE(state.x, 20);
    QCOMPARE(state.y, 20);
    QCOMPARE(state.width, 30);
    QCOMPARE(state.height, 30);
    QVERIFY(qFuzzyCompare(state.confidence, 1.0));

    tracker.releaseTarget();
    QVERIFY(!tracker.isTargetLocked());

    const int w = 128;
    const int h = 128;
    auto createFrameWithBox = [w, h](int bx, int by) {
        std::vector<std::uint8_t> frame(static_cast<std::size_t>(w * h * 3), 50U);
        for (int y = by; y < by + 30 && y < h; ++y) {
            for (int x = bx; x < bx + 30 && x < w; ++x) {
                const std::size_t idx = static_cast<std::size_t>((y * w + x) * 3);
                frame[idx + 0U] = 240U;
                frame[idx + 1U] = 240U;
                frame[idx + 2U] = 240U;
            }
        }
        return frame;
    };

    tracker.setAutoAcquire(true);
    tracker.setMaxCoastFrames(15);
    QCOMPARE(tracker.getMaxCoastFrames(), 15);
    tracker.setProcessNoise(0.01, 0.1);
    tracker.setMeasurementNoise(0.1);

    auto f1 = createFrameWithBox(20, 20);
    tracker.process(f1.data(), w, h, PixelFormat::RGB24);

    auto f2 = createFrameWithBox(26, 24);
    tracker.process(f2.data(), w, h, PixelFormat::RGB24);

    auto f3 = createFrameWithBox(32, 28);
    tracker.process(f3.data(), w, h, PixelFormat::RGB24);

    QVERIFY(!f3.empty());
    auto trackedState = tracker.getTargetState();
    QVERIFY(trackedState.locked);
    QVERIFY(!trackedState.isCoasting);
    QVERIFY(trackedState.vx > 0.0);
    QVERIFY(trackedState.vy > 0.0);

    // Test Latency Lookahead Prediction
    auto lookaheadState = tracker.getTargetState(0.10);
    QVERIFY(lookaheadState.predictedErrorX > lookaheadState.errorX);
    QVERIFY(lookaheadState.predictedErrorY > lookaheadState.errorY);

    // Test Occlusion Coasting: Target disappears behind obstacle
    std::vector<std::uint8_t> blankFrame(static_cast<std::size_t>(w * h * 3), 50U);
    tracker.process(blankFrame.data(), w, h, PixelFormat::RGB24);

    auto coastState = tracker.getTargetState();
    QVERIFY(coastState.locked);
    QVERIFY(coastState.isCoasting);
    QVERIFY(coastState.x >= trackedState.x); // Kalman continues predictive trajectory

    // Feed blank frames exceeding maxCoastFrames (15 frames) -> Lock must be gracefully released
    for (int i = 0; i < 20; ++i) {
        tracker.process(blankFrame.data(), w, h, PixelFormat::RGB24);
    }
    QVERIFY(!tracker.isTargetLocked());
}

void TestVideoDecoder::testPerimeterTripwireFilter()
{
    PerimeterTripwireFilter trip(0.1, 0.5, 0.9, 0.5, PerimeterTripwireFilter::Direction::Bidirectional);
    double x1, y1, x2, y2;
    trip.getTripwire(x1, y1, x2, y2);
    QVERIFY(qFuzzyCompare(x1, 0.1));
    QVERIFY(qFuzzyCompare(y1, 0.5));
    QVERIFY(qFuzzyCompare(x2, 0.9));
    QVERIFY(qFuzzyCompare(y2, 0.5));
    QCOMPARE(trip.getDirection(), PerimeterTripwireFilter::Direction::Bidirectional);

    trip.setTripwire(0.0, 0.5, 1.0, 0.5);
    trip.setDirection(PerimeterTripwireFilter::Direction::A_to_B);
    QCOMPARE(trip.getDirection(), PerimeterTripwireFilter::Direction::A_to_B);
    QCOMPARE(trip.getIntrusionCount(), 0ULL);
    QVERIFY(!trip.hasAlarm());

    const int w = 128;
    const int h = 128;
    auto createTargetFrame = [w, h](int cy) {
        std::vector<std::uint8_t> frame(static_cast<std::size_t>(w * h * 3), 40U);
        for (int y = cy - 10; y < cy + 10 && y < h; ++y) {
            for (int x = 54; x < 74 && x < w; ++x) {
                const std::size_t idx = static_cast<std::size_t>((y * w + x) * 3);
                frame[idx + 0U] = 230U;
                frame[idx + 1U] = 230U;
                frame[idx + 2U] = 230U;
            }
        }
        return frame;
    };

    trip.setDirection(PerimeterTripwireFilter::Direction::Bidirectional);
    auto f1 = createTargetFrame(40);
    trip.process(f1.data(), w, h, PixelFormat::RGB24);

    auto f2 = createTargetFrame(75);
    trip.process(f2.data(), w, h, PixelFormat::RGB24);

    QVERIFY(trip.getIntrusionCount() >= 1ULL);
    QVERIFY(trip.hasAlarm());

    trip.resetIntrusionCount();
    QCOMPARE(trip.getIntrusionCount(), 0ULL);
    QVERIFY(!trip.hasAlarm());
}

void TestVideoDecoder::testMotionHeatmapFilter()
{
    MotionHeatmapFilter heatmap(0.95, 0.40, 20);
    QVERIFY(qFuzzyCompare(heatmap.getDecayFactor(), 0.95));
    QVERIFY(qFuzzyCompare(heatmap.getOpacity(), 0.40));
    QCOMPARE(heatmap.getThreshold(), 20);

    heatmap.setDecayFactor(0.90);
    QVERIFY(qFuzzyCompare(heatmap.getDecayFactor(), 0.90));
    heatmap.setOpacity(0.50);
    QVERIFY(qFuzzyCompare(heatmap.getOpacity(), 0.50));
    heatmap.setThreshold(25);
    QCOMPARE(heatmap.getThreshold(), 25);

    const int w = 64;
    const int h = 64;
    std::vector<std::uint8_t> baseFrame(static_cast<std::size_t>(w * h * 3), 100U);
    heatmap.process(baseFrame.data(), w, h, PixelFormat::RGB24);

    std::vector<std::uint8_t> motionFrame = baseFrame;
    for (int y = 0; y < 20; ++y) {
        for (int x = 0; x < 20; ++x) {
            const std::size_t idx = static_cast<std::size_t>((y * w + x) * 3);
            motionFrame[idx + 0U] = 220U;
            motionFrame[idx + 1U] = 220U;
            motionFrame[idx + 2U] = 220U;
        }
    }

    heatmap.process(motionFrame.data(), w, h, PixelFormat::RGB24);
    QVERIFY(!motionFrame.empty());
    heatmap.reset();
}

void TestVideoDecoder::testPrivacyMaskFilter()
{
    const int w = 64;
    const int h = 64;
    std::vector<std::uint8_t> frame(static_cast<std::size_t>(w * h * 3), 255U); // Pure white

    PrivacyMaskFilter filter(PrivacyMaskFilter::ConcealmentMode::Blackout);
    filter.setMaskColor(0U, 0U, 0U); // Black
    filter.setBlurKernelSize(25);
    filter.setMosaicBlockSize(8);
    QCOMPARE(filter.getBlurKernelSize(), 25);
    QCOMPARE(filter.getMosaicBlockSize(), 8);

    // Add zone covering top-left quadrant [0..32, 0..32]
    int zoneId = filter.addZone(0.0, 0.0, 0.5, 0.5, PrivacyMaskFilter::ConcealmentMode::Blackout, "Zone1");
    QVERIFY(zoneId > 0);
    QCOMPARE(filter.getZones().size(), 1U);

    filter.process(frame.data(), w, h, PixelFormat::RGB24);

    // Pixel inside zone (10, 10) must be black
    std::size_t insideIdx = static_cast<std::size_t>((10 * w + 10) * 3);
    QCOMPARE(frame[insideIdx + 0U], 0U);
    QCOMPARE(frame[insideIdx + 1U], 0U);
    QCOMPARE(frame[insideIdx + 2U], 0U);

    // Pixel outside zone (50, 50) must remain white
    std::size_t outsideIdx = static_cast<std::size_t>((50 * w + 50) * 3);
    QCOMPARE(frame[outsideIdx + 0U], 255U);
    QCOMPARE(frame[outsideIdx + 1U], 255U);
    QCOMPARE(frame[outsideIdx + 2U], 255U);

    // Test zone disabling
    filter.setZoneEnabled(zoneId, false);
    std::fill(frame.begin(), frame.end(), 255U);
    filter.process(frame.data(), w, h, PixelFormat::RGB24);
    QCOMPARE(frame[insideIdx + 0U], 255U);

    // Test Mosaic mode
    filter.setZoneEnabled(zoneId, true);
    PrivacyMaskFilter::PrivacyZone z;
    z.id = zoneId;
    z.xNorm = 0.0;
    z.yNorm = 0.0;
    z.widthNorm = 0.5;
    z.heightNorm = 0.5;
    z.mode = PrivacyMaskFilter::ConcealmentMode::Mosaic;
    filter.clearZones();
    filter.addZone(z);
    filter.process(frame.data(), w, h, PixelFormat::RGB24);
    QVERIFY(!frame.empty());

    // Test Blur mode
    filter.setDefaultMode(PrivacyMaskFilter::ConcealmentMode::Blur);
    QCOMPARE(filter.getDefaultMode(), PrivacyMaskFilter::ConcealmentMode::Blur);

    // Test remove zone
    QVERIFY(filter.removeZone(z.id));
    QVERIFY(filter.getZones().empty());
}

void TestVideoDecoder::testTimestampWatermarkFilter()
{
    const int w = 200;
    const int h = 100;
    std::vector<std::uint8_t> frame(static_cast<std::size_t>(w * h * 3), 0U); // Black background

    TimestampWatermarkFilter filter(TimestampWatermarkFilter::Position::TopLeft, "TEST-CAM", true, true);
    filter.setCustomTimestamp("2026-09-16 14:00:00.000 UTC");
    filter.setUseSystemClock(false);
    QVERIFY(!filter.isUsingSystemClock());
    QCOMPARE(filter.getCameraName(), std::string("TEST-CAM"));
    QCOMPARE(filter.getFrameCounter(), 0ULL);

    filter.process(frame.data(), w, h, PixelFormat::RGB24);
    QCOMPARE(filter.getFrameCounter(), 1ULL);

    // Top-left area should have scrim / text drawn (pixels no longer 0)
    bool hasDrawn = false;
    for (int y = 0; y < 25; ++y) {
        for (int x = 0; x < 80; ++x) {
            std::size_t idx = static_cast<std::size_t>((y * w + x) * 3);
            if (frame[idx + 0U] > 0U || frame[idx + 1U] > 0U || frame[idx + 2U] > 0U) {
                hasDrawn = true;
                break;
            }
        }
    }
    QVERIFY(hasDrawn);

    // Consecutive frames increment counter
    filter.process(frame.data(), w, h, PixelFormat::RGB24);
    QCOMPARE(filter.getFrameCounter(), 2ULL);

    filter.resetFrameCounter();
    QCOMPARE(filter.getFrameCounter(), 0ULL);

    // Test GPS coordinates and position
    filter.setGpsCoordinates(37.7749, -122.4194, 45.0, true);
    filter.setPosition(TimestampWatermarkFilter::Position::BottomRight);
    QCOMPARE(filter.getPosition(), TimestampWatermarkFilter::Position::BottomRight);
    filter.setColor(TimestampWatermarkFilter::Color::Amber);
    QCOMPARE(filter.getColor(), TimestampWatermarkFilter::Color::Amber);
    filter.setScrimOpacity(0.5);
    QCOMPARE(filter.getScrimOpacity(), 0.5);

    filter.process(frame.data(), w, h, PixelFormat::BGR24);
    QCOMPARE(filter.getFrameCounter(), 1ULL);
    filter.clearGpsCoordinates();
}

void TestVideoDecoder::testTelemetryOsdFilter()
{
    const int w = 320;
    const int h = 240;
    std::vector<std::uint8_t> frame(static_cast<std::size_t>(w * h * 3), 0U);

    TelemetryOsdFilter filter(TelemetryOsdFilter::Color::TacticalGreen, true, true);
    QCOMPARE(filter.getColor(), TelemetryOsdFilter::Color::TacticalGreen);
    QVERIFY(filter.getShowCompass());
    QVERIFY(filter.getShowReticleAngles());

    // Test heading cardinal formatting
    QCOMPARE(TelemetryOsdFilter::formatHeading(0.0), std::string("N"));
    QCOMPARE(TelemetryOsdFilter::formatHeading(90.0), std::string("E"));
    QCOMPARE(TelemetryOsdFilter::formatHeading(180.0), std::string("S"));
    QCOMPARE(TelemetryOsdFilter::formatHeading(270.0), std::string("W"));
    QCOMPARE(TelemetryOsdFilter::formatHeading(45.0), std::string("NE"));

    // Set telemetry data
    TelemetryOsdFilter::TelemetryData data;
    data.panDegrees = 135.0;
    data.tiltDegrees = -8.5;
    data.zoomMagnification = 12.0;
    data.horizontalFovDegrees = 5.2;
    data.sensorPayload = "EO DAYLIGHT";
    data.statusMessage = "SYS OK";
    filter.setTelemetry(data);

    TelemetryOsdFilter::TelemetryData retrieved = filter.getTelemetry();
    QCOMPARE(retrieved.panDegrees, 135.0);
    QCOMPARE(retrieved.tiltDegrees, -8.5);
    QCOMPARE(retrieved.zoomMagnification, 12.0);

    filter.process(frame.data(), w, h, PixelFormat::RGB24);

    // Compass banner at top center should have drawn non-black pixels
    bool hasCompass = false;
    for (int y = 5; y < 25; ++y) {
        for (int x = 120; x < 200; ++x) {
            std::size_t idx = static_cast<std::size_t>((y * w + x) * 3);
            if (frame[idx + 0U] > 0U || frame[idx + 1U] > 0U || frame[idx + 2U] > 0U) {
                hasCompass = true;
                break;
            }
        }
    }
    QVERIFY(hasCompass);

    filter.setPanTiltZoom(200.0, 15.0, 20.0);
    QCOMPARE(filter.getTelemetry().panDegrees, 200.0);

    filter.setColor(TelemetryOsdFilter::Color::Cyan);
    filter.setShowCompass(false);
    filter.setShowReticleAngles(false);
    QVERIFY(!filter.getShowCompass());
    QVERIFY(!filter.getShowReticleAngles());
}

void TestVideoDecoder::testPictureInPictureFilter()
{
    const int w = 200;
    const int h = 200;
    // Fill main frame with green, but center region with red
    std::vector<std::uint8_t> frame(static_cast<std::size_t>(w * h * 3), 0U);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            std::size_t idx = static_cast<std::size_t>((y * w + x) * 3);
            if (x >= 80 && x <= 120 && y >= 80 && y <= 120) {
                frame[idx + 0U] = 255U; // Red
                frame[idx + 1U] = 0U;
                frame[idx + 2U] = 0U;
            } else {
                frame[idx + 0U] = 0U;
                frame[idx + 1U] = 200U; // Green
                frame[idx + 2U] = 0U;
            }
        }
    }

    PictureInPictureFilter pip(
        PictureInPictureFilter::Mode::DigitalZoom, PictureInPictureFilter::Corner::TopRight, 0.30, 2.0);
    QCOMPARE(pip.getMode(), PictureInPictureFilter::Mode::DigitalZoom);
    QCOMPARE(pip.getCorner(), PictureInPictureFilter::Corner::TopRight);
    QCOMPARE(pip.getScaleRatio(), 0.30);
    QCOMPARE(pip.getDigitalZoomFactor(), 2.0);
    QVERIFY(pip.getShowBadge());

    pip.process(frame.data(), w, h, PixelFormat::RGB24);

    // In top-right corner (approx x=130..190, y=10..60), center red pixels should be rendered!
    bool hasZoomedRed = false;
    for (int y = 20; y < 60; ++y) {
        for (int x = 140; x < 185; ++x) {
            std::size_t idx = static_cast<std::size_t>((y * w + x) * 3);
            if (frame[idx + 0U] > 200U && frame[idx + 1U] < 50U) {
                hasZoomedRed = true;
                break;
            }
        }
    }
    QVERIFY(hasZoomedRed);

    // Test SecondaryFeed mode
    std::vector<std::uint8_t> secFrame(static_cast<std::size_t>(50 * 50 * 3), 0U);
    for (std::size_t i = 0U; i < 50U * 50U; ++i) {
        secFrame[i * 3U + 2U] = 255U; // Blue
    }

    pip.setMode(PictureInPictureFilter::Mode::SecondaryFeed);
    pip.setCorner(PictureInPictureFilter::Corner::BottomLeft);
    pip.setSecondaryFrame(secFrame.data(), 50, 50, PixelFormat::RGB24);

    pip.process(frame.data(), w, h, PixelFormat::RGB24);

    // Bottom-left corner (x=15..60, y=140..185) should have blue secondary feed
    bool hasSecBlue = false;
    for (int y = 145; y < 180; ++y) {
        for (int x = 20; x < 55; ++x) {
            std::size_t idx = static_cast<std::size_t>((y * w + x) * 3);
            if (frame[idx + 2U] > 200U && frame[idx + 0U] < 50U) {
                hasSecBlue = true;
                break;
            }
        }
    }
    QVERIFY(hasSecBlue);
    pip.clearSecondaryFrame();
}

void TestVideoDecoder::testVideoFiltersPipelineIntegration()
{
    MockVideoDecoder decoder;
    QVERIFY(decoder.initialize("mock://test", PixelFormat::RGB24));

    // Decode baseline frame 0
    decoder.seek(0.0);
    QVERIFY(decoder.decodeNextFrame());
    const FrameInfo baseFrame = decoder.getRawFrameData();
    std::vector<std::uint8_t> baseBytes(baseFrame.data, baseFrame.data + baseFrame.size);

    // Add OpenCV tactical filter
    auto falseColor = std::make_shared<FalseColorFilter>(FalseColorPalette::Jet);
    decoder.addFrameProcessor(falseColor);

    decoder.seek(0.0);
    QVERIFY(decoder.decodeNextFrame());
    const FrameInfo filteredFrame = decoder.getRawFrameData();

    bool hasDifference = false;
    for (std::size_t i = 0U; i < filteredFrame.size; ++i) {
        if (filteredFrame.data[i] != baseBytes[i]) {
            hasDifference = true;
            break;
        }
    }
    QVERIFY(hasDifference);

    decoder.close();
}

void TestVideoDecoder::testConcurrentProcessorReconfiguration()
{
    MockVideoDecoder decoder;
    QVERIFY(decoder.initialize("mock://test", PixelFormat::RGB24));

    std::atomic<bool> running { true };
    std::atomic<int> framesDecoded { 0 };

    std::thread decodeThread([&]() {
        while (running.load(std::memory_order_relaxed)) {
            if (decoder.decodeNextFrame()) {
                framesDecoded.fetch_add(1, std::memory_order_relaxed);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // Concurrently mutate the processor chain while decoding is actively progressing
    for (int cycle = 0; cycle < 10; ++cycle) {
        decoder.addFrameProcessor(std::make_shared<FalseColorFilter>(FalseColorPalette::Iron256));
        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        decoder.addFrameProcessor(std::make_shared<ClaheFilter>(2.5, 8, 1.0));
        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        decoder.clearFrameProcessors();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        decoder.addFrameProcessor(std::make_shared<TemporalDenoiseFilter>(0.5, 30.0));
        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        decoder.clearFrameProcessors();
    }

    running.store(false, std::memory_order_relaxed);
    decodeThread.join();

    QVERIFY(framesDecoded.load() > 10);
    decoder.close();
}
#endif

QTEST_MAIN(TestVideoDecoder)
#include "TestVideoDecoder.moc"
