/// @file IntegralImage.cpp
/// @brief Implementation of Integral Images (Summed-Area Tables) and fast local window statistics.

#include "IntegralImage.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Math {

void IntegralImage::compute(const std::uint8_t* pixels, int width, int height, int stride, bool computeSquared)
{
    if (pixels == nullptr || width <= 0 || height <= 0) {
        m_width = 0;
        m_height = 0;
        m_hasSquared = false;
        m_sumTable.clear();
        m_sqSumTable.clear();
        m_sourcePixels.clear();
        return;
    }

    m_width = width;
    m_height = height;
    m_hasSquared = computeSquared;

    const std::size_t uWidth = static_cast<std::size_t>(width);
    const std::size_t uHeight = static_cast<std::size_t>(height);
    const std::size_t actualStride = (stride > 0) ? static_cast<std::size_t>(stride) : uWidth;
    const std::size_t tablePitch = uWidth + 1U;
    const std::size_t tableTotal = (uHeight + 1U) * tablePitch;

    m_sumTable.assign(tableTotal, 0U);
    if (m_hasSquared) {
        m_sqSumTable.assign(tableTotal, 0U);
    } else {
        m_sqSumTable.clear();
    }

    m_sourcePixels.resize(uWidth * uHeight);

    // Single-pass row cumulative summation
    for (std::size_t y = 0U; y < uHeight; ++y) {
        const std::uint8_t* rowSrc = pixels + (y * actualStride);
        std::uint8_t* rowDst = m_sourcePixels.data() + (y * uWidth);
        std::memcpy(rowDst, rowSrc, uWidth);

        std::uint64_t rowSum = 0U;
        std::uint64_t rowSqSum = 0U;

        const std::size_t prevTableRowOffset = y * tablePitch;
        const std::size_t currTableRowOffset = (y + 1U) * tablePitch;

        for (std::size_t x = 0U; x < uWidth; ++x) {
            const std::uint64_t p = static_cast<std::uint64_t>(rowSrc[x]);
            rowSum += p;
            m_sumTable[currTableRowOffset + (x + 1U)] = m_sumTable[prevTableRowOffset + (x + 1U)] + rowSum;

            if (m_hasSquared) {
                rowSqSum += (p * p);
                m_sqSumTable[currTableRowOffset + (x + 1U)] = m_sqSumTable[prevTableRowOffset + (x + 1U)] + rowSqSum;
            }
        }
    }
}

bool IntegralImage::isValid() const noexcept
{
    return (m_width > 0 && m_height > 0 && !m_sumTable.empty());
}

int IntegralImage::getWidth() const noexcept
{
    return m_width;
}

int IntegralImage::getHeight() const noexcept
{
    return m_height;
}

std::uint64_t IntegralImage::computeSum(int x, int y, int w, int h) const noexcept
{
    if (!isValid() || w <= 0 || h <= 0) {
        return 0U;
    }

    const int x0 = std::clamp(x, 0, m_width);
    const int y0 = std::clamp(y, 0, m_height);
    const int x1 = std::clamp(x + w, 0, m_width);
    const int y1 = std::clamp(y + h, 0, m_height);

    if (x1 <= x0 || y1 <= y0) {
        return 0U;
    }

    const std::size_t tablePitch = static_cast<std::size_t>(m_width + 1);
    const auto ux0 = static_cast<std::size_t>(x0);
    const auto uy0 = static_cast<std::size_t>(y0);
    const auto ux1 = static_cast<std::size_t>(x1);
    const auto uy1 = static_cast<std::size_t>(y1);

    const std::uint64_t br = m_sumTable[uy1 * tablePitch + ux1];
    const std::uint64_t tr = m_sumTable[uy0 * tablePitch + ux1];
    const std::uint64_t bl = m_sumTable[uy1 * tablePitch + ux0];
    const std::uint64_t tl = m_sumTable[uy0 * tablePitch + ux0];

    return (br + tl) - (tr + bl);
}

std::uint64_t IntegralImage::computeSum(const Rect& rect) const noexcept
{
    return computeSum(rect.x, rect.y, rect.width, rect.height);
}

std::uint64_t IntegralImage::computeSquaredSum(int x, int y, int w, int h) const noexcept
{
    if (!isValid() || !m_hasSquared || w <= 0 || h <= 0) {
        return 0U;
    }

    const int x0 = std::clamp(x, 0, m_width);
    const int y0 = std::clamp(y, 0, m_height);
    const int x1 = std::clamp(x + w, 0, m_width);
    const int y1 = std::clamp(y + h, 0, m_height);

    if (x1 <= x0 || y1 <= y0) {
        return 0U;
    }

    const std::size_t tablePitch = static_cast<std::size_t>(m_width + 1);
    const auto ux0 = static_cast<std::size_t>(x0);
    const auto uy0 = static_cast<std::size_t>(y0);
    const auto ux1 = static_cast<std::size_t>(x1);
    const auto uy1 = static_cast<std::size_t>(y1);

    const std::uint64_t br = m_sqSumTable[uy1 * tablePitch + ux1];
    const std::uint64_t tr = m_sqSumTable[uy0 * tablePitch + ux1];
    const std::uint64_t bl = m_sqSumTable[uy1 * tablePitch + ux0];
    const std::uint64_t tl = m_sqSumTable[uy0 * tablePitch + ux0];

    return (br + tl) - (tr + bl);
}

