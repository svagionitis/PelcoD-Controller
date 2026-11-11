/// @file WarpEngine.cpp
/// @brief High-throughput SIMD image warping and rolling shutter compensation implementation.
#include "WarpEngine.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <cmath>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace Stabilizer {

namespace {

    /// @brief Clamp coordinate to valid image boundary [0 .. maxVal - 1].
    [[nodiscard]] inline constexpr std::int32_t clampCoord(std::int32_t val, std::int32_t maxVal) noexcept
    {
        return (val < 0) ? 0 : ((val >= maxVal) ? (maxVal - 1) : val);
    }

    /// @brief Symmetrically reflect coordinate across image boundary.
    [[nodiscard]] inline std::int32_t reflectCoord(std::int32_t val, std::int32_t maxVal) noexcept
    {
        if (maxVal <= 1) {
            return 0;
        }
        std::int32_t v = val;
        while ((v < 0) || (v >= maxVal)) {
            if (v < 0) {
                v = -v;
            } else if (v >= maxVal) {
                v = (2 * (maxVal - 1)) - v;
            }
        }
        return v;
    }

    /// @brief Sample a single pixel according to border handling mode.
    [[nodiscard]] inline std::uint8_t samplePixel(
        const FrameView& view, std::int32_t x, std::int32_t y, BorderMode mode, std::uint8_t borderVal) noexcept
    {
        const std::int32_t w = static_cast<std::int32_t>(view.width());
        const std::int32_t h = static_cast<std::int32_t>(view.height());

        if ((x >= 0) && (x < w) && (y >= 0) && (y < h)) {
            return *(view.rowPtr(static_cast<std::uint32_t>(y)) + static_cast<std::uint32_t>(x));
        }

        if (mode == BorderMode::Constant) {
            return borderVal;
        }
        if (mode == BorderMode::Replicate) {
            const std::int32_t cx = clampCoord(x, w);
            const std::int32_t cy = clampCoord(y, h);
            return *(view.rowPtr(static_cast<std::uint32_t>(cy)) + static_cast<std::uint32_t>(cx));
        }
        // BorderMode::Reflect
        const std::int32_t rx = reflectCoord(x, w);
        const std::int32_t ry = reflectCoord(y, h);
        return *(view.rowPtr(static_cast<std::uint32_t>(ry)) + static_cast<std::uint32_t>(rx));
    }

    /// @brief 1D Catmull-Rom cubic Hermite spline interpolation weight calculation.
    [[nodiscard]] inline float catmullRomWeight(float t) noexcept
    {
        const float at = std::abs(t);
        if (at <= 1.0F) {
            return (1.5F * at * at * at) - (2.5F * at * at) + 1.0F;
        }
        if (at < 2.0F) {
            return (-0.5F * at * at * at) + (2.5F * at * at) - (4.0F * at) + 2.0F;
        }
        return 0.0F;
    }

    /// @brief Bicubic Catmull-Rom interpolation for a single continuous coordinate.
    [[nodiscard]] inline std::uint8_t interpolateBicubic(
        const FrameView& view, float xs, float ys, BorderMode mode, std::uint8_t borderVal) noexcept
    {
        const std::int32_t xi = static_cast<std::int32_t>(std::floor(xs));
        const std::int32_t yi = static_cast<std::int32_t>(std::floor(ys));
        const float fx = xs - static_cast<float>(xi);
        const float fy = ys - static_cast<float>(yi);

        float sum = 0.0F;
        for (std::int32_t m = -1; m <= 2; ++m) {
            const float wy = catmullRomWeight(static_cast<float>(m) - fy);
            for (std::int32_t n = -1; n <= 2; ++n) {
                const float wx = catmullRomWeight(static_cast<float>(n) - fx);
                const std::uint8_t p = samplePixel(view, xi + n, yi + m, mode, borderVal);
                sum += static_cast<float>(p) * wx * wy;
            }
        }

        const std::int32_t rounded = static_cast<std::int32_t>(std::round(sum));
        return static_cast<std::uint8_t>(clampCoord(rounded, 256));
    }

} // namespace

WarpEngine::WarpEngine() noexcept
    : config_ { }
{
}

WarpEngine::WarpEngine(const WarpConfig& config) noexcept
    : config_ { config }
{
}

void WarpEngine::setConfig(const WarpConfig& config) noexcept
{
    config_ = config;
}

const WarpConfig& WarpEngine::config() const noexcept
{
    return config_;
}

