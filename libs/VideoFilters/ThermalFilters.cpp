/**
 * @file ThermalFilters.cpp
 * @brief Implementations of thermal and false-color filters, isotherms, and hotspot radiometry.
 */

#include "ThermalFilters.h"

#if defined(PELCOD_HAS_FILTERS)

#include <algorithm>
#include <fstream>
#include <map>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

namespace Video::Filters {

// --- FalseColorFilter ---
FalseColorFilter::FalseColorFilter(FalseColorPalette palette)
    : m_palette(palette)
{
    initDefaultUserPalette();
}

void FalseColorFilter::initDefaultUserPalette()
{
    std::map<std::uint8_t, std::vector<std::uint8_t>> controlPoints = { { 0, { 0, 0, 0 } }, { 64, { 128, 0, 128 } },
        { 128, { 255, 0, 0 } }, { 192, { 255, 255, 0 } }, { 255, { 255, 255, 255 } } };
    generateInterpolatedPalette(controlPoints, true);
}

void FalseColorFilter::setUserPalette(const std::vector<std::uint8_t>& lut256x3)
{
    if (lut256x3.size() == 768) {
        m_userPalette = lut256x3;
    }
}

void FalseColorFilter::generateInterpolatedPalette(
    const std::map<std::uint8_t, std::vector<std::uint8_t>>& controlPoints, bool /*smooth*/)
{
    if (controlPoints.empty()) {
        return;
    }

    m_userPalette.resize(768);

    auto it = controlPoints.begin();
    std::size_t prevIdx = it->first;
    std::vector<std::uint8_t> prevColor = it->second;

    for (std::size_t i = 0U; i <= prevIdx; ++i) {
        m_userPalette[i * 3U + 0U] = prevColor[0];
        m_userPalette[i * 3U + 1U] = prevColor[1];
        m_userPalette[i * 3U + 2U] = prevColor[2];
    }

    for (++it; it != controlPoints.end(); ++it) {
        std::size_t nextIdx = it->first;
        std::vector<std::uint8_t> nextColor = it->second;

        if (nextIdx > prevIdx) {
            float span = static_cast<float>(nextIdx - prevIdx);
            for (std::size_t i = prevIdx; i <= nextIdx; ++i) {
                float t = static_cast<float>(i - prevIdx) / span;
                m_userPalette[i * 3U + 0U] = static_cast<std::uint8_t>(
                    static_cast<float>(prevColor[0]) + t * static_cast<float>(nextColor[0] - prevColor[0]));
                m_userPalette[i * 3U + 1U] = static_cast<std::uint8_t>(
                    static_cast<float>(prevColor[1]) + t * static_cast<float>(nextColor[1] - prevColor[1]));
                m_userPalette[i * 3U + 2U] = static_cast<std::uint8_t>(
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

    std::vector<std::uint8_t> buffer(768);
    file.read(reinterpret_cast<char*>(buffer.data()), 768);
    if (file.gcount() != 768) {
        return false;
    }

    if (isYuv) {
        m_userPalette.resize(768);
        for (std::size_t i = 0U; i < 256U; ++i) {
            double y = static_cast<double>(buffer[i * 3U + 0U]);
            double u = static_cast<double>(buffer[i * 3U + 1U]) - 128.0;
            double v = static_cast<double>(buffer[i * 3U + 2U]) - 128.0;

            double r = y + 1.13983 * v;
            double g = y - 0.39465 * u - 0.58060 * v;
            double b = y + 2.03211 * u;

            m_userPalette[i * 3U + 0U] = cv::saturate_cast<std::uint8_t>(r);
            m_userPalette[i * 3U + 1U] = cv::saturate_cast<std::uint8_t>(g);
            m_userPalette[i * 3U + 2U] = cv::saturate_cast<std::uint8_t>(b);
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
        std::vector<std::uint8_t> yuvBuffer(768);
        for (std::size_t i = 0U; i < 256U; ++i) {
            double r = m_userPalette[i * 3U + 0U];
            double g = m_userPalette[i * 3U + 1U];
            double b = m_userPalette[i * 3U + 2U];

            double y = 0.299 * r + 0.587 * g + 0.114 * b;
            double u = -0.14713 * r - 0.28886 * g + 0.436 * b + 128.0;
            double v = 0.615 * r - 0.51499 * g - 0.10001 * b + 128.0;

            yuvBuffer[i * 3U + 0U] = cv::saturate_cast<std::uint8_t>(y);
            yuvBuffer[i * 3U + 1U] = cv::saturate_cast<std::uint8_t>(u);
            yuvBuffer[i * 3U + 2U] = cv::saturate_cast<std::uint8_t>(v);
        }
        file.write(reinterpret_cast<const char*>(yuvBuffer.data()), 768);
    } else {
        file.write(reinterpret_cast<const char*>(m_userPalette.data()), 768);
    }
    return file.good();
}

void FalseColorFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
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

    if (format == PixelFormat::BGR24) {
        colored.copyTo(mat);
    } else {
        cv::cvtColor(colored, mat, cv::COLOR_BGR2RGB);
    }
}

// --- IsothermFilter ---
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

void IsothermFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }

    const int low = std::min(m_lowThreshold, m_highThreshold);
    const int high = std::max(m_lowThreshold, m_highThreshold);

    std::uint8_t alertR = 255U;
    std::uint8_t alertG = 0U;
    std::uint8_t alertB = 0U;
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
            const std::uint8_t r = data[idx + rOff];
            const std::uint8_t g = data[idx + gOff];
            const std::uint8_t b = data[idx + bOff];

            const int luma = (299 * static_cast<int>(r) + 587 * static_cast<int>(g) + 114 * static_cast<int>(b)) / 1000;

            if (luma >= low && luma <= high) {
                if (m_color == HighlightColor::Iron256) {
                    const double norm
                        = (high > low) ? static_cast<double>(luma - low) / static_cast<double>(high - low) : 0.5;
                    data[idx + rOff] = static_cast<std::uint8_t>(std::min(255.0, norm * 2.0 * 255.0));
                    data[idx + gOff]
                        = static_cast<std::uint8_t>(std::min(255.0, std::max(0.0, (norm - 0.5) * 2.0 * 255.0)));
                    data[idx + bOff] = static_cast<std::uint8_t>(std::max(0.0, (0.5 - norm) * 2.0 * 255.0));
                } else {
                    data[idx + rOff] = alertR;
                    data[idx + gOff] = alertG;
                    data[idx + bOff] = alertB;
                }
            } else {
                const std::uint8_t bgLuma
                    = m_whiteHotBackground ? static_cast<std::uint8_t>(luma) : static_cast<std::uint8_t>(255 - luma);
                data[idx + 0U] = bgLuma;
                data[idx + 1U] = bgLuma;
                data[idx + 2U] = bgLuma;
            }
        }
    }
}

// --- HotspotTrackerFilter ---
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

void HotspotTrackerFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
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
        m_stats.hotVal = static_cast<std::uint8_t>(std::max(0.0, std::min(255.0, maxVal)));
        m_stats.coldX = minLoc.x;
        m_stats.coldY = minLoc.y;
        m_stats.coldVal = static_cast<std::uint8_t>(std::max(0.0, std::min(255.0, minVal)));
        m_stats.centerMean = static_cast<std::uint8_t>(std::max(0.0, std::min(255.0, centerMeanScalar[0])));
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

} // namespace Video::Filters

#endif // PELCOD_HAS_FILTERS
