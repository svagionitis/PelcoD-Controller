/**
 * @file TrackingFilters.cpp
 * @brief Implementations of MTI, optical flow, centroid tracker, perimeter tripwire, and motion heatmap.
 */

#include "TrackingFilters.h"

#if defined(PELCOD_HAS_FILTERS)

#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <iomanip>
#include <mutex>
#include <numeric>
#include <opencv2/opencv.hpp>
#include <sstream>

namespace Video::Filters {

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
        std::scoped_lock lock(m_impl->targetsMutex);
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
    std::scoped_lock lock(m_impl->targetsMutex);
    return m_impl->targets.size();
}

std::vector<MovingTargetIndicatorFilter::TargetBox> MovingTargetIndicatorFilter::getTargets() const
{
    if (!m_impl) {
        return {};
    }
    std::scoped_lock lock(m_impl->targetsMutex);
    return m_impl->targets;
}

void MovingTargetIndicatorFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
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
        std::scoped_lock lock(m_impl->targetsMutex);
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
        std::scoped_lock lock(m_impl->targetsMutex);
        m_impl->targets = std::move(newTargets);
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

void OpticalFlowFieldFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
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
    int maxCoastFrames { 30 };
    int lastWidth { 640 };
    int lastHeight { 360 };

    cv::KalmanFilter kalman;
    bool kalmanInitialized { false };
    float qPos { 1e-2f };
    float qVel { 1e-1f };
    float qAcc { 1e-1f };
    float rPos { 1e-1f };
    bool adaptiveNoise { true };

    bool scaleAdaptation { true };
    bool appearanceFusion { true };
    double appearanceLearningRate { 0.02 };
    double initialWidth { 40.0 };
    double initialHeight { 40.0 };
    double dynamicLookaheadLatency { 0.10 };
    cv::Mat modelHist;

    struct BreadcrumbPoint {
        cv::Point2f position;
        double speed { 0.0 }; ///< Speed in pixels/sec
        std::chrono::steady_clock::time_point timestamp;
    };

    TrajectoryConfig trajectoryConfig;
    PredictiveLeadConfig predictiveLeadConfig;
    double boresightLeadOffsetX { 0.0 };
    double boresightLeadOffsetY { 0.0 };
    std::deque<BreadcrumbPoint> trajectoryHistory;
    std::chrono::steady_clock::time_point lastProcessTime;
    bool hasLastProcessTime { false };

    bool trajectoryTrail { true };
    int maxTrajectoryPoints { 60 };
    bool predictiveVector { true };
    double predictiveVectorLookahead { 1.5 };

    void initKalman(float initX, float initY)
    {
        kalman.init(6, 2, 0, CV_32F);
        kalman.transitionMatrix = (cv::Mat_<float>(6, 6) << 1.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
            0.0f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

        kalman.measurementMatrix
            = (cv::Mat_<float>(2, 6) << 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);

        cv::setIdentity(kalman.processNoiseCov, cv::Scalar::all(0.0));
        kalman.processNoiseCov.at<float>(0, 0) = qPos;
        kalman.processNoiseCov.at<float>(1, 1) = qPos;
        kalman.processNoiseCov.at<float>(2, 2) = qVel;
        kalman.processNoiseCov.at<float>(3, 3) = qVel;
        kalman.processNoiseCov.at<float>(4, 4) = qAcc;
        kalman.processNoiseCov.at<float>(5, 5) = qAcc;

        cv::setIdentity(kalman.measurementNoiseCov, cv::Scalar::all(static_cast<double>(rPos)));
        cv::setIdentity(kalman.errorCovPost, cv::Scalar::all(1.0));
        cv::setIdentity(kalman.errorCovPre, cv::Scalar::all(1.0));

        kalman.statePost = (cv::Mat_<float>(6, 1) << initX, initY, 0.0f, 0.0f, 0.0f, 0.0f);
        kalman.statePre = kalman.statePost.clone();
        kalmanInitialized = true;
    }

    void extractAppearanceModel(const cv::Mat& bgrOrRgb, const cv::Rect& roi, PixelFormat format)
    {
        const cv::Rect bounded = roi & cv::Rect(0, 0, bgrOrRgb.cols, bgrOrRgb.rows);
        if (bounded.width < 5 || bounded.height < 5) {
            return;
        }
        cv::Mat hsv;
        const int code = (format == PixelFormat::RGB24) ? cv::COLOR_RGB2HSV : cv::COLOR_BGR2HSV;
        cv::cvtColor(bgrOrRgb(bounded), hsv, code);
        int histSize[] = { 16, 16 };
        float hRanges[] = { 0, 180 };
        float sRanges[] = { 0, 256 };
        const float* ranges[] = { hRanges, sRanges };
        int channels[] = { 0, 1 };
        cv::calcHist(&hsv, 1, channels, cv::Mat(), modelHist, 2, histSize, ranges, true, false);
        cv::normalize(modelHist, modelHist, 0, 255, cv::NORM_MINMAX);
    }

    cv::Point2f computeAppearanceCentroid(
        const cv::Mat& bgrOrRgb, const cv::Rect& searchArea, PixelFormat format, double& score)
    {
        const cv::Rect bounded = searchArea & cv::Rect(0, 0, bgrOrRgb.cols, bgrOrRgb.rows);
        if (bounded.width < 10 || bounded.height < 10 || modelHist.empty()) {
            score = 1.0;
            return cv::Point2f(static_cast<float>(searchArea.x) + static_cast<float>(searchArea.width) / 2.0f,
                static_cast<float>(searchArea.y) + static_cast<float>(searchArea.height) / 2.0f);
        }
        cv::Mat hsv;
        const int code = (format == PixelFormat::RGB24) ? cv::COLOR_RGB2HSV : cv::COLOR_BGR2HSV;
        cv::cvtColor(bgrOrRgb(bounded), hsv, code);
        float hRanges[] = { 0, 180 };
        float sRanges[] = { 0, 256 };
        const float* ranges[] = { hRanges, sRanges };
        int channels[] = { 0, 1 };
        cv::Mat backproj;
        cv::calcBackProject(&hsv, 1, channels, modelHist, backproj, ranges);

        cv::Rect candidateInBounded(
            targetRect.x - bounded.x, targetRect.y - bounded.y, targetRect.width, targetRect.height);
        candidateInBounded = candidateInBounded & cv::Rect(0, 0, bounded.width, bounded.height);
        if (candidateInBounded.width >= 5 && candidateInBounded.height >= 5) {
            cv::Mat candHsv = hsv(candidateInBounded);
            cv::Mat candHist;
            int histSize[] = { 16, 16 };
            cv::calcHist(&candHsv, 1, channels, cv::Mat(), candHist, 2, histSize, ranges, true, false);
            cv::normalize(candHist, candHist, 0, 255, cv::NORM_MINMAX);
            const double dist = cv::compareHist(modelHist, candHist, cv::HISTCMP_BHATTACHARYYA);
            score = std::clamp(1.0 - dist, 0.0, 1.0);

            if (score > 0.80 && appearanceLearningRate > 0.0) {
                cv::addWeighted(
                    modelHist, 1.0 - appearanceLearningRate, candHist, appearanceLearningRate, 0.0, modelHist);
                cv::normalize(modelHist, modelHist, 0, 255, cv::NORM_MINMAX);
            }
        } else {
            score = 0.5;
        }

        const int maxShiftX = std::max(0, bounded.width - 5);
        const int maxShiftY = std::max(0, bounded.height - 5);
        cv::Rect trackWin(std::clamp(candidateInBounded.x, 0, maxShiftX),
            std::clamp(candidateInBounded.y, 0, maxShiftY),
            std::min(candidateInBounded.width, bounded.width - std::clamp(candidateInBounded.x, 0, maxShiftX)),
            std::min(candidateInBounded.height, bounded.height - std::clamp(candidateInBounded.y, 0, maxShiftY)));
        if (trackWin.width >= 5 && trackWin.height >= 5) {
            cv::meanShift(
                backproj, trackWin, cv::TermCriteria(cv::TermCriteria::EPS | cv::TermCriteria::COUNT, 10, 1.0));
            return cv::Point2f(static_cast<float>(bounded.x + trackWin.x) + static_cast<float>(trackWin.width) / 2.0f,
                static_cast<float>(bounded.y + trackWin.y) + static_cast<float>(trackWin.height) / 2.0f);
        }

        return cv::Point2f(static_cast<float>(searchArea.x) + static_cast<float>(searchArea.width) / 2.0f,
            static_cast<float>(searchArea.y) + static_cast<float>(searchArea.height) / 2.0f);
    }

    double computeScaleChange(const std::vector<cv::Point2f>& prevPts, const std::vector<cv::Point2f>& currPts)
    {
        if (prevPts.size() < 3 || currPts.size() < 3 || prevPts.size() != currPts.size()) {
            return 1.0;
        }
        std::vector<double> ratios;
        ratios.reserve(prevPts.size() * (prevPts.size() - 1) / 2);
        for (std::size_t i = 0U; i < prevPts.size(); ++i) {
            for (std::size_t j = i + 1U; j < prevPts.size(); ++j) {
                const double dPrev = cv::norm(prevPts[i] - prevPts[j]);
                if (dPrev >= 4.0) {
                    const double dCurr = cv::norm(currPts[i] - currPts[j]);
                    ratios.push_back(dCurr / dPrev);
                }
            }
        }
        if (ratios.empty()) {
            return 1.0;
        }
        std::sort(ratios.begin(), ratios.end());
        const double medianRatio = ratios[ratios.size() / 2];
        return std::clamp(medianRatio, 0.85, 1.15);
    }
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
    std::scoped_lock lock(m_impl->stateMutex);
    m_impl->targetRect = cv::Rect(x, y, std::max(10, width), std::max(10, height));
    m_impl->initialWidth = static_cast<double>(m_impl->targetRect.width);
    m_impl->initialHeight = static_cast<double>(m_impl->targetRect.height);
    m_impl->modelHist.release();
    m_impl->trackedPoints.clear();
    m_impl->trajectoryHistory.clear();
    m_impl->hasLastProcessTime = false;
    m_impl->lostFrames = 0;
    m_impl->state.locked = true;
    m_impl->state.isCoasting = false;
    m_impl->state.x = x;
    m_impl->state.y = y;
    m_impl->state.width = width;
    m_impl->state.height = height;
    m_impl->state.vx = 0.0;
    m_impl->state.vy = 0.0;
    m_impl->state.ax = 0.0;
    m_impl->state.ay = 0.0;
    m_impl->state.scaleFactor = 1.0;
    m_impl->state.appearanceScore = 1.0;
    m_impl->state.confidence = 1.0;
    m_impl->state.normalizedWidth
        = (m_impl->lastWidth > 0) ? (static_cast<double>(width) / static_cast<double>(m_impl->lastWidth)) : 0.0;
    m_impl->state.normalizedHeight
        = (m_impl->lastHeight > 0) ? (static_cast<double>(height) / static_cast<double>(m_impl->lastHeight)) : 0.0;

    const float cx = static_cast<float>(x) + static_cast<float>(width) / 2.0f;
    const float cy = static_cast<float>(y) + static_cast<float>(height) / 2.0f;
    m_impl->initKalman(cx, cy);
}

