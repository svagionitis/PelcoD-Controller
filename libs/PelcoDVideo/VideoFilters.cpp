/**
 * @file VideoFilters.cpp
 * @brief Implementations of OpenCV-based video filters and Sightline-inspired enhancement algorithms.
 */

#include "VideoFilters.h"

#if defined(PELCOD_HAS_FILTERS)

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <opencv2/opencv.hpp>

namespace PelcoD::Video {

// --- BrightnessContrastFilter ---
BrightnessContrastFilter::BrightnessContrastFilter(double alpha, int beta)
    : m_alpha(alpha)
    , m_beta(beta)
{
}

void BrightnessContrastFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    mat.convertTo(mat, -1, m_alpha, m_beta);
}

// --- GaussianBlurFilter ---
GaussianBlurFilter::GaussianBlurFilter(int kernelSize)
    : m_kernelSize(kernelSize)
{
}

void GaussianBlurFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
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

void EdgeDetectionFilter::process(uint8_t* data, int width, int height, PixelFormat format)
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

// --- TextOverlayFilter ---
TextOverlayFilter::TextOverlayFilter(const std::string& text, int x, int y, double scale)
    : m_text(text)
    , m_x(x)
    , m_y(y)
    , m_scale(scale)
{
}

void TextOverlayFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Scalar color = (format == PixelFormat::BGR24) ? cv::Scalar(0, 255, 255) : cv::Scalar(255, 255, 0); // Yellow

    cv::putText(mat, m_text, cv::Point(m_x, m_y), cv::FONT_HERSHEY_SIMPLEX, m_scale, color, 2);
}

// --- MirrorFilter ---
MirrorFilter::MirrorFilter(bool horizontal)
    : m_horizontal(horizontal)
{
}

void MirrorFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::flip(mat, mat, m_horizontal ? 1 : 0);
}

// --- InvertColorsFilter ---
void InvertColorsFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::bitwise_not(mat, mat);
}

// --- GrayscaleFilter ---
void GrayscaleFilter::process(uint8_t* data, int width, int height, PixelFormat format)
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
void SepiaFilter::process(uint8_t* data, int width, int height, PixelFormat format)
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

// --- SharpenFilter (Upgraded) ---
SharpenFilter::SharpenFilter(double strength, int radius)
    : m_strength(strength)
    , m_radius(std::max(1, radius))
{
}

void SharpenFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
    if (!data || width <= 0 || height <= 0 || m_strength <= 0.0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);

    // Unsharp mask implementation with variable radius:
    // Enhanced = Mat + Strength * (Mat - Blurred) = (1 + Strength) * Mat - Strength * Blurred
    int ksize = 2 * m_radius + 1;
    cv::Mat blurred;
    cv::GaussianBlur(mat, blurred, cv::Size(ksize, ksize), 0);
    cv::addWeighted(mat, 1.0 + m_strength, blurred, -m_strength, 0.0, mat);
}

// --- ColorTintFilter ---
ColorTintFilter::ColorTintFilter(double rScale, double gScale, double bScale)
    : m_rScale(rScale)
    , m_gScale(gScale)
    , m_bScale(bScale)
{
}

void ColorTintFilter::process(uint8_t* data, int width, int height, PixelFormat format)
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

// --- ClaheFilter (Upgraded with Blend) ---
ClaheFilter::ClaheFilter(double clipLimit, int tileGridSize, double blend)
    : m_clipLimit(clipLimit)
    , m_tileGridSize(tileGridSize)
    , m_blend(std::clamp(blend, 0.0, 1.0))
{
}

void ClaheFilter::process(uint8_t* data, int width, int height, PixelFormat format)
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

// --- BilateralFilter ---
BilateralFilter::BilateralFilter(int d, double sigmaColor, double sigmaSpace)
    : m_d(d)
    , m_sigmaColor(sigmaColor)
    , m_sigmaSpace(sigmaSpace)
{
}

void BilateralFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat temp;
    cv::bilateralFilter(mat, temp, m_d, m_sigmaColor, m_sigmaSpace);
    temp.copyTo(mat);
}

// --- GammaCorrectionFilter ---
GammaCorrectionFilter::GammaCorrectionFilter(double gamma)
    : m_gamma(gamma)
{
}

void GammaCorrectionFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
    if (!data || width <= 0 || height <= 0 || m_gamma <= 0.0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat lookUpTable(1, 256, CV_8U);
    uint8_t* p = lookUpTable.ptr();
    for (int i = 0; i < 256; ++i) {
        p[i] = cv::saturate_cast<uint8_t>(pow(i / 255.0, m_gamma) * 255.0);
    }
    cv::LUT(mat, lookUpTable, mat);
}

// --- VignetteFilter ---
void VignetteFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
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

// --- MosaicFilter ---
MosaicFilter::MosaicFilter(int blockSize)
    : m_blockSize(blockSize)
{
}

void MosaicFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
    if (!data || width <= 0 || height <= 0 || m_blockSize <= 1) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat temp;
    int w = std::max(1, width / m_blockSize);
    int h = std::max(1, height / m_blockSize);
    cv::resize(mat, temp, cv::Size(w, h), 0, 0, cv::INTER_NEAREST);
    cv::resize(temp, mat, mat.size(), 0, 0, cv::INTER_NEAREST);
}

// --- ThresholdFilter ---
ThresholdFilter::ThresholdFilter(double thresholdValue)
    : m_thresholdValue(thresholdValue)
{
}

void ThresholdFilter::process(uint8_t* data, int width, int height, PixelFormat format)
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

// =========================================================================
// Sightline Enhancement Suite: Thermal, Tactical, and Vision Filters
// =========================================================================

// --- FalseColorFilter ---
FalseColorFilter::FalseColorFilter(FalseColorPalette palette)
    : m_palette(palette)
{
    initDefaultUserPalette();
}

void FalseColorFilter::initDefaultUserPalette()
{
    // Initialize default Iron-like gradient user palette
    std::map<uint8_t, std::vector<uint8_t>> controlPoints = {
        { 0, { 0, 0, 0 } }, // Black
        { 64, { 128, 0, 128 } }, // Purple
        { 128, { 255, 0, 0 } }, // Red
        { 192, { 255, 255, 0 } }, // Yellow
        { 255, { 255, 255, 255 } } // White
    };
    generateInterpolatedPalette(controlPoints, true);
}

void FalseColorFilter::setUserPalette(const std::vector<uint8_t>& lut256x3)
{
    if (lut256x3.size() == 768) {
        m_userPalette = lut256x3;
    }
}

void FalseColorFilter::generateInterpolatedPalette(
    const std::map<uint8_t, std::vector<uint8_t>>& controlPoints, bool smooth)
{
    (void)smooth;
    if (controlPoints.empty()) {
        return;
    }

    m_userPalette.resize(768);

    auto it = controlPoints.begin();
    std::size_t prevIdx = it->first;
    std::vector<uint8_t> prevColor = it->second;

    for (std::size_t i = 0U; i <= prevIdx; ++i) {
        m_userPalette[i * 3U + 0U] = prevColor[0];
        m_userPalette[i * 3U + 1U] = prevColor[1];
        m_userPalette[i * 3U + 2U] = prevColor[2];
    }

    for (++it; it != controlPoints.end(); ++it) {
        std::size_t nextIdx = it->first;
        std::vector<uint8_t> nextColor = it->second;

        if (nextIdx > prevIdx) {
            float span = static_cast<float>(nextIdx - prevIdx);
            for (std::size_t i = prevIdx; i <= nextIdx; ++i) {
                float t = static_cast<float>(i - prevIdx) / span;
                m_userPalette[i * 3U + 0U] = static_cast<uint8_t>(
                    static_cast<float>(prevColor[0]) + t * static_cast<float>(nextColor[0] - prevColor[0]));
                m_userPalette[i * 3U + 1U] = static_cast<uint8_t>(
                    static_cast<float>(prevColor[1]) + t * static_cast<float>(nextColor[1] - prevColor[1]));
                m_userPalette[i * 3U + 2U] = static_cast<uint8_t>(
                    static_cast<float>(prevColor[2]) + t * static_cast<float>(nextColor[2] - prevColor[2]));
            }
        }
        prevIdx = nextIdx;
        prevColor = nextColor;
    }

    for (std::size_t i = prevIdx; i < 256U; ++i) {
        m_userPalette[i * 3U + 0U] = prevColor[0];
        m_userPalette[i * 3U + 1U] = prevColor[1];
        m_userPalette[i * 3U + 2U] = prevColor[2];
    }
}

