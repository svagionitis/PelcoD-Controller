/**
 * @file SpatialFilters.cpp
 * @brief Implementations of spatial filters including blur, sharpen, convolutions, denoise, dehaze, and lens corrections.
 */

#include "SpatialFilters.h"

#if defined(PELCOD_HAS_FILTERS)

#include <algorithm>
#include <cmath>
#include <cstring>
#include <opencv2/opencv.hpp>

namespace Video::Filters {

// --- GaussianBlurFilter ---
GaussianBlurFilter::GaussianBlurFilter(int kernelSize)
    : m_kernelSize(kernelSize)
{
}

void GaussianBlurFilter::process(std::uint8_t* data, int width, int height, PixelFormat /*format*/)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    int ksize = m_kernelSize;
    if (ksize % 2 == 0) {
        ksize += 1;
    }
    if (ksize <= 0) {
        ksize = 1;
    }
    cv::GaussianBlur(mat, mat, cv::Size(ksize, ksize), 0);
}

// --- EdgeDetectionFilter ---
EdgeDetectionFilter::EdgeDetectionFilter(double threshold1, double threshold2)
    : m_threshold1(threshold1)
    , m_threshold2(threshold2)
{
}

void EdgeDetectionFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat gray, edges;

    if (format == PixelFormat::BGR24) {
        cv::cvtColor(mat, gray, cv::COLOR_BGR2GRAY);
    } else {
        cv::cvtColor(mat, gray, cv::COLOR_RGB2GRAY);
    }

    cv::Canny(gray, edges, m_threshold1, m_threshold2);

    if (format == PixelFormat::BGR24) {
        cv::cvtColor(edges, mat, cv::COLOR_GRAY2BGR);
    } else {
        cv::cvtColor(edges, mat, cv::COLOR_GRAY2RGB);
    }
}

// --- SharpenFilter ---
SharpenFilter::SharpenFilter(double strength, int radius)
    : m_strength(strength)
    , m_radius(std::max(1, radius))
{
}

void SharpenFilter::process(std::uint8_t* data, int width, int height, PixelFormat /*format*/)
{
    if (!data || width <= 0 || height <= 0 || m_strength <= 0.0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);

    int ksize = 2 * m_radius + 1;
    cv::Mat blurred;
    cv::GaussianBlur(mat, blurred, cv::Size(ksize, ksize), 0);
    cv::addWeighted(mat, 1.0 + m_strength, blurred, -m_strength, 0.0, mat);
}

// --- BilateralFilter ---
BilateralFilter::BilateralFilter(int d, double sigmaColor, double sigmaSpace)
    : m_d(d)
    , m_sigmaColor(sigmaColor)
    , m_sigmaSpace(sigmaSpace)
{
}

void BilateralFilter::process(std::uint8_t* data, int width, int height, PixelFormat /*format*/)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat temp;
    cv::bilateralFilter(mat, temp, m_d, m_sigmaColor, m_sigmaSpace);
    temp.copyTo(mat);
}

// --- CustomConvolutionFilter ---
CustomConvolutionFilter::CustomConvolutionFilter()
    : m_rows(3)
    , m_cols(3)
    , m_normalize(false)
    , m_bias(0.0)
{
    m_kernelData = { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
}

CustomConvolutionFilter::CustomConvolutionFilter(
    const std::vector<float>& kernel, int kernelRows, int kernelCols, bool normalize, double bias)
    : m_rows(kernelRows)
    , m_cols(kernelCols)
    , m_normalize(normalize)
    , m_bias(bias)
{
    setKernel(kernel, kernelRows, kernelCols, normalize, bias);
}

void CustomConvolutionFilter::setKernel(
    const std::vector<float>& kernel, int rows, int cols, bool normalize, double bias)
{
    if (rows > 0 && cols > 0 && static_cast<int>(kernel.size()) == rows * cols) {
        m_kernelData = kernel;
        m_rows = rows;
        m_cols = cols;
        m_normalize = normalize;
        m_bias = bias;
    }
}

void CustomConvolutionFilter::process(std::uint8_t* data, int width, int height, PixelFormat /*format*/)
{
    if (!data || width <= 0 || height <= 0 || m_kernelData.empty() || m_rows <= 0 || m_cols <= 0) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat kernel(m_rows, m_cols, CV_32F, const_cast<float*>(m_kernelData.data()));

    cv::Mat activeKernel = kernel.clone();
    if (m_normalize) {
        float sum = static_cast<float>(cv::sum(activeKernel)[0]);
        if (std::abs(sum) > 1e-6f) {
            activeKernel /= sum;
        }
    }

    cv::filter2D(mat, mat, mat.depth(), activeKernel, cv::Point(-1, -1), m_bias);
}

// --- TemporalDenoiseFilter ---
TemporalDenoiseFilter::TemporalDenoiseFilter(double blendRate, double motionThreshold)
    : m_blendRate(std::clamp(blendRate, 0.0, 0.95))
    , m_motionThreshold(std::max(0.0, motionThreshold))
{
}

void TemporalDenoiseFilter::reset()
{
    m_historyBuffer.clear();
    m_lastWidth = 0;
    m_lastHeight = 0;
}

void TemporalDenoiseFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0 || m_blendRate <= 0.0) {
        return;
    }

    const std::size_t frameBytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3U;
    if (width != m_lastWidth || height != m_lastHeight || m_historyBuffer.size() != frameBytes) {
        m_historyBuffer.assign(data, data + frameBytes);
        m_lastWidth = width;
        m_lastHeight = height;
        return;
    }

    cv::Mat current(height, width, CV_8UC3, data);
    cv::Mat history(height, width, CV_8UC3, m_historyBuffer.data());

    cv::Mat diff;
    cv::absdiff(current, history, diff);

    cv::Mat grayDiff;
    if (format == PixelFormat::BGR24) {
        cv::cvtColor(diff, grayDiff, cv::COLOR_BGR2GRAY);
    } else {
        cv::cvtColor(diff, grayDiff, cv::COLOR_RGB2GRAY);
    }

    cv::Mat staticMask;
    cv::threshold(grayDiff, staticMask, m_motionThreshold, 255, cv::THRESH_BINARY_INV);

    cv::Mat blended;
    cv::addWeighted(current, 1.0 - m_blendRate, history, m_blendRate, 0.0, blended);

    blended.copyTo(current, staticMask);

    std::memcpy(m_historyBuffer.data(), data, frameBytes);
}

