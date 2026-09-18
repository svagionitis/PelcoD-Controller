/// @file PhaseCorrelation.cpp
/// @brief Implementation of 2D Phase Correlation Global Motion Estimator.

#include "PhaseCorrelation.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace PelcoD {

PhaseCorrelationEstimator::PhaseCorrelationEstimator(PhaseCorrelationConfig config)
    : m_config(std::move(config))
{
}

void PhaseCorrelationEstimator::setConfig(const PhaseCorrelationConfig& config)
{
    m_config = config;
}

const PhaseCorrelationConfig& PhaseCorrelationEstimator::getConfig() const noexcept
{
    return m_config;
}

MotionResult PhaseCorrelationEstimator::estimateMotion(
    const uint8_t* refPixels, const uint8_t* curPixels, int width, int height, int stride) const
{
    if (refPixels == nullptr || curPixels == nullptr || width <= 0 || height <= 0) {
        return MotionResult {};
    }

    const std::size_t uWidth = static_cast<std::size_t>(width);
    const std::size_t uHeight = static_cast<std::size_t>(height);
    const std::size_t actualStride = (stride > 0) ? static_cast<std::size_t>(stride) : uWidth;

    std::size_t gw
        = Math::isPowerOfTwo(m_config.gridWidth) ? m_config.gridWidth : Math::nextPowerOfTwo(m_config.gridWidth);
    std::size_t gh
        = Math::isPowerOfTwo(m_config.gridHeight) ? m_config.gridHeight : Math::nextPowerOfTwo(m_config.gridHeight);
    if (gw < 2U) {
        gw = 64U;
    }
    if (gh < 2U) {
        gh = 64U;
    }

    const std::size_t gridTotal = gw * gh;
    std::vector<double> refGrid(gridTotal, 0.0);
    std::vector<double> curGrid(gridTotal, 0.0);

    const double scaleX = static_cast<double>(uWidth) / static_cast<double>(gw);
    const double scaleY = static_cast<double>(uHeight) / static_cast<double>(gh);

    // Resample frames onto power-of-two grid using bilinear interpolation
    for (std::size_t r = 0U; r < gh; ++r) {
        const double srcY = (static_cast<double>(r) + 0.5) * scaleY - 0.5;
        const double clampedY = std::clamp(srcY, 0.0, static_cast<double>(uHeight - 1U));
        const auto y0 = static_cast<std::size_t>(clampedY);
        const std::size_t y1 = std::min(y0 + 1U, uHeight - 1U);
        const double fy = clampedY - static_cast<double>(y0);

        const std::size_t gridRowOffset = r * gw;

        for (std::size_t c = 0U; c < gw; ++c) {
            const double srcX = (static_cast<double>(c) + 0.5) * scaleX - 0.5;
            const double clampedX = std::clamp(srcX, 0.0, static_cast<double>(uWidth - 1U));
            const auto x0 = static_cast<std::size_t>(clampedX);
            const std::size_t x1 = std::min(x0 + 1U, uWidth - 1U);
            const double fx = clampedX - static_cast<double>(x0);

            const double w00 = (1.0 - fx) * (1.0 - fy);
            const double w01 = fx * (1.0 - fy);
            const double w10 = (1.0 - fx) * fy;
            const double w11 = fx * fy;

            const double pRef = w00 * static_cast<double>(refPixels[y0 * actualStride + x0])
                + w01 * static_cast<double>(refPixels[y0 * actualStride + x1])
                + w10 * static_cast<double>(refPixels[y1 * actualStride + x0])
                + w11 * static_cast<double>(refPixels[y1 * actualStride + x1]);

            const double pCur = w00 * static_cast<double>(curPixels[y0 * actualStride + x0])
                + w01 * static_cast<double>(curPixels[y0 * actualStride + x1])
                + w10 * static_cast<double>(curPixels[y1 * actualStride + x0])
                + w11 * static_cast<double>(curPixels[y1 * actualStride + x1]);

            refGrid[gridRowOffset + c] = pRef;
            curGrid[gridRowOffset + c] = pCur;
        }
    }

    // Apply 2D window to eliminate edge boundary wrap-around leakage
    Math::applyWindow2D(refGrid, gh, gw, m_config.windowType);
    Math::applyWindow2D(curGrid, gh, gw, m_config.windowType);

    std::vector<Math::Complex> fRef(gridTotal);
    std::vector<Math::Complex> fCur(gridTotal);
    for (std::size_t i = 0U; i < gridTotal; ++i) {
        fRef[i] = Math::Complex { refGrid[i], 0.0 };
        fCur[i] = Math::Complex { curGrid[i], 0.0 };
    }

    // 2D Forward FFT
    Math::fft2D(fRef, gh, gw, false);
    Math::fft2D(fCur, gh, gw, false);

    // Compute normalized cross-power spectrum: R = (F_cur * conj(F_ref)) / (|F_cur * conj(F_ref)| + eps)
    std::vector<Math::Complex> crossSpectrum(gridTotal);
    constexpr double eps = 1e-12;
    for (std::size_t i = 0U; i < gridTotal; ++i) {
        const Math::Complex prod = fCur[i] * std::conj(fRef[i]);
        const double mag = std::abs(prod);
        crossSpectrum[i] = prod / (mag + eps);
    }

    // 2D Inverse FFT to spatial domain impulse surface
    Math::fft2D(crossSpectrum, gh, gw, true);

    // Locate impulse peak
    double maxVal = -1.0;
    std::size_t bestR = 0U;
    std::size_t bestC = 0U;
    for (std::size_t r = 0U; r < gh; ++r) {
        const std::size_t rowOffset = r * gw;
        for (std::size_t c = 0U; c < gw; ++c) {
            const double val = std::real(crossSpectrum[rowOffset + c]);
            if (val > maxVal) {
                maxVal = val;
                bestR = r;
                bestC = c;
            }
        }
    }

    // Unwrap periodic circular shift to signed translation
    double shiftX = 0.0;
    if (bestC > (gw / 2U)) {
        shiftX = static_cast<double>(bestC) - static_cast<double>(gw);
    } else {
        shiftX = static_cast<double>(bestC);
    }

    double shiftY = 0.0;
    if (bestR > (gh / 2U)) {
        shiftY = static_cast<double>(bestR) - static_cast<double>(gh);
    } else {
        shiftY = static_cast<double>(bestR);
    }

    // Parabolic sub-pixel peak refinement along horizontal axis
    const std::size_t cPrev = (bestC + gw - 1U) % gw;
    const std::size_t cNext = (bestC + 1U) % gw;
    const double v1 = std::real(crossSpectrum[bestR * gw + cPrev]);
    const double v2 = std::real(crossSpectrum[bestR * gw + bestC]);
    const double v3 = std::real(crossSpectrum[bestR * gw + cNext]);
    const double denomX = 2.0 * (v1 - 2.0 * v2 + v3);
    if (std::abs(denomX) > 1e-12) {
        double deltaSubX = (v1 - v3) / denomX;
        deltaSubX = std::clamp(deltaSubX, -1.0, 1.0);
        shiftX += deltaSubX;
    }

    // Parabolic sub-pixel peak refinement along vertical axis
    const std::size_t rPrev = (bestR + gh - 1U) % gh;
    const std::size_t rNext = (bestR + 1U) % gh;
    const double u1 = std::real(crossSpectrum[rPrev * gw + bestC]);
    const double u2 = std::real(crossSpectrum[bestR * gw + bestC]);
    const double u3 = std::real(crossSpectrum[rNext * gw + bestC]);
    const double denomY = 2.0 * (u1 - 2.0 * u2 + u3);
    if (std::abs(denomY) > 1e-12) {
        double deltaSubY = (u1 - u3) / denomY;
        deltaSubY = std::clamp(deltaSubY, -1.0, 1.0);
        shiftY += deltaSubY;
    }

    // Scale translation back to source image coordinate space
    const double finalDeltaX = shiftX * scaleX;
    const double finalDeltaY = shiftY * scaleY;

    MotionResult result {};
    result.deltaX = finalDeltaX;
    result.deltaY = finalDeltaY;
    result.peakCorrelation = std::clamp(maxVal, 0.0, 1.0);

    const double maxDispX = m_config.maxTranslationFraction * static_cast<double>(uWidth);
    const double maxDispY = m_config.maxTranslationFraction * static_cast<double>(uHeight);

    result.isConfident = (result.peakCorrelation >= m_config.confidenceThreshold) && (std::abs(finalDeltaX) <= maxDispX)
        && (std::abs(finalDeltaY) <= maxDispY);

    return result;
}

} // namespace PelcoD