void CentroidTargetTrackerFilter::releaseTarget()
{
    if (!m_impl) {
        return;
    }
    std::scoped_lock lock(m_impl->stateMutex);
    m_impl->targetRect = cv::Rect();
    m_impl->modelHist.release();
    m_impl->trackedPoints.clear();
    m_impl->trajectoryHistory.clear();
    m_impl->hasLastProcessTime = false;
    m_impl->lostFrames = 0;
    m_impl->kalmanInitialized = false;
    m_impl->state = TargetState();
}

bool CentroidTargetTrackerFilter::isTargetLocked() const
{
    if (!m_impl) {
        return false;
    }
    std::scoped_lock lock(m_impl->stateMutex);
    return m_impl->state.locked;
}

CentroidTargetTrackerFilter::TargetState CentroidTargetTrackerFilter::getTargetState(
    double lookaheadLatencySeconds) const
{
    if (!m_impl) {
        return {};
    }
    std::scoped_lock lock(m_impl->stateMutex);
    TargetState copy = m_impl->state;

    const double effectiveLookahead
        = (lookaheadLatencySeconds >= 0.0) ? lookaheadLatencySeconds : m_impl->dynamicLookaheadLatency;

    if (copy.locked) {
        if (m_impl->lastWidth > 0 && m_impl->lastHeight > 0) {
            copy.normalizedWidth = static_cast<double>(copy.width) / static_cast<double>(m_impl->lastWidth);
            copy.normalizedHeight = static_cast<double>(copy.height) / static_cast<double>(m_impl->lastHeight);
        }
        if (effectiveLookahead > 0.0 && m_impl->lastWidth > 0 && m_impl->lastHeight > 0) {
            const double framesAhead = effectiveLookahead * 30.0;
            const double predCx = static_cast<double>(copy.x) + static_cast<double>(copy.width) / 2.0
                + copy.vx * framesAhead + 0.5 * copy.ax * framesAhead * framesAhead;
            const double predCy = static_cast<double>(copy.y) + static_cast<double>(copy.height) / 2.0
                + copy.vy * framesAhead + 0.5 * copy.ay * framesAhead * framesAhead;
            const double halfW = static_cast<double>(m_impl->lastWidth) / 2.0;
            const double halfH = static_cast<double>(m_impl->lastHeight) / 2.0;
            copy.predictedErrorX = (predCx - halfW) / halfW;
            copy.predictedErrorY = (predCy - halfH) / halfH;
        } else {
            copy.predictedErrorX = copy.errorX;
            copy.predictedErrorY = copy.errorY;
        }
    }
    return copy;
}

