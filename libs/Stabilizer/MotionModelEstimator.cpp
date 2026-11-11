/// @file MotionModelEstimator.cpp
/// @brief Implementation of MSAC parametric motion model estimator.
#include "MotionModelEstimator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace Stabilizer {

namespace {

    /// @brief Lightweight, zero-heap, deterministic XorShift32 pseudo-random generator.
    class FastPrng {
    public:
        explicit constexpr FastPrng(std::uint32_t seed) noexcept
            : state_ { (seed == 0U) ? 0x12345678U : seed }
        {
        }

        [[nodiscard]] std::uint32_t next() noexcept
        {
            std::uint32_t x = state_;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            state_ = x;
            return x;
        }

        [[nodiscard]] std::uint32_t nextBounded(std::uint32_t maxExclusive) noexcept
        {
            if (maxExclusive == 0U) {
                return 0U;
            }
            return next() % maxExclusive;
        }

    private:
        std::uint32_t state_ { 0x12345678U };
    };

} // namespace

MotionModelEstimator::MotionModelEstimator(const MotionEstimatorConfig& config) noexcept
    : config_ { config.isValid() ? config : MotionEstimatorConfig { } }
{
}

bool MotionModelEstimator::setConfig(const MotionEstimatorConfig& config) noexcept
{
    if (!config.isValid()) {
        return false;
    }
    config_ = config;
    return true;
}

void MotionModelEstimator::solveTranslation(
    const SubPixelPoint& p, const SubPixelPoint& pPrime, AffineTransform2D& outTransform) noexcept
{
    outTransform = AffineTransform2D::translation(pPrime.x - p.x, pPrime.y - p.y);
}

bool MotionModelEstimator::solveEuclidean(const SubPixelPoint& p0, const SubPixelPoint& p0Prime,
    const SubPixelPoint& p1, const SubPixelPoint& p1Prime, AffineTransform2D& outTransform) noexcept
{
    const float dx = p1.x - p0.x;
    const float dy = p1.y - p0.y;
    const float dxPrime = p1Prime.x - p0Prime.x;
    const float dyPrime = p1Prime.y - p0Prime.y;

    const float lenSq = (dx * dx) + (dy * dy);
    const float lenSqPrime = (dxPrime * dxPrime) + (dyPrime * dyPrime);
    if ((lenSq < 1e-6F) || (lenSqPrime < 1e-6F)) {
        return false;
    }

    const float crossVal = (dx * dyPrime) - (dy * dxPrime);
    const float dotVal = (dx * dxPrime) + (dy * dyPrime);
    const float theta = std::atan2(crossVal, dotVal);

    const float cosTheta = std::cos(theta);
    const float sinTheta = std::sin(theta);

    const float cx = 0.5F * (p0.x + p1.x);
    const float cy = 0.5F * (p0.y + p1.y);
    const float cxPrime = 0.5F * (p0Prime.x + p1Prime.x);
    const float cyPrime = 0.5F * (p0Prime.y + p1Prime.y);

    const float tx = cxPrime - ((cosTheta * cx) - (sinTheta * cy));
    const float ty = cyPrime - ((sinTheta * cx) + (cosTheta * cy));

    outTransform = { cosTheta, -sinTheta, tx, sinTheta, cosTheta, ty };
    return true;
}

bool MotionModelEstimator::solveSimilarity(const SubPixelPoint& p0, const SubPixelPoint& p0Prime,
    const SubPixelPoint& p1, const SubPixelPoint& p1Prime, AffineTransform2D& outTransform) noexcept
{
    const float dx = p1.x - p0.x;
    const float dy = p1.y - p0.y;
    const float dxPrime = p1Prime.x - p0Prime.x;
    const float dyPrime = p1Prime.y - p0Prime.y;

    const float dSq = (dx * dx) + (dy * dy);
    if (dSq < 1e-6F) {
        return false;
    }

    const float invDSq = 1.0F / dSq;
    const float alpha = ((dx * dxPrime) + (dy * dyPrime)) * invDSq;
    const float beta = ((dx * dyPrime) - (dy * dxPrime)) * invDSq;

    const float tx = p0Prime.x - ((alpha * p0.x) - (beta * p0.y));
    const float ty = p0Prime.y - ((beta * p0.x) + (alpha * p0.y));

    outTransform = { alpha, -beta, tx, beta, alpha, ty };
    return true;
}

