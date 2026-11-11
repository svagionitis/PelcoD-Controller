/// @file OpticalFlowTracker.cpp
/// @brief Multi-scale Pyramidal Lucas-Kanade optical flow tracker with bidirectional validation.
#include "OpticalFlowTracker.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "FeatureDetector.h"
#include "FramePyramid.h"
#include "FrameView.h"
#include "StabilizerArena.h"
#include "StabilizerTypes.h"

namespace Stabilizer {

OpticalFlowTracker::OpticalFlowTracker(const OpticalFlowConfig& config) noexcept
{
    if (config.isValid()) {
        config_ = config;
    }
}

bool OpticalFlowTracker::setConfig(const OpticalFlowConfig& config) noexcept
{
    if (config.isValid()) {
        config_ = config;
        return true;
    }
    return false;
}

float OpticalFlowTracker::sampleBilinear(const FrameView& view, float x, float y) noexcept
{
    float clampedX = (x < 0.0F) ? 0.0F : x;
    float clampedY = (y < 0.0F) ? 0.0F : y;

    const float maxX = static_cast<float>(view.width() - 1U);
    const float maxY = static_cast<float>(view.height() - 1U);

    if (clampedX > maxX) {
        clampedX = maxX;
    }
    if (clampedY > maxY) {
        clampedY = maxY;
    }

    const auto x0 = static_cast<std::uint32_t>(clampedX);
    const auto y0 = static_cast<std::uint32_t>(clampedY);
    const std::uint32_t x1 = std::min(x0 + 1U, view.width() - 1U);
    const std::uint32_t y1 = std::min(y0 + 1U, view.height() - 1U);

    const float fx = clampedX - static_cast<float>(x0);
    const float fy = clampedY - static_cast<float>(y0);

    const float p00
        = static_cast<float>(view.pixelClamped(static_cast<std::int32_t>(x0), static_cast<std::int32_t>(y0)));
    const float p10
        = static_cast<float>(view.pixelClamped(static_cast<std::int32_t>(x1), static_cast<std::int32_t>(y0)));
    const float p01
        = static_cast<float>(view.pixelClamped(static_cast<std::int32_t>(x0), static_cast<std::int32_t>(y1)));
    const float p11
        = static_cast<float>(view.pixelClamped(static_cast<std::int32_t>(x1), static_cast<std::int32_t>(y1)));

    const float top = p00 + (fx * (p10 - p00));
    const float bottom = p01 + (fx * (p11 - p01));
    return top + (fy * (bottom - top));
}

bool OpticalFlowTracker::trackLevel(const FrameView& viewI, const FrameView& viewJ, const SubPixelPoint& ptI,
    const SubPixelPoint& initialGuessJ, SubPixelPoint& outRefinedJ, float& outMinEigen, TrackingStatus& outStatus,
    float* patchI, float* patchIx, float* patchIy) const noexcept
{
    const auto halfW = static_cast<float>(config_.windowSize.halfWidth());
    const auto w = static_cast<std::int32_t>(config_.windowSize.halfWidth());

    if ((ptI.x - halfW < 1.0F) || (ptI.x + halfW > static_cast<float>(viewI.width() - 2U)) || (ptI.y - halfW < 1.0F)
        || (ptI.y + halfW > static_cast<float>(viewI.height() - 2U))) {
        outStatus = TrackingStatus::LostOutOfBounds;
        return false;
    }

    float g11 = 0.0F;
    float g12 = 0.0F;
    float g22 = 0.0F;
    std::size_t patchIdx = 0U;

    for (std::int32_t dy = -w; dy <= w; ++dy) {
        const float py = ptI.y + static_cast<float>(dy);
        for (std::int32_t dx = -w; dx <= w; ++dx) {
            const float px = ptI.x + static_cast<float>(dx);
            const float ival = sampleBilinear(viewI, px, py);
            const float ix = (sampleBilinear(viewI, px + 1.0F, py) - sampleBilinear(viewI, px - 1.0F, py)) * 0.5F;
            const float iy = (sampleBilinear(viewI, px, py + 1.0F) - sampleBilinear(viewI, px, py - 1.0F)) * 0.5F;

            patchI[patchIdx] = ival;
            patchIx[patchIdx] = ix;
            patchIy[patchIdx] = iy;
            ++patchIdx;

            g11 += ix * ix;
            g12 += ix * iy;
            g22 += iy * iy;
        }
    }

    const float trace = g11 + g22;
    const float det = (g11 * g22) - (g12 * g12);
    const float diff = g11 - g22;
    const float discr = std::sqrt((diff * diff) + (4.0F * g12 * g12));
    const float lambdaMin = 0.5F * (trace - discr);

    outMinEigen = lambdaMin;

    if ((lambdaMin < config_.minEigenvalue) || (det < 1e-6F)) {
        outStatus = TrackingStatus::LostIllConditioned;
        return false;
    }

    const float invDet = 1.0F / det;
    const float invG11 = g22 * invDet;
    const float invG12 = -g12 * invDet;
    const float invG22 = g11 * invDet;

    SubPixelPoint currJ = initialGuessJ;
    const float epsSq = config_.minEpsilon * config_.minEpsilon;

    for (std::uint16_t iter = 0U; iter < config_.maxIterations; ++iter) {
        if ((currJ.x - halfW < 1.0F) || (currJ.x + halfW > static_cast<float>(viewJ.width() - 2U))
            || (currJ.y - halfW < 1.0F) || (currJ.y + halfW > static_cast<float>(viewJ.height() - 2U))) {
            outStatus = TrackingStatus::LostOutOfBounds;
            return false;
        }

        float bx = 0.0F;
        float by = 0.0F;
        patchIdx = 0U;

        for (std::int32_t dy = -w; dy <= w; ++dy) {
            const float qy = currJ.y + static_cast<float>(dy);
            for (std::int32_t dx = -w; dx <= w; ++dx) {
                const float qx = currJ.x + static_cast<float>(dx);
                const float jval = sampleBilinear(viewJ, qx, qy);
                const float deltaI = patchI[patchIdx] - jval;

                bx += deltaI * patchIx[patchIdx];
                by += deltaI * patchIy[patchIdx];
                ++patchIdx;
            }
        }

        const float deltaU = (invG11 * bx) + (invG12 * by);
        const float deltaV = (invG12 * bx) + (invG22 * by);

        currJ.x += deltaU;
        currJ.y += deltaV;

        if (((deltaU * deltaU) + (deltaV * deltaV)) < epsSq) {
            break;
        }
    }

    if (std::isnan(currJ.x) || std::isnan(currJ.y) || std::isinf(currJ.x) || std::isinf(currJ.y)) {
        outStatus = TrackingStatus::LostConvergence;
        return false;
    }

    if ((currJ.x - halfW < 1.0F) || (currJ.x + halfW > static_cast<float>(viewJ.width() - 2U))
        || (currJ.y - halfW < 1.0F) || (currJ.y + halfW > static_cast<float>(viewJ.height() - 2U))) {
        outStatus = TrackingStatus::LostOutOfBounds;
        return false;
    }

    outRefinedJ = currJ;
    outStatus = TrackingStatus::Tracked;
    return true;
}

bool OpticalFlowTracker::trackSingle(const FramePyramid& pyrPrev, const FramePyramid& pyrCurr,
    const SubPixelPoint& startPt, SubPixelPoint& outTargetPt, float& outMinEigen, TrackingStatus& outStatus,
    float* patchI, float* patchIx, float* patchIy) const noexcept
{
    const auto availableLevels = std::min(pyrPrev.levelCount(), pyrCurr.levelCount());
    const auto activeLevels = std::min(config_.numPyramidLevels, availableLevels);

    if (activeLevels == 0U) {
        outStatus = TrackingStatus::LostConvergence;
        return false;
    }

    MotionVector accumDisp { 0.0F, 0.0F };

    for (std::int32_t lvl = static_cast<std::int32_t>(activeLevels) - 1; lvl >= 0; --lvl) {
        const auto uLevel = static_cast<std::uint8_t>(lvl);
        const FrameView viewI = pyrPrev.level(uLevel);
        const FrameView viewJ = pyrCurr.level(uLevel);

        const float scale = 1.0F / static_cast<float>(1U << uLevel);
        const SubPixelPoint ptILvl { startPt.x * scale, startPt.y * scale };

        SubPixelPoint guessJLvl { };
        if (lvl == static_cast<std::int32_t>(activeLevels) - 1) {
            guessJLvl = ptILvl;
        } else {
            guessJLvl = SubPixelPoint { ptILvl.x + (accumDisp.dx * 2.0F), ptILvl.y + (accumDisp.dy * 2.0F) };
        }

        SubPixelPoint refinedJLvl { };
        float lvlMinEigen = 0.0F;
        TrackingStatus lvlStatus = TrackingStatus::LostConvergence;

        if (!trackLevel(
                viewI, viewJ, ptILvl, guessJLvl, refinedJLvl, lvlMinEigen, lvlStatus, patchI, patchIx, patchIy)) {
            outStatus = lvlStatus;
            return false;
        }

        if (lvl == 0) {
            outMinEigen = lvlMinEigen;
            outTargetPt = refinedJLvl;
        }

        accumDisp.dx = refinedJLvl.x - ptILvl.x;
        accumDisp.dy = refinedJLvl.y - ptILvl.y;
    }

    outStatus = TrackingStatus::Tracked;
    return true;
}

std::size_t OpticalFlowTracker::trackFeatures(const FramePyramid& pyrPrev, const FramePyramid& pyrCurr,
    const FeaturePoint* inFeatures, std::size_t numFeatures, TrackedCorrespondence* outMatches,
    StabilizerArena& arena) noexcept
{
    if ((inFeatures == nullptr) || (outMatches == nullptr) || (numFeatures == 0U)) {
        return 0U;
    }

    const std::size_t patchPixels = config_.windowSize.totalPixels();
    const std::size_t patchBytes = patchPixels * sizeof(float);

    auto* patchI = static_cast<float*>(arena.allocate(patchBytes));
    auto* patchIx = static_cast<float*>(arena.allocate(patchBytes));
    auto* patchIy = static_cast<float*>(arena.allocate(patchBytes));

    if ((patchI == nullptr) || (patchIx == nullptr) || (patchIy == nullptr)) {
        for (std::size_t i = 0U; i < numFeatures; ++i) {
            outMatches[i].status = TrackingStatus::LostConvergence;
        }
        return 0U;
    }

    std::size_t successCount = 0U;

    for (std::size_t i = 0U; i < numFeatures; ++i) {
        outMatches[i].feature = inFeatures[i];
        outMatches[i].currentPt = inFeatures[i].pt;
        outMatches[i].displacement = MotionVector { 0.0F, 0.0F };
        outMatches[i].forwardBackwardError = 0.0F;
        outMatches[i].minEigenvalue = 0.0F;
        outMatches[i].status = TrackingStatus::LostConvergence;

        if (!inFeatures[i].pt.isValid()) {
            outMatches[i].status = TrackingStatus::LostOutOfBounds;
            continue;
        }

        SubPixelPoint fwdPt { };
        float fwdMinEigen = 0.0F;
        TrackingStatus fwdStatus = TrackingStatus::LostConvergence;

        if (!trackSingle(pyrPrev, pyrCurr, inFeatures[i].pt, fwdPt, fwdMinEigen, fwdStatus, patchI, patchIx, patchIy)) {
            outMatches[i].status = fwdStatus;
            continue;
        }

        outMatches[i].currentPt = fwdPt;
        outMatches[i].displacement = MotionVector { fwdPt.x - inFeatures[i].pt.x, fwdPt.y - inFeatures[i].pt.y };
        outMatches[i].minEigenvalue = fwdMinEigen;

        if (config_.enableBidirectional) {
            SubPixelPoint bwdPt { };
            float bwdMinEigen = 0.0F;
            TrackingStatus bwdStatus = TrackingStatus::LostConvergence;

            if (!trackSingle(pyrCurr, pyrPrev, fwdPt, bwdPt, bwdMinEigen, bwdStatus, patchI, patchIx, patchIy)) {
                outMatches[i].status = TrackingStatus::LostForwardBackward;
                continue;
            }

            const float dx = inFeatures[i].pt.x - bwdPt.x;
            const float dy = inFeatures[i].pt.y - bwdPt.y;
            const float fbError = std::sqrt((dx * dx) + (dy * dy));
            outMatches[i].forwardBackwardError = fbError;

            if (fbError <= config_.maxForwardBackwardError) {
                outMatches[i].status = TrackingStatus::Tracked;
                ++successCount;
            } else {
                outMatches[i].status = TrackingStatus::LostForwardBackward;
            }
        } else {
            outMatches[i].status = TrackingStatus::Tracked;
            ++successCount;
        }
    }

    return successCount;
}

} // namespace Stabilizer
