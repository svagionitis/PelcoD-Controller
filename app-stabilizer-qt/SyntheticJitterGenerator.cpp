/// @file SyntheticJitterGenerator.cpp
/// @brief Procedural test pattern and calibrated vibration synthesizer.
#include "SyntheticJitterGenerator.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <cmath>

namespace StabilizerApp {

namespace {
    constexpr float TwoPi = 6.28318530717958647692F;

    [[nodiscard]] inline constexpr std::int32_t clampInt(std::int32_t val, std::int32_t maxVal) noexcept
    {
        return (val < 0) ? 0 : ((val >= maxVal) ? (maxVal - 1) : val);
    }
} // namespace

SyntheticJitterGenerator::SyntheticJitterGenerator(const SyntheticJitterConfig& config)
    : config_ { config }
    , prng_ { config.seed }
{
    renderBasePattern();
}

void SyntheticJitterGenerator::setConfig(const SyntheticJitterConfig& config)
{
    config_ = config;
    prng_.seed(config.seed);
    reset();
    renderBasePattern();
}

const SyntheticJitterConfig& SyntheticJitterGenerator::config() const noexcept
{
    return config_;
}

void SyntheticJitterGenerator::reset() noexcept
{
    currentTimeSec_ = 0.0;
    frameIndex_ = 0U;
    driftVx_ = 0.0F;
    driftVy_ = 0.0F;
    driftPosX_ = 0.0F;
    driftPosY_ = 0.0F;
    prng_.seed(config_.seed);
}

void SyntheticJitterGenerator::renderBasePattern()
{
    const std::uint32_t w = config_.width;
    const std::uint32_t h = config_.height;
    const std::size_t totalBytes = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3U;
    basePattern_.resize(totalBytes);

    const auto cx = static_cast<float>(w) * 0.5F;
    const auto cy = static_cast<float>(h) * 0.5F;

    for (std::uint32_t y = 0U; y < h; ++y) {
        const auto fy = static_cast<float>(y);
        const std::size_t rowOffset = static_cast<std::size_t>(y) * static_cast<std::size_t>(w) * 3U;

        for (std::uint32_t x = 0U; x < w; ++x) {
            const auto fx = static_cast<float>(x);

            // Pattern 1: 48x48 pixel checkerboard for strong Shi-Tomasi corners
            const bool check = (((x / 48U) + (y / 48U)) % 2U) == 0U;
            std::uint8_t r = check ? 220U : 40U;
            std::uint8_t g = check ? 220U : 40U;
            std::uint8_t b = check ? 220U : 40U;

            // Pattern 2: Concentric circular rings from center
            const float dx = fx - cx;
            const float dy = fy - cy;
            const float dist = std::sqrt((dx * dx) + (dy * dy));
            const auto ring = static_cast<std::uint32_t>(dist) / 32U;
            if ((ring % 2U) == 1U) {
                r = static_cast<std::uint8_t>(255U - r);
                g = static_cast<std::uint8_t>((g + 80U) % 256U);
            }

            // Pattern 3: Fixed center reticle / crosshairs
            if ((std::abs(dx) < 2.0F) || (std::abs(dy) < 2.0F)) {
                r = 255U;
                g = 40U;
                b = 40U;
            }

            const std::size_t offset = rowOffset + (static_cast<std::size_t>(x) * 3U);
            basePattern_[offset] = r;
            basePattern_[offset + 1U] = g;
            basePattern_[offset + 2U] = b;
        }
    }
}

void SyntheticJitterGenerator::sampleSubpixel(
    float srcX, float srcY, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) const noexcept
{
    const auto w = static_cast<std::int32_t>(config_.width);
    const auto h = static_cast<std::int32_t>(config_.height);

    const auto x0 = static_cast<std::int32_t>(std::floor(srcX));
    const auto y0 = static_cast<std::int32_t>(std::floor(srcY));
    const float s = srcX - static_cast<float>(x0);
    const float t = srcY - static_cast<float>(y0);

    const auto s256 = static_cast<std::uint32_t>(std::clamp(s * 256.0F, 0.0F, 256.0F));
    const auto t256 = static_cast<std::uint32_t>(std::clamp(t * 256.0F, 0.0F, 256.0F));

    const std::uint32_t w00 = (256U - s256) * (256U - t256);
    const std::uint32_t w10 = s256 * (256U - t256);
    const std::uint32_t w01 = (256U - s256) * t256;
    const std::uint32_t w11 = s256 * t256;

    const auto getPixel = [this, w, h](std::int32_t px, std::int32_t py, std::uint8_t& pr, std::uint8_t& pg,
                              std::uint8_t& pb) noexcept {
        const std::int32_t cx = clampInt(px, w);
        const std::int32_t cy = clampInt(py, h);
        const std::size_t idx
            = (static_cast<std::size_t>(cy) * static_cast<std::size_t>(w) * 3U) + (static_cast<std::size_t>(cx) * 3U);
        pr = basePattern_[idx];
        pg = basePattern_[idx + 1U];
        pb = basePattern_[idx + 2U];
    };

    std::uint8_t r00 = 0U, g00 = 0U, b00 = 0U;
    std::uint8_t r10 = 0U, g10 = 0U, b10 = 0U;
    std::uint8_t r01 = 0U, g01 = 0U, b01 = 0U;
    std::uint8_t r11 = 0U, g11 = 0U, b11 = 0U;

    getPixel(x0, y0, r00, g00, b00);
    getPixel(x0 + 1, y0, r10, g10, b10);
    getPixel(x0, y0 + 1, r01, g01, b01);
    getPixel(x0 + 1, y0 + 1, r11, g11, b11);

    r = static_cast<std::uint8_t>(((w00 * r00) + (w10 * r10) + (w01 * r01) + (w11 * r11) + 32768U) >> 16U);
    g = static_cast<std::uint8_t>(((w00 * g00) + (w10 * g10) + (w01 * g01) + (w11 * g11) + 32768U) >> 16U);
    b = static_cast<std::uint8_t>(((w00 * b00) + (w10 * b10) + (w01 * b01) + (w11 * b11) + 32768U) >> 16U);
}

double SyntheticJitterGenerator::generateNextFrame(std::uint8_t* dstRgb, double dt)
{
    if ((dstRgb == nullptr) || (basePattern_.empty())) {
        return currentTimeSec_;
    }

    currentTimeSec_ += dt;
    ++frameIndex_;
    const auto t = static_cast<float>(currentTimeSec_);

    // 1. Mast Harmonic Vibration: x = A*sin(2*pi*f*t), y = A*cos(2*pi*f*t)
    const float mastDx = config_.mastAmpX * std::sin(TwoPi * config_.mastFreqX * t);
    const float mastDy = config_.mastAmpY * std::cos(TwoPi * config_.mastFreqY * t);

    // 2. Brownian Motion Drift
    if (config_.enableDrift) {
        const float fdt = static_cast<float>(dt);
        const float accX = normalDist_(prng_) * config_.driftNoiseSigma;
        const float accY = normalDist_(prng_) * config_.driftNoiseSigma;

        driftVx_ = (driftVx_ * config_.driftDamping) + (accX * fdt);
        driftVy_ = (driftVy_ * config_.driftDamping) + (accY * fdt);

        driftPosX_ += driftVx_ * fdt;
        driftPosY_ += driftVy_ * fdt;

        // Soft limit drift within +/- 40 pixels
        driftPosX_ = std::clamp(driftPosX_, -40.0F, 40.0F);
        driftPosY_ = std::clamp(driftPosY_, -40.0F, 40.0F);
    }

    const float totalDx = mastDx + driftPosX_;
    const float totalDy = mastDy + driftPosY_;

    const std::uint32_t w = config_.width;
    const std::uint32_t h = config_.height;
    const auto fh = static_cast<float>(h);

    for (std::uint32_t y = 0U; y < h; ++y) {
        float rowShear = 0.0F;
        if (config_.enableRollingShutter) {
            const float yFrac = static_cast<float>(y) / fh;
            rowShear = config_.rsAmp * std::sin((TwoPi * config_.rsFreq * t) + (TwoPi * yFrac * 0.80F));
        }

        const float srcY = static_cast<float>(y) + totalDy;
        const std::size_t rowOffset = static_cast<std::size_t>(y) * static_cast<std::size_t>(w) * 3U;

        for (std::uint32_t x = 0U; x < w; ++x) {
            const float srcX = static_cast<float>(x) + totalDx + rowShear;

            std::uint8_t r = 0U, g = 0U, b = 0U;
            sampleSubpixel(srcX, srcY, r, g, b);

            const std::size_t offset = rowOffset + (static_cast<std::size_t>(x) * 3U);
            dstRgb[offset] = r;
            dstRgb[offset + 1U] = g;
            dstRgb[offset + 2U] = b;
        }
    }

    return currentTimeSec_;
}

} // namespace StabilizerApp