bool FalseColorFilter::loadUserPaletteFromFile(const std::string& filepath, bool isYuv)
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    std::vector<uint8_t> buffer(768);
    file.read(reinterpret_cast<char*>(buffer.data()), 768);
    if (file.gcount() != 768) {
        return false;
    }

    if (isYuv) {
        // Convert Sightline YUV binary format (Y, U, V) to RGB
        m_userPalette.resize(768);
        for (std::size_t i = 0U; i < 256U; ++i) {
            double y = static_cast<double>(buffer[i * 3U + 0U]);
            double u = static_cast<double>(buffer[i * 3U + 1U]) - 128.0;
            double v = static_cast<double>(buffer[i * 3U + 2U]) - 128.0;

            double r = y + 1.13983 * v;
            double g = y - 0.39465 * u - 0.58060 * v;
            double b = y + 2.03211 * u;

            m_userPalette[i * 3U + 0U] = cv::saturate_cast<uint8_t>(r);
            m_userPalette[i * 3U + 1U] = cv::saturate_cast<uint8_t>(g);
            m_userPalette[i * 3U + 2U] = cv::saturate_cast<uint8_t>(b);
        }
    } else {
        m_userPalette = std::move(buffer);
    }
    return true;
}

bool FalseColorFilter::saveUserPaletteToFile(const std::string& filepath, bool asYuv) const
{
    if (m_userPalette.size() != 768) {
        return false;
    }

    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    if (asYuv) {
        std::vector<uint8_t> yuvBuffer(768);
        for (std::size_t i = 0U; i < 256U; ++i) {
            double r = m_userPalette[i * 3U + 0U];
            double g = m_userPalette[i * 3U + 1U];
            double b = m_userPalette[i * 3U + 2U];

            double y = 0.299 * r + 0.587 * g + 0.114 * b;
            double u = -0.14713 * r - 0.28886 * g + 0.436 * b + 128.0;
            double v = 0.615 * r - 0.51499 * g - 0.10001 * b + 128.0;

            yuvBuffer[i * 3U + 0U] = cv::saturate_cast<uint8_t>(y);
            yuvBuffer[i * 3U + 1U] = cv::saturate_cast<uint8_t>(u);
            yuvBuffer[i * 3U + 2U] = cv::saturate_cast<uint8_t>(v);
        }
        file.write(reinterpret_cast<const char*>(yuvBuffer.data()), 768);
    } else {
        file.write(reinterpret_cast<const char*>(m_userPalette.data()), 768);
    }
    return file.good();
}

void FalseColorFilter::process(uint8_t* data, int width, int height, PixelFormat format)
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

    cv::Mat colored;
    switch (m_palette) {
    case FalseColorPalette::WhiteHot:
        if (format == PixelFormat::BGR24) {
            cv::cvtColor(gray, mat, cv::COLOR_GRAY2BGR);
        } else {
            cv::cvtColor(gray, mat, cv::COLOR_GRAY2RGB);
        }
        return;

    case FalseColorPalette::BlackHot:
        cv::bitwise_not(gray, gray);
        if (format == PixelFormat::BGR24) {
            cv::cvtColor(gray, mat, cv::COLOR_GRAY2BGR);
        } else {
            cv::cvtColor(gray, mat, cv::COLOR_GRAY2RGB);
        }
        return;

    case FalseColorPalette::Iron256:
        cv::applyColorMap(gray, colored, cv::COLORMAP_INFERNO);
        break;

    case FalseColorPalette::Jet:
        cv::applyColorMap(gray, colored, cv::COLORMAP_JET);
        break;

    case FalseColorPalette::Rainbow:
        cv::applyColorMap(gray, colored, cv::COLORMAP_RAINBOW);
        break;

    case FalseColorPalette::HotCold:
    case FalseColorPalette::IceFire:
        cv::applyColorMap(gray, colored, cv::COLORMAP_COOL);
        break;

    case FalseColorPalette::HotIron:
        cv::applyColorMap(gray, colored, cv::COLORMAP_HOT);
        break;

    case FalseColorPalette::Turbo:
        cv::applyColorMap(gray, colored, cv::COLORMAP_TURBO);
        break;

    case FalseColorPalette::Bone:
        cv::applyColorMap(gray, colored, cv::COLORMAP_BONE);
        break;

    case FalseColorPalette::UserPalette:
    default: {
        if (m_userPalette.size() != 768) {
            initDefaultUserPalette();
        }
        cv::Mat rgbLut(1, 256, CV_8UC3, m_userPalette.data());
        cv::Mat bgrLut;
        cv::cvtColor(rgbLut, bgrLut, cv::COLOR_RGB2BGR);
        cv::Mat gray3;
        cv::cvtColor(gray, gray3, cv::COLOR_GRAY2BGR);
        cv::LUT(gray3, bgrLut, colored);
        break;
    }
    }

    // Apply color result back to mat
    if (format == PixelFormat::BGR24) {
        colored.copyTo(mat);
    } else {
        cv::cvtColor(colored, mat, cv::COLOR_BGR2RGB);
    }
}

// --- LocalAreaProcessingFilter (LAP) ---
LocalAreaProcessingFilter::LocalAreaProcessingFilter(int strength, double blend, double lapMinDiff)
    : m_strength(std::clamp(strength, 1, 18))
    , m_blend(std::clamp(blend, 0.0, 1.0))
    , m_lapMinDiff(std::max(0.0, lapMinDiff))
{
}

void LocalAreaProcessingFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
    if (!data || width <= 0 || height <= 0 || m_blend <= 0.0) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat floatMat;
    mat.convertTo(floatMat, CV_32FC3);

    // Compute local mean using box filter (kernel radius based on strength)
    int ksize = 2 * m_strength + 1;
    cv::Mat localMean;
    cv::boxFilter(floatMat, localMean, CV_32FC3, cv::Size(ksize, ksize));

    // LAP difference = Input - LocalMean
    cv::Mat diff = floatMat - localMean;

    // Apply lapMinDiff dead-zone / soft threshold to prevent noise contouring in flat regions
    if (m_lapMinDiff > 0.0) {
        std::vector<cv::Mat> diffChannels;
        cv::split(diff, diffChannels);
        for (auto& chan : diffChannels) {
            cv::Mat absChan = cv::abs(chan);
            cv::Mat mask;
            cv::threshold(absChan, mask, m_lapMinDiff, 1.0, cv::THRESH_BINARY);
            cv::multiply(chan, mask, chan);
        }
        cv::merge(diffChannels, diff);
    }

    // Enhanced = Input + Difference
    cv::Mat enhanced = floatMat + diff;
    cv::Mat enhanced8U;
    enhanced.convertTo(enhanced8U, CV_8UC3);

    // Alpha blend with original
    if (m_blend >= 1.0) {
        enhanced8U.copyTo(mat);
    } else {
        cv::addWeighted(mat, 1.0 - m_blend, enhanced8U, m_blend, 0.0, mat);
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

void HistogramEqualizationFilter::process(uint8_t* data, int width, int height, PixelFormat format)
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
        const uint8_t* pY = y.data;
        const std::size_t totalPixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        for (std::size_t i = 0U; i < totalPixels; ++i) {
            hist[pY[i]] += (static_cast<double>(pGrad[i]) + 1.0); // Base count + gradient energy
        }
    } else {
        // Standard intensity histogram
        const uint8_t* pY = y.data;
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
        if (m_gamma != 1.0) {
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
    uint8_t* pLut = lutMat.ptr();
    for (std::size_t i = 0U; i < 256U; ++i) {
        pLut[i] = cv::saturate_cast<uint8_t>(currentLut[i]);
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

void ColorEnhanceFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0 || m_factor == 1.0) {
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

// --- CustomConvolutionFilter ---
CustomConvolutionFilter::CustomConvolutionFilter()
    : m_rows(3)
    , m_cols(3)
    , m_normalize(false)
    , m_bias(0.0)
{
    // Default identity kernel (centered 1 surrounded by 0s)
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

void CustomConvolutionFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
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

void TemporalDenoiseFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
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

    // Compute pixel delta across 3 channels
    cv::Mat grayDiff;
    if (format == PixelFormat::BGR24) {
        cv::cvtColor(diff, grayDiff, cv::COLOR_BGR2GRAY);
    } else {
        cv::cvtColor(diff, grayDiff, cv::COLOR_RGB2GRAY);
    }

    // Create mask for static regions (difference < motionThreshold)
    cv::Mat staticMask;
    cv::threshold(grayDiff, staticMask, m_motionThreshold, 255, cv::THRESH_BINARY_INV);

    // Blend static regions: result = (1 - blendRate) * current + blendRate * history
    cv::Mat blended;
    cv::addWeighted(current, 1.0 - m_blendRate, history, m_blendRate, 0.0, blended);

    // Copy blended result only to static regions; motion regions keep current pixel
    blended.copyTo(current, staticMask);

    // Update history buffer with latest frame output
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

void LensDistortionFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
    if (!data || width <= 0 || height <= 0 || (m_k1 == 0.0 && m_k2 == 0.0)) {
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

void DarkChannelDehazeFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
    if (!data || width <= 0 || height <= 0 || m_omega <= 0.0) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);

    // 1. Calculate dark channel across RGB color planes
    std::vector<cv::Mat> channels(3);
    cv::split(mat, channels);
    cv::Mat minChannel;
    cv::min(channels[0], channels[1], minChannel);
    cv::min(minChannel, channels[2], minChannel);

    cv::Mat darkChannel;
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(m_patchSize, m_patchSize));
    cv::erode(minChannel, darkChannel, kernel);

    // 2. Estimate atmospheric light A from brightest dark channel pixels
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

    // 3. Compute transmission map
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

    // 4. Recover scene radiance J = (I - A) / max(t, t0) + A
    for (std::size_t c = 0U; c < 3U; ++c) {
        cv::Mat chFloat;
        channels[c].convertTo(chFloat, CV_32F);
        cv::Mat diff = chFloat - A[c];
        cv::Mat recovered = (diff / transmission) + A[c];
        recovered.convertTo(channels[c], CV_8U);
    }
    cv::merge(channels, mat);
}

// --- ImageStabilizationFilter ---
struct ImageStabilizationFilter::Impl {
    cv::Mat prevGray;
    double prevX { 0.0 };
    double prevY { 0.0 };
    double prevA { 0.0 };
    double smoothX { 0.0 };
    double smoothY { 0.0 };
    double smoothA { 0.0 };
    bool hasPrev { false };
};

ImageStabilizationFilter::ImageStabilizationFilter(
    double smoothingFactor, double maxJitterPixels, double cropMarginPercent)
    : m_smoothingFactor(std::clamp(smoothingFactor, 0.0, 0.98))
    , m_maxJitterPixels(std::max(5.0, maxJitterPixels))
    , m_cropMarginPercent(std::clamp(cropMarginPercent, 0.0, 0.2))
    , m_impl(std::make_unique<Impl>())
{
}

ImageStabilizationFilter::~ImageStabilizationFilter() = default;
ImageStabilizationFilter::ImageStabilizationFilter(ImageStabilizationFilter&&) noexcept = default;
ImageStabilizationFilter& ImageStabilizationFilter::operator=(ImageStabilizationFilter&&) noexcept = default;

void ImageStabilizationFilter::reset()
{
    if (m_impl) {
        m_impl->prevGray.release();
        m_impl->prevX = 0.0;
        m_impl->prevY = 0.0;
        m_impl->prevA = 0.0;
        m_impl->smoothX = 0.0;
        m_impl->smoothY = 0.0;
        m_impl->smoothA = 0.0;
        m_impl->hasPrev = false;
    }
}

void ImageStabilizationFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0 || !m_impl) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat curGray;
    if (format == PixelFormat::BGR24) {
        cv::cvtColor(mat, curGray, cv::COLOR_BGR2GRAY);
    } else {
        cv::cvtColor(mat, curGray, cv::COLOR_RGB2GRAY);
    }

    if (!m_impl->hasPrev || m_impl->prevGray.cols != width || m_impl->prevGray.rows != height) {
        m_impl->prevGray = curGray;
        m_impl->hasPrev = true;
        return;
    }

    std::vector<cv::Point2f> prevPts;
    cv::goodFeaturesToTrack(m_impl->prevGray, prevPts, 150, 0.01, 15.0);

    if (prevPts.size() < 10U) {
        m_impl->prevGray = curGray;
        return;
    }

    std::vector<cv::Point2f> curPts;
    std::vector<uchar> status;
    std::vector<float> err;
    cv::calcOpticalFlowPyrLK(m_impl->prevGray, curGray, prevPts, curPts, status, err);

    std::vector<cv::Point2f> prevClean, curClean;
    for (std::size_t i = 0U; i < status.size(); ++i) {
        if (status[i]) {
            prevClean.push_back(prevPts[i]);
            curClean.push_back(curPts[i]);
        }
    }

    double dx = 0.0, dy = 0.0, da = 0.0;
    if (prevClean.size() >= 8U) {
        const cv::Mat affine = cv::estimateAffinePartial2D(prevClean, curClean);
        if (!affine.empty()) {
            dx = affine.at<double>(0, 2);
            dy = affine.at<double>(1, 2);
            da = std::atan2(affine.at<double>(1, 0), affine.at<double>(0, 0));
        }
    }

    m_impl->prevX += dx;
    m_impl->prevY += dy;
    m_impl->prevA += da;

    if (std::abs(dx) > m_maxJitterPixels || std::abs(dy) > m_maxJitterPixels) {
        m_impl->smoothX = m_impl->prevX;
        m_impl->smoothY = m_impl->prevY;
        m_impl->smoothA = m_impl->prevA;
    } else {
        m_impl->smoothX = m_smoothingFactor * m_impl->smoothX + (1.0 - m_smoothingFactor) * m_impl->prevX;
        m_impl->smoothY = m_smoothingFactor * m_impl->smoothY + (1.0 - m_smoothingFactor) * m_impl->prevY;
        m_impl->smoothA = m_smoothingFactor * m_impl->smoothA + (1.0 - m_smoothingFactor) * m_impl->prevA;
    }

    const double diffX = m_impl->smoothX - m_impl->prevX;
    const double diffY = m_impl->smoothY - m_impl->prevY;
    const double diffA = m_impl->smoothA - m_impl->prevA;

    cv::Mat warp(2, 3, CV_64F);
    const double cosA = std::cos(diffA);
    const double sinA = std::sin(diffA);
    warp.at<double>(0, 0) = cosA;
    warp.at<double>(0, 1) = -sinA;
    warp.at<double>(1, 0) = sinA;
    warp.at<double>(1, 1) = cosA;

    const double cx = static_cast<double>(width) / 2.0;
    const double cy = static_cast<double>(height) / 2.0;
    warp.at<double>(0, 2) = diffX + (cx - (cosA * cx - sinA * cy));
    warp.at<double>(1, 2) = diffY + (cy - (sinA * cx + cosA * cy));

    cv::Mat stabilized;
    cv::warpAffine(mat, stabilized, warp, mat.size(), cv::INTER_LINEAR, cv::BORDER_REFLECT_101);

    if (m_cropMarginPercent > 0.001) {
        const int cropX = static_cast<int>(static_cast<double>(width) * m_cropMarginPercent);
        const int cropY = static_cast<int>(static_cast<double>(height) * m_cropMarginPercent);
        const cv::Rect roi(cropX, cropY, width - 2 * cropX, height - 2 * cropY);
        const cv::Mat cropped = stabilized(roi);
        cv::resize(cropped, mat, mat.size(), 0.0, 0.0, cv::INTER_LINEAR);
    } else {
        stabilized.copyTo(mat);
    }

    m_impl->prevGray = curGray;
}

// --- WhiteBalanceFilter ---
WhiteBalanceFilter::WhiteBalanceFilter(Mode mode, double strength)
    : m_mode(mode)
    , m_strength(std::clamp(strength, 0.0, 1.0))
{
}

void WhiteBalanceFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    (void)format;
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

void ChromaticAberrationFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0 || (m_redCoeff == 0.0 && m_blueCoeff == 0.0)) {
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

// -----------------------------------------------------------------------------
// IsothermFilter Implementation
// -----------------------------------------------------------------------------
IsothermFilter::IsothermFilter(int lowThreshold, int highThreshold, HighlightColor color, bool whiteHotBackground)
    : m_preset(Preset::Custom)
    , m_lowThreshold(lowThreshold)
    , m_highThreshold(highThreshold)
    , m_color(color)
    , m_whiteHotBackground(whiteHotBackground)
{
}

void IsothermFilter::setPreset(Preset preset)
{
    m_preset = preset;
    switch (preset) {
    case Preset::HumanBody:
        m_lowThreshold = 140;
        m_highThreshold = 180;
        m_color = HighlightColor::Amber;
        break;
    case Preset::HighHeat:
        m_lowThreshold = 200;
        m_highThreshold = 255;
        m_color = HighlightColor::Red;
        break;
    case Preset::Custom:
    default:
        break;
    }
}

void IsothermFilter::setThresholds(int low, int high)
{
    m_preset = Preset::Custom;
    m_lowThreshold = std::max(0, std::min(255, low));
    m_highThreshold = std::max(m_lowThreshold, std::min(255, high));
}

void IsothermFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }

    const int low = std::min(m_lowThreshold, m_highThreshold);
    const int high = std::max(m_lowThreshold, m_highThreshold);

    uint8_t alertR = 255U;
    uint8_t alertG = 0U;
    uint8_t alertB = 0U;
    switch (m_color) {
    case HighlightColor::Amber:
        alertR = 255U;
        alertG = 191U;
        alertB = 0U;
        break;
    case HighlightColor::Cyan:
        alertR = 0U;
        alertG = 255U;
        alertB = 255U;
        break;
    case HighlightColor::Red:
    default:
        alertR = 255U;
        alertG = 0U;
        alertB = 0U;
        break;
    }

    const std::size_t numPixels = static_cast<std::size_t>(width * height);
    if (format == PixelFormat::RGB24 || format == PixelFormat::BGR24) {
        const std::size_t rOff = (format == PixelFormat::RGB24) ? 0U : 2U;
        const std::size_t gOff = 1U;
        const std::size_t bOff = (format == PixelFormat::RGB24) ? 2U : 0U;

        for (std::size_t i = 0U; i < numPixels; ++i) {
            const std::size_t idx = i * 3U;
            const uint8_t r = data[idx + rOff];
            const uint8_t g = data[idx + gOff];
            const uint8_t b = data[idx + bOff];

            const int luma = (299 * static_cast<int>(r) + 587 * static_cast<int>(g) + 114 * static_cast<int>(b)) / 1000;

            if (luma >= low && luma <= high) {
                if (m_color == HighlightColor::Iron256) {
                    const double norm
                        = (high > low) ? static_cast<double>(luma - low) / static_cast<double>(high - low) : 0.5;
                    data[idx + rOff] = static_cast<uint8_t>(std::min(255.0, norm * 2.0 * 255.0));
                    data[idx + gOff] = static_cast<uint8_t>(std::min(255.0, std::max(0.0, (norm - 0.5) * 2.0 * 255.0)));
                    data[idx + bOff] = static_cast<uint8_t>(std::max(0.0, (0.5 - norm) * 2.0 * 255.0));
                } else {
                    data[idx + rOff] = alertR;
                    data[idx + gOff] = alertG;
                    data[idx + bOff] = alertB;
                }
            } else {
                const uint8_t bgLuma
                    = m_whiteHotBackground ? static_cast<uint8_t>(luma) : static_cast<uint8_t>(255 - luma);
                data[idx + 0U] = bgLuma;
                data[idx + 1U] = bgLuma;
                data[idx + 2U] = bgLuma;
            }
        }
    }
}

// -----------------------------------------------------------------------------
// HotspotTrackerFilter Implementation
// -----------------------------------------------------------------------------
HotspotTrackerFilter::HotspotTrackerFilter(bool showOverlay, int centerBoxSize)
    : m_showOverlay(showOverlay)
    , m_centerBoxSize(centerBoxSize)
{
}

HotspotTrackerFilter::RadiometryStats HotspotTrackerFilter::getStats() const
{
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void HotspotTrackerFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat gray;
    const int convCode = (format == PixelFormat::RGB24) ? cv::COLOR_RGB2GRAY : cv::COLOR_BGR2GRAY;
    cv::cvtColor(mat, gray, convCode);

    double minVal = 0.0;
    double maxVal = 0.0;
    cv::Point minLoc;
    cv::Point maxLoc;
    cv::minMaxLoc(gray, &minVal, &maxVal, &minLoc, &maxLoc);

    const int boxSize = std::max(4, std::min(m_centerBoxSize, std::min(width, height) / 2));
    const int boxX = std::max(0, (width - boxSize) / 2);
    const int boxY = std::max(0, (height - boxSize) / 2);
    cv::Rect centerRect(boxX, boxY, boxSize, boxSize);
    cv::Scalar centerMeanScalar = cv::mean(gray(centerRect));

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.hotX = maxLoc.x;
        m_stats.hotY = maxLoc.y;
        m_stats.hotVal = static_cast<uint8_t>(std::max(0.0, std::min(255.0, maxVal)));
        m_stats.coldX = minLoc.x;
        m_stats.coldY = minLoc.y;
        m_stats.coldVal = static_cast<uint8_t>(std::max(0.0, std::min(255.0, minVal)));
        m_stats.centerMean = static_cast<uint8_t>(std::max(0.0, std::min(255.0, centerMeanScalar[0])));
    }

    if (!m_showOverlay || mat.channels() != 3) {
        return;
    }

    const cv::Scalar redColor = (format == PixelFormat::RGB24) ? cv::Scalar(255, 32, 32) : cv::Scalar(32, 32, 255);
    const cv::Scalar blueColor = (format == PixelFormat::RGB24) ? cv::Scalar(32, 128, 255) : cv::Scalar(255, 128, 32);
    const cv::Scalar yellowColor = (format == PixelFormat::RGB24) ? cv::Scalar(255, 220, 0) : cv::Scalar(0, 220, 255);

    auto drawTargetMarker = [&](const cv::Point& pt, const cv::Scalar& color, const std::string& label) {
        const int r = 7;
        cv::circle(mat, pt, r, color, 1, cv::LINE_AA);
        cv::line(mat, cv::Point(pt.x - r - 4, pt.y), cv::Point(pt.x + r + 4, pt.y), color, 1, cv::LINE_AA);
        cv::line(mat, cv::Point(pt.x, pt.y - r - 4), cv::Point(pt.x, pt.y + r + 4), color, 1, cv::LINE_AA);
        const int textX = std::min(width - 60, std::max(4, pt.x + r + 4));
        const int textY = std::min(height - 4, std::max(12, pt.y - r));
        cv::putText(mat, label, cv::Point(textX, textY), cv::FONT_HERSHEY_PLAIN, 0.8, color, 1, cv::LINE_AA);
    };

    drawTargetMarker(maxLoc, redColor, "HOT:" + std::to_string(static_cast<int>(maxVal)));
    drawTargetMarker(minLoc, blueColor, "COLD:" + std::to_string(static_cast<int>(minVal)));

    cv::rectangle(mat, centerRect, yellowColor, 1, cv::LINE_AA);
    const std::string centerText = "AVG:" + std::to_string(static_cast<int>(centerMeanScalar[0]));
    cv::putText(
        mat, centerText, cv::Point(boxX + 2, boxY - 3), cv::FONT_HERSHEY_PLAIN, 0.8, yellowColor, 1, cv::LINE_AA);
}

// -----------------------------------------------------------------------------
// MovingTargetIndicatorFilter Implementation
// -----------------------------------------------------------------------------
struct MovingTargetIndicatorFilter::Impl {
    cv::Ptr<cv::BackgroundSubtractorMOG2> bgSubtractor;
    std::vector<TargetBox> targets;
    mutable std::mutex targetsMutex;
    int frameCounter { 0 };

