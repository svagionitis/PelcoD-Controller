/// @file TestLensBenchmarks.cpp
/// @brief Microsecond latency benchmarking test suite for optical algorithms in LensAppQt.

#include <cstddef>
#include <cstdint>

#include <chrono>
#include <memory>
#include <vector>

#include <QCoreApplication>
#include <QGuiApplication>
#include <gtest/gtest.h>

#include "FocusEvaluator.h"
#include "LensConfigModel.h"
#include "LensPlayerController.h"
#include "LensTelemetryRingBuffer.h"
#include "PeakFitter.h"
#include "SyntheticDefocusGenerator.h"
#include "ZoomTrackFocusEngine.h"
#include "ZoomTrackTypes.h"

namespace LensApp {

class LensBenchmarksTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (qGuiApp == nullptr) {
            static int argc { 1 };
            static char appName[] { "TestLensBenchmarks" };
            static char* argv[] { appName, nullptr };
            new QGuiApplication(argc, argv);
        }
    }
};

TEST_F(LensBenchmarksTest, SyntheticDefocusGenerationLatency)
{
    SyntheticDefocusConfig cfg { };
    cfg.width = 640U;
    cfg.height = 480U;
    cfg.pattern = DefocusPatternType::SiemensStar;
    SyntheticDefocusGenerator generator(cfg);

    std::vector<std::uint8_t> rgbBuf(cfg.width * cfg.height * 3U, 0U);

    // Warm-up
    for (std::size_t i { 0U }; i < 5U; ++i) {
        static_cast<void>(generator.generateNextFrame(rgbBuf.data(), 0.033));
    }

    constexpr std::size_t iterations { 20U };
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i { 0U }; i < iterations; ++i) {
        static_cast<void>(generator.generateNextFrame(rgbBuf.data(), 0.033));
    }
    const auto t1 = std::chrono::steady_clock::now();

    const auto totalUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    const double avgUs = static_cast<double>(totalUs) / static_cast<double>(iterations);

    EXPECT_LT(avgUs, 100000.0); // Budget < 100 ms in unoptimized Debug mode
}

TEST_F(LensBenchmarksTest, FocusEvaluatorKernelLatencies)
{
    constexpr std::size_t width { 640U };
    constexpr std::size_t height { 480U };
    std::vector<std::uint8_t> luma(width * height, 0U);

    for (std::size_t y { 0U }; y < height; ++y) {
        for (std::size_t x { 0U }; x < width; ++x) {
            luma[y * width + x] = static_cast<std::uint8_t>((x + y) & 0xFFU);
        }
    }

    LensControl::FocusEvaluator evaluator;
    constexpr std::size_t iterations { 50U };

    // Tenengrad kernel benchmark
    evaluator.setMetricType(LensControl::FocusMetricType::SobelGradientSquared);
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i { 0U }; i < iterations; ++i) {
        static_cast<void>(evaluator.evaluate(luma.data(), width, height, width, 0ULL, static_cast<std::uint32_t>(i)));
    }
    const auto t1 = std::chrono::steady_clock::now();
    const auto tenengradUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    const double avgTenengradUs = static_cast<double>(tenengradUs) / static_cast<double>(iterations);

    EXPECT_LT(avgTenengradUs, 5000.0); // Budget < 5.0 ms in Debug mode

    // DiscreteLaplacian kernel benchmark
    evaluator.setMetricType(LensControl::FocusMetricType::DiscreteLaplacian);
    const auto t2 = std::chrono::steady_clock::now();
    for (std::size_t i { 0U }; i < iterations; ++i) {
        static_cast<void>(evaluator.evaluate(luma.data(), width, height, width, 0ULL, static_cast<std::uint32_t>(i)));
    }
    const auto t3 = std::chrono::steady_clock::now();
    const auto laplacianUs = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();
    const double avgLaplacianUs = static_cast<double>(laplacianUs) / static_cast<double>(iterations);

    EXPECT_LT(avgLaplacianUs, 5000.0); // Budget < 5.0 ms in Debug mode
}