bool MotionModelEstimator::solveAffine(
    const SubPixelPoint* p, const SubPixelPoint* pPrime, AffineTransform2D& outTransform) noexcept
{
    if ((p == nullptr) || (pPrime == nullptr)) {
        return false;
    }

    const float x0 = p[0].x;
    const float y0 = p[0].y;
    const float x1 = p[1].x;
    const float y1 = p[1].y;
    const float x2 = p[2].x;
    const float y2 = p[2].y;

    // Determinant of 3x3 matrix [x0 y0 1; x1 y1 1; x2 y2 1]
    const float det = (x0 * (y1 - y2)) - (y0 * (x1 - x2)) + ((x1 * y2) - (x2 * y1));
    if (std::abs(det) < 1.0F) {
        return false; // Collinear sample points
    }

    const float invDet = 1.0F / det;

    // Inverse matrix elements
    const float m00 = (y1 - y2) * invDet;
    const float m01 = -(y0 - y2) * invDet;
    const float m02 = (y0 - y1) * invDet;

    const float m10 = -(x1 - x2) * invDet;
    const float m11 = (x0 - x2) * invDet;
    const float m12 = -(x0 - x1) * invDet;

    const float m20 = ((x1 * y2) - (x2 * y1)) * invDet;
    const float m21 = -((x0 * y2) - (x2 * y0)) * invDet;
    const float m22 = ((x0 * y1) - (x1 * y0)) * invDet;

    const float x0P = pPrime[0].x;
    const float x1P = pPrime[1].x;
    const float x2P = pPrime[2].x;
    const float y0P = pPrime[0].y;
    const float y1P = pPrime[1].y;
    const float y2P = pPrime[2].y;

    outTransform.a11 = (m00 * x0P) + (m01 * x1P) + (m02 * x2P);
    outTransform.a12 = (m10 * x0P) + (m11 * x1P) + (m12 * x2P);
    outTransform.tx = (m20 * x0P) + (m21 * x1P) + (m22 * x2P);

    outTransform.a21 = (m00 * y0P) + (m01 * y1P) + (m02 * y2P);
    outTransform.a22 = (m10 * y0P) + (m11 * y1P) + (m12 * y2P);
    outTransform.ty = (m20 * y0P) + (m21 * y1P) + (m22 * y2P);

    return true;
}

bool MotionModelEstimator::checkTransform(const AffineTransform2D& t) noexcept
{
    const float det = (t.a11 * t.a22) - (t.a12 * t.a21);
    // Physically valid transformations preserve orientation and have positive determinant
    if (det <= 0.0F) {
        return false;
    }

    // Zoom/scale factor check: det(A) represents area scaling ratio
    if ((det < 0.25F) || (det > 4.0F)) {
        return false;
    }

    // Basis vector orthogonality & shear check
    const float luSq = (t.a11 * t.a11) + (t.a21 * t.a21);
    const float lvSq = (t.a12 * t.a12) + (t.a22 * t.a22);
    if ((luSq < 1e-4F) || (lvSq < 1e-4F)) {
        return false;
    }

    const float dotUV = (t.a11 * t.a12) + (t.a21 * t.a22);
    const float cosPhi = dotUV / std::sqrt(luSq * lvSq);
    if (std::abs(cosPhi) > 0.40F) {
        return false; // Excessive shear angle (> 23.5 degrees)
    }

    // Translation magnitude sanity bound
    const float transSq = (t.tx * t.tx) + (t.ty * t.ty);
    constexpr float MaxTransSq = 4000.0F * 4000.0F;
    if (transSq > MaxTransSq) {
        return false;
    }

    return true;
}

bool MotionModelEstimator::checkDegenerate(const SubPixelPoint* p, std::size_t count, StabilizationMode mode) noexcept
{
    if (p == nullptr) {
        return true;
    }

    constexpr float MinDistSq = 3.0F * 3.0F; // Minimum 3 pixels distance
    if (count >= 2U) {
        const float dx = p[1].x - p[0].x;
        const float dy = p[1].y - p[0].y;
        if (((dx * dx) + (dy * dy)) < MinDistSq) {
            return true;
        }
    }

    if (count >= 3U) {
        const float dx12 = p[2].x - p[1].x;
        const float dy12 = p[2].y - p[1].y;
        if (((dx12 * dx12) + (dy12 * dy12)) < MinDistSq) {
            return true;
        }

        const float dx20 = p[0].x - p[2].x;
        const float dy20 = p[0].y - p[2].y;
        if (((dx20 * dx20) + (dy20 * dy20)) < MinDistSq) {
            return true;
        }

        // Collinear check via signed triangle area
        const float doubleArea
            = (p[0].x * (p[1].y - p[2].y)) - (p[0].y * (p[1].x - p[2].x)) + ((p[1].x * p[2].y) - (p[2].x * p[1].y));
        if (std::abs(doubleArea) < 2.0F) {
            return true;
        }
    }

    (void)mode;
    return false;
}