    Impl()
    {
        bgSubtractor = cv::createBackgroundSubtractorMOG2(50, 16.0, false);
    }
};

MovingTargetIndicatorFilter::MovingTargetIndicatorFilter(int minArea, int maxArea, int maxTargets)
    : m_minArea(minArea)
    , m_maxArea(maxArea)
    , m_maxTargets(maxTargets)
    , m_impl(std::make_unique<Impl>())
{
}

MovingTargetIndicatorFilter::~MovingTargetIndicatorFilter() = default;
MovingTargetIndicatorFilter::MovingTargetIndicatorFilter(MovingTargetIndicatorFilter&&) noexcept = default;
MovingTargetIndicatorFilter& MovingTargetIndicatorFilter::operator=(MovingTargetIndicatorFilter&&) noexcept = default;

void MovingTargetIndicatorFilter::reset()
{
    if (m_impl) {
        std::lock_guard<std::mutex> lock(m_impl->targetsMutex);
        m_impl->bgSubtractor = cv::createBackgroundSubtractorMOG2(50, 16.0, false);
        m_impl->targets.clear();
        m_impl->frameCounter = 0;
    }
}

std::size_t MovingTargetIndicatorFilter::getTargetCount() const
{
    if (!m_impl) {
        return 0U;
    }
    std::lock_guard<std::mutex> lock(m_impl->targetsMutex);
    return m_impl->targets.size();
}

std::vector<MovingTargetIndicatorFilter::TargetBox> MovingTargetIndicatorFilter::getTargets() const
{
    if (!m_impl) {
        return {};
    }
    std::lock_guard<std::mutex> lock(m_impl->targetsMutex);
    return m_impl->targets;
}

void MovingTargetIndicatorFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0 || !m_impl) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat gray;
    const int convCode = (format == PixelFormat::RGB24) ? cv::COLOR_RGB2GRAY : cv::COLOR_BGR2GRAY;
    cv::cvtColor(mat, gray, convCode);

    cv::Mat fgMask;
    m_impl->bgSubtractor->apply(gray, fgMask);
    m_impl->frameCounter++;

    if (m_impl->frameCounter < 3) {
        std::lock_guard<std::mutex> lock(m_impl->targetsMutex);
        m_impl->targets.clear();
        return;
    }

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::morphologyEx(fgMask, fgMask, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(fgMask, fgMask, cv::MORPH_DILATE, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(fgMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<TargetBox> newTargets;
    const cv::Scalar bracketColor = (format == PixelFormat::RGB24) ? cv::Scalar(0, 255, 64) : cv::Scalar(64, 255, 0);

    int targetId = 1;
    for (const auto& contour : contours) {
        const double area = cv::contourArea(contour);
        if (area >= static_cast<double>(m_minArea) && area <= static_cast<double>(m_maxArea)) {
            const cv::Rect r = cv::boundingRect(contour);
            TargetBox tb;
            tb.x = r.x;
            tb.y = r.y;
            tb.width = r.width;
            tb.height = r.height;
            tb.id = targetId;
            newTargets.push_back(tb);

            if (mat.channels() == 3) {
                const int cornerLen = std::max(4, std::min(12, std::min(r.width, r.height) / 3));
                cv::line(mat, cv::Point(r.x, r.y), cv::Point(r.x + cornerLen, r.y), bracketColor, 1, cv::LINE_AA);
                cv::line(mat, cv::Point(r.x, r.y), cv::Point(r.x, r.y + cornerLen), bracketColor, 1, cv::LINE_AA);
                cv::line(mat, cv::Point(r.x + r.width, r.y), cv::Point(r.x + r.width - cornerLen, r.y), bracketColor, 1,
                    cv::LINE_AA);
                cv::line(mat, cv::Point(r.x + r.width, r.y), cv::Point(r.x + r.width, r.y + cornerLen), bracketColor, 1,
                    cv::LINE_AA);
                cv::line(mat, cv::Point(r.x, r.y + r.height), cv::Point(r.x + cornerLen, r.y + r.height), bracketColor,
                    1, cv::LINE_AA);
                cv::line(mat, cv::Point(r.x, r.y + r.height), cv::Point(r.x, r.y + r.height - cornerLen), bracketColor,
                    1, cv::LINE_AA);
                cv::line(mat, cv::Point(r.x + r.width, r.y + r.height),
                    cv::Point(r.x + r.width - cornerLen, r.y + r.height), bracketColor, 1, cv::LINE_AA);
                cv::line(mat, cv::Point(r.x + r.width, r.y + r.height),
                    cv::Point(r.x + r.width, r.y + r.height - cornerLen), bracketColor, 1, cv::LINE_AA);

                const std::string tag = "T" + std::to_string(targetId);
                cv::putText(mat, tag, cv::Point(r.x, std::max(10, r.y - 2)), cv::FONT_HERSHEY_PLAIN, 0.8, bracketColor,
                    1, cv::LINE_AA);
            }

            targetId++;
            if (static_cast<int>(newTargets.size()) >= m_maxTargets) {
                break;
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_impl->targetsMutex);
        m_impl->targets = std::move(newTargets);
    }
}

// -----------------------------------------------------------------------------
// TacticalReticleOverlayFilter Implementation
// -----------------------------------------------------------------------------
TacticalReticleOverlayFilter::TacticalReticleOverlayFilter(Style style, Color color, int lineThickness, int deadbandGap)
    : m_style(style)
    , m_color(color)
    , m_lineThickness(std::max(1, lineThickness))
    , m_deadbandGap(std::max(2, deadbandGap))
{
}

void TacticalReticleOverlayFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0 || (format != PixelFormat::RGB24 && format != PixelFormat::BGR24)) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);

    cv::Scalar cvColor;
    switch (m_color) {
    case Color::Red:
        cvColor = (format == PixelFormat::RGB24) ? cv::Scalar(255, 48, 48) : cv::Scalar(48, 48, 255);
        break;
    case Color::Amber:
        cvColor = (format == PixelFormat::RGB24) ? cv::Scalar(255, 191, 0) : cv::Scalar(0, 191, 255);
        break;
    case Color::White:
        cvColor = cv::Scalar(255, 255, 255);
        break;
    case Color::Cyan:
        cvColor = (format == PixelFormat::RGB24) ? cv::Scalar(0, 255, 255) : cv::Scalar(255, 255, 0);
        break;
    case Color::TacticalGreen:
    default:
        cvColor = (format == PixelFormat::RGB24) ? cv::Scalar(0, 255, 64) : cv::Scalar(64, 255, 0);
        break;
    }

    const int cx = width / 2;
    const int cy = height / 2;
    const int gap = m_deadbandGap;
    const int thick = m_lineThickness;

    if (m_style == Style::Crosshair) {
        const int armLen = std::min(width, height) / 4;
        cv::line(mat, cv::Point(cx - armLen, cy), cv::Point(cx - gap, cy), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(cx + gap, cy), cv::Point(cx + armLen, cy), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(cx, cy - armLen), cv::Point(cx, cy - gap), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(cx, cy + gap), cv::Point(cx, cy + armLen), cvColor, thick, cv::LINE_AA);
        cv::circle(mat, cv::Point(cx, cy), gap / 2, cvColor, thick, cv::LINE_AA);
    } else if (m_style == Style::MilDot) {
        const int armLen = std::min(width, height) / 3;
        cv::line(mat, cv::Point(cx - armLen, cy), cv::Point(cx - gap, cy), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(cx + gap, cy), cv::Point(cx + armLen, cy), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(cx, cy - armLen), cv::Point(cx, cy - gap), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(cx, cy + gap), cv::Point(cx, cy + armLen), cvColor, thick, cv::LINE_AA);

        const int milSpacing = 20;
        for (int d = gap + milSpacing; d <= armLen; d += milSpacing) {
            cv::circle(mat, cv::Point(cx + d, cy), 2, cvColor, -1, cv::LINE_AA);
            cv::circle(mat, cv::Point(cx - d, cy), 2, cvColor, -1, cv::LINE_AA);
            cv::circle(mat, cv::Point(cx, cy + d), 2, cvColor, -1, cv::LINE_AA);
            cv::circle(mat, cv::Point(cx, cy - d), 2, cvColor, -1, cv::LINE_AA);
        }
        cv::circle(mat, cv::Point(cx, cy), 2, cvColor, -1, cv::LINE_AA);
    } else if (m_style == Style::Stadiametric) {
        const int armLen = std::min(width, height) / 3;
        cv::line(mat, cv::Point(cx - armLen, cy), cv::Point(cx + armLen, cy), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(cx, cy - armLen), cv::Point(cx, cy + armLen), cvColor, thick, cv::LINE_AA);

        const int tickSpacing = 16;
        for (int i = 1; i <= 6; ++i) {
            const int y = cy + i * tickSpacing;
            const int tickW = 6 + i * 4;
            cv::line(mat, cv::Point(cx - tickW, y), cv::Point(cx + tickW, y), cvColor, thick, cv::LINE_AA);
        }
    } else if (m_style == Style::CornerBrackets) {
        const int margin = 20;
        const int bracketLen = 30;
        cv::line(mat, cv::Point(margin, margin), cv::Point(margin + bracketLen, margin), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(margin, margin), cv::Point(margin, margin + bracketLen), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(width - margin, margin), cv::Point(width - margin - bracketLen, margin), cvColor, thick,
            cv::LINE_AA);
        cv::line(mat, cv::Point(width - margin, margin), cv::Point(width - margin, margin + bracketLen), cvColor, thick,
            cv::LINE_AA);
        cv::line(mat, cv::Point(margin, height - margin), cv::Point(margin + bracketLen, height - margin), cvColor,
            thick, cv::LINE_AA);
        cv::line(mat, cv::Point(margin, height - margin), cv::Point(margin, height - margin - bracketLen), cvColor,
            thick, cv::LINE_AA);
        cv::line(mat, cv::Point(width - margin, height - margin),
            cv::Point(width - margin - bracketLen, height - margin), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(width - margin, height - margin),
            cv::Point(width - margin, height - margin - bracketLen), cvColor, thick, cv::LINE_AA);

        cv::line(mat, cv::Point(cx - 8, cy), cv::Point(cx + 8, cy), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(cx, cy - 8), cv::Point(cx, cy + 8), cvColor, thick, cv::LINE_AA);
    }
}