TEST_F(LensBenchmarksTest, PeakFitterGaussianLatency)
{
    using namespace LensControl;

    SamplePoint p1 { };
    p1.pos = MotorPosition { 20000U };
    p1.score.metric = 100U;

    SamplePoint p2 { };
    p2.pos = MotorPosition { 20500U };
    p2.score.metric = 450U;

    SamplePoint p3 { };
    p3.pos = MotorPosition { 21000U };
    p3.score.metric = 120U;

    constexpr std::size_t iterations { 10000U };
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i { 0U }; i < iterations; ++i) {
        const auto res = PeakFitter::fitGaussian(p1, p2, p3);
        static_cast<void>(res.isValid);
    }
    const auto t1 = std::chrono::steady_clock::now();

    const auto totalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    const double avgNs = static_cast<double>(totalNs) / static_cast<double>(iterations);

    EXPECT_LT(avgNs, 50000.0); // Budget < 50 us (50,000 ns)
}

TEST_F(LensBenchmarksTest, PchipSplineInterpolationLatency)
{
    using namespace LensControl;

    TrackCurve curve { };
    curve.knotCount = 5U;
    curve.distanceMeters = 10.0F;

    curve.knots[0].zoom = MotorPosition { 0U };
    curve.knots[0].focus = MotorPosition { 1000U };
    curve.knots[1].zoom = MotorPosition { 10000U };
    curve.knots[1].focus = MotorPosition { 3000U };
    curve.knots[2].zoom = MotorPosition { 25000U };
    curve.knots[2].focus = MotorPosition { 8000U };
    curve.knots[3].zoom = MotorPosition { 45000U };
    curve.knots[3].focus = MotorPosition { 18000U };
    curve.knots[4].zoom = MotorPosition { 65535U };
    curve.knots[4].focus = MotorPosition { 32000U };

    ZoomTrackFocusEngine engine;
    EXPECT_TRUE(engine.addCurve(curve));
    engine.setMode(InterpolationMode::PchipSpline);

    constexpr std::size_t iterations { 10000U };
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i { 0U }; i < iterations; ++i) {
        const auto f = engine.evaluateFocus(MotorPosition { static_cast<std::uint16_t>(i % 65535U) });
        static_cast<void>(f.value());
    }
    const auto t1 = std::chrono::steady_clock::now();

    const auto totalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    const double avgNs = static_cast<double>(totalNs) / static_cast<double>(iterations);

    EXPECT_LT(avgNs, 10000.0); // Budget < 10 us (10,000 ns)
}

TEST_F(LensBenchmarksTest, RingBufferAppendLatency)
{
    LensTelemetryRingBuffer ring;

    TelemetrySample sample { };
    sample.timestampNs = 1000000000ULL;
    sample.frameIndex = 1U;
    sample.channel = 0U;
    sample.focusPosition = 5000U;
    sample.zoomPosition = 10000U;
    sample.metricScore = 25.5;

    constexpr std::size_t iterations { 5000U };
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i { 0U }; i < iterations; ++i) {
        sample.frameIndex = static_cast<std::uint32_t>(i);
        ring.appendSample(sample);
    }
    const auto t1 = std::chrono::steady_clock::now();

    const auto totalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    const double avgNs = static_cast<double>(totalNs) / static_cast<double>(iterations);

    EXPECT_LT(avgNs, 5000.0); // Budget < 5 us (5,000 ns)
}

TEST_F(LensBenchmarksTest, EmergencyStopLatency)
{
    LensPlayerController controller;

    constexpr std::size_t iterations { 5U };
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i { 0U }; i < iterations; ++i) {
        controller.emergencyStop();
        QCoreApplication::processEvents();
    }
    const auto t1 = std::chrono::steady_clock::now();

    const auto totalUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    const double avgUs = static_cast<double>(totalUs) / static_cast<double>(iterations);

    EXPECT_LT(avgUs, 20000.0); // Budget < 20 ms in Debug mode
}

} // namespace LensApp

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    if (qEnvironmentVariableIsEmpty("QT_PLUGIN_PATH")) {
        qputenv("QT_PLUGIN_PATH", "C:/Qt/6.6.1/msvc2019_64/plugins");
    }
    int fakeArgc = 1;
    char fakeName[] = "TestLensBenchmarks";
    char* fakeArgv[] = { fakeName, nullptr };
    QGuiApplication app(fakeArgc, fakeArgv);
    return RUN_ALL_TESTS();
}