// --- LensDistortionFilter ---
LensDistortionFilter::LensDistortionFilter(double k1, double k2, double centerOffsetX, double centerOffsetY)
    : m_k1(k1)
    , m_k2(k2)
    , m_centerOffsetX(centerOffsetX)
    , m_centerOffsetY(centerOffsetY)
{
}

void LensDistortionFilter::setParameters(double k1, double k2, double centerOffsetX, double centerOffsetY)
{
    m_k1 = k1;
    m_k2 = k2;
    m_centerOffsetX = centerOffsetX;
    m_centerOffsetY = centerOffsetY;
}

void LensDistortionFilter::process(std::uint8_t* data, int width, int height, PixelFormat /*format*/)
{
    if (!data || width <= 0 || height <= 0 || (std::abs(m_k1) < 1e-6 && std::abs(m_k2) < 1e-6)) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat mapX(height, width, CV_32F);
    cv::Mat mapY(height, width, CV_32F);

    double cx = (width / 2.0) + m_centerOffsetX * width;
    double cy = (height / 2.0) + m_centerOffsetY * height;
    double maxRadius = std::sqrt(cx * cx + cy * cy);
    if (maxRadius <= 0.0) {
        return;
    }

    for (int y = 0; y < height; ++y) {
        float* pMapX = mapX.ptr<float>(y);
        float* pMapY = mapY.ptr<float>(y);
        for (int x = 0; x < width; ++x) {
            double dx = (x - cx) / maxRadius;
            double dy = (y - cy) / maxRadius;
            double r2 = dx * dx + dy * dy;
            double r4 = r2 * r2;
            double factor = 1.0 + m_k1 * r2 + m_k2 * r4;

            pMapX[x] = static_cast<float>(cx + dx * factor * maxRadius);
            pMapY[x] = static_cast<float>(cy + dy * factor * maxRadius);
        }
    }

    cv::Mat corrected;
    cv::remap(mat, corrected, mapX, mapY, cv::INTER_LINEAR, cv::BORDER_REFLECT_101);
    corrected.copyTo(mat);
}

// --- DarkChannelDehazeFilter ---
DarkChannelDehazeFilter::DarkChannelDehazeFilter(double omega, int patchSize, double t0)
    : m_omega(std::clamp(omega, 0.0, 1.0))
    , m_patchSize(std::max(3, patchSize | 1))
    , m_t0(std::clamp(t0, 0.01, 0.5))
{
}