AffineTransform2D WarpEngine::composeZoom(
    const AffineTransform2D& transform, const FrameDimensions& dims, const CropRatio& crop) noexcept
{
    const float mu = crop.value();
    if (mu <= 1e-6F) {
        return transform;
    }
    const float kappa = 1.0F / (1.0F - (2.0F * mu));
    const float cx = static_cast<float>(dims.width()) * 0.5F;
    const float cy = static_cast<float>(dims.height()) * 0.5F;
    const AffineTransform2D zoom { kappa, 0.0F, (1.0F - kappa) * cx, 0.0F, kappa, (1.0F - kappa) * cy };
    return zoom.compose(transform);
}

bool WarpEngine::warpFrame(const FrameView& src, FrameView& dst, const AffineTransform2D& transform) noexcept
{
    if (src.isEmpty() || dst.isEmpty()) {
        return false;
    }
    if ((src.width() != dst.width()) || (src.height() != dst.height())) {
        return false;
    }

    AffineTransform2D forward = transform;
    if (config_.enableAutoCrop) {
        forward = composeZoom(forward, dst.dimensions(), config_.cropRatio);
    }

    AffineTransform2D invWarp { AffineTransform2D::identity() };
    if (!forward.invert(invWarp)) {
        invWarp = AffineTransform2D::identity();
    }

    const std::uint32_t height = dst.height();
    const std::uint32_t width = dst.width();

    for (std::uint32_t y = 0U; y < height; ++y) {
        std::uint8_t* dstRow = const_cast<std::uint8_t*>(dst.rowPtr(y));

#if defined(__AVX2__)
        warpScanlineAvx2(src, dstRow, y, width, invWarp);
#elif defined(__ARM_NEON)
        warpScanlineNeon(src, dstRow, y, width, invWarp);
#else
        warpScanlineScalar(src, dstRow, y, width, invWarp);
#endif
    }

    return true;
}

bool WarpEngine::warpRollingShutter(const FrameView& src, FrameView& dst, const AffineTransform2D& currentTransform,
    const AffineTransform2D& nextTransform) noexcept
{
    if (src.isEmpty() || dst.isEmpty()) {
        return false;
    }
    if ((src.width() != dst.width()) || (src.height() != dst.height())) {
        return false;
    }

    const std::uint32_t height = dst.height();
    const std::uint32_t width = dst.width();
    const float readoutRatio = config_.rsProfile.readoutRatio.value();

    for (std::uint32_t y = 0U; y < height; ++y) {
        const float normY = (height > 1U) ? (static_cast<float>(y) / static_cast<float>(height - 1U)) : 0.0F;
        const float scanFrac = config_.rsProfile.scanTopToBottom ? normY : (1.0F - normY);
        const float alpha = scanFrac * readoutRatio;
        const float oneMinusAlpha = 1.0F - alpha;

        AffineTransform2D rowFwd { (oneMinusAlpha * currentTransform.a11) + (alpha * nextTransform.a11),
            (oneMinusAlpha * currentTransform.a12) + (alpha * nextTransform.a12),
            (oneMinusAlpha * currentTransform.tx) + (alpha * nextTransform.tx),
            (oneMinusAlpha * currentTransform.a21) + (alpha * nextTransform.a21),
            (oneMinusAlpha * currentTransform.a22) + (alpha * nextTransform.a22),
            (oneMinusAlpha * currentTransform.ty) + (alpha * nextTransform.ty) };

        if (config_.enableAutoCrop) {
            rowFwd = composeZoom(rowFwd, dst.dimensions(), config_.cropRatio);
        }

        AffineTransform2D invWarp { AffineTransform2D::identity() };
        if (!rowFwd.invert(invWarp)) {
            invWarp = AffineTransform2D::identity();
        }

        std::uint8_t* dstRow = const_cast<std::uint8_t*>(dst.rowPtr(y));

#if defined(__AVX2__)
        warpScanlineAvx2(src, dstRow, y, width, invWarp);
#elif defined(__ARM_NEON)
        warpScanlineNeon(src, dstRow, y, width, invWarp);
#else
        warpScanlineScalar(src, dstRow, y, width, invWarp);
#endif
    }

    return true;
}