// -----------------------------------------------------------------------------
// OpticalFlowFieldFilter Implementation
// -----------------------------------------------------------------------------
struct OpticalFlowFieldFilter::Impl {
    cv::Mat prevGray;
};

OpticalFlowFieldFilter::OpticalFlowFieldFilter(DisplayMode mode, int gridStep, double minVelocity, double arrowScale)
    : m_mode(mode)
    , m_gridStep(std::max(4, gridStep))
    , m_minVelocity(std::max(0.0, minVelocity))
    , m_arrowScale(arrowScale)
    , m_impl(std::make_unique<Impl>())
{
}

OpticalFlowFieldFilter::~OpticalFlowFieldFilter() = default;
OpticalFlowFieldFilter::OpticalFlowFieldFilter(OpticalFlowFieldFilter&&) noexcept = default;
OpticalFlowFieldFilter& OpticalFlowFieldFilter::operator=(OpticalFlowFieldFilter&&) noexcept = default;

void OpticalFlowFieldFilter::reset()
{
    if (m_impl) {
        m_impl->prevGray.release();
    }
}

void OpticalFlowFieldFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0 || !m_impl
        || (format != PixelFormat::RGB24 && format != PixelFormat::BGR24)) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat gray;
    const int convCode = (format == PixelFormat::RGB24) ? cv::COLOR_RGB2GRAY : cv::COLOR_BGR2GRAY;
    cv::cvtColor(mat, gray, convCode);

    if (m_impl->prevGray.empty() || m_impl->prevGray.size() != gray.size()) {
        m_impl->prevGray = gray.clone();
        return;
    }

    cv::Mat flow;
    cv::calcOpticalFlowFarneback(m_impl->prevGray, gray, flow, 0.5, 3, 15, 3, 5, 1.2, 0);
    m_impl->prevGray = gray.clone();

    if (m_mode == DisplayMode::VectorArrows) {
        const cv::Scalar arrowColor = (format == PixelFormat::RGB24) ? cv::Scalar(0, 255, 64) : cv::Scalar(64, 255, 0);
        const int step = m_gridStep;
        for (int y = step / 2; y < height; y += step) {
            for (int x = step / 2; x < width; x += step) {
                const cv::Point2f flowAtPoint = flow.at<cv::Point2f>(y, x);
                const double mag = std::hypot(static_cast<double>(flowAtPoint.x), static_cast<double>(flowAtPoint.y));
                if (mag >= m_minVelocity) {
                    const int endX = std::max(0,
                        std::min(width - 1,
                            static_cast<int>(std::round(
                                static_cast<double>(x) + static_cast<double>(flowAtPoint.x) * m_arrowScale))));
                    const int endY = std::max(0,
                        std::min(height - 1,
                            static_cast<int>(std::round(
                                static_cast<double>(y) + static_cast<double>(flowAtPoint.y) * m_arrowScale))));
                    cv::arrowedLine(mat, cv::Point(x, y), cv::Point(endX, endY), arrowColor, 1, cv::LINE_AA, 0, 0.25);
                }
            }
        }
    } else if (m_mode == DisplayMode::ColorFlow) {
        std::vector<cv::Mat> hsvChannels(3);
        hsvChannels[1] = cv::Mat(height, width, CV_8UC1, cv::Scalar(255));

        cv::Mat flowPlanes[2];
        cv::split(flow, flowPlanes);

        cv::Mat magnitude;
        cv::Mat angle;
        cv::cartToPolar(flowPlanes[0], flowPlanes[1], magnitude, angle, true);

        angle.convertTo(hsvChannels[0], CV_8U, 0.5);
        cv::normalize(magnitude, hsvChannels[2], 0, 255, cv::NORM_MINMAX, CV_8U);

        cv::Mat hsv;
        cv::merge(hsvChannels, hsv);
        cv::Mat bgrFlow;
        cv::cvtColor(hsv, bgrFlow, cv::COLOR_HSV2BGR);

        if (format == PixelFormat::RGB24) {
            cv::cvtColor(bgrFlow, bgrFlow, cv::COLOR_BGR2RGB);
        }
        cv::addWeighted(mat, 0.6, bgrFlow, 0.4, 0.0, mat);
    }
}

// -----------------------------------------------------------------------------
// CentroidTargetTrackerFilter Implementation
// -----------------------------------------------------------------------------
struct CentroidTargetTrackerFilter::Impl {
    TargetState state;
    mutable std::mutex stateMutex;
    cv::Mat prevGray;
    std::vector<cv::Point2f> trackedPoints;
    cv::Rect targetRect;
    int lostFrames { 0 };
};

CentroidTargetTrackerFilter::CentroidTargetTrackerFilter(bool autoAcquire, int targetWidth, int targetHeight)
    : m_autoAcquire(autoAcquire)
    , m_defaultWidth(std::max(10, targetWidth))
    , m_defaultHeight(std::max(10, targetHeight))
    , m_impl(std::make_unique<Impl>())
{
}

CentroidTargetTrackerFilter::~CentroidTargetTrackerFilter() = default;
CentroidTargetTrackerFilter::CentroidTargetTrackerFilter(CentroidTargetTrackerFilter&&) noexcept = default;
CentroidTargetTrackerFilter& CentroidTargetTrackerFilter::operator=(CentroidTargetTrackerFilter&&) noexcept = default;

void CentroidTargetTrackerFilter::acquireTarget(int x, int y, int width, int height)
{
    if (!m_impl) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_impl->stateMutex);
    m_impl->targetRect = cv::Rect(x, y, std::max(10, width), std::max(10, height));
    m_impl->trackedPoints.clear();
    m_impl->lostFrames = 0;
    m_impl->state.locked = true;
    m_impl->state.x = x;
    m_impl->state.y = y;
    m_impl->state.width = width;
    m_impl->state.height = height;
    m_impl->state.confidence = 1.0;
}

void CentroidTargetTrackerFilter::releaseTarget()
{
    if (!m_impl) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_impl->stateMutex);
    m_impl->targetRect = cv::Rect();
    m_impl->trackedPoints.clear();
    m_impl->lostFrames = 0;
    m_impl->state = TargetState();
}

