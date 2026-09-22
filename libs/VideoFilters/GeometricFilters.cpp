/**
 * @file GeometricFilters.cpp
 * @brief Implementations of geometric filters including mirror, mosaic, LAP, EIS stabilization, and PiP.
 */

#include "GeometricFilters.h"

#if defined(PELCOD_HAS_FILTERS)

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <opencv2/opencv.hpp>
#include <utility>
#include <vector>

namespace Video::Filters {

// --- MirrorFilter ---
MirrorFilter::MirrorFilter(bool horizontal)
    : m_horizontal(horizontal)
{
}

void MirrorFilter::process(std::uint8_t* data, int width, int height, PixelFormat /*format*/)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::flip(mat, mat, m_horizontal ? 1 : 0);
}

// --- MosaicFilter ---
MosaicFilter::MosaicFilter(int blockSize)
    : m_blockSize(blockSize)
{
}

void MosaicFilter::process(std::uint8_t* data, int width, int height, PixelFormat /*format*/)
{
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

// --- LocalAreaProcessingFilter (LAP) ---
LocalAreaProcessingFilter::LocalAreaProcessingFilter(int strength, double blend, double lapMinDiff)
    : m_strength(std::clamp(strength, 1, 18))
    , m_blend(std::clamp(blend, 0.0, 1.0))
    , m_lapMinDiff(std::max(0.0, lapMinDiff))
{
}

void LocalAreaProcessingFilter::process(std::uint8_t* data, int width, int height, PixelFormat /*format*/)
{
    if (!data || width <= 0 || height <= 0 || m_blend <= 0.0) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat floatMat;
    mat.convertTo(floatMat, CV_32FC3);

    int ksize = 2 * m_strength + 1;
    cv::Mat localMean;
    cv::boxFilter(floatMat, localMean, CV_32FC3, cv::Size(ksize, ksize));

    cv::Mat diff = floatMat - localMean;

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

    cv::Mat enhanced = floatMat + diff;
    cv::Mat enhanced8U;
    enhanced.convertTo(enhanced8U, CV_8UC3);

    if (m_blend >= 1.0) {
        enhanced8U.copyTo(mat);
    } else {
        cv::addWeighted(mat, 1.0 - m_blend, enhanced8U, m_blend, 0.0, mat);
    }
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

    double lastDx { 0.0 };
    double lastDy { 0.0 };
    MotionCallback motionCb { nullptr };
    std::chrono::steady_clock::time_point lastFrameTime {};
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

void ImageStabilizationFilter::setMotionCallback(MotionCallback callback)
{
    if (m_impl) {
        m_impl->motionCb = std::move(callback);
    }
}

void ImageStabilizationFilter::getLastFrameMotion(double& dx, double& dy) const noexcept
{
    if (m_impl) {
        dx = m_impl->lastDx;
        dy = m_impl->lastDy;
    } else {
        dx = 0.0;
        dy = 0.0;
    }
}

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
        m_impl->lastDx = 0.0;
        m_impl->lastDy = 0.0;
        m_impl->lastFrameTime = {};
        m_impl->hasPrev = false;
    }
}

void ImageStabilizationFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
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
    m_impl->lastDx = dx;
    m_impl->lastDy = dy;

    const auto now = std::chrono::steady_clock::now();
    double dt = 0.0333;
    if (m_impl->lastFrameTime.time_since_epoch().count() > 0) {
        dt = std::chrono::duration<double>(now - m_impl->lastFrameTime).count();
    }
    m_impl->lastFrameTime = now;
    if (m_impl->motionCb) {
        m_impl->motionCb(dx, dy, dt);
    }

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

// --- PictureInPictureFilter ---
PictureInPictureFilter::PictureInPictureFilter(Mode mode, Corner corner, double scaleRatio, double digitalZoomFactor)
    : m_mode(mode)
    , m_corner(corner)
    , m_scaleRatio(scaleRatio)
    , m_digitalZoomFactor(digitalZoomFactor)
{
}

void PictureInPictureFilter::setBorder(bool showBorder, std::uint8_t r, std::uint8_t g, std::uint8_t b, int thickness)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_showBorder = showBorder;
    m_borderR = r;
    m_borderG = g;
    m_borderB = b;
    m_borderThickness = std::max(1, thickness);
}

void PictureInPictureFilter::setSecondaryFrame(const std::uint8_t* data, int width, int height, PixelFormat format)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!data || width <= 0 || height <= 0) {
        m_secondaryBuffer.clear();
        m_secondaryWidth = 0;
        m_secondaryHeight = 0;
        return;
    }
    std::size_t bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3U;
    m_secondaryBuffer.assign(data, data + bytes);
    m_secondaryWidth = width;
    m_secondaryHeight = height;
    m_secondaryFormat = format;
}

void PictureInPictureFilter::clearSecondaryFrame()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_secondaryBuffer.clear();
    m_secondaryWidth = 0;
    m_secondaryHeight = 0;
}

void PictureInPictureFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }

    Mode mode = Mode::DigitalZoom;
    Corner corner = Corner::TopRight;
    double scaleR = 0.28;
    double zoomFactor = 2.0;
    bool showB = true;
    std::uint8_t br = 0;
    std::uint8_t bg = 255;
    std::uint8_t bb = 64;
    int bThick = 2;
    bool badge = true;
    std::vector<std::uint8_t> secBuf;
    int secW = 0;
    int secH = 0;
    PixelFormat secFmt = PixelFormat::RGB24;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        mode = m_mode;
        corner = m_corner;
        scaleR = m_scaleRatio;
        zoomFactor = m_digitalZoomFactor;
        showB = m_showBorder;
        br = m_borderR;
        bg = m_borderG;
        bb = m_borderB;
        bThick = m_borderThickness;
        badge = m_showBadge;
        if (mode == Mode::SecondaryFeed && !m_secondaryBuffer.empty()) {
            secBuf = m_secondaryBuffer;
            secW = m_secondaryWidth;
            secH = m_secondaryHeight;
            secFmt = m_secondaryFormat;
        }
    }

    if (width < 128 || height < 96) {
        return;
    }

    int pipW
        = std::clamp(static_cast<int>(std::round(static_cast<double>(width) * scaleR)), 64, std::max(64, width / 2));
    int pipH = std::clamp(static_cast<int>(std::round(
                              static_cast<double>(pipW) * static_cast<double>(height) / static_cast<double>(width))),
        48, std::max(48, height / 2));

    int margin = 12;
    int pipX = margin;
    int pipY = margin;

    switch (corner) {
    case Corner::TopLeft:
        pipX = margin;
        pipY = margin;
        break;
    case Corner::TopRight:
        pipX = width - pipW - margin;
        pipY = margin;
        break;
    case Corner::BottomLeft:
        pipX = margin;
        pipY = height - pipH - margin;
        break;
    case Corner::BottomRight:
        pipX = width - pipW - margin;
        pipY = height - pipH - margin;
        break;
    }

    pipX = std::clamp(pipX, 0, std::max(0, width - pipW));
    pipY = std::clamp(pipY, 0, std::max(0, height - pipH));
    pipW = std::min(pipW, width - pipX);
    pipH = std::min(pipH, height - pipY);

    if (pipW <= 4 || pipH <= 4) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Rect pipRect(pipX, pipY, pipW, pipH);
    cv::Mat dstRoi = mat(pipRect);

    if (mode == Mode::DigitalZoom) {
        double z = std::max(1.1, zoomFactor);
        int cropW = std::clamp(static_cast<int>(std::round(static_cast<double>(width) / z)), 2, width);
        int cropH = std::clamp(static_cast<int>(std::round(static_cast<double>(height) / z)), 2, height);
        int cropX = (width - cropW) / 2;
        int cropY = (height - cropH) / 2;
        cv::Rect cropRect(cropX, cropY, cropW, cropH);
        cv::Mat crop = mat(cropRect).clone();
        cv::resize(crop, dstRoi, dstRoi.size(), 0, 0, cv::INTER_LINEAR);
    } else { // SecondaryFeed
        if (!secBuf.empty() && secW > 0 && secH > 0) {
            cv::Mat secMat(secH, secW, CV_8UC3, secBuf.data());
            cv::Mat convertedSec;
            if (secFmt != format) {
                cv::cvtColor(
                    secMat, convertedSec, (format == PixelFormat::BGR24) ? cv::COLOR_RGB2BGR : cv::COLOR_BGR2RGB);
            } else {
                convertedSec = secMat;
            }
            cv::resize(convertedSec, dstRoi, dstRoi.size(), 0, 0, cv::INTER_LINEAR);
        } else {
            dstRoi.setTo(cv::Scalar(20, 20, 20));
            cv::putText(dstRoi, "NO AUX FEED", cv::Point(8, pipH / 2), cv::FONT_HERSHEY_PLAIN, 0.8,
                cv::Scalar(160, 160, 160), 1, cv::LINE_AA);
        }
    }

    if (showB) {
        cv::Scalar borderColor = (format == PixelFormat::BGR24) ? cv::Scalar(bb, bg, br) : cv::Scalar(br, bg, bb);
        cv::rectangle(mat, pipRect, borderColor, bThick);
    }

    if (badge) {
        char badgeBuf[32];
        if (mode == Mode::DigitalZoom) {
            std::snprintf(badgeBuf, sizeof(badgeBuf), "PIP: %.1fx", zoomFactor);
        } else {
            std::snprintf(badgeBuf, sizeof(badgeBuf), "PIP: AUX");
        }
        int base = 0;
        cv::Size bSz = cv::getTextSize(badgeBuf, cv::FONT_HERSHEY_PLAIN, 0.8, 1, &base);
        cv::Rect badgeBox(pipX + 2, pipY + 2, bSz.width + 6, bSz.height + 4);
        if (badgeBox.x + badgeBox.width <= width && badgeBox.y + badgeBox.height <= height) {
            cv::Mat bRoi = mat(badgeBox);
            cv::Mat dark(bRoi.size(), bRoi.type(), cv::Scalar(10, 10, 10));
            cv::addWeighted(dark, 0.7, bRoi, 0.3, 0.0, bRoi);
            cv::putText(mat, badgeBuf, cv::Point(pipX + 5, pipY + bSz.height + 3), cv::FONT_HERSHEY_PLAIN, 0.8,
                (format == PixelFormat::BGR24) ? cv::Scalar(0, 255, 255) : cv::Scalar(255, 255, 0), 1, cv::LINE_AA);
        }
    }
}

} // namespace Video::Filters

#endif // PELCOD_HAS_FILTERS
