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
    void testVideoFiltersPipelineIntegration();
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
#endif

QTEST_MAIN(TestVideoDecoder)
#include "TestVideoDecoder.moc"