bool CentroidTargetTrackerFilter::isTargetLocked() const
{
    if (!m_impl) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_impl->stateMutex);
    return m_impl->state.locked;
}

CentroidTargetTrackerFilter::TargetState CentroidTargetTrackerFilter::getTargetState() const
{
    if (!m_impl) {
        return {};
    }
    std::lock_guard<std::mutex> lock(m_impl->stateMutex);
    return m_impl->state;
}

void CentroidTargetTrackerFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0 || !m_impl
        || (format != PixelFormat::RGB24 && format != PixelFormat::BGR24)) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat gray;
    const int convCode = (format == PixelFormat::RGB24) ? cv::COLOR_RGB2GRAY : cv::COLOR_BGR2GRAY;
    cv::cvtColor(mat, gray, convCode);

    std::lock_guard<std::mutex> lock(m_impl->stateMutex);

    if (!m_impl->state.locked && m_autoAcquire && !m_impl->prevGray.empty()) {
        cv::Mat diff;
        cv::absdiff(m_impl->prevGray, gray, diff);
        cv::threshold(diff, diff, 25, 255, cv::THRESH_BINARY);
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(diff, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        double maxArea = 0.0;
        cv::Rect bestRect;
        for (const auto& c : contours) {
            const double a = cv::contourArea(c);
            if (a > 200.0 && a > maxArea) {
                maxArea = a;
                bestRect = cv::boundingRect(c);
            }
        }
        if (maxArea > 200.0) {
            m_impl->targetRect = bestRect;
            m_impl->state.locked = true;
            m_impl->state.confidence = 1.0;
            m_impl->trackedPoints.clear();
            m_impl->lostFrames = 0;
        }
    }

    if (m_impl->state.locked && !m_impl->prevGray.empty()) {
        if (m_impl->trackedPoints.empty()) {
            const cv::Rect bounded = m_impl->targetRect & cv::Rect(0, 0, width, height);
            if (bounded.width >= 10 && bounded.height >= 10) {
                cv::Mat roi = m_impl->prevGray(bounded);
                std::vector<cv::Point2f> pts;
                cv::goodFeaturesToTrack(roi, pts, 25, 0.01, 5.0);
                for (const auto& p : pts) {
                    m_impl->trackedPoints.push_back(
                        cv::Point2f(p.x + static_cast<float>(bounded.x), p.y + static_cast<float>(bounded.y)));
                }
            }
        }

        if (!m_impl->trackedPoints.empty()) {
            std::vector<cv::Point2f> nextPts;
            std::vector<uchar> status;
            std::vector<float> err;
            cv::calcOpticalFlowPyrLK(m_impl->prevGray, gray, m_impl->trackedPoints, nextPts, status, err);

            std::vector<cv::Point2f> goodNext;
            cv::Point2f meanShift(0.0f, 0.0f);
            for (std::size_t i = 0U; i < status.size(); ++i) {
                if (status[i]) {
                    goodNext.push_back(nextPts[i]);
                    meanShift += (nextPts[i] - m_impl->trackedPoints[i]);
                }
            }

            if (!goodNext.empty()) {
                meanShift.x /= static_cast<float>(goodNext.size());
                meanShift.y /= static_cast<float>(goodNext.size());

                m_impl->targetRect.x = std::max(0,
                    std::min(width - m_impl->targetRect.width,
                        static_cast<int>(std::round(static_cast<float>(m_impl->targetRect.x) + meanShift.x))));
                m_impl->targetRect.y = std::max(0,
                    std::min(height - m_impl->targetRect.height,
                        static_cast<int>(std::round(static_cast<float>(m_impl->targetRect.y) + meanShift.y))));
                m_impl->trackedPoints = goodNext;
                m_impl->state.vx = static_cast<double>(meanShift.x);
                m_impl->state.vy = static_cast<double>(meanShift.y);
                m_impl->state.confidence = std::min(1.0, static_cast<double>(goodNext.size()) / 15.0);
                m_impl->lostFrames = 0;
            } else {
                m_impl->lostFrames++;
                m_impl->state.confidence *= 0.8;
                if (m_impl->lostFrames > 10) {
                    m_impl->state.locked = false;
                    m_impl->targetRect = cv::Rect();
                }
            }
        }
    }

    m_impl->prevGray = gray.clone();

    if (m_impl->state.locked) {
        m_impl->state.x = m_impl->targetRect.x;
        m_impl->state.y = m_impl->targetRect.y;
        m_impl->state.width = m_impl->targetRect.width;
        m_impl->state.height = m_impl->targetRect.height;

        const double cx
            = static_cast<double>(m_impl->targetRect.x) + static_cast<double>(m_impl->targetRect.width) / 2.0;
        const double cy
            = static_cast<double>(m_impl->targetRect.y) + static_cast<double>(m_impl->targetRect.height) / 2.0;
        const double halfW = static_cast<double>(width) / 2.0;
        const double halfH = static_cast<double>(height) / 2.0;
        m_impl->state.errorX = (cx - halfW) / halfW;
        m_impl->state.errorY = (cy - halfH) / halfH;

        const cv::Scalar lockColor = (m_impl->state.confidence > 0.5)
            ? ((format == PixelFormat::RGB24) ? cv::Scalar(0, 255, 64) : cv::Scalar(64, 255, 0))
            : ((format == PixelFormat::RGB24) ? cv::Scalar(255, 200, 0) : cv::Scalar(0, 200, 255));

        const cv::Rect r = m_impl->targetRect;
        cv::rectangle(mat, r, lockColor, 2, cv::LINE_AA);
        cv::line(mat, cv::Point(r.x + r.width / 2 - 4, r.y + r.height / 2),
            cv::Point(r.x + r.width / 2 + 4, r.y + r.height / 2), lockColor, 1);
        cv::line(mat, cv::Point(r.x + r.width / 2, r.y + r.height / 2 - 4),
            cv::Point(r.x + r.width / 2, r.y + r.height / 2 + 4), lockColor, 1);

        const std::string tag = "LOCK [dX:" + std::to_string(static_cast<int>(m_impl->state.errorX * 100.0))
            + "% dY:" + std::to_string(static_cast<int>(m_impl->state.errorY * 100.0)) + "%]";
        cv::putText(
            mat, tag, cv::Point(r.x, std::max(12, r.y - 4)), cv::FONT_HERSHEY_PLAIN, 0.8, lockColor, 1, cv::LINE_AA);
    }
}

// -----------------------------------------------------------------------------
// PerimeterTripwireFilter Implementation
// -----------------------------------------------------------------------------
struct PerimeterTripwireFilter::Impl {
    std::size_t intrusionCount { 0U };
    int alarmFrames { 0 };
    cv::Mat prevGray;
    std::vector<cv::Point2f> prevCentroids;
    mutable std::mutex mutex;
};

PerimeterTripwireFilter::PerimeterTripwireFilter(
    double x1Norm, double y1Norm, double x2Norm, double y2Norm, Direction direction)
    : m_x1Norm(x1Norm)
    , m_y1Norm(y1Norm)
    , m_x2Norm(x2Norm)
    , m_y2Norm(y2Norm)
    , m_direction(direction)
    , m_impl(std::make_unique<Impl>())
{
}

PerimeterTripwireFilter::~PerimeterTripwireFilter() = default;
PerimeterTripwireFilter::PerimeterTripwireFilter(PerimeterTripwireFilter&&) noexcept = default;
PerimeterTripwireFilter& PerimeterTripwireFilter::operator=(PerimeterTripwireFilter&&) noexcept = default;

void PerimeterTripwireFilter::setTripwire(double x1Norm, double y1Norm, double x2Norm, double y2Norm)
{
    m_x1Norm = x1Norm;
    m_y1Norm = y1Norm;
    m_x2Norm = x2Norm;
    m_y2Norm = y2Norm;
}

void PerimeterTripwireFilter::getTripwire(double& x1Norm, double& y1Norm, double& x2Norm, double& y2Norm) const
{
    x1Norm = m_x1Norm;
    y1Norm = m_y1Norm;
    x2Norm = m_x2Norm;
    y2Norm = m_y2Norm;
}

