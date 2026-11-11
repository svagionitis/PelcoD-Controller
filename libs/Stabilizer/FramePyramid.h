/// @file FramePyramid.h
/// @brief Multi-scale Gaussian image pyramid engine for coarse-to-fine tracking.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "FrameBuffer.h"
#include "FrameView.h"
#include "StabilizerTypes.h"

namespace Stabilizer {

/// @brief Pre-allocated 3-level Gaussian image pyramid downsampler.
class FramePyramid {
public:
    static constexpr std::uint8_t MaxPyramidLevels = 3U;

    FramePyramid() noexcept = default;
    explicit FramePyramid(FrameDimensions baseDims);

    /// @brief Construct multi-scale pyramid from an input frame view.
    /// @param baseView Full-resolution level-0 grayscale frame.
    /// @return true if pyramid constructed cleanly; false on dimension mismatch.
    [[nodiscard]] bool build(const FrameView& baseView) noexcept;

    /// @brief Access individual pyramid level (0=Full, 1=Half, 2=Quarter).
    /// @param levelIdx Level index [0 .. MaxPyramidLevels-1].
    [[nodiscard]] FrameView level(std::uint8_t levelIdx) const noexcept;

    /// @brief Number of active pyramid levels.
    [[nodiscard]] std::uint8_t levelCount() const noexcept
    {
        return numLevels_;
    }

    /// @brief Base level dimensions.
    [[nodiscard]] const FrameDimensions& baseDimensions() const noexcept
    {
        return baseDims_;
    }

    /// @brief Dimensions for a specific pyramid level.
    [[nodiscard]] FrameDimensions levelDimensions(std::uint8_t levelIdx) const noexcept;

private:
    void downsampleLevel(const FrameView& src, FrameBuffer& dst, FrameBuffer& temp) noexcept;

    FrameDimensions baseDims_ { };
    std::uint8_t numLevels_ { MaxPyramidLevels };
    FrameBuffer storageLevel1_ { };
    FrameBuffer storageLevel2_ { };
    FrameBuffer tempBuffer1_ { };
    FrameBuffer tempBuffer2_ { };
    std::array<FrameView, MaxPyramidLevels> views_ { };
};

} // namespace Stabilizer