bool WarpEngine::warpYuv420p(const FrameView& srcY, const FrameView& srcU, const FrameView& srcV, FrameView& dstY,
    FrameView& dstU, FrameView& dstV, const AffineTransform2D& transform) noexcept
{
    if (srcY.isEmpty() || srcU.isEmpty() || srcV.isEmpty() || dstY.isEmpty() || dstU.isEmpty() || dstV.isEmpty()) {
        return false;
    }
    if ((srcY.width() != dstY.width()) || (srcY.height() != dstY.height())) {
        return false;
    }
    if ((srcU.width() != dstU.width()) || (srcU.height() != dstU.height()) || (srcV.width() != dstV.width())
        || (srcV.height() != dstV.height())) {
        return false;
    }

    // 1. Warp full-resolution luma (Y) plane
    if (!warpFrame(srcY, dstY, transform)) {
        return false;
    }

    // 2. Warp chroma (U and V) planes with coordinate-scaled translation
    const AffineTransform2D chromaTransform { transform.a11, transform.a12, transform.tx * 0.5F, transform.a21,
        transform.a22, transform.ty * 0.5F };

    if (!warpFrame(srcU, dstU, chromaTransform)) {
        return false;
    }
    if (!warpFrame(srcV, dstV, chromaTransform)) {
        return false;
    }

    return true;
}

void WarpEngine::warpScanlineScalar(const FrameView& src, std::uint8_t* dstRow, std::uint32_t y, std::uint32_t width,
    const AffineTransform2D& invWarp) const noexcept
{
    const float yd = static_cast<float>(y);
    const float x0 = (invWarp.a12 * yd) + invWarp.tx;
    const float y0 = (invWarp.a22 * yd) + invWarp.ty;
    const float dx = invWarp.a11;
    const float dy = invWarp.a21;

    const BorderMode mode = config_.borderMode;
    const std::uint8_t borderVal = config_.borderValue;
    const std::int32_t w = static_cast<std::int32_t>(src.width());
    const std::int32_t h = static_cast<std::int32_t>(src.height());

    if (config_.interpolation == WarpInterpolation::NearestNeighbor) {
        constexpr float scale16 = 65536.0F;
        std::int32_t curX = static_cast<std::int32_t>(x0 * scale16);
        std::int32_t curY = static_cast<std::int32_t>(y0 * scale16);
        const std::int32_t stepX = static_cast<std::int32_t>(dx * scale16);
        const std::int32_t stepY = static_cast<std::int32_t>(dy * scale16);

        for (std::uint32_t x = 0U; x < width; ++x) {
            const std::int32_t xi = (curX + 32768) >> 16;
            const std::int32_t yi = (curY + 32768) >> 16;
            dstRow[x] = samplePixel(src, xi, yi, mode, borderVal);
            curX += stepX;
            curY += stepY;
        }
        return;
    }

    if (config_.interpolation == WarpInterpolation::Bicubic) {
        float curX = x0;
        float curY = y0;
        for (std::uint32_t x = 0U; x < width; ++x) {
            dstRow[x] = interpolateBicubic(src, curX, curY, mode, borderVal);
            curX += dx;
            curY += dy;
        }
        return;
    }

    // Default: Bilinear Interpolation using Fixed-Point 16.16 DDA
    constexpr float scale16 = 65536.0F;
    std::int32_t curX = static_cast<std::int32_t>(x0 * scale16);
    std::int32_t curY = static_cast<std::int32_t>(y0 * scale16);
    const std::int32_t stepX = static_cast<std::int32_t>(dx * scale16);
    const std::int32_t stepY = static_cast<std::int32_t>(dy * scale16);

    for (std::uint32_t x = 0U; x < width; ++x) {
        const std::int32_t xi = curX >> 16;
        const std::int32_t yi = curY >> 16;
        const std::int32_t wx = (curX >> 8) & 0xFF;
        const std::int32_t wy = (curY >> 8) & 0xFF;

        std::int32_t p00 = 0;
        std::int32_t p10 = 0;
        std::int32_t p01 = 0;
        std::int32_t p11 = 0;

        if ((xi >= 0) && (xi + 1 < w) && (yi >= 0) && (yi + 1 < h)) {
            const std::uint8_t* r0 = src.rowPtr(static_cast<std::uint32_t>(yi));
            const std::uint8_t* r1 = src.rowPtr(static_cast<std::uint32_t>(yi + 1));
            p00 = r0[xi];
            p10 = r0[xi + 1];
            p01 = r1[xi];
            p11 = r1[xi + 1];
        } else {
            p00 = samplePixel(src, xi, yi, mode, borderVal);
            p10 = samplePixel(src, xi + 1, yi, mode, borderVal);
            p01 = samplePixel(src, xi, yi + 1, mode, borderVal);
            p11 = samplePixel(src, xi + 1, yi + 1, mode, borderVal);
        }

        const std::int32_t w00 = (256 - wx) * (256 - wy);
        const std::int32_t w10 = wx * (256 - wy);
        const std::int32_t w01 = (256 - wx) * wy;
        const std::int32_t w11 = wx * wy;

        const std::int32_t val = (w00 * p00) + (w10 * p10) + (w01 * p01) + (w11 * p11) + 32768;
        dstRow[x] = static_cast<std::uint8_t>(val >> 16);

        curX += stepX;
        curY += stepY;
    }
}