std::uint64_t IntegralImage::computeSquaredSum(const Rect& rect) const noexcept
{
    return computeSquaredSum(rect.x, rect.y, rect.width, rect.height);
}

double IntegralImage::computeMean(int x, int y, int w, int h) const noexcept
{
    if (!isValid() || w <= 0 || h <= 0) {
        return 0.0;
    }

    const int x0 = std::clamp(x, 0, m_width);
    const int y0 = std::clamp(y, 0, m_height);
    const int x1 = std::clamp(x + w, 0, m_width);
    const int y1 = std::clamp(y + h, 0, m_height);

    if (x1 <= x0 || y1 <= y0) {
        return 0.0;
    }

    const double count = static_cast<double>((x1 - x0) * (y1 - y0));
    const double sum = static_cast<double>(computeSum(x, y, w, h));
    return sum / count;
}

double IntegralImage::computeMean(const Rect& rect) const noexcept
{
    return computeMean(rect.x, rect.y, rect.width, rect.height);
}

double IntegralImage::computeVariance(int x, int y, int w, int h) const noexcept
{
    if (!isValid() || !m_hasSquared || w <= 0 || h <= 0) {
        return 0.0;
    }

    const int x0 = std::clamp(x, 0, m_width);
    const int y0 = std::clamp(y, 0, m_height);
    const int x1 = std::clamp(x + w, 0, m_width);
    const int y1 = std::clamp(y + h, 0, m_height);

    if (x1 <= x0 || y1 <= y0) {
        return 0.0;
    }

    const double count = static_cast<double>((x1 - x0) * (y1 - y0));
    if (count <= 1.0) {
        return 0.0;
    }

    const double s1 = static_cast<double>(computeSum(x, y, w, h));
    const double s2 = static_cast<double>(computeSquaredSum(x, y, w, h));
    const double mean = s1 / count;
    const double var = (s2 / count) - (mean * mean);

    return std::max(0.0, var);
}

double IntegralImage::computeVariance(const Rect& rect) const noexcept
{
    return computeVariance(rect.x, rect.y, rect.width, rect.height);
}

double IntegralImage::computeStdDev(int x, int y, int w, int h) const noexcept
{
    return std::sqrt(computeVariance(x, y, w, h));
}

double IntegralImage::computeStdDev(const Rect& rect) const noexcept
{
    return computeStdDev(rect.x, rect.y, rect.width, rect.height);
}

void IntegralImage::boxBlur(std::uint8_t* dst, int radius) const
{
    if (!isValid() || dst == nullptr) {
        return;
    }

    if (radius <= 0) {
        std::memcpy(dst, m_sourcePixels.data(), m_sourcePixels.size());
        return;
    }

    const int winSize = 2 * radius + 1;
    const auto uWidth = static_cast<std::size_t>(m_width);
    const auto uHeight = static_cast<std::size_t>(m_height);

    for (std::size_t y = 0U; y < uHeight; ++y) {
        const int iy = static_cast<int>(y);
        const std::size_t rowOffset = y * uWidth;

        for (std::size_t x = 0U; x < uWidth; ++x) {
            const int ix = static_cast<int>(x);
            const double mean = computeMean(ix - radius, iy - radius, winSize, winSize);
            dst[rowOffset + x] = static_cast<std::uint8_t>(std::clamp(std::round(mean), 0.0, 255.0));
        }
    }
}

void IntegralImage::adaptiveThreshold(std::uint8_t* dst, int windowSize, double thresholdFraction) const
{
    if (!isValid() || dst == nullptr) {
        return;
    }

    const int win = (windowSize > 0) ? windowSize : 16;
    const int half = win / 2;
    const double factor = 1.0 - thresholdFraction;
    const auto uWidth = static_cast<std::size_t>(m_width);
    const auto uHeight = static_cast<std::size_t>(m_height);

    for (std::size_t y = 0U; y < uHeight; ++y) {
        const int iy = static_cast<int>(y);
        const std::size_t rowOffset = y * uWidth;

        for (std::size_t x = 0U; x < uWidth; ++x) {
            const int ix = static_cast<int>(x);
            const double localMean = computeMean(ix - half, iy - half, win, win);
            const double p = static_cast<double>(m_sourcePixels[rowOffset + x]);

            dst[rowOffset + x] = (p <= (localMean * factor)) ? 0U : 255U;
        }
    }
}

} // namespace Math
