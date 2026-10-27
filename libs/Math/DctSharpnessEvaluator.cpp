/// @file DctSharpnessEvaluator.cpp
/// @brief Implementation of 2D 8x8 DCT-based optical sharpness and focus evaluator.

#include "DctSharpnessEvaluator.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <algorithm>

#include "Dct.h"

namespace Math {

DctSharpnessEvaluator::DctSharpnessEvaluator(DctSharpnessConfig config)
{
    setConfig(config);
}

void DctSharpnessEvaluator::setConfig(const DctSharpnessConfig& config) noexcept
{
    m_config = config;
    m_config.minFrequencySum = std::clamp(m_config.minFrequencySum, 1, 14);
    m_config.blockStride = std::max(1, m_config.blockStride);
    m_config.roiNormX = std::clamp(m_config.roiNormX, 0.0, 0.9);
    m_config.roiNormY = std::clamp(m_config.roiNormY, 0.0, 0.9);
    m_config.roiNormWidth = std::clamp(m_config.roiNormWidth, 0.05, 1.0 - m_config.roiNormX);
    m_config.roiNormHeight = std::clamp(m_config.roiNormHeight, 0.05, 1.0 - m_config.roiNormY);
}

const DctSharpnessConfig& DctSharpnessEvaluator::getConfig() const noexcept
{
    return m_config;
}

double DctSharpnessEvaluator::accumulateBlockAc(const DctMatrix8x8& dct) const noexcept
{
    double blockAc = 0.0;
    if (m_config.mode == DctMetricMode::HighFrequencyEnergy) {
        for (int u = 0; u < 8; ++u) {
            for (int v = 0; v < 8; ++v) {
                if (u + v >= m_config.minFrequencySum) {
                    blockAc += dct[u][v] * dct[u][v];
                }
            }
        }
    } else if (m_config.mode == DctMetricMode::ModifiedDct) {
        for (int u = 1; u < 7; ++u) {
            for (int v = 1; v < 7; ++v) {
                if (u + v >= m_config.minFrequencySum) {
                    const double dH = std::abs((2.0 * dct[u][v]) - dct[u - 1][v] - dct[u + 1][v]);
                    const double dV = std::abs((2.0 * dct[u][v]) - dct[u][v - 1] - dct[u][v + 1]);
                    blockAc += dH + dV;
                }
            }
        }
    } else { // HighFrequencyAcSum
        for (int u = 0; u < 8; ++u) {
            for (int v = 0; v < 8; ++v) {
                if (u + v >= m_config.minFrequencySum) {
                    blockAc += std::abs(dct[u][v]);
                }
            }
        }
    }
    return blockAc;
}

double DctSharpnessEvaluator::evaluateBlock(const std::uint8_t* block, int stride) const noexcept
{
    if (block == nullptr || stride < 8) {
        return 0.0;
    }

    DctMatrix8x8 dct {};
    Math::dct8x8(block, stride, dct);
    return accumulateBlockAc(dct);
}

SharpnessResult DctSharpnessEvaluator::evaluate(
    const std::uint8_t* grayPixels, int width, int height, int stride) const noexcept
{
    if (grayPixels == nullptr || width < 8 || height < 8) {
        return {};
    }

    const int effStride = (stride > 0) ? stride : width;
    if (effStride < width) {
        return {};
    }

    const double dWidth = static_cast<double>(width);
    const double dHeight = static_cast<double>(height);

    const int roiX = std::clamp(static_cast<int>(m_config.roiNormX * dWidth), 0, width - 8);
    const int roiY = std::clamp(static_cast<int>(m_config.roiNormY * dHeight), 0, height - 8);
    const int roiW = std::clamp(static_cast<int>(m_config.roiNormWidth * dWidth), 8, width - roiX);
    const int roiH = std::clamp(static_cast<int>(m_config.roiNormHeight * dHeight), 8, height - roiY);

    const int step = std::max(1, m_config.blockStride);
    const auto uStride = static_cast<std::size_t>(effStride);

    double totalScore = 0.0;
    double totalDc = 0.0;
    std::size_t blockCount = 0U;

    for (int y = roiY; y + 8 <= roiY + roiH; y += step) {
        const auto uy = static_cast<std::size_t>(y);
        for (int x = roiX; x + 8 <= roiX + roiW; x += step) {
            const auto ux = static_cast<std::size_t>(x);
            const std::uint8_t* blockPtr = grayPixels + (uy * uStride) + ux;
            DctMatrix8x8 dct {};
            Math::dct8x8(blockPtr, effStride, dct);

            // DC component: dct[0][0]
            const double dc = std::abs(dct[0][0]);
            totalDc += dc;

            totalScore += accumulateBlockAc(dct);
            ++blockCount;
        }
    }

    SharpnessResult result {};
    if (blockCount > 0U) {
        result.blocksEvaluated = blockCount;
        result.rawScore = totalScore / static_cast<double>(blockCount);
        // Average spatial luminance: DC / sqrt(8)
        result.meanLuminance = (totalDc / static_cast<double>(blockCount)) * 0.35355339059327376;

        if (m_config.enableNormalization && totalDc > 1e-6) {
            result.normalizedScore = totalScore / totalDc;
        } else {
            result.normalizedScore = result.rawScore;
        }
    }

    return result;
}

} // namespace Math