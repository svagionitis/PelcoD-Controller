/// @file TestVideoStabilizerEngine.cpp
/// @brief Unit tests for VideoStabilizerEngine and StabilizerBenchmark.
#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "FrameBuffer.h"
#include "FrameView.h"
#include "StabilizerBenchmark.h"
#include "StabilizerTypes.h"
#include "TrajectoryFilter.h"
#include "VideoStabilizerEngine.h"

namespace {

using namespace Stabilizer;

/// @brief Helper to generate a textured test frame with grid corners and offset.
void generateSyntheticPattern(FrameBuffer& buffer, float shiftX, float shiftY)
{
    const FrameDimensions& dims = buffer.dimensions();
    const std::uint32_t width = dims.width();
    const std::uint32_t height = dims.height();

    for (std::uint32_t y = 0U; y < height; ++y) {
        std::uint8_t* row = buffer.rowPtr(y);
        const float sampleY = static_cast<float>(y) - shiftY;

        for (std::uint32_t x = 0U; x < width; ++x) {
            const float sampleX = static_cast<float>(x) - shiftX;

            const auto gx = static_cast<std::int32_t>(std::floor(sampleX / 24.0F));
            const auto gy = static_cast<std::int32_t>(std::floor(sampleY / 24.0F));
            const bool checker = ((gx + gy) % 2) == 0;
            std::uint8_t val = checker ? 190U : 60U;

            const float mx = std::fmod(sampleX, 24.0F);
            const float my = std::fmod(sampleY, 24.0F);
            if ((std::abs(mx - 12.0F) < 3.0F) && (std::abs(my - 12.0F) < 3.0F)) {
                val = checker ? 245U : 15U;
            }
            row[x] = val;
        }
    }
}

TEST(TestVideoStabilizerEngine, ConfigureValidation)
{
    VideoStabilizerEngine engine;
    EXPECT_FALSE(engine.isConfigured());
    EXPECT_EQ(engine.state(), EngineState::Uninitialized);

    VideoStabilizerConfig badCfg;
    badCfg.dimensions = FrameDimensions(10U, 10U); // Below minimum 16
    EXPECT_FALSE(engine.configure(badCfg));
    EXPECT_FALSE(engine.isConfigured());

    VideoStabilizerConfig validCfg;
    validCfg.dimensions = FrameDimensions(320U, 240U);
    EXPECT_TRUE(engine.configure(validCfg));
    EXPECT_TRUE(engine.isConfigured());
    EXPECT_EQ(engine.state(), EngineState::Initializing);
}

TEST(TestVideoStabilizerEngine, FirstFrameReferenceInit)
{
    const FrameDimensions dims(320U, 240U);
    VideoStabilizerConfig cfg;
    cfg.dimensions = dims;

    VideoStabilizerEngine engine;
    ASSERT_TRUE(engine.configure(cfg));

    FrameBuffer inBuf(dims);
    FrameBuffer outBuf(dims);
    generateSyntheticPattern(inBuf, 0.0F, 0.0F);

    FrameView inView(inBuf.data(), dims);
    FrameView outView(outBuf.data(), dims);

    const bool success = engine.processFrame(inView, outView, 1.0F / 30.0F);
    EXPECT_TRUE(success);
    EXPECT_EQ(engine.state(), EngineState::Tracking);

    const auto diag = engine.diagnostics();
    EXPECT_EQ(diag.frameIndex, 1U);
    EXPECT_GT(diag.detectedFeatures, 5U);
    EXPECT_FLOAT_EQ(diag.rawTransform.tx, 0.0F);
    EXPECT_FLOAT_EQ(diag.rawTransform.ty, 0.0F);
}

TEST(TestVideoStabilizerEngine, PureStationaryNoMotion)
{
    const FrameDimensions dims(320U, 240U);
    VideoStabilizerConfig cfg;
    cfg.dimensions = dims;

    VideoStabilizerEngine engine;
    ASSERT_TRUE(engine.configure(cfg));

    FrameBuffer inBuf(dims);
    FrameBuffer outBuf(dims);
    generateSyntheticPattern(inBuf, 0.0F, 0.0F);

    FrameView inView(inBuf.data(), dims);
    FrameView outView(outBuf.data(), dims);

    for (std::size_t i = 0U; i < 20U; ++i) {
        ASSERT_TRUE(engine.processFrame(inView, outView, 1.0F / 30.0F));
    }

    const auto diag = engine.diagnostics();
    EXPECT_EQ(engine.state(), EngineState::Tracking);
    EXPECT_NEAR(diag.rawTransform.tx, 0.0F, 0.5F);
    EXPECT_NEAR(diag.rawTransform.ty, 0.0F, 0.5F);
    EXPECT_NEAR(diag.correctionTransform.tx, 0.0F, 1.0F);
    EXPECT_NEAR(diag.correctionTransform.ty, 0.0F, 1.0F);
}

TEST(TestVideoStabilizerEngine, ConstantVelocityPan)
{
    const FrameDimensions dims(320U, 240U);
    VideoStabilizerConfig cfg;
    cfg.dimensions = dims;
    cfg.filter.maxCropMargin = CropRatio(0.15F);

    VideoStabilizerEngine engine;
    ASSERT_TRUE(engine.configure(cfg));

    FrameBuffer inBuf(dims);
    FrameBuffer outBuf(dims);

    for (std::size_t i = 0U; i < 25U; ++i) {
        const float panX = static_cast<float>(i) * 1.5F;
        generateSyntheticPattern(inBuf, panX, 0.0F);

        FrameView inView(inBuf.data(), dims);
        FrameView outView(outBuf.data(), dims);

        ASSERT_TRUE(engine.processFrame(inView, outView, 1.0F / 30.0F));
    }

    const auto diag = engine.diagnostics();
    EXPECT_EQ(engine.state(), EngineState::Tracking);
    EXPECT_NEAR(diag.rawTransform.tx, 1.5F, 1.0F);
}

TEST(TestVideoStabilizerEngine, PtzFeedforwardTelemetry)
{
    const FrameDimensions dims(320U, 240U);
    VideoStabilizerConfig cfg;
    cfg.dimensions = dims;

    VideoStabilizerEngine engine;
    ASSERT_TRUE(engine.configure(cfg));

    FrameBuffer inBuf(dims);
    FrameBuffer outBuf(dims);
    generateSyntheticPattern(inBuf, 0.0F, 0.0F);

    FrameView inView(inBuf.data(), dims);
    FrameView outView(outBuf.data(), dims);

    // Initial frame
    ASSERT_TRUE(engine.processFrame(inView, outView, 1.0F / 30.0F));

    // Send telemetry update
    PtzTelemetrySample ptz;
    ptz.panRate = AngularRate(10.0F);
    ptz.tiltRate = AngularRate(0.0F);
    engine.updateTelemetry(ptz);

    // Process next frame
    ASSERT_TRUE(engine.processFrame(inView, outView, 1.0F / 30.0F));

    const auto diag = engine.diagnostics();
    EXPECT_TRUE(diag.isPtzFeedforwardActive);
}

TEST(TestVideoStabilizerEngine, LowTextureDegradedMode)
{
    const FrameDimensions dims(320U, 240U);
    VideoStabilizerConfig cfg;
    cfg.dimensions = dims;
    cfg.maxDegradedFrames = 10U;

    VideoStabilizerEngine engine;
    ASSERT_TRUE(engine.configure(cfg));

    FrameBuffer inBuf(dims);
    FrameBuffer outBuf(dims);

    // Initial frame is textured
    generateSyntheticPattern(inBuf, 0.0F, 0.0F);
    FrameView inView(inBuf.data(), dims);
    FrameView outView(outBuf.data(), dims);
    ASSERT_TRUE(engine.processFrame(inView, outView, 1.0F / 30.0F));

    // Next frames are completely flat (zero contrast)
    std::memset(inBuf.data(), 128, inBuf.dimensions().totalBytes());
    for (std::size_t i = 0U; i < 5U; ++i) {
        static_cast<void>(engine.processFrame(inView, outView, 1.0F / 30.0F));
    }

    // Engine should have transitioned to Degraded mode gracefully
    EXPECT_EQ(engine.state(), EngineState::Degraded);
}

TEST(TestVideoStabilizerEngine, PlanarYuv420pProcessing)
{
    const FrameDimensions lumaDims(320U, 240U);
    const FrameDimensions chromaDims(160U, 120U);

    VideoStabilizerConfig cfg;
    cfg.dimensions = lumaDims;

    VideoStabilizerEngine engine;
    ASSERT_TRUE(engine.configure(cfg));

    FrameBuffer inY(lumaDims);
    FrameBuffer inU(chromaDims);
    FrameBuffer inV(chromaDims);

    FrameBuffer outY(lumaDims);
    FrameBuffer outU(chromaDims);
    FrameBuffer outV(chromaDims);

    generateSyntheticPattern(inY, 0.0F, 0.0F);
    std::memset(inU.data(), 128, inU.dimensions().totalBytes());
    std::memset(inV.data(), 128, inV.dimensions().totalBytes());

    FrameView vInY(inY.data(), lumaDims);
    FrameView vInU(inU.data(), chromaDims);
    FrameView vInV(inV.data(), chromaDims);

    FrameView vOutY(outY.data(), lumaDims);
    FrameView vOutU(outU.data(), chromaDims);
    FrameView vOutV(outV.data(), chromaDims);

    EXPECT_TRUE(engine.processYuv420p(vInY, vInU, vInV, vOutY, vOutU, vOutV, 1.0F / 30.0F));
    EXPECT_EQ(engine.state(), EngineState::Tracking);
}

TEST(TestVideoStabilizerEngine, ResetLifecycle)
{
    const FrameDimensions dims(320U, 240U);
    VideoStabilizerConfig cfg;
    cfg.dimensions = dims;

    VideoStabilizerEngine engine;
    ASSERT_TRUE(engine.configure(cfg));

    FrameBuffer inBuf(dims);
    FrameBuffer outBuf(dims);
    generateSyntheticPattern(inBuf, 0.0F, 0.0F);

    FrameView inView(inBuf.data(), dims);
    FrameView outView(outBuf.data(), dims);

    ASSERT_TRUE(engine.processFrame(inView, outView, 1.0F / 30.0F));
    EXPECT_EQ(engine.state(), EngineState::Tracking);

    engine.reset();
    EXPECT_EQ(engine.state(), EngineState::Initializing);
}

TEST(TestVideoStabilizerEngine, BenchmarkHarnessRun)
{
    BenchmarkWorkload workload;
    workload.name = "QuickTestWorkload";
    workload.dimensions = FrameDimensions(320U, 240U);
    workload.frameCount = 15U;
    workload.jitterAmplitudePx = 3.0F;

    StabilizerBenchmark benchmark;
    BenchmarkStats stats = benchmark.run(workload);

    EXPECT_GT(stats.meanMs, 0.0);
    EXPECT_GT(stats.fps, 0.0);
    EXPECT_GE(stats.jitterAttenuationDb, 0.0);
}

} // namespace