void WarpEngine::warpScanlineAvx2(const FrameView& src, std::uint8_t* dstRow, std::uint32_t y, std::uint32_t width,
    const AffineTransform2D& invWarp) const noexcept
{
#if defined(__AVX2__)
    if (config_.interpolation != WarpInterpolation::Bilinear) {
        warpScanlineScalar(src, dstRow, y, width, invWarp);
        return;
    }

    const float yd = static_cast<float>(y);
    const float x0 = (invWarp.a12 * yd) + invWarp.tx;
    const float y0 = (invWarp.a22 * yd) + invWarp.ty;
    const float dx = invWarp.a11;
    const float dy = invWarp.a21;

    constexpr float scale16 = 65536.0F;
    std::int32_t curX = static_cast<std::int32_t>(x0 * scale16);
    std::int32_t curY = static_cast<std::int32_t>(y0 * scale16);
    const std::int32_t stepX = static_cast<std::int32_t>(dx * scale16);
    const std::int32_t stepY = static_cast<std::int32_t>(dy * scale16);

    const std::int32_t w = static_cast<std::int32_t>(src.width());
    const std::int32_t h = static_cast<std::int32_t>(src.height());

    std::uint32_t x = 0U;
    const std::uint32_t simdEnd = (width >= 8U) ? (width - 7U) : 0U;

    const __m256i stepVecX
        = _mm256_setr_epi32(0, stepX, 2 * stepX, 3 * stepX, 4 * stepX, 5 * stepX, 6 * stepX, 7 * stepX);
    const __m256i stepVecY
        = _mm256_setr_epi32(0, stepY, 2 * stepY, 3 * stepY, 4 * stepY, 5 * stepY, 6 * stepY, 7 * stepY);
    const __m256i c256 = _mm256_set1_epi32(256);
    const __m256i c32768 = _mm256_set1_epi32(32768);
    const __m256i mask0xFF = _mm256_set1_epi32(0xFF);

    while (x < simdEnd) {
        const __m256i vecBaseX = _mm256_set1_epi32(curX);
        const __m256i vecBaseY = _mm256_set1_epi32(curY);
        const __m256i vecX = _mm256_add_epi32(vecBaseX, stepVecX);
        const __m256i vecY = _mm256_add_epi32(vecBaseY, stepVecY);

        const __m256i vecXi = _mm256_srai_epi32(vecX, 16);
        const __m256i vecYi = _mm256_srai_epi32(vecY, 16);

        // Check if all 8 coordinates fall completely within the safe interior
        alignas(32) std::int32_t arrXi[8];
        alignas(32) std::int32_t arrYi[8];
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(arrXi), vecXi);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(arrYi), vecYi);

        bool safe = true;
        for (std::size_t i = 0U; i < 8U; ++i) {
            if ((arrXi[i] < 0) || (arrXi[i] + 1 >= w) || (arrYi[i] < 0) || (arrYi[i] + 1 >= h)) {
                safe = false;
                break;
            }
        }

        if (safe) {
            const __m256i vecWx = _mm256_and_si256(_mm256_srli_epi32(vecX, 8), mask0xFF);
            const __m256i vecWy = _mm256_and_si256(_mm256_srli_epi32(vecY, 8), mask0xFF);
            const __m256i vecInvWx = _mm256_sub_epi32(c256, vecWx);
            const __m256i vecInvWy = _mm256_sub_epi32(c256, vecWy);

            const __m256i w00 = _mm256_mullo_epi32(vecInvWx, vecInvWy);
            const __m256i w10 = _mm256_mullo_epi32(vecWx, vecInvWy);
            const __m256i w01 = _mm256_mullo_epi32(vecInvWx, vecWy);
            const __m256i w11 = _mm256_mullo_epi32(vecWx, vecWy);

            alignas(32) std::int32_t p00[8];
            alignas(32) std::int32_t p10[8];
            alignas(32) std::int32_t p01[8];
            alignas(32) std::int32_t p11[8];

            for (std::size_t i = 0U; i < 8U; ++i) {
                const std::uint8_t* r0 = src.rowPtr(static_cast<std::uint32_t>(arrYi[i]));
                const std::uint8_t* r1 = src.rowPtr(static_cast<std::uint32_t>(arrYi[i] + 1));
                p00[i] = r0[arrXi[i]];
                p10[i] = r0[arrXi[i] + 1];
                p01[i] = r1[arrXi[i]];
                p11[i] = r1[arrXi[i] + 1];
            }

            const __m256i vecP00 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p00));
            const __m256i vecP10 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p10));
            const __m256i vecP01 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p01));
            const __m256i vecP11 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p11));

            const __m256i t0 = _mm256_mullo_epi32(w00, vecP00);
            const __m256i t1 = _mm256_mullo_epi32(w10, vecP10);
            const __m256i t2 = _mm256_mullo_epi32(w01, vecP01);
            const __m256i t3 = _mm256_mullo_epi32(w11, vecP11);

            const __m256i sum0 = _mm256_add_epi32(t0, t1);
            const __m256i sum1 = _mm256_add_epi32(t2, t3);
            const __m256i sumAll = _mm256_add_epi32(_mm256_add_epi32(sum0, sum1), c32768);
            const __m256i resVal = _mm256_srli_epi32(sumAll, 16);

            alignas(32) std::int32_t arrOut[8];
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(arrOut), resVal);

            for (std::size_t i = 0U; i < 8U; ++i) {
                dstRow[x + i] = static_cast<std::uint8_t>(arrOut[i]);
            }
        } else {
            // Boundary fallback for this 8-pixel batch
            const BorderMode mode = config_.borderMode;
            const std::uint8_t borderVal = config_.borderValue;
            for (std::size_t i = 0U; i < 8U; ++i) {
                const std::int32_t xi = arrXi[i];
                const std::int32_t yi = arrYi[i];
                const std::int32_t wx
                    = (arrXi[i] >= 0) ? (((curX + (static_cast<std::int32_t>(i) * stepX)) >> 8) & 0xFF) : 0;
                const std::int32_t wy
                    = (arrYi[i] >= 0) ? (((curY + (static_cast<std::int32_t>(i) * stepY)) >> 8) & 0xFF) : 0;

                const std::int32_t p00 = samplePixel(src, xi, yi, mode, borderVal);
                const std::int32_t p10 = samplePixel(src, xi + 1, yi, mode, borderVal);
                const std::int32_t p01 = samplePixel(src, xi, yi + 1, mode, borderVal);
                const std::int32_t p11 = samplePixel(src, xi + 1, yi + 1, mode, borderVal);

                const std::int32_t w00 = (256 - wx) * (256 - wy);
                const std::int32_t w10 = wx * (256 - wy);
                const std::int32_t w01 = (256 - wx) * wy;
                const std::int32_t w11 = wx * wy;

                const std::int32_t val = (w00 * p00) + (w10 * p10) + (w01 * p01) + (w11 * p11) + 32768;
                dstRow[x + i] = static_cast<std::uint8_t>(val >> 16);
            }
        }

        curX += 8 * stepX;
        curY += 8 * stepY;
        x += 8U;
    }

    // Scalar tail processing
    while (x < width) {
        const std::int32_t xi = curX >> 16;
        const std::int32_t yi = curY >> 16;
        const std::int32_t wx = (curX >> 8) & 0xFF;
        const std::int32_t wy = (curY >> 8) & 0xFF;

        const BorderMode mode = config_.borderMode;
        const std::uint8_t borderVal = config_.borderValue;

        const std::int32_t p00 = samplePixel(src, xi, yi, mode, borderVal);
        const std::int32_t p10 = samplePixel(src, xi + 1, yi, mode, borderVal);
        const std::int32_t p01 = samplePixel(src, xi, yi + 1, mode, borderVal);
        const std::int32_t p11 = samplePixel(src, xi + 1, yi + 1, mode, borderVal);

        const std::int32_t w00 = (256 - wx) * (256 - wy);
        const std::int32_t w10 = wx * (256 - wy);
        const std::int32_t w01 = (256 - wx) * wy;
        const std::int32_t w11 = wx * wy;

        const std::int32_t val = (w00 * p00) + (w10 * p10) + (w01 * p01) + (w11 * p11) + 32768;
        dstRow[x] = static_cast<std::uint8_t>(val >> 16);

        curX += stepX;
        curY += stepY;
        ++x;
    }
#else
    warpScanlineScalar(src, dstRow, y, width, invWarp);
#endif
}

void WarpEngine::warpScanlineNeon(const FrameView& src, std::uint8_t* dstRow, std::uint32_t y, std::uint32_t width,
    const AffineTransform2D& invWarp) const noexcept
{
#if defined(__ARM_NEON)
    // On ARM64 platforms, fallback or NEON implementation
    warpScanlineScalar(src, dstRow, y, width, invWarp);
#else
    warpScanlineScalar(src, dstRow, y, width, invWarp);
#endif
}

} // namespace Stabilizer
