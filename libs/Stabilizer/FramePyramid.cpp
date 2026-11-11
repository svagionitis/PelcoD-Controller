/// @file FramePyramid.cpp
/// @brief Multi-scale Gaussian image pyramid engine implementation.
#include "FramePyramid.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace Stabilizer {

FramePyramid::FramePyramid(FrameDimensions baseDims)
    : baseDims_ { baseDims }
    , numLevels_ { MaxPyramidLevels }
{
    if (!baseDims_.isValid()) {
        return;
    }

    const std::uint32_t w1 = baseDims_.width() / 2U;
    const std::uint32_t h1 = baseDims_.height() / 2U;
    const std::uint32_t w2 = w1 / 2U;
    const std::uint32_t h2 = h1 / 2U;

    storageLevel1_ = FrameBuffer { FrameDimensions { w1, h1 } };
    storageLevel2_ = FrameBuffer { FrameDimensions { w2, h2 } };

    // Intermediate scratch buffers for separable filter passes
    tempBuffer1_ = FrameBuffer { FrameDimensions { w1, baseDims_.height() } };
    tempBuffer2_ = FrameBuffer { FrameDimensions { w2, h1 } };
}

bool FramePyramid::build(const FrameView& baseView) noexcept
{
    if (baseView.isEmpty() || (baseView.dimensions() != baseDims_)) {
        return false;
    }
    if (!storageLevel1_.isValid() || !storageLevel2_.isValid()) {
        return false;
    }

    // Level 0 is the base full-resolution image
    views_[0] = baseView;

    // Downsample Level 0 -> Level 1
    downsampleLevel(views_[0], storageLevel1_, tempBuffer1_);
    views_[1] = storageLevel1_.view();

    // Downsample Level 1 -> Level 2
    downsampleLevel(views_[1], storageLevel2_, tempBuffer2_);
    views_[2] = storageLevel2_.view();

    return true;
}

FrameView FramePyramid::level(std::uint8_t levelIdx) const noexcept
{
    if (levelIdx >= MaxPyramidLevels) {
        return FrameView { };
    }
    return views_[levelIdx];
}

FrameDimensions FramePyramid::levelDimensions(std::uint8_t levelIdx) const noexcept
{
    if (!baseDims_.isValid()) {
        return FrameDimensions { };
    }
    if (levelIdx == 0U) {
        return baseDims_;
    }
    if (levelIdx == 1U) {
        return FrameDimensions { baseDims_.width() / 2U, baseDims_.height() / 2U };
    }
    if (levelIdx == 2U) {
        return FrameDimensions { baseDims_.width() / 4U, baseDims_.height() / 4U };
    }
    return FrameDimensions { };
}

void FramePyramid::downsampleLevel(const FrameView& src, FrameBuffer& dst, FrameBuffer& temp) noexcept
{
    const std::uint32_t srcW = src.width();
    const std::uint32_t srcH = src.height();
    const std::uint32_t dstW = dst.dimensions().width();
    const std::uint32_t dstH = dst.dimensions().height();

    // Pass 1: Horizontal 5-tap Gaussian blur + 2x decimation into temp [dstW x srcH]
    for (std::uint32_t y = 0U; y < srcH; ++y) {
        const std::uint8_t* srcRow = src.rowPtr(y);
        std::uint8_t* tempRow = temp.rowPtr(y);

        for (std::uint32_t x = 0U; x < dstW; ++x) {
            const std::int32_t cx = static_cast<std::int32_t>(x * 2U);

            const std::uint32_t xm2 = static_cast<std::uint32_t>(std::max(0, cx - 2));
            const std::uint32_t xm1 = static_cast<std::uint32_t>(std::max(0, cx - 1));
            const std::uint32_t x0 = static_cast<std::uint32_t>(cx);
            const std::uint32_t xp1
                = static_cast<std::uint32_t>(std::min(static_cast<std::int32_t>(srcW - 1U), cx + 1));
            const std::uint32_t xp2
                = static_cast<std::uint32_t>(std::min(static_cast<std::int32_t>(srcW - 1U), cx + 2));

            const std::uint32_t sum = static_cast<std::uint32_t>(srcRow[xm2])
                + (4U * static_cast<std::uint32_t>(srcRow[xm1])) + (6U * static_cast<std::uint32_t>(srcRow[x0]))
                + (4U * static_cast<std::uint32_t>(srcRow[xp1])) + static_cast<std::uint32_t>(srcRow[xp2]);

            tempRow[x] = static_cast<std::uint8_t>((sum + 8U) >> 4);
        }
    }

    // Pass 2: Vertical 5-tap Gaussian blur + 2x decimation into dst [dstW x dstH]
    for (std::uint32_t y = 0U; y < dstH; ++y) {
        const std::int32_t cy = static_cast<std::int32_t>(y * 2U);

        const std::uint32_t ym2 = static_cast<std::uint32_t>(std::max(0, cy - 2));
        const std::uint32_t ym1 = static_cast<std::uint32_t>(std::max(0, cy - 1));
        const std::uint32_t y0 = static_cast<std::uint32_t>(cy);
        const std::uint32_t yp1 = static_cast<std::uint32_t>(std::min(static_cast<std::int32_t>(srcH - 1U), cy + 1));
        const std::uint32_t yp2 = static_cast<std::uint32_t>(std::min(static_cast<std::int32_t>(srcH - 1U), cy + 2));

        const std::uint8_t* rm2 = temp.rowPtr(ym2);
        const std::uint8_t* rm1 = temp.rowPtr(ym1);
        const std::uint8_t* r0 = temp.rowPtr(y0);
        const std::uint8_t* rp1 = temp.rowPtr(yp1);
        const std::uint8_t* rp2 = temp.rowPtr(yp2);
        std::uint8_t* dstRow = dst.rowPtr(y);

        for (std::uint32_t x = 0U; x < dstW; ++x) {
            const std::uint32_t sum = static_cast<std::uint32_t>(rm2[x]) + (4U * static_cast<std::uint32_t>(rm1[x]))
                + (6U * static_cast<std::uint32_t>(r0[x])) + (4U * static_cast<std::uint32_t>(rp1[x]))
                + static_cast<std::uint32_t>(rp2[x]);

            dstRow[x] = static_cast<std::uint8_t>((sum + 8U) >> 4);
        }
    }
}

} // namespace Stabilizer