void MotionModelEstimator::evaluateModel(const AffineTransform2D& model, const SubPixelPoint* p,
    const SubPixelPoint* pPrime, std::size_t count, float thresholdSquared, float& outCost,
    std::uint32_t& outInliers) noexcept
{
    outCost = 0.0F;
    outInliers = 0U;

    for (std::size_t i = 0U; i < count; ++i) {
        const float predX = (model.a11 * p[i].x) + (model.a12 * p[i].y) + model.tx;
        const float predY = (model.a21 * p[i].x) + (model.a22 * p[i].y) + model.ty;

        const float errX = pPrime[i].x - predX;
        const float errY = pPrime[i].y - predY;
        const float errSq = (errX * errX) + (errY * errY);

        if (errSq < thresholdSquared) {
            outCost += errSq;
            ++outInliers;
        } else {
            outCost += thresholdSquared;
        }
    }
}

bool MotionModelEstimator::refineInliers(const SubPixelPoint* p, const SubPixelPoint* pPrime, const bool* inlierMask,
    std::size_t count, StabilizationMode mode, AffineTransform2D& outTransform) noexcept
{
    if ((p == nullptr) || (pPrime == nullptr) || (inlierMask == nullptr)) {
        return false;
    }

    std::uint32_t inlierCount = 0U;
    for (std::size_t i = 0U; i < count; ++i) {
        if (inlierMask[i]) {
            ++inlierCount;
        }
    }

    if (inlierCount < sampleSize(mode)) {
        return false;
    }

    const auto inlierCountF = static_cast<float>(inlierCount);

    if (mode == StabilizationMode::TranslationOnly) {
        float sumDx = 0.0F;
        float sumDy = 0.0F;
        for (std::size_t i = 0U; i < count; ++i) {
            if (inlierMask[i]) {
                sumDx += (pPrime[i].x - p[i].x);
                sumDy += (pPrime[i].y - p[i].y);
            }
        }
        outTransform = AffineTransform2D::translation(sumDx / inlierCountF, sumDy / inlierCountF);
        return true;
    }

    // Compute centroids for Rigid and Similarity
    float cx = 0.0F;
    float cy = 0.0F;
    float cxPrime = 0.0F;
    float cyPrime = 0.0F;
    for (std::size_t i = 0U; i < count; ++i) {
        if (inlierMask[i]) {
            cx += p[i].x;
            cy += p[i].y;
            cxPrime += pPrime[i].x;
            cyPrime += pPrime[i].y;
        }
    }
    cx /= inlierCountF;
    cy /= inlierCountF;
    cxPrime /= inlierCountF;
    cyPrime /= inlierCountF;

    if (mode == StabilizationMode::TranslationRotation) {
        float cxx = 0.0F;
        float cxy = 0.0F;
        float cyx = 0.0F;
        float cyy = 0.0F;
        for (std::size_t i = 0U; i < count; ++i) {
            if (inlierMask[i]) {
                const float ux = p[i].x - cx;
                const float uy = p[i].y - cy;
                const float vx = pPrime[i].x - cxPrime;
                const float vy = pPrime[i].y - cyPrime;

                cxx += ux * vx;
                cxy += ux * vy;
                cyx += uy * vx;
                cyy += uy * vy;
            }
        }

        const float theta = std::atan2(cxy - cyx, cxx + cyy);
        const float cosTheta = std::cos(theta);
        const float sinTheta = std::sin(theta);

        const float tx = cxPrime - ((cosTheta * cx) - (sinTheta * cy));
        const float ty = cyPrime - ((sinTheta * cx) + (cosTheta * cy));

        const AffineTransform2D candidate { cosTheta, -sinTheta, tx, sinTheta, cosTheta, ty };
        if (checkTransform(candidate)) {
            outTransform = candidate;
            return true;
        }
        return false;
    }

    if (mode == StabilizationMode::Similarity) {
        float numAlpha = 0.0F;
        float numBeta = 0.0F;
        float den = 0.0F;
        for (std::size_t i = 0U; i < count; ++i) {
            if (inlierMask[i]) {
                const float ux = p[i].x - cx;
                const float uy = p[i].y - cy;
                const float vx = pPrime[i].x - cxPrime;
                const float vy = pPrime[i].y - cyPrime;

                numAlpha += (ux * vx) + (uy * vy);
                numBeta += (ux * vy) - (uy * vx);
                den += (ux * ux) + (uy * uy);
            }
        }

        if (den < 1e-6F) {
            return false;
        }

        const float alpha = numAlpha / den;
        const float beta = numBeta / den;
        const float tx = cxPrime - ((alpha * cx) - (beta * cy));
        const float ty = cyPrime - ((beta * cx) + (alpha * cy));

        const AffineTransform2D candidate { alpha, -beta, tx, beta, alpha, ty };
        if (checkTransform(candidate)) {
            outTransform = candidate;
            return true;
        }
        return false;
    }

    // Affine 6-DOF normal equations: (A^T A) x = A^T b
    float g00 = 0.0F;
    float g01 = 0.0F;
    float g02 = 0.0F;
    float g11 = 0.0F;
    float g12 = 0.0F;

    float cx0 = 0.0F;
    float cx1 = 0.0F;
    float cx2 = 0.0F;
    float cy0 = 0.0F;
    float cy1 = 0.0F;
    float cy2 = 0.0F;

    for (std::size_t i = 0U; i < count; ++i) {
        if (inlierMask[i]) {
            const float xi = p[i].x;
            const float yi = p[i].y;
            const float xP = pPrime[i].x;
            const float yP = pPrime[i].y;

            g00 += xi * xi;
            g01 += xi * yi;
            g02 += xi;
            g11 += yi * yi;
            g12 += yi;

            cx0 += xi * xP;
            cx1 += yi * xP;
            cx2 += xP;

            cy0 += xi * yP;
            cy1 += yi * yP;
            cy2 += yP;
        }
    }

    const float g22Val = inlierCountF;

    // Normal matrix determinant using cofactor expansion
    const float c00 = (g11 * g22Val) - (g12 * g12);
    const float c01 = -((g01 * g22Val) - (g12 * g02));
    const float c02 = (g01 * g12) - (g11 * g02);

    const float detGamma = (g00 * c00) + (g01 * c01) + (g02 * c02);
    if (std::abs(detGamma) < 1e-4F) {
        return false;
    }

    const float invDet = 1.0F / detGamma;

    const float c11 = (g00 * g22Val) - (g02 * g02);
    const float c12 = -((g00 * g12) - (g01 * g02));
    const float c22 = (g00 * g11) - (g01 * g01);

    // Compute affine coefficients
    const float a11 = ((c00 * cx0) + (c01 * cx1) + (c02 * cx2)) * invDet;
    const float a12 = ((c01 * cx0) + (c11 * cx1) + (c12 * cx2)) * invDet;
    const float tx = ((c02 * cx0) + (c12 * cx1) + (c22 * cx2)) * invDet;

    const float a21 = ((c00 * cy0) + (c01 * cy1) + (c02 * cy2)) * invDet;
    const float a22 = ((c01 * cy0) + (c11 * cy1) + (c12 * cy2)) * invDet;
    const float ty = ((c02 * cy0) + (c12 * cy1) + (c22 * cy2)) * invDet;

    const AffineTransform2D candidate { a11, a12, tx, a21, a22, ty };
    if (checkTransform(candidate)) {
        outTransform = candidate;
        return true;
    }

    return false;
}