void CentroidTargetTrackerFilter::setDynamicLookaheadLatency(double seconds) noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        m_impl->dynamicLookaheadLatency = std::clamp(seconds, 0.0, 1.0);
    }
}

double CentroidTargetTrackerFilter::getDynamicLookaheadLatency() const noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        return m_impl->dynamicLookaheadLatency;
    }
    return 0.10;
}

void CentroidTargetTrackerFilter::setMaxCoastFrames(int frames) noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        m_impl->maxCoastFrames = std::max(0, frames);
    }
}

int CentroidTargetTrackerFilter::getMaxCoastFrames() const noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        return m_impl->maxCoastFrames;
    }
    return 30;
}

void CentroidTargetTrackerFilter::setProcessNoise(double qPos, double qVel, double qAcc) noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        m_impl->qPos = static_cast<float>(std::max(1e-6, qPos));
        m_impl->qVel = static_cast<float>(std::max(1e-6, qVel));
        m_impl->qAcc = static_cast<float>(std::max(1e-6, qAcc));
        if (m_impl->kalmanInitialized) {
            m_impl->kalman.processNoiseCov.at<float>(0, 0) = m_impl->qPos;
            m_impl->kalman.processNoiseCov.at<float>(1, 1) = m_impl->qPos;
            m_impl->kalman.processNoiseCov.at<float>(2, 2) = m_impl->qVel;
            m_impl->kalman.processNoiseCov.at<float>(3, 3) = m_impl->qVel;
            m_impl->kalman.processNoiseCov.at<float>(4, 4) = m_impl->qAcc;
            m_impl->kalman.processNoiseCov.at<float>(5, 5) = m_impl->qAcc;
        }
    }
}

void CentroidTargetTrackerFilter::setMeasurementNoise(double rPos) noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        m_impl->rPos = static_cast<float>(std::max(1e-6, rPos));
    }
}

void CentroidTargetTrackerFilter::setAdaptiveProcessNoiseEnabled(bool enabled) noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        m_impl->adaptiveNoise = enabled;
    }
}

bool CentroidTargetTrackerFilter::isAdaptiveProcessNoiseEnabled() const noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        return m_impl->adaptiveNoise;
    }
    return true;
}

void CentroidTargetTrackerFilter::setTrajectoryTrail(bool enabled, int maxPoints) noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        m_impl->trajectoryTrail = enabled;
        m_impl->trajectoryConfig.enabled = enabled;
        m_impl->trajectoryConfig.maxPoints = std::clamp(maxPoints, 5, 200);
        m_impl->maxTrajectoryPoints = m_impl->trajectoryConfig.maxPoints;
        if (!enabled) {
            m_impl->trajectoryHistory.clear();
        }
    }
}

bool CentroidTargetTrackerFilter::isTrajectoryTrail() const noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        return m_impl->trajectoryConfig.enabled;
    }
    return true;
}

int CentroidTargetTrackerFilter::getTrajectoryMaxPoints() const noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        return m_impl->trajectoryConfig.maxPoints;
    }
    return 60;
}

void CentroidTargetTrackerFilter::setPredictiveVector(bool enabled, double lookaheadSeconds) noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        m_impl->predictiveVector = enabled;
        m_impl->predictiveLeadConfig.enabled = enabled;
        m_impl->predictiveLeadConfig.lookaheadSeconds = std::clamp(lookaheadSeconds, 0.1, 10.0);
        m_impl->predictiveVectorLookahead = m_impl->predictiveLeadConfig.lookaheadSeconds;
    }
}

bool CentroidTargetTrackerFilter::isPredictiveVector() const noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        return m_impl->predictiveLeadConfig.enabled;
    }
    return true;
}

double CentroidTargetTrackerFilter::getPredictiveVectorLookahead() const noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        return m_impl->predictiveLeadConfig.lookaheadSeconds;
    }
    return 1.5;
}

void CentroidTargetTrackerFilter::setTrajectoryConfig(const TrajectoryConfig& config) noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        m_impl->trajectoryConfig = config;
        m_impl->trajectoryConfig.maxDurationSec = std::clamp(config.maxDurationSec, 0.1, 30.0);
        m_impl->trajectoryConfig.maxPoints = std::clamp(config.maxPoints, 5, 200);
        m_impl->trajectoryTrail = config.enabled;
        m_impl->maxTrajectoryPoints = m_impl->trajectoryConfig.maxPoints;
        if (!config.enabled) {
            m_impl->trajectoryHistory.clear();
        }
    }
}

CentroidTargetTrackerFilter::TrajectoryConfig CentroidTargetTrackerFilter::getTrajectoryConfig() const noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        return m_impl->trajectoryConfig;
    }
    return {};
}

void CentroidTargetTrackerFilter::setPredictiveLeadConfig(const PredictiveLeadConfig& config) noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        m_impl->predictiveLeadConfig = config;
        m_impl->predictiveLeadConfig.lookaheadSeconds = std::clamp(config.lookaheadSeconds, 0.1, 10.0);
        m_impl->predictiveVector = config.enabled;
        m_impl->predictiveVectorLookahead = m_impl->predictiveLeadConfig.lookaheadSeconds;
    }
}