bool PerimeterTripwireFilter::hasAlarm() const
{
    if (!m_impl) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->alarmFrames > 0;
}

std::size_t PerimeterTripwireFilter::getIntrusionCount() const
{
    if (!m_impl) {
        return 0U;
    }
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->intrusionCount;
}

void PerimeterTripwireFilter::resetIntrusionCount()
{
    if (!m_impl) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->intrusionCount = 0U;
    m_impl->alarmFrames = 0;
}

static bool segmentsIntersect(
    const cv::Point2f& p1, const cv::Point2f& p2, const cv::Point2f& q1, const cv::Point2f& q2, double& orientationSign)
{
    auto ccw = [](const cv::Point2f& a, const cv::Point2f& b, const cv::Point2f& c) -> double {
        return static_cast<double>((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x));
    };

    const double d1 = ccw(q1, q2, p1);
    const double d2 = ccw(q1, q2, p2);
    const double d3 = ccw(p1, p2, q1);
    const double d4 = ccw(p1, p2, q2);

    orientationSign = d1 - d2;
    return (((d1 > 0.0 && d2 < 0.0) || (d1 < 0.0 && d2 > 0.0)) && ((d3 > 0.0 && d4 < 0.0) || (d3 < 0.0 && d4 > 0.0)));
}

void PerimeterTripwireFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0 || !m_impl
        || (format != PixelFormat::RGB24 && format != PixelFormat::BGR24)) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat gray;
    const int convCode = (format == PixelFormat::RGB24) ? cv::COLOR_RGB2GRAY : cv::COLOR_BGR2GRAY;
    cv::cvtColor(mat, gray, convCode);

    std::lock_guard<std::mutex> lock(m_impl->mutex);

    const cv::Point2f tripA(static_cast<float>(m_x1Norm * static_cast<double>(width)),
        static_cast<float>(m_y1Norm * static_cast<double>(height)));
    const cv::Point2f tripB(static_cast<float>(m_x2Norm * static_cast<double>(width)),
        static_cast<float>(m_y2Norm * static_cast<double>(height)));

    if (!m_impl->prevGray.empty()) {
        cv::Mat diff;
        cv::absdiff(m_impl->prevGray, gray, diff);
        cv::threshold(diff, diff, 25, 255, cv::THRESH_BINARY);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(diff, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        std::vector<cv::Point2f> currentCentroids;
        for (const auto& c : contours) {
            if (cv::contourArea(c) > 100.0) {
                const cv::Moments m = cv::moments(c);
                if (m.m00 > 0.0) {
                    currentCentroids.push_back(
                        cv::Point2f(static_cast<float>(m.m10 / m.m00), static_cast<float>(m.m01 / m.m00)));
                }
            }
        }

        for (const auto& cur : currentCentroids) {
            for (const auto& prev : m_impl->prevCentroids) {
                if (cv::norm(cur - prev) < 100.0) {
                    double orient = 0.0;
                    if (segmentsIntersect(prev, cur, tripA, tripB, orient)) {
                        bool trigger = false;
                        if (m_direction == Direction::Bidirectional) {
                            trigger = true;
                        } else if (m_direction == Direction::A_to_B && orient > 0.0) {
                            trigger = true;
                        } else if (m_direction == Direction::B_to_A && orient < 0.0) {
                            trigger = true;
                        }
                        if (trigger) {
                            m_impl->intrusionCount++;
                            m_impl->alarmFrames = 15;
                        }
                    }
                }
            }
        }

        m_impl->prevCentroids = std::move(currentCentroids);
    } else {
        cv::Mat thresh;
        cv::threshold(gray, thresh, 128, 255, cv::THRESH_BINARY);
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        for (const auto& c : contours) {
            if (cv::contourArea(c) > 100.0) {
                const cv::Moments m = cv::moments(c);
                if (m.m00 > 0.0) {
                    m_impl->prevCentroids.push_back(
                        cv::Point2f(static_cast<float>(m.m10 / m.m00), static_cast<float>(m.m01 / m.m00)));
                }
            }
        }
    }

    m_impl->prevGray = gray.clone();

    const bool inAlarm = (m_impl->alarmFrames > 0);
    if (m_impl->alarmFrames > 0) {
        m_impl->alarmFrames--;
    }

    const cv::Scalar tripColor = inAlarm
        ? ((format == PixelFormat::RGB24) ? cv::Scalar(255, 32, 32) : cv::Scalar(32, 32, 255))
        : ((format == PixelFormat::RGB24) ? cv::Scalar(255, 191, 0) : cv::Scalar(0, 191, 255));

    cv::line(mat, tripA, tripB, tripColor, inAlarm ? 3 : 2, cv::LINE_AA);
    cv::circle(mat, tripA, 4, tripColor, -1);
    cv::circle(mat, tripB, 4, tripColor, -1);

    const std::string badge = "TRIPWIRE ALARMS: " + std::to_string(m_impl->intrusionCount);
    cv::putText(mat, badge, cv::Point(static_cast<int>(tripA.x), std::max(14, static_cast<int>(tripA.y) - 6)),
        cv::FONT_HERSHEY_PLAIN, 0.85, tripColor, 1, cv::LINE_AA);
}

// -----------------------------------------------------------------------------
// MotionHeatmapFilter Implementation
// -----------------------------------------------------------------------------
struct MotionHeatmapFilter::Impl {
    cv::Mat prevGray;
    cv::Mat accumHeatmap;
    mutable std::mutex mutex;
};

MotionHeatmapFilter::MotionHeatmapFilter(double decayFactor, double opacity, int threshold)
    : m_decayFactor(std::max(0.01, std::min(0.999, decayFactor)))
    , m_opacity(std::max(0.0, std::min(1.0, opacity)))
    , m_threshold(std::max(1, std::min(255, threshold)))
    , m_impl(std::make_unique<Impl>())
{
}

MotionHeatmapFilter::~MotionHeatmapFilter() = default;
MotionHeatmapFilter::MotionHeatmapFilter(MotionHeatmapFilter&&) noexcept = default;
MotionHeatmapFilter& MotionHeatmapFilter::operator=(MotionHeatmapFilter&&) noexcept = default;

void MotionHeatmapFilter::reset()
{
    if (m_impl) {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->prevGray.release();
        m_impl->accumHeatmap.release();
    }
}

void MotionHeatmapFilter::process(uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0 || !m_impl
        || (format != PixelFormat::RGB24 && format != PixelFormat::BGR24)) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat gray;
    const int convCode = (format == PixelFormat::RGB24) ? cv::COLOR_RGB2GRAY : cv::COLOR_BGR2GRAY;
    cv::cvtColor(mat, gray, convCode);

    std::lock_guard<std::mutex> lock(m_impl->mutex);

    if (m_impl->prevGray.empty() || m_impl->prevGray.size() != gray.size()) {
        m_impl->prevGray = gray.clone();
        m_impl->accumHeatmap = cv::Mat::zeros(height, width, CV_32F);
        return;
    }

    cv::Mat diff;
    cv::absdiff(m_impl->prevGray, gray, diff);
    m_impl->prevGray = gray.clone();

    cv::Mat motionMask;
    cv::threshold(diff, motionMask, m_threshold, 1.0, cv::THRESH_BINARY);
    cv::Mat motionFloat;
    motionMask.convertTo(motionFloat, CV_32F);

    m_impl->accumHeatmap = m_impl->accumHeatmap * static_cast<float>(m_decayFactor)
        + motionFloat * (1.0f - static_cast<float>(m_decayFactor));

    cv::Mat normHeatmap;
    cv::normalize(m_impl->accumHeatmap, normHeatmap, 0, 255, cv::NORM_MINMAX, CV_8U);

    cv::Mat colorHeatmap;
    cv::applyColorMap(normHeatmap, colorHeatmap, cv::COLORMAP_JET);

    if (format == PixelFormat::RGB24) {
        cv::cvtColor(colorHeatmap, colorHeatmap, cv::COLOR_BGR2RGB);
    }

    cv::addWeighted(mat, 1.0 - m_opacity, colorHeatmap, m_opacity, 0.0, mat);
}

} // namespace PelcoD::Video

#endif // PELCOD_HAS_FILTERS
