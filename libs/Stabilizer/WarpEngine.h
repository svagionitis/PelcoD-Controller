/// @file WarpEngine.h
/// @brief SIMD-accelerated geometric warp synthesis and rolling shutter compensation.
#pragma once

#include <cstddef>
#include <cstdint>

#include "FrameView.h"
#include "StabilizerTypes.h"

namespace Stabilizer {

/// @brief Sub-pixel interpolation algorithm selector.
enum class WarpInterpolation : std::uint8_t {
    NearestNeighbor = 0U, ///< Zero-order hold nearest neighbor (fast preview)
    Bilinear, ///< First-order bilinear interpolation (default)
    Bicubic ///< Third-order Catmull-Rom cubic spline (high sharpness)
};

/// @brief Boundary handling policy for coordinates outside source frame.
enum class BorderMode : std::uint8_t {
    Constant = 0U, ///< Fill out-of-bounds pixels with constant value
    Replicate, ///< Clamp out-of-bounds coordinates to nearest border pixel
    Reflect ///< Mirror coordinates symmetrically across border
};

/// @brief Invariant-enforced CMOS sensor readout ratio [0.0 .. 1.0].
class ReadoutRatio {
public:
    constexpr ReadoutRatio() noexcept = default;

    /// @brief Construct readout ratio clamped to [0.0 .. 1.0].
    /// @param val Ratio of readout duration to total frame interval.
    explicit constexpr ReadoutRatio(float val) noexcept
        : value_ { (val < 0.0F) ? 0.0F : ((val > 1.0F) ? 1.0F : val) }
    {
    }

    /// @brief Underlying float ratio.
    [[nodiscard]] constexpr float value() const noexcept
    {
        return value_;
    }

private:
    float value_ { 0.80F }; ///< Default 80% rolling shutter readout duty cycle
};

/// @brief CMOS rolling shutter hardware timing profile.
struct RollingShutterProfile {
    ReadoutRatio readoutRatio { 0.80F }; ///< Ratio of scanline readout to frame time
    bool scanTopToBottom { true }; ///< Scanline direction (true: top->bottom)

    /// @brief Check whether profile values are valid.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return (readoutRatio.value() >= 0.0F) && (readoutRatio.value() <= 1.0F);
    }
};

/// @brief Comprehensive configuration parameters for WarpEngine.
struct WarpConfig {
    WarpInterpolation interpolation { WarpInterpolation::Bilinear };
    BorderMode borderMode { BorderMode::Constant };
    std::uint8_t borderValue { 0U }; ///< Constant border luma value (0 = black)
    CropRatio cropRatio { 0.05F }; ///< Auto-crop zoom ratio [0.0 .. 0.30]
    bool enableAutoCrop { true }; ///< Apply dynamic canvas zoom to hide borders
    bool enableRollingShutter { false }; ///< Enable scanline-dependent transform interpolation
    RollingShutterProfile rsProfile { }; ///< Hardware rolling shutter parameters

    /// @brief Check whether configuration parameters are valid.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return rsProfile.isValid();
    }
};

/// @brief High-throughput SIMD image warping and rolling shutter compensation engine.
class WarpEngine {
public:
    /// @brief Construct warp engine with default configuration.
    WarpEngine() noexcept;

    /// @brief Construct warp engine with explicit configuration.
    /// @param config Engine configuration parameters.
    explicit WarpEngine(const WarpConfig& config) noexcept;

    ~WarpEngine() noexcept = default;

    WarpEngine(const WarpEngine&) = delete;
    WarpEngine& operator=(const WarpEngine&) = delete;
    WarpEngine(WarpEngine&&) noexcept = default;
    WarpEngine& operator=(WarpEngine&&) noexcept = default;

    /// @brief Warp a grayscale luma frame using an affine stabilizing transform.
    /// @param src Non-owning view of raw input frame.
    /// @param dst Non-owning view of pre-allocated destination frame buffer.
    /// @param transform Stabilizing affine transformation matrix (forward).
    /// @return true on success; false if dimensions mismatch or views empty.
    [[nodiscard]] bool warpFrame(const FrameView& src, FrameView& dst, const AffineTransform2D& transform) noexcept;

    /// @brief Warp a grayscale luma frame with rolling shutter jello compensation.
    /// @param src Non-owning view of raw input frame.
    /// @param dst Non-owning view of pre-allocated destination frame buffer.
    /// @param currentTransform Stabilizing transform for current frame.
    /// @param nextTransform Stabilizing transform for subsequent frame.
    /// @return true on success; false if dimensions mismatch or views empty.
    [[nodiscard]] bool warpRollingShutter(const FrameView& src, FrameView& dst,
        const AffineTransform2D& currentTransform, const AffineTransform2D& nextTransform) noexcept;

    /// @brief Warp planar YUV420p video frame with coordinate-aligned chroma channels.
    /// @param srcY View of input luma (Y) plane (W x H).
    /// @param srcU View of input chroma U plane (W/2 x H/2).
    /// @param srcV View of input chroma V plane (W/2 x H/2).
    /// @param dstY View of output luma (Y) plane (W x H).
    /// @param dstU View of output chroma U plane (W/2 x H/2).
    /// @param dstV View of output chroma V plane (W/2 x H/2).
    /// @param transform Stabilizing affine transformation.
    /// @return true on success; false if dimensions mismatch.
    [[nodiscard]] bool warpYuv420p(const FrameView& srcY, const FrameView& srcU, const FrameView& srcV, FrameView& dstY,
        FrameView& dstU, FrameView& dstV, const AffineTransform2D& transform) noexcept;

    /// @brief Update runtime engine configuration.
    /// @param config New configuration parameters.
    void setConfig(const WarpConfig& config) noexcept;

    /// @brief Active configuration parameters.
    [[nodiscard]] const WarpConfig& config() const noexcept;

    /// @brief Compose stabilizing transformation with auto-crop centering zoom.
    /// @param transform Base stabilizing affine transform.
    /// @param dims Image dimensions.
    /// @param crop Margin crop ratio.
    /// @return Composed affine transformation incorporating zoom.
    [[nodiscard]] static AffineTransform2D composeZoom(
        const AffineTransform2D& transform, const FrameDimensions& dims, const CropRatio& crop) noexcept;

private:
    WarpConfig config_ { };

    void warpScanlineScalar(const FrameView& src, std::uint8_t* dstRow, std::uint32_t y, std::uint32_t width,
        const AffineTransform2D& invWarp) const noexcept;

    void warpScanlineAvx2(const FrameView& src, std::uint8_t* dstRow, std::uint32_t y, std::uint32_t width,
        const AffineTransform2D& invWarp) const noexcept;

    void warpScanlineNeon(const FrameView& src, std::uint8_t* dstRow, std::uint32_t y, std::uint32_t width,
        const AffineTransform2D& invWarp) const noexcept;
};

} // namespace Stabilizer