void DarkChannelDehazeFilter::process(std::uint8_t* data, int width, int height, PixelFormat /*format*/)
{
    if (!data || width <= 0 || height <= 0 || m_omega <= 0.0) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);

    std::vector<cv::Mat> channels(3);
    cv::split(mat, channels);
    cv::Mat minChannel;
    cv::min(channels[0], channels[1], minChannel);
    cv::min(minChannel, channels[2], minChannel);

    cv::Mat darkChannel;
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(m_patchSize, m_patchSize));
    cv::erode(minChannel, darkChannel, kernel);

    const int numTopPixels = std::max(1, static_cast<int>(static_cast<double>(width * height) * 0.001));
    cv::Mat flatDark = darkChannel.reshape(1, 1);
    cv::Mat sortedIdx;
    cv::sortIdx(flatDark, sortedIdx, cv::SORT_EVERY_ROW + cv::SORT_DESCENDING);

    double sumA[3] = { 0.0, 0.0, 0.0 };
    for (int i = 0; i < numTopPixels; ++i) {
        const int idx = sortedIdx.at<int>(0, i);
        const int r = idx / width;
        const int c = idx % width;
        const cv::Vec3b pixel = mat.at<cv::Vec3b>(r, c);
        sumA[0] += static_cast<double>(pixel[0]);
        sumA[1] += static_cast<double>(pixel[1]);
        sumA[2] += static_cast<double>(pixel[2]);
    }
    const double denom = static_cast<double>(numTopPixels);
    const double A[3]
        = { std::max(1.0, sumA[0] / denom), std::max(1.0, sumA[1] / denom), std::max(1.0, sumA[2] / denom) };

    cv::Mat normCh0, normCh1, normCh2;
    channels[0].convertTo(normCh0, CV_32F, 1.0 / A[0]);
    channels[1].convertTo(normCh1, CV_32F, 1.0 / A[1]);
    channels[2].convertTo(normCh2, CV_32F, 1.0 / A[2]);

    cv::Mat minNorm;
    cv::min(normCh0, normCh1, minNorm);
    cv::min(minNorm, normCh2, minNorm);

    cv::Mat darkNorm;
    cv::erode(minNorm, darkNorm, kernel);

    cv::Mat transmission = 1.0f - static_cast<float>(m_omega) * darkNorm;
    cv::blur(transmission, transmission, cv::Size(m_patchSize, m_patchSize));
    cv::max(transmission, static_cast<float>(m_t0), transmission);

    for (std::size_t c = 0U; c < 3U; ++c) {
        cv::Mat chFloat;
        channels[c].convertTo(chFloat, CV_32F);
        cv::Mat diff = chFloat - A[c];
        cv::Mat recovered = (diff / transmission) + A[c];
        recovered.convertTo(channels[c], CV_8U);
    }
    cv::merge(channels, mat);
}

// --- ChromaticAberrationFilter ---
ChromaticAberrationFilter::ChromaticAberrationFilter(
    double redCoeff, double blueCoeff, double centerOffsetX, double centerOffsetY)
    : m_redCoeff(redCoeff)
    , m_blueCoeff(blueCoeff)
    , m_centerOffsetX(centerOffsetX)
    , m_centerOffsetY(centerOffsetY)
{
}

void ChromaticAberrationFilter::setParameters(
    double redCoeff, double blueCoeff, double centerOffsetX, double centerOffsetY)
{
    m_redCoeff = redCoeff;
    m_blueCoeff = blueCoeff;
    m_centerOffsetX = centerOffsetX;
    m_centerOffsetY = centerOffsetY;
}

void ChromaticAberrationFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0 || (std::abs(m_redCoeff) < 1e-6 && std::abs(m_blueCoeff) < 1e-6)) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);
    std::vector<cv::Mat> channels(3);
    cv::split(mat, channels);

    const std::size_t bIdx = (format == PixelFormat::BGR24) ? 0U : 2U;
    const std::size_t rIdx = (format == PixelFormat::BGR24) ? 2U : 0U;

    const double cx = (static_cast<double>(width) / 2.0) + m_centerOffsetX * static_cast<double>(width);
    const double cy = (static_cast<double>(height) / 2.0) + m_centerOffsetY * static_cast<double>(height);
    const double maxRadius = std::sqrt(cx * cx + cy * cy);
    if (maxRadius <= 0.0) {
        return;
    }

    auto warpChannel = [&](cv::Mat& ch, double k) {
        if (std::abs(k) < 1e-6)
            return;
        cv::Mat mapX(height, width, CV_32F);
        cv::Mat mapY(height, width, CV_32F);
        for (int y = 0; y < height; ++y) {
            float* pMapX = mapX.ptr<float>(y);
            float* pMapY = mapY.ptr<float>(y);
            for (int x = 0; x < width; ++x) {
                const double dx = (static_cast<double>(x) - cx) / maxRadius;
                const double dy = (static_cast<double>(y) - cy) / maxRadius;
                const double r2 = dx * dx + dy * dy;
                const double factor = 1.0 + k * r2;
                pMapX[x] = static_cast<float>(cx + dx * factor * maxRadius);
                pMapY[x] = static_cast<float>(cy + dy * factor * maxRadius);
            }
        }
        cv::Mat warped;
        cv::remap(ch, warped, mapX, mapY, cv::INTER_LINEAR, cv::BORDER_REFLECT_101);
        ch = warped;
    };

    warpChannel(channels[rIdx], m_redCoeff);
    warpChannel(channels[bIdx], m_blueCoeff);

    cv::merge(channels, mat);
}

} // namespace Video::Filters

#endif // PELCOD_HAS_FILTERS
