/**
 * @file ColorFilters.cpp
 * @brief Implementations of video filters for color adjustments, contrast, tone, and histogram equalization.
 */

#include "ColorFilters.h"

#if defined(PELCOD_HAS_FILTERS)

#include <algorithm>
#include <cmath>
#include <numeric>
#include <opencv2/opencv.hpp>

namespace Video::Filters {

// --- BrightnessContrastFilter ---
BrightnessContrastFilter::BrightnessContrastFilter(double alpha, int beta)
    : m_alpha(alpha)
    , m_beta(beta)
{
}

void BrightnessContrastFilter::process(std::uint8_t* data, int width, int height, PixelFormat /*format*/)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    mat.convertTo(mat, -1, m_alpha, m_beta);
}

// --- InvertColorsFilter ---
void InvertColorsFilter::process(std::uint8_t* data, int width, int height, PixelFormat /*format*/)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::bitwise_not(mat, mat);
}

// --- GrayscaleFilter ---
void GrayscaleFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat gray;
    if (format == PixelFormat::BGR24) {
        cv::cvtColor(mat, gray, cv::COLOR_BGR2GRAY);
        cv::cvtColor(gray, mat, cv::COLOR_GRAY2BGR);
    } else {
        cv::cvtColor(mat, gray, cv::COLOR_RGB2GRAY);
        cv::cvtColor(gray, mat, cv::COLOR_GRAY2RGB);
    }
}

// --- SepiaFilter ---
void SepiaFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);

    cv::Mat sepiaKernel;
    if (format == PixelFormat::BGR24) {
        sepiaKernel = (cv::Mat_<float>(3, 3) << 0.131f, 0.534f, 0.272f, 0.168f, 0.686f, 0.349f, 0.189f, 0.769f, 0.393f);
    } else {
        sepiaKernel = (cv::Mat_<float>(3, 3) << 0.393f, 0.769f, 0.189f, 0.349f, 0.686f, 0.168f, 0.272f, 0.534f, 0.131f);
    }
    cv::transform(mat, mat, sepiaKernel);
}

// --- ColorTintFilter ---
ColorTintFilter::ColorTintFilter(double rScale, double gScale, double bScale)
    : m_rScale(rScale)
    , m_gScale(gScale)
    , m_bScale(bScale)
{
}

void ColorTintFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    std::vector<cv::Mat> channels;
    cv::split(mat, channels);

    if (format == PixelFormat::BGR24) {
        channels[0] *= m_bScale;
        channels[1] *= m_gScale;
        channels[2] *= m_rScale;
    } else {
        channels[0] *= m_rScale;
        channels[1] *= m_gScale;
        channels[2] *= m_bScale;
    }

    cv::merge(channels, mat);
}

// --- ClaheFilter ---
ClaheFilter::ClaheFilter(double clipLimit, int tileGridSize, double blend)
    : m_clipLimit(clipLimit)
    , m_tileGridSize(tileGridSize)
    , m_blend(std::clamp(blend, 0.0, 1.0))
{
}

void ClaheFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0 || m_blend <= 0.0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat lab;
    cv::cvtColor(mat, lab, (format == PixelFormat::BGR24) ? cv::COLOR_BGR2Lab : cv::COLOR_RGB2Lab);
    std::vector<cv::Mat> channels;
    cv::split(lab, channels);

    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(m_clipLimit, cv::Size(m_tileGridSize, m_tileGridSize));
    clahe->apply(channels[0], channels[0]);

    cv::merge(channels, lab);
    cv::Mat enhanced;
    cv::cvtColor(lab, enhanced, (format == PixelFormat::BGR24) ? cv::COLOR_Lab2BGR : cv::COLOR_Lab2RGB);

    if (m_blend >= 1.0) {
        enhanced.copyTo(mat);
    } else {
        cv::addWeighted(mat, 1.0 - m_blend, enhanced, m_blend, 0.0, mat);
    }
}

// --- GammaCorrectionFilter ---
GammaCorrectionFilter::GammaCorrectionFilter(double gamma)
    : m_gamma(gamma)
{
}

