/// @file SyntheticDefocusGenerator.cpp
/// @brief Procedural optical test pattern and calibrated defocus blur synthesizer.
#include "SyntheticDefocusGenerator.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <vector>

namespace LensApp {

namespace {

    constexpr float kPi = 3.14159265358979323846F;
    constexpr std::size_t kBytesPerPixel = 3U; // RGB24
    constexpr int kMaxKernelRadius = 15;

} // namespace

SyntheticDefocusGenerator::SyntheticDefocusGenerator(const SyntheticDefocusConfig& config)
    : config_(config)
    , prng_(config.prngSeed)
{
    setConfig(config);
}

void SyntheticDefocusGenerator::setConfig(const SyntheticDefocusConfig& config)
{
    config_ = config.isValid() ? config : SyntheticDefocusConfig { };
    const std::size_t byteCount
        = static_cast<std::size_t>(config_.width) * static_cast<std::size_t>(config_.height) * kBytesPerPixel;

    if (basePattern_.size() != byteCount) {
        basePattern_.assign(byteCount, 0U);
        scratchBuffer_.assign(byteCount, 0U);
    }

    renderBasePattern();
    reset();
}

const SyntheticDefocusConfig& SyntheticDefocusGenerator::config() const noexcept
{
    return config_;
}

void SyntheticDefocusGenerator::setTargetPlane(std::uint16_t focusPlane) noexcept
{
    config_.targetPlaneFocus = focusPlane;
}

std::uint16_t SyntheticDefocusGenerator::targetPlane() const noexcept
{
    return config_.targetPlaneFocus;
}

void SyntheticDefocusGenerator::updateMotorState(std::uint16_t focus, std::uint16_t zoom) noexcept
{
    currentFocus_ = focus;
    currentZoom_ = zoom;
}

void SyntheticDefocusGenerator::reset() noexcept
{
    currentTimeSec_ = 0.0;
    frameIndex_ = 0U;
    prng_.seed(config_.prngSeed);
}

double SyntheticDefocusGenerator::generateNextFrame(std::uint8_t* dstRgb, double dt)
{
    if (dstRgb == nullptr) {
        return currentTimeSec_;
    }

    currentTimeSec_ += (dt > 0.0) ? dt : (1.0 / config_.frameRate);
    ++frameIndex_;

    // Calculate optical defocus Circle of Confusion radius sigma
    const float focusDelta = std::abs(static_cast<float>(currentFocus_) - static_cast<float>(config_.targetPlaneFocus));
    const float normalizedDelta = focusDelta / 65535.0F;

    const float zoomFactor = static_cast<float>(currentZoom_) / 65535.0F;
    const float dofCompression = 1.0F + (zoomFactor * config_.zoomCoupling);

    const float sigma
        = std::clamp(normalizedDelta * config_.blurSensitivity * dofCompression, 0.0F, config_.maxBlurSigma);

    if (sigma < 0.25F) {
        // In-focus condition: direct copy from base pattern
        std::memcpy(dstRgb, basePattern_.data(), basePattern_.size());
    } else {
        // Defocused condition: separable Gaussian convolution
        applySeparableBlur(dstRgb, sigma);
    }

    if ((config_.noiseSigma > 0.0F) || (config_.vignetteFactor > 0.0F)) {
        applyNoiseAndVignette(dstRgb);
    }

    return currentTimeSec_;
}

void SyntheticDefocusGenerator::renderBasePattern()
{
    switch (config_.pattern) {
    case DefocusPatternType::SiemensStar:
        renderSiemensStar();
        break;
    case DefocusPatternType::IsoChart:
        renderIsoChart();
        break;
    case DefocusPatternType::NaturalTexture:
        renderNaturalTexture();
        break;
    default:
        renderSiemensStar();
        break;
    }
}

void SyntheticDefocusGenerator::renderSiemensStar()
{
    const int w = static_cast<int>(config_.width);
    const int h = static_cast<int>(config_.height);
    const float cx = static_cast<float>(w) * 0.5F;
    const float cy = static_cast<float>(h) * 0.5F;
    constexpr int kSpokes = 36;
    constexpr float kSpokeAngle = (2.0F * kPi) / static_cast<float>(kSpokes);

    for (int y = 0; y < h; ++y) {
        const float dy = static_cast<float>(y) - cy;
        const std::size_t rowOffset = static_cast<std::size_t>(y) * static_cast<std::size_t>(w) * kBytesPerPixel;

        for (int x = 0; x < w; ++x) {
            const float dx = static_cast<float>(x) - cx;
            const float r = std::sqrt((dx * dx) + (dy * dy));
            float theta = std::atan2(dy, dx);
            if (theta < 0.0F) {
                theta += (2.0F * kPi);
            }

            const int spokeIndex = static_cast<int>(theta / kSpokeAngle);
            std::uint8_t val = ((spokeIndex % 2) == 0) ? 235U : 20U;

            // Concentric reference rings
            const int ring = static_cast<int>(r / 35.0F);
            if ((ring % 4) == 0) {
                val = static_cast<std::uint8_t>(255U - val);
            }

            // Center dead zone
            if (r < 6.0F) {
                val = 128U;
            }

            const std::size_t idx = rowOffset + (static_cast<std::size_t>(x) * kBytesPerPixel);
            basePattern_[idx] = val;
            basePattern_[idx + 1U] = val;
            basePattern_[idx + 2U] = val;
        }
    }
}

void SyntheticDefocusGenerator::renderIsoChart()
{
    const int w = static_cast<int>(config_.width);
    const int h = static_cast<int>(config_.height);

    for (int y = 0; y < h; ++y) {
        const std::size_t rowOffset = static_cast<std::size_t>(y) * static_cast<std::size_t>(w) * kBytesPerPixel;

        for (int x = 0; x < w; ++x) {
            std::uint8_t val = 220U;

            // Background checker pattern
            const int checkX = x / 60;
            const int checkY = y / 60;
            if (((checkX + checkY) % 2) == 0) {
                val = 200U;
            }

            // Slanted high-contrast edge block in center (5 degree tilt)
            const float cx = static_cast<float>(w) * 0.5F;
            const float cy = static_cast<float>(h) * 0.5F;
            const float dx = static_cast<float>(x) - cx;
            const float dy = static_cast<float>(y) - cy;

            // Rotate coordinates by 5 degrees
            constexpr float kRad = 0.0872665F; // 5 deg
            const float rx = (dx * std::cos(kRad)) - (dy * std::sin(kRad));
            const float ry = (dx * std::sin(kRad)) + (dy * std::cos(kRad));

            if ((std::abs(rx) < 180.0F) && (std::abs(ry) < 120.0F)) {
                val = (rx < 0.0F) ? 245U : 15U;
            }

            // Fine resolution wedges at corners
            if (((x % 8) < 4) && (y < 80 || y > (h - 80))) {
                val = (val > 128U) ? 250U : 20U;
            }

            const std::size_t idx = rowOffset + (static_cast<std::size_t>(x) * kBytesPerPixel);
            basePattern_[idx] = val;
            basePattern_[idx + 1U] = val;
            basePattern_[idx + 2U] = val;
        }
    }
}

void SyntheticDefocusGenerator::renderNaturalTexture()
{
    const int w = static_cast<int>(config_.width);
    const int h = static_cast<int>(config_.height);

    for (int y = 0; y < h; ++y) {
        const float fy = static_cast<float>(y);
        const std::size_t rowOffset = static_cast<std::size_t>(y) * static_cast<std::size_t>(w) * kBytesPerPixel;

        for (int x = 0; x < w; ++x) {
            const float fx = static_cast<float>(x);

            // Multi-frequency harmonic synthesized surface
            const float wave1 = std::sin(fx * 0.03F) * std::cos(fy * 0.03F);
            const float wave2 = std::sin(fx * 0.12F + fy * 0.08F);
            const float wave3 = std::cos(fx * 0.28F - fy * 0.20F);
            const float composite = (wave1 * 0.50F) + (wave2 * 0.35F) + (wave3 * 0.15F);

            const int pixelVal = static_cast<int>(128.0F + (composite * 105.0F));
            const auto val = static_cast<std::uint8_t>(std::clamp(pixelVal, 0, 255));

            const std::size_t idx = rowOffset + (static_cast<std::size_t>(x) * kBytesPerPixel);
            basePattern_[idx] = val;
            basePattern_[idx + 1U] = val;
            basePattern_[idx + 2U] = val;
        }
    }
}

void SyntheticDefocusGenerator::applySeparableBlur(std::uint8_t* dstRgb, float sigma)
{
    const int w = static_cast<int>(config_.width);
    const int h = static_cast<int>(config_.height);

    // Compute discrete 1D Gaussian kernel
    const int radius = std::min(static_cast<int>(std::ceil(3.0F * sigma)), kMaxKernelRadius);
    const int kernelSize = (2 * radius) + 1;

    std::array<float, 2 * kMaxKernelRadius + 1> kernel { };
    float sum = 0.0F;
    const float twoSigmaSq = 2.0F * sigma * sigma;

    for (int k = -radius; k <= radius; ++k) {
        const float weight = std::exp(-static_cast<float>(k * k) / twoSigmaSq);
        const std::size_t kIdx = static_cast<std::size_t>(k + radius);
        kernel[kIdx] = weight;
        sum += weight;
    }
    const float invSum = (sum > 0.0F) ? (1.0F / sum) : 1.0F;
    for (int k = 0; k < kernelSize; ++k) {
        kernel[static_cast<std::size_t>(k)] *= invSum;
    }

    // Pass 1: Horizontal 1D convolution (basePattern_ -> scratchBuffer_)
    for (int y = 0; y < h; ++y) {
        const std::size_t rowOffset = static_cast<std::size_t>(y) * static_cast<std::size_t>(w) * kBytesPerPixel;

        for (int x = 0; x < w; ++x) {
            float accR = 0.0F;
            float accG = 0.0F;
            float accB = 0.0F;

            for (int k = -radius; k <= radius; ++k) {
                const int sampleX = std::clamp(x + k, 0, w - 1);
                const std::size_t srcIdx = rowOffset + (static_cast<std::size_t>(sampleX) * kBytesPerPixel);
                const float weight = kernel[static_cast<std::size_t>(k + radius)];

                accR += static_cast<float>(basePattern_[srcIdx]) * weight;
                accG += static_cast<float>(basePattern_[srcIdx + 1U]) * weight;
                accB += static_cast<float>(basePattern_[srcIdx + 2U]) * weight;
            }

            const std::size_t dstIdx = rowOffset + (static_cast<std::size_t>(x) * kBytesPerPixel);
            scratchBuffer_[dstIdx] = static_cast<std::uint8_t>(std::clamp(accR, 0.0F, 255.0F));
            scratchBuffer_[dstIdx + 1U] = static_cast<std::uint8_t>(std::clamp(accG, 0.0F, 255.0F));
            scratchBuffer_[dstIdx + 2U] = static_cast<std::uint8_t>(std::clamp(accB, 0.0F, 255.0F));
        }
    }

    // Pass 2: Vertical 1D convolution (scratchBuffer_ -> dstRgb)
    for (int y = 0; y < h; ++y) {
        const std::size_t rowOffset = static_cast<std::size_t>(y) * static_cast<std::size_t>(w) * kBytesPerPixel;

        for (int x = 0; x < w; ++x) {
            float accR = 0.0F;
            float accG = 0.0F;
            float accB = 0.0F;

            for (int k = -radius; k <= radius; ++k) {
                const int sampleY = std::clamp(y + k, 0, h - 1);
                const std::size_t srcIdx
                    = (static_cast<std::size_t>(sampleY) * static_cast<std::size_t>(w) * kBytesPerPixel)
                    + (static_cast<std::size_t>(x) * kBytesPerPixel);
                const float weight = kernel[static_cast<std::size_t>(k + radius)];

                accR += static_cast<float>(scratchBuffer_[srcIdx]) * weight;
                accG += static_cast<float>(scratchBuffer_[srcIdx + 1U]) * weight;
                accB += static_cast<float>(scratchBuffer_[srcIdx + 2U]) * weight;
            }

            const std::size_t dstIdx = rowOffset + (static_cast<std::size_t>(x) * kBytesPerPixel);
            dstRgb[dstIdx] = static_cast<std::uint8_t>(std::clamp(accR, 0.0F, 255.0F));
            dstRgb[dstIdx + 1U] = static_cast<std::uint8_t>(std::clamp(accG, 0.0F, 255.0F));
            dstRgb[dstIdx + 2U] = static_cast<std::uint8_t>(std::clamp(accB, 0.0F, 255.0F));
        }
    }
}

void SyntheticDefocusGenerator::applyNoiseAndVignette(std::uint8_t* dstRgb)
{
    const int w = static_cast<int>(config_.width);
    const int h = static_cast<int>(config_.height);
    const float cx = static_cast<float>(w) * 0.5F;
    const float cy = static_cast<float>(h) * 0.5F;
    const float maxRadiusSq = (cx * cx) + (cy * cy);
    const float invMaxRadiusSq = (maxRadiusSq > 0.0F) ? (1.0F / maxRadiusSq) : 1.0F;

    for (int y = 0; y < h; ++y) {
        const float dy = static_cast<float>(y) - cy;
        const float dySq = dy * dy;
        const std::size_t rowOffset = static_cast<std::size_t>(y) * static_cast<std::size_t>(w) * kBytesPerPixel;

        for (int x = 0; x < w; ++x) {
            const float dx = static_cast<float>(x) - cx;
            const float rSq = (dx * dx) + dySq;

            // Optical vignetting falloff
            const float vignette = 1.0F - (config_.vignetteFactor * (rSq * invMaxRadiusSq));

            // Gaussian noise sample
            const float noise = (config_.noiseSigma > 0.0F) ? (normalDist_(prng_) * config_.noiseSigma) : 0.0F;

            const std::size_t idx = rowOffset + (static_cast<std::size_t>(x) * kBytesPerPixel);
            for (std::size_t c = 0U; c < kBytesPerPixel; ++c) {
                const float val = (static_cast<float>(dstRgb[idx + c]) * vignette) + noise;
                dstRgb[idx + c] = static_cast<std::uint8_t>(std::clamp(val, 0.0F, 255.0F));
            }
        }
    }
}

} // namespace LensApp