bool MotionModelEstimator::estimateMotion(const TrackedCorrespondence* matches, std::size_t numMatches,
    MotionEstimationResult& outResult, StabilizerArena* arena) noexcept
{
    outResult = MotionEstimationResult { };
    outResult.transform = AffineTransform2D::identity();

    if ((matches == nullptr) || (numMatches == 0U)) {
        outResult.isDegenerate = true;
        return false;
    }

    // Static scratch buffer for up to 512 correspondences to guarantee zero heap
    constexpr std::size_t MaxLocalPoints = 512U;
    std::array<SubPixelPoint, MaxLocalPoints> localPts { };
    std::array<SubPixelPoint, MaxLocalPoints> localPtsPrime { };
    std::array<bool, MaxLocalPoints> bestInlierMask { };

    SubPixelPoint* pSrc = localPts.data();
    SubPixelPoint* pDst = localPtsPrime.data();
    bool* bestMask = bestInlierMask.data();

    // If caller provided an arena and match count exceeds local buffer, allocate from arena
    if ((arena != nullptr) && (numMatches > MaxLocalPoints)) {
        const std::size_t ptsBytes = numMatches * sizeof(SubPixelPoint);
        const std::size_t boolBytes = numMatches * sizeof(bool);
        auto* memPts = static_cast<SubPixelPoint*>(arena->allocate(ptsBytes, alignof(SubPixelPoint)));
        auto* memPtsPrime = static_cast<SubPixelPoint*>(arena->allocate(ptsBytes, alignof(SubPixelPoint)));
        auto* memBest = static_cast<bool*>(arena->allocate(boolBytes, alignof(bool)));
        if ((memPts != nullptr) && (memPtsPrime != nullptr) && (memBest != nullptr)) {
            pSrc = memPts;
            pDst = memPtsPrime;
            bestMask = memBest;
        }
    }

    // Harvest valid, successfully tracked correspondences
    std::size_t validCount = 0U;
    const std::size_t pointCap = (pSrc == localPts.data()) ? MaxLocalPoints : numMatches;
    for (std::size_t i = 0U; (i < numMatches) && (validCount < pointCap); ++i) {
        if (matches[i].isTracked()) {
            pSrc[validCount] = matches[i].feature.pt;
            pDst[validCount] = matches[i].currentPt;
            ++validCount;
        }
    }

    outResult.totalPoints = static_cast<std::uint32_t>(validCount);

    const std::uint32_t minSamplePts = sampleSize(config_.mode);
    if (validCount < minSamplePts) {
        outResult.isDegenerate = true;
        return false;
    }

    const float tSq = config_.inlierThreshold.squared();
    float bestCost = std::numeric_limits<float>::max();
    std::uint32_t bestInlierCount = 0U;
    AffineTransform2D bestTransform = AffineTransform2D::identity();

    FastPrng prng { config_.randomSeed };
    std::uint32_t maxIterations = config_.maxIterations;
    std::uint32_t iterationsExecuted = 0U;

    for (std::uint32_t iter = 0U; iter < maxIterations; ++iter) {
        ++iterationsExecuted;

        // Sample minimal subset of indices
        std::array<std::size_t, 3> sampleIndices { };
        for (std::uint32_t s = 0U; s < minSamplePts; ++s) {
            bool unique = false;
            std::size_t drawn = 0U;
            for (std::uint32_t attempt = 0U; attempt < 10U; ++attempt) {
                drawn = static_cast<std::size_t>(prng.nextBounded(static_cast<std::uint32_t>(validCount)));
                unique = true;
                for (std::uint32_t prev = 0U; prev < s; ++prev) {
                    if (sampleIndices[prev] == drawn) {
                        unique = false;
                        break;
                    }
                }
                if (unique) {
                    break;
                }
            }
            sampleIndices[s] = drawn;
        }

        // Check geometry degeneracy of sample subset
        std::array<SubPixelPoint, 3> subsetPts { };
        std::array<SubPixelPoint, 3> subsetPtsPrime { };
        for (std::uint32_t s = 0U; s < minSamplePts; ++s) {
            subsetPts[s] = pSrc[sampleIndices[s]];
            subsetPtsPrime[s] = pDst[sampleIndices[s]];
        }

        if (checkDegenerate(subsetPts.data(), minSamplePts, config_.mode)) {
            continue;
        }

        // Solve minimal model
        AffineTransform2D candidateModel = AffineTransform2D::identity();
        bool solved = false;

        switch (config_.mode) {
        case StabilizationMode::TranslationOnly:
            solveTranslation(subsetPts[0], subsetPtsPrime[0], candidateModel);
            solved = true;
            break;
        case StabilizationMode::TranslationRotation:
            solved = solveEuclidean(subsetPts[0], subsetPtsPrime[0], subsetPts[1], subsetPtsPrime[1], candidateModel);
            break;
        case StabilizationMode::Similarity:
            solved = solveSimilarity(subsetPts[0], subsetPtsPrime[0], subsetPts[1], subsetPtsPrime[1], candidateModel);
            break;
        case StabilizationMode::Affine:
        case StabilizationMode::RollingShutterComp:
        default:
            solved = solveAffine(subsetPts.data(), subsetPtsPrime.data(), candidateModel);
            break;
        }

        if (!solved || !checkTransform(candidateModel)) {
            continue;
        }

        // Evaluate candidate model via MSAC bounded loss
        float currentCost = 0.0F;
        std::uint32_t currentInliers = 0U;
        evaluateModel(candidateModel, pSrc, pDst, validCount, tSq, currentCost, currentInliers);

        if (currentCost < bestCost) {
            bestCost = currentCost;
            bestInlierCount = currentInliers;
            bestTransform = candidateModel;

            // Update best inlier mask
            for (std::size_t i = 0U; i < validCount; ++i) {
                const float predX
                    = (bestTransform.a11 * pSrc[i].x) + (bestTransform.a12 * pSrc[i].y) + bestTransform.tx;
                const float predY
                    = (bestTransform.a21 * pSrc[i].x) + (bestTransform.a22 * pSrc[i].y) + bestTransform.ty;
                const float errSq
                    = ((pDst[i].x - predX) * (pDst[i].x - predX)) + ((pDst[i].y - predY) * (pDst[i].y - predY));
                bestMask[i] = (errSq < tSq);
            }

            // Adaptive iteration termination
            const float inlierRatio = static_cast<float>(bestInlierCount) / static_cast<float>(validCount);
            if (inlierRatio >= config_.minInlierRatio) {
                const float inlierProb = std::pow(inlierRatio, static_cast<float>(minSamplePts));
                if (inlierProb > 1e-4F) {
                    const float logConf = std::log(1.0F - config_.confidence);
                    const float logInlier = std::log(1.0F - inlierProb);
                    const auto reqIter = static_cast<std::uint32_t>(std::ceil(logConf / logInlier));
                    const auto clampedIter = std::clamp(reqIter, config_.minIterations, config_.maxIterations);
                    if (clampedIter < maxIterations) {
                        maxIterations = clampedIter;
                    }
                }
            }
        }
    }

    outResult.iterationsUsed = iterationsExecuted;

    const float finalInlierRatio
        = (validCount > 0U) ? (static_cast<float>(bestInlierCount) / static_cast<float>(validCount)) : 0.0F;

    if ((bestInlierCount < minSamplePts) || (finalInlierRatio < config_.minInlierRatio)) {
        outResult.isDegenerate = true;
        outResult.isSuccess = false;
        return false;
    }

    // Linear least-squares refinement over all consensus inliers
    if (config_.enableRefinement) {
        AffineTransform2D refined = AffineTransform2D::identity();
        if (refineInliers(pSrc, pDst, bestMask, validCount, config_.mode, refined)) {
            bestTransform = refined;

            // Re-evaluate inliers & cost with refined transform
            float refinedCost = 0.0F;
            std::uint32_t refinedInliers = 0U;
            evaluateModel(bestTransform, pSrc, pDst, validCount, tSq, refinedCost, refinedInliers);
            bestCost = refinedCost;
            bestInlierCount = refinedInliers;

            for (std::size_t i = 0U; i < validCount; ++i) {
                const float predX
                    = (bestTransform.a11 * pSrc[i].x) + (bestTransform.a12 * pSrc[i].y) + bestTransform.tx;
                const float predY
                    = (bestTransform.a21 * pSrc[i].x) + (bestTransform.a22 * pSrc[i].y) + bestTransform.ty;
                const float errSq
                    = ((pDst[i].x - predX) * (pDst[i].x - predX)) + ((pDst[i].y - predY) * (pDst[i].y - predY));
                bestMask[i] = (errSq < tSq);
            }
        }
    }

    // Compute mean reprojection error of consensus inliers
    float sumError = 0.0F;
    for (std::size_t i = 0U; i < validCount; ++i) {
        if (bestMask[i]) {
            const float predX = (bestTransform.a11 * pSrc[i].x) + (bestTransform.a12 * pSrc[i].y) + bestTransform.tx;
            const float predY = (bestTransform.a21 * pSrc[i].x) + (bestTransform.a22 * pSrc[i].y) + bestTransform.ty;
            const float errSq
                = ((pDst[i].x - predX) * (pDst[i].x - predX)) + ((pDst[i].y - predY) * (pDst[i].y - predY));
            sumError += std::sqrt(errSq);
        }
    }

    outResult.transform = bestTransform;
    outResult.inlierCount = bestInlierCount;
    outResult.inlierRatio = static_cast<float>(bestInlierCount) / static_cast<float>(validCount);
    outResult.meanInlierError = (bestInlierCount > 0U) ? (sumError / static_cast<float>(bestInlierCount)) : 0.0F;
    outResult.msacCost = bestCost;
    outResult.isDegenerate = false;
    outResult.isSuccess = true;

    return true;
}

} // namespace Stabilizer