void GammaCorrectionFilter::process(std::uint8_t* data, int width, int height, PixelFormat /*format*/)
{
    if (!data || width <= 0 || height <= 0 || m_gamma <= 0.0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat lookUpTable(1, 256, CV_8U);
    std::uint8_t* p = lookUpTable.ptr();
    for (int i = 0; i < 256; ++i) {
        p[i] = cv::saturate_cast<std::uint8_t>(pow(i / 255.0, m_gamma) * 255.0);
    }
    cv::LUT(mat, lookUpTable, mat);
}

// --- VignetteFilter ---
void VignetteFilter::process(std::uint8_t* data, int width, int height, PixelFormat /*format*/)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat mask = cv::Mat::zeros(height, width, CV_32F);
    cv::Point center(width / 2, height / 2);
    double max_dist = cv::norm(center);
    if (max_dist <= 0.0) {
        return;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double dist = cv::norm(cv::Point(x, y) - center);
            mask.at<float>(y, x) = static_cast<float>(1.0 - (dist / max_dist));
        }
    }

    std::vector<cv::Mat> channels;
    cv::split(mat, channels);
    for (auto& channel : channels) {
        cv::Mat fChan;
        channel.convertTo(fChan, CV_32F);
        cv::multiply(fChan, mask, fChan);
        fChan.convertTo(channel, CV_8U);
    }
    cv::merge(channels, mat);
}

// --- ThresholdFilter ---
ThresholdFilter::ThresholdFilter(double thresholdValue)
    : m_thresholdValue(thresholdValue)
{
}

void ThresholdFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat gray;
    if (format == PixelFormat::BGR24) {
        cv::cvtColor(mat, gray, cv::COLOR_BGR2GRAY);
    } else {
        cv::cvtColor(mat, gray, cv::COLOR_RGB2GRAY);
    }
    cv::threshold(gray, gray, m_thresholdValue, 255, cv::THRESH_BINARY);
    if (format == PixelFormat::BGR24) {
        cv::cvtColor(gray, mat, cv::COLOR_GRAY2BGR);
    } else {
        cv::cvtColor(gray, mat, cv::COLOR_GRAY2RGB);
    }
}

// --- HistogramEqualizationFilter ---
HistogramEqualizationFilter::HistogramEqualizationFilter(
    Mode mode, double blend, double brightnessOffset, double histAveRate, double maxPercentBin, double gamma)
    : m_mode(mode)
    , m_blend(std::clamp(blend, 0.0, 1.0))
    , m_brightnessOffset(brightnessOffset)
    , m_histAveRate(std::clamp(histAveRate, 0.0, 0.95))
    , m_maxPercentBin(std::clamp(maxPercentBin, 0.01, 1.0))
    , m_gamma(std::max(0.1, gamma))
{
}

void HistogramEqualizationFilter::resetTemporalMap()
{
    m_prevLut.clear();
}

void HistogramEqualizationFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0 || m_blend <= 0.0) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat ycrcb;
    cv::cvtColor(mat, ycrcb, (format == PixelFormat::BGR24) ? cv::COLOR_BGR2YCrCb : cv::COLOR_RGB2YCrCb);
    std::vector<cv::Mat> channels;
    cv::split(ycrcb, channels);
    cv::Mat y = channels[0];

    std::vector<double> hist(256, 0.0);

    if (m_mode == Mode::FeatureBased) {
        // High-pass Sobel gradient energy weighting
        cv::Mat gradX, gradY, gradMag;
        cv::Sobel(y, gradX, CV_32F, 1, 0, 3);
        cv::Sobel(y, gradY, CV_32F, 0, 1, 3);
        cv::magnitude(gradX, gradY, gradMag);

        const float* pGrad = reinterpret_cast<const float*>(gradMag.data);
        const std::uint8_t* pY = y.data;
        const std::size_t totalPixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        for (std::size_t i = 0U; i < totalPixels; ++i) {
            hist[pY[i]] += (static_cast<double>(pGrad[i]) + 1.0); // Base count + gradient energy
        }
    } else {
        // Standard intensity histogram
        const std::uint8_t* pY = y.data;
        const std::size_t totalPixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        for (std::size_t i = 0U; i < totalPixels; ++i) {
            hist[pY[i]] += 1.0;
        }

        if (m_mode == Mode::SquareRoot) {
            // Apply square root to bin counts to suppress uniform background dominance
            for (std::size_t i = 0U; i < 256U; ++i) {
                hist[i] = std::sqrt(hist[i]);
            }
        }
    }

    // Clamp histogram bins to maxPercentBin
    double totalWeight = std::accumulate(hist.begin(), hist.end(), 0.0);
    if (totalWeight <= 0.0) {
        return;
    }
    double maxBinVal = m_maxPercentBin * totalWeight;
    for (std::size_t i = 0U; i < 256U; ++i) {
        if (hist[i] > maxBinVal) {
            hist[i] = maxBinVal;
        }
    }

    // Recalculate total weight after clamping and compute CDF
    totalWeight = std::accumulate(hist.begin(), hist.end(), 0.0);
    std::vector<double> cdf(256, 0.0);
    double cumulative = 0.0;
    for (std::size_t i = 0U; i < 256U; ++i) {
        cumulative += hist[i];
        cdf[i] = cumulative / totalWeight;
    }

    // Build equalization LUT with brightness offset and gamma mapping
    std::vector<float> currentLut(256, 0.0f);
    for (std::size_t i = 0U; i < 256U; ++i) {
        double val = cdf[i];
        if (std::abs(m_gamma - 1.0) > 1e-6) {
            val = std::pow(val, m_gamma);
        }
        double mapped = val * 255.0 + m_brightnessOffset;
        currentLut[i] = static_cast<float>(std::clamp(mapped, 0.0, 255.0));
    }

    // Temporal smoothing (anti-flicker across frames)
    if (m_histAveRate > 0.0 && m_prevLut.size() == 256) {
        float rate = static_cast<float>(m_histAveRate);
        for (std::size_t i = 0U; i < 256U; ++i) {
            currentLut[i] = (1.0f - rate) * currentLut[i] + rate * m_prevLut[i];
        }
    }
    m_prevLut = currentLut;

    // Apply LUT to intensity channel
    cv::Mat lutMat(1, 256, CV_8U);
    std::uint8_t* pLut = lutMat.ptr();
    for (std::size_t i = 0U; i < 256U; ++i) {
        pLut[i] = cv::saturate_cast<std::uint8_t>(currentLut[i]);
    }

    cv::Mat equalizedY;
    cv::LUT(y, lutMat, equalizedY);

    if (m_blend < 1.0) {
        cv::addWeighted(y, 1.0 - m_blend, equalizedY, m_blend, 0.0, channels[0]);
    } else {
        channels[0] = equalizedY;
    }

    cv::merge(channels, ycrcb);
    cv::cvtColor(ycrcb, mat, (format == PixelFormat::BGR24) ? cv::COLOR_YCrCb2BGR : cv::COLOR_YCrCb2RGB);
}