CentroidTargetTrackerFilter::PredictiveLeadConfig CentroidTargetTrackerFilter::getPredictiveLeadConfig() const noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        return m_impl->predictiveLeadConfig;
    }
    return {};
}

void CentroidTargetTrackerFilter::setBoresightLeadOffset(double leadX, double leadY) noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        m_impl->boresightLeadOffsetX = std::clamp(leadX, -1.0, 1.0);
        m_impl->boresightLeadOffsetY = std::clamp(leadY, -1.0, 1.0);
    }
}

std::pair<double, double> CentroidTargetTrackerFilter::getBoresightLeadOffset() const noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        return { m_impl->boresightLeadOffsetX, m_impl->boresightLeadOffsetY };
    }
    return { 0.0, 0.0 };
}

void CentroidTargetTrackerFilter::setScaleAdaptation(bool enabled) noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        m_impl->scaleAdaptation = enabled;
    }
}

bool CentroidTargetTrackerFilter::isScaleAdaptation() const noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        return m_impl->scaleAdaptation;
    }
    return true;
}

void CentroidTargetTrackerFilter::setAppearanceFusion(bool enabled) noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        m_impl->appearanceFusion = enabled;
    }
}

bool CentroidTargetTrackerFilter::isAppearanceFusion() const noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        return m_impl->appearanceFusion;
    }
    return true;
}

void CentroidTargetTrackerFilter::setAppearanceLearningRate(double rate) noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        m_impl->appearanceLearningRate = std::clamp(rate, 0.0, 1.0);
    }
}

double CentroidTargetTrackerFilter::getAppearanceLearningRate() const noexcept
{
    if (m_impl) {
        std::scoped_lock lock(m_impl->stateMutex);
        return m_impl->appearanceLearningRate;
    }
    return 0.02;
}

void CentroidTargetTrackerFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0 || !m_impl
        || (format != PixelFormat::RGB24 && format != PixelFormat::BGR24)) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat gray;
    const int convCode = (format == PixelFormat::RGB24) ? cv::COLOR_RGB2GRAY : cv::COLOR_BGR2GRAY;
    cv::cvtColor(mat, gray, convCode);

    std::scoped_lock lock(m_impl->stateMutex);
    m_impl->lastWidth = width;
    m_impl->lastHeight = height;

    // Kalman prediction step
    cv::Mat prediction;
    if (m_impl->kalmanInitialized) {
        prediction = m_impl->kalman.predict();
    }

    bool justAcquired = false;

    // Auto-acquire when unlocked
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
            m_impl->initialWidth = static_cast<double>(bestRect.width);
            m_impl->initialHeight = static_cast<double>(bestRect.height);
            m_impl->state.locked = true;
            m_impl->state.isCoasting = false;
            m_impl->state.scaleFactor = 1.0;
            m_impl->state.appearanceScore = 1.0;
            m_impl->state.confidence = 1.0;
            m_impl->trackedPoints.clear();
            m_impl->lostFrames = 0;
            const float cx = static_cast<float>(bestRect.x) + static_cast<float>(bestRect.width) / 2.0f;
            const float cy = static_cast<float>(bestRect.y) + static_cast<float>(bestRect.height) / 2.0f;
            m_impl->initKalman(cx, cy);
            justAcquired = true;
        }
    }

    // Tracking step
    if (m_impl->state.locked) {
        if (justAcquired || m_impl->modelHist.empty()) {
            m_impl->extractAppearanceModel(mat, m_impl->targetRect, format);
        }

        if (justAcquired) {
            // Seed initial tracked points on current frame for subsequent optical flow
            const cv::Rect bounded = m_impl->targetRect & cv::Rect(0, 0, width, height);
            if (bounded.width >= 10 && bounded.height >= 10) {
                cv::Mat roi = gray(bounded);
                std::vector<cv::Point2f> pts;
                cv::goodFeaturesToTrack(roi, pts, 25, 0.01, 5.0);
                for (const auto& p : pts) {
                    m_impl->trackedPoints.push_back(
                        cv::Point2f(p.x + static_cast<float>(bounded.x), p.y + static_cast<float>(bounded.y)));
                }
            }
        } else if (!m_impl->prevGray.empty()) {
            bool trackedSuccessfully = false;

            if (m_impl->trackedPoints.empty() && !m_impl->state.isCoasting) {
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

            if (!m_impl->trackedPoints.empty() && !m_impl->state.isCoasting) {
                std::vector<cv::Point2f> nextPts;
                std::vector<uchar> status;
                std::vector<float> err;
                cv::calcOpticalFlowPyrLK(m_impl->prevGray, gray, m_impl->trackedPoints, nextPts, status, err);

                std::vector<cv::Point2f> goodPrev;
                std::vector<cv::Point2f> goodNext;
                cv::Point2f meanShift(0.0f, 0.0f);
                for (std::size_t i = 0U; i < status.size(); ++i) {
                    if (status[i]) {
                        goodPrev.push_back(m_impl->trackedPoints[i]);
                        goodNext.push_back(nextPts[i]);
                        meanShift += (nextPts[i] - m_impl->trackedPoints[i]);
                    }
                }

                if (!goodNext.empty()) {
                    trackedSuccessfully = true;

                    // Dynamic Scale Adaptation
                    if (m_impl->scaleAdaptation && goodPrev.size() >= 3) {
                        const double sRatio = m_impl->computeScaleChange(goodPrev, goodNext);
                        m_impl->state.scaleFactor = std::clamp(m_impl->state.scaleFactor * sRatio, 0.25, 4.0);
                        const int newW = std::clamp(
                            static_cast<int>(std::round(m_impl->initialWidth * m_impl->state.scaleFactor)), 10, width);
                        const int newH = std::clamp(
                            static_cast<int>(std::round(m_impl->initialHeight * m_impl->state.scaleFactor)), 10,
                            height);
                        m_impl->targetRect.width = newW;
                        m_impl->targetRect.height = newH;
                    }

                    meanShift.x /= static_cast<float>(goodNext.size());
                    meanShift.y /= static_cast<float>(goodNext.size());

                    float measCenterX = static_cast<float>(m_impl->targetRect.x)
                        + static_cast<float>(m_impl->targetRect.width) / 2.0f + meanShift.x;
                    float measCenterY = static_cast<float>(m_impl->targetRect.y)
                        + static_cast<float>(m_impl->targetRect.height) / 2.0f + meanShift.y;

                    // Appearance Model Fusion
                    if (m_impl->appearanceFusion && !m_impl->modelHist.empty()) {
                        const int padX = m_impl->targetRect.width / 2;
                        const int padY = m_impl->targetRect.height / 2;
                        const int searchLeft = std::clamp(m_impl->targetRect.x - padX, 0, width);
                        const int searchTop = std::clamp(m_impl->targetRect.y - padY, 0, height);
                        const cv::Rect searchArea(searchLeft, searchTop,
                            std::min(m_impl->targetRect.width + 2 * padX, width - searchLeft),
                            std::min(m_impl->targetRect.height + 2 * padY, height - searchTop));

                        double appScore = 1.0;
                        const cv::Point2f appCenter
                            = m_impl->computeAppearanceCentroid(mat, searchArea, format, appScore);
                        m_impl->state.appearanceScore = appScore;

                        if (appScore > 0.35) {
                            const float gamma = static_cast<float>(std::clamp(0.20 * appScore, 0.0, 0.25));
                            measCenterX = (1.0f - gamma) * measCenterX + gamma * appCenter.x;
                            measCenterY = (1.0f - gamma) * measCenterY + gamma * appCenter.y;
                        }
                    }

                    if (m_impl->kalmanInitialized) {
                        if (!prediction.empty()) {
                            const float innovX = measCenterX - prediction.at<float>(0);
                            const float innovY = measCenterY - prediction.at<float>(1);
                            const float innovNorm = std::sqrt(innovX * innovX + innovY * innovY);

                            if (m_impl->adaptiveNoise) {
                                const float scale = std::clamp(innovNorm / 3.0f, 1.0f, 10.0f);
                                m_impl->kalman.processNoiseCov.at<float>(4, 4) = m_impl->qAcc * scale * scale;
                                m_impl->kalman.processNoiseCov.at<float>(5, 5) = m_impl->qAcc * scale * scale;
                            } else {
                                m_impl->kalman.processNoiseCov.at<float>(4, 4) = m_impl->qAcc;
                                m_impl->kalman.processNoiseCov.at<float>(5, 5) = m_impl->qAcc;
                            }
                        }

                        cv::Mat measurement = (cv::Mat_<float>(2, 1) << measCenterX, measCenterY);
                        cv::Mat estimated = m_impl->kalman.correct(measurement);
                        const float estCenterX = estimated.at<float>(0);
                        const float estCenterY = estimated.at<float>(1);
                        m_impl->state.vx = static_cast<double>(estimated.at<float>(2));
                        m_impl->state.vy = static_cast<double>(estimated.at<float>(3));
                        m_impl->state.ax = static_cast<double>(estimated.at<float>(4));
                        m_impl->state.ay = static_cast<double>(estimated.at<float>(5));

                        m_impl->targetRect.x = std::clamp(static_cast<int>(std::round(estCenterX
                                                              - static_cast<float>(m_impl->targetRect.width) / 2.0f)),
                            0, width - m_impl->targetRect.width);
                        m_impl->targetRect.y = std::clamp(static_cast<int>(std::round(estCenterY
                                                              - static_cast<float>(m_impl->targetRect.height) / 2.0f)),
                            0, height - m_impl->targetRect.height);
                    } else {
                        m_impl->targetRect.x = std::clamp(
                            static_cast<int>(std::round(static_cast<float>(m_impl->targetRect.x) + meanShift.x)), 0,
                            width - m_impl->targetRect.width);
                        m_impl->targetRect.y = std::clamp(
                            static_cast<int>(std::round(static_cast<float>(m_impl->targetRect.y) + meanShift.y)), 0,
                            height - m_impl->targetRect.height);
                        m_impl->state.vx = static_cast<double>(meanShift.x);
                        m_impl->state.vy = static_cast<double>(meanShift.y);
                        m_impl->state.ax = 0.0;
                        m_impl->state.ay = 0.0;
                    }

                    m_impl->trackedPoints = goodNext;
                    m_impl->state.confidence = std::min(1.0, static_cast<double>(goodNext.size()) / 15.0);
                    m_impl->lostFrames = 0;
                    m_impl->state.isCoasting = false;
                }
            }

            if (!trackedSuccessfully) {
                // Optical flow lost features: occlusion coasting via Kalman prediction
                m_impl->lostFrames++;
                if (m_impl->lostFrames <= m_impl->maxCoastFrames && m_impl->kalmanInitialized && !prediction.empty()) {
                    const float predCenterX = prediction.at<float>(0);
                    const float predCenterY = prediction.at<float>(1);
                    m_impl->state.vx = static_cast<double>(prediction.at<float>(2));
                    m_impl->state.vy = static_cast<double>(prediction.at<float>(3));
                    m_impl->state.ax = static_cast<double>(prediction.at<float>(4));
                    m_impl->state.ay = static_cast<double>(prediction.at<float>(5));

                    m_impl->targetRect.x = std::clamp(
                        static_cast<int>(std::round(predCenterX - static_cast<float>(m_impl->targetRect.width) / 2.0f)),
                        0, width - m_impl->targetRect.width);
                    m_impl->targetRect.y = std::clamp(static_cast<int>(std::round(predCenterY
                                                          - static_cast<float>(m_impl->targetRect.height) / 2.0f)),
                        0, height - m_impl->targetRect.height);
                    m_impl->state.isCoasting = true;
                    m_impl->state.confidence = std::max(0.05,
                        1.0 - static_cast<double>(m_impl->lostFrames) / static_cast<double>(m_impl->maxCoastFrames));
                    m_impl->trackedPoints.clear();
                } else if (m_impl->lostFrames > m_impl->maxCoastFrames) {
                    m_impl->state.locked = false;
                    m_impl->state.isCoasting = false;
                    m_impl->kalmanInitialized = false;
                    m_impl->trajectoryHistory.clear();
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
        m_impl->state.normalizedWidth
            = (width > 0) ? (static_cast<double>(m_impl->targetRect.width) / static_cast<double>(width)) : 0.0;
        m_impl->state.normalizedHeight
            = (height > 0) ? (static_cast<double>(m_impl->targetRect.height) / static_cast<double>(height)) : 0.0;

        const double cx
            = static_cast<double>(m_impl->targetRect.x) + static_cast<double>(m_impl->targetRect.width) / 2.0;
        const double cy
            = static_cast<double>(m_impl->targetRect.y) + static_cast<double>(m_impl->targetRect.height) / 2.0;
        const double halfW = static_cast<double>(width) / 2.0;
        const double halfH = static_cast<double>(height) / 2.0;
        m_impl->state.errorX = (cx - halfW) / halfW;
        m_impl->state.errorY = (cy - halfH) / halfH;
        m_impl->state.predictedErrorX = m_impl->state.errorX;
        m_impl->state.predictedErrorY = m_impl->state.errorY;

        cv::Scalar lockColor;
        if (m_impl->state.isCoasting) {
            lockColor
                = (format == PixelFormat::RGB24) ? cv::Scalar(255, 165, 0) : cv::Scalar(0, 165, 255); // Amber Coasting
        } else if (m_impl->state.confidence > 0.5) {
            lockColor
                = (format == PixelFormat::RGB24) ? cv::Scalar(0, 255, 64) : cv::Scalar(64, 255, 0); // Green Locked
        } else {
            lockColor
                = (format == PixelFormat::RGB24) ? cv::Scalar(255, 200, 0) : cv::Scalar(0, 200, 255); // Yellow Low Conf
        }

        const cv::Rect r = m_impl->targetRect;
        cv::rectangle(mat, r, lockColor, 2, cv::LINE_AA);
        cv::line(mat, cv::Point(r.x + r.width / 2 - 4, r.y + r.height / 2),
            cv::Point(r.x + r.width / 2 + 4, r.y + r.height / 2), lockColor, 1);
        cv::line(mat, cv::Point(r.x + r.width / 2, r.y + r.height / 2 - 4),
            cv::Point(r.x + r.width / 2, r.y + r.height / 2 + 4), lockColor, 1);

        const cv::Point centerPt(r.x + r.width / 2, r.y + r.height / 2);

        // Compute delta time and velocity metrics
        const auto now = std::chrono::steady_clock::now();
        double dtSec = 0.0333; // Default 30 FPS
        if (m_impl->hasLastProcessTime) {
            const std::chrono::duration<double> elapsed = now - m_impl->lastProcessTime;
            dtSec = std::clamp(elapsed.count(), 0.001, 0.5);
        }
        m_impl->lastProcessTime = now;
        m_impl->hasLastProcessTime = true;

        const double vx = m_impl->state.vx;
        const double vy = m_impl->state.vy;
        const double ax = m_impl->state.ax;
        const double ay = m_impl->state.ay;
        const double speedPxPerSec = std::sqrt(vx * vx + vy * vy) / dtSec;
        const double vSq = vx * vx + vy * vy;

        // Turn rate omega in rad/frame and heading in degrees
        double omega = 0.0;
        if (vSq > 1e-4) {
            omega = (vx * ay - vy * ax) / vSq;
        }
        const double omegaRps = omega / dtSec;
        double headingDeg = std::atan2(vy, vx) * 180.0 / 3.14159265358979323846;
        if (headingDeg < 0.0) {
            headingDeg += 360.0;
        }
        m_impl->state.turnRateRps = omegaRps;
        m_impl->state.headingDeg = headingDeg;

        // Trajectory breadcrumbs path
        if (m_impl->trajectoryConfig.enabled) {
            m_impl->trajectoryHistory.push_back(
                { cv::Point2f(static_cast<float>(centerPt.x), static_cast<float>(centerPt.y)), speedPxPerSec, now });

            // Decoupled physical time decay pruning
            while (!m_impl->trajectoryHistory.empty()) {
                const double ageSec
                    = std::chrono::duration<double>(now - m_impl->trajectoryHistory.front().timestamp).count();
                if (ageSec > m_impl->trajectoryConfig.maxDurationSec
                    || m_impl->trajectoryHistory.size()
                        > static_cast<std::size_t>(m_impl->trajectoryConfig.maxPoints)) {
                    m_impl->trajectoryHistory.pop_front();
                } else {
                    break;
                }
            }

            const std::size_t nPts = m_impl->trajectoryHistory.size();
            if (nPts >= 2) {
                auto getColorForPoint = [&](const Impl::BreadcrumbPoint& pt, double alphaRatio) -> cv::Scalar {
                    cv::Scalar base;
                    if (m_impl->trajectoryConfig.speedGradient) {
                        const double s = std::clamp(pt.speed / 300.0, 0.0, 1.0);
                        if (s < 0.5) {
                            const double t = s * 2.0;
                            const double rC = 255.0 * t;
                            const double gC = 255.0 - 55.0 * t;
                            const double bC = 64.0 * (1.0 - t);
                            base = (format == PixelFormat::RGB24) ? cv::Scalar(rC, gC, bC) : cv::Scalar(bC, gC, rC);
                        } else {
                            const double t = (s - 0.5) * 2.0;
                            const double rC = 255.0;
                            const double gC = 200.0 * (1.0 - t) + 40.0 * t;
                            const double bC = 40.0 * t;
                            base = (format == PixelFormat::RGB24) ? cv::Scalar(rC, gC, bC) : cv::Scalar(bC, gC, rC);
                        }
                    } else {
                        base = lockColor;
                    }
                    const double effectiveAlpha = 0.25 + 0.75 * alphaRatio;
                    return base * effectiveAlpha;
                };

                if (m_impl->trajectoryConfig.smoothSpline && nPts >= 4) {
                    // Catmull-Rom spline interpolation between breadcrumb control points
                    for (std::size_t i = 0; i < nPts - 1; ++i) {
                        const cv::Point2f p0 = (i == 0) ? m_impl->trajectoryHistory[0].position
                                                        : m_impl->trajectoryHistory[i - 1].position;
                        const cv::Point2f p1 = m_impl->trajectoryHistory[i].position;
                        const cv::Point2f p2 = m_impl->trajectoryHistory[i + 1].position;
                        const cv::Point2f p3 = (i + 2 < nPts) ? m_impl->trajectoryHistory[i + 2].position : p2;

                        const double u2 = static_cast<double>(i + 1) / static_cast<double>(nPts);
                        const cv::Scalar segColor = getColorForPoint(m_impl->trajectoryHistory[i + 1], u2);

                        constexpr int SUBDIVISIONS = 4;
                        cv::Point2f prevSub = p1;
                        for (int step = 1; step <= SUBDIVISIONS; ++step) {
                            const float t = static_cast<float>(step) / static_cast<float>(SUBDIVISIONS);
                            const float t2 = t * t;
                            const float t3 = t2 * t;
                            const cv::Point2f subPt = 0.5f
                                * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
                                    + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
                            cv::line(mat, prevSub, subPt, segColor, 1, cv::LINE_AA);
                            prevSub = subPt;
                        }
                    }
                } else {
                    for (std::size_t i = 1U; i < nPts; ++i) {
                        const double alpha = static_cast<double>(i) / static_cast<double>(nPts);
                        const cv::Scalar segColor = getColorForPoint(m_impl->trajectoryHistory[i], alpha);
                        cv::line(mat, cv::Point(m_impl->trajectoryHistory[i - 1].position),
                            cv::Point(m_impl->trajectoryHistory[i].position), segColor, 1, cv::LINE_AA);
                    }
                }

                // Render breadcrumb dots with tapered radius
                for (std::size_t i = 0; i < nPts; ++i) {
                    if (i % 2 == 0 || i == nPts - 1) {
                        const double alpha = static_cast<double>(i + 1) / static_cast<double>(nPts);
                        const cv::Scalar dotColor = getColorForPoint(m_impl->trajectoryHistory[i], alpha);
                        const int radius = std::max(1, static_cast<int>(std::round(1.0 + 2.5 * alpha)));
                        cv::circle(
                            mat, cv::Point(m_impl->trajectoryHistory[i].position), radius, dotColor, -1, cv::LINE_AA);
                    }
                }
            }
        }

        // Predictive lead vector & interception reticle projection
        if (m_impl->predictiveLeadConfig.enabled && (std::abs(vx) > 0.05 || std::abs(vy) > 0.05)) {
            const double fps = 30.0;
            const double framesAhead = m_impl->predictiveLeadConfig.lookaheadSeconds * fps;
            cv::Point futurePt;

            if (m_impl->predictiveLeadConfig.curvilinearPrediction && std::abs(omega) > 0.005 && vSq > 0.05) {
                // CTRA Curvilinear prediction arc
                constexpr int ARC_STEPS = 8;
                cv::Point prevArcPt = centerPt;
                for (int s = 1; s <= ARC_STEPS; ++s) {
                    const double kStep = framesAhead * (static_cast<double>(s) / static_cast<double>(ARC_STEPS));
                    const double sinWk = std::sin(omega * kStep);
                    const double cosWk = std::cos(omega * kStep);
                    const double dX = (vx / omega) * sinWk - (vy / omega) * (1.0 - cosWk) + 0.5 * ax * kStep * kStep;
                    const double dY = (vx / omega) * (1.0 - cosWk) + (vy / omega) * sinWk + 0.5 * ay * kStep * kStep;
                    const cv::Point arcPt(
                        std::clamp(static_cast<int>(std::round(static_cast<double>(centerPt.x) + dX)), 0, width - 1),
                        std::clamp(static_cast<int>(std::round(static_cast<double>(centerPt.y) + dY)), 0, height - 1));
                    cv::line(mat, prevArcPt, arcPt, lockColor, (s == ARC_STEPS ? 2 : 1), cv::LINE_AA);
                    prevArcPt = arcPt;
                }
                futurePt = prevArcPt;

                // Draw arrow tip on final tangent
                const double sinWk = std::sin(omega * framesAhead);
                const double cosWk = std::cos(omega * framesAhead);
                const cv::Point arrowTip = futurePt;
                const cv::Point arrowBase(
                    std::clamp(
                        static_cast<int>(std::round(futurePt.x - (vx * cosWk - vy * sinWk) * 2.0)), 0, width - 1),
                    std::clamp(
                        static_cast<int>(std::round(futurePt.y - (vx * sinWk + vy * cosWk) * 2.0)), 0, height - 1));
                cv::arrowedLine(mat, arrowBase, arrowTip, lockColor, 2, cv::LINE_AA, 0, 0.4);
            } else {
                // Standard second-order quadratic CA model
                const double predX
                    = static_cast<double>(centerPt.x) + vx * framesAhead + 0.5 * ax * framesAhead * framesAhead;
                const double predY
                    = static_cast<double>(centerPt.y) + vy * framesAhead + 0.5 * ay * framesAhead * framesAhead;
                futurePt = cv::Point(std::clamp(static_cast<int>(std::round(predX)), 0, width - 1),
                    std::clamp(static_cast<int>(std::round(predY)), 0, height - 1));
                cv::arrowedLine(mat, centerPt, futurePt, lockColor, 2, cv::LINE_AA, 0, 0.15);
            }

            m_impl->state.predictedTargetX = futurePt.x;
            m_impl->state.predictedTargetY = futurePt.y;

            // Kalman Uncertainty Covariance Ellipse
            if (m_impl->predictiveLeadConfig.showUncertaintyEllipse && m_impl->kalmanInitialized) {
                const float k = static_cast<float>(framesAhead);
                const float k2 = k * k;
                const float k3 = k2 * k;
                const float k4 = k2 * k2;

                const cv::Mat& P = m_impl->kalman.errorCovPost;
                const float p00 = P.at<float>(0, 0);
                const float p11 = P.at<float>(1, 1);
                const float p22 = P.at<float>(2, 2);
                const float p33 = P.at<float>(3, 3);
                const float p44 = P.at<float>(4, 4);
                const float p55 = P.at<float>(5, 5);
                const float p02 = P.at<float>(0, 2);
                const float p13 = P.at<float>(1, 3);
                const float p04 = P.at<float>(0, 4);
                const float p15 = P.at<float>(1, 5);
                const float p24 = P.at<float>(2, 4);
                const float p35 = P.at<float>(3, 5);
                const float p01 = P.at<float>(0, 1);

                const double cxx = std::max(
                    1.0, static_cast<double>(p00 + 2.0f * k * p02 + k2 * p22 + k2 * p04 + k3 * p24 + 0.25f * k4 * p44));
                const double cyy = std::max(
                    1.0, static_cast<double>(p11 + 2.0f * k * p13 + k2 * p33 + k2 * p15 + k3 * p35 + 0.25f * k4 * p55));
                const double cxy = static_cast<double>(p01);

                const double tr = cxx + cyy;
                const double diff = cxx - cyy;
                const double disc = std::sqrt(std::max(0.0, diff * diff + 4.0 * cxy * cxy));
                const double l1 = std::max(1.0, 0.5 * (tr + disc));
                const double l2 = std::max(1.0, 0.5 * (tr - disc));
                const double semiMajor = std::clamp(2.0 * std::sqrt(l1), 4.0, 160.0);
                const double semiMinor = std::clamp(2.0 * std::sqrt(l2), 3.0, 160.0);
                const double angleDeg = 0.5 * std::atan2(2.0 * cxy, diff) * 180.0 / 3.14159265358979323846;

                m_impl->state.uncertaintyMajor = semiMajor;
                m_impl->state.uncertaintyMinor = semiMinor;
                m_impl->state.uncertaintyAngleDeg = angleDeg;

                const cv::Scalar ellipseColor = lockColor * 0.75;
                cv::ellipse(mat, futurePt,
                    cv::Size(static_cast<int>(std::round(semiMajor)), static_cast<int>(std::round(semiMinor))),
                    angleDeg, 0.0, 360.0, ellipseColor, 1, cv::LINE_AA);
            }

            // Tactical Interception reticle and lookahead / heading label
            cv::circle(mat, futurePt, 6, lockColor, 1, cv::LINE_AA);
            cv::drawMarker(mat, futurePt, lockColor, cv::MARKER_CROSS, 10, 1, cv::LINE_AA);

            char timeBuf[32];
            std::snprintf(
                timeBuf, sizeof(timeBuf), "+%.1fs [%.0f°]", m_impl->predictiveLeadConfig.lookaheadSeconds, headingDeg);
            cv::putText(mat, timeBuf, cv::Point(futurePt.x + 8, futurePt.y - 4), cv::FONT_HERSHEY_PLAIN, 0.8, lockColor,
                1, cv::LINE_AA);
        } else {
            // Standard velocity vector projection
            const cv::Point arrowEnd(centerPt.x + static_cast<int>(std::round(m_impl->state.vx * 4.0)),
                centerPt.y + static_cast<int>(std::round(m_impl->state.vy * 4.0)));
            cv::arrowedLine(mat, centerPt, arrowEnd, lockColor, 1, cv::LINE_AA, 0, 0.3);
        }

        // Boresight Lead Setpoint Marker (Visual PTZ Steering Setpoint)
        if (m_impl->predictiveLeadConfig.showBoresightLeadSetpoint
            && (std::abs(m_impl->boresightLeadOffsetX) > 0.001 || std::abs(m_impl->boresightLeadOffsetY) > 0.001)) {
            const int ptzX
                = std::clamp(static_cast<int>(std::round(halfW + m_impl->boresightLeadOffsetX * halfW)), 0, width - 1);
            const int ptzY
                = std::clamp(static_cast<int>(std::round(halfH + m_impl->boresightLeadOffsetY * halfH)), 0, height - 1);
            const cv::Scalar ptzLeadColor
                = (format == PixelFormat::RGB24) ? cv::Scalar(0, 220, 255) : cv::Scalar(255, 220, 0);
            cv::drawMarker(mat, cv::Point(ptzX, ptzY), ptzLeadColor, cv::MARKER_DIAMOND, 14, 1, cv::LINE_AA);
            cv::line(mat, cv::Point(static_cast<int>(halfW), static_cast<int>(halfH)), cv::Point(ptzX, ptzY),
                ptzLeadColor, 1, cv::LINE_AA);
            cv::putText(mat, "PTZ LEAD", cv::Point(ptzX + 8, ptzY + 4), cv::FONT_HERSHEY_PLAIN, 0.75, ptzLeadColor, 1,
                cv::LINE_AA);
        }

        std::string tag;
        if (m_impl->state.isCoasting) {
            tag = "COASTING [dX:" + std::to_string(static_cast<int>(m_impl->state.errorX * 100.0))
                + "% dY:" + std::to_string(static_cast<int>(m_impl->state.errorY * 100.0)) + "%]";
        } else {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "LOCK %.1fx [%d%%] [dX:%d%% dY:%d%%]", m_impl->state.scaleFactor,
                static_cast<int>(m_impl->state.appearanceScore * 100.0), static_cast<int>(m_impl->state.errorX * 100.0),
                static_cast<int>(m_impl->state.errorY * 100.0));
            tag = buf;
        }
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
    std::scoped_lock lock(m_impl->mutex);
    return m_impl->alarmFrames > 0;
}

std::size_t PerimeterTripwireFilter::getIntrusionCount() const
{
    if (!m_impl) {
        return 0U;
    }
    std::scoped_lock lock(m_impl->mutex);
    return m_impl->intrusionCount;
}

void PerimeterTripwireFilter::resetIntrusionCount()
{
    if (!m_impl) {
        return;
    }
    std::scoped_lock lock(m_impl->mutex);
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

void PerimeterTripwireFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0 || !m_impl
        || (format != PixelFormat::RGB24 && format != PixelFormat::BGR24)) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat gray;
    const int convCode = (format == PixelFormat::RGB24) ? cv::COLOR_RGB2GRAY : cv::COLOR_BGR2GRAY;
    cv::cvtColor(mat, gray, convCode);

    std::scoped_lock lock(m_impl->mutex);

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
        std::scoped_lock lock(m_impl->mutex);
        m_impl->prevGray.release();
        m_impl->accumHeatmap.release();
    }
}

void MotionHeatmapFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0 || !m_impl
        || (format != PixelFormat::RGB24 && format != PixelFormat::BGR24)) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Mat gray;
    const int convCode = (format == PixelFormat::RGB24) ? cv::COLOR_RGB2GRAY : cv::COLOR_BGR2GRAY;
    cv::cvtColor(mat, gray, convCode);

    std::scoped_lock lock(m_impl->mutex);

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

} // namespace Video::Filters

#endif // PELCOD_HAS_FILTERS