// --- ColorEnhanceFilter ---
ColorEnhanceFilter::ColorEnhanceFilter(double factor)
    : m_factor(std::max(0.0, factor))
{
}

void ColorEnhanceFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0 || std::abs(m_factor - 1.0) < 1e-6) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat hsv;
    cv::cvtColor(mat, hsv, (format == PixelFormat::BGR24) ? cv::COLOR_BGR2HSV : cv::COLOR_RGB2HSV);

    std::vector<cv::Mat> channels;
    cv::split(hsv, channels);

    // Channel 1 is Saturation in OpenCV HSV (0-255)
    channels[1].convertTo(channels[1], CV_32F);
    channels[1] *= m_factor;
    cv::threshold(channels[1], channels[1], 255.0, 255.0, cv::THRESH_TRUNC);
    channels[1].convertTo(channels[1], CV_8U);

    cv::merge(channels, hsv);
    cv::cvtColor(hsv, mat, (format == PixelFormat::BGR24) ? cv::COLOR_HSV2BGR : cv::COLOR_HSV2RGB);
}

// --- WhiteBalanceFilter ---
WhiteBalanceFilter::WhiteBalanceFilter(Mode mode, double strength)
    : m_mode(mode)
    , m_strength(std::clamp(strength, 0.0, 1.0))
{
}

void WhiteBalanceFilter::process(std::uint8_t* data, int width, int height, PixelFormat /*format*/)
{
    if (!data || width <= 0 || height <= 0 || m_strength <= 0.0) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);
    std::vector<cv::Mat> channels(3);
    cv::split(mat, channels);

    double gain[3] = { 1.0, 1.0, 1.0 };
    if (m_mode == Mode::GrayWorld) {
        const cv::Scalar meanVal = cv::mean(mat);
        const double avgMean = (meanVal[0] + meanVal[1] + meanVal[2]) / 3.0;
        gain[0] = avgMean / std::max(1.0, meanVal[0]);
        gain[1] = avgMean / std::max(1.0, meanVal[1]);
        gain[2] = avgMean / std::max(1.0, meanVal[2]);
    } else { // WhitePatch
        double minVal = 0.0, maxVal = 0.0;
        for (std::size_t i = 0U; i < 3U; ++i) {
            cv::minMaxLoc(channels[i], &minVal, &maxVal);
            gain[i] = 255.0 / std::max(1.0, maxVal);
        }
    }

    for (std::size_t i = 0U; i < 3U; ++i) {
        const double g = 1.0 + m_strength * (gain[i] - 1.0);
        cv::Mat scaled;
        channels[i].convertTo(scaled, CV_8U, g);
        channels[i] = scaled;
    }

    cv::merge(channels, mat);
}

} // namespace Video::Filters

#endif // PELCOD_HAS_FILTERS
