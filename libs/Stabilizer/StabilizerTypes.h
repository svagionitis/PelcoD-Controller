/// @file StabilizerTypes.h
/// @brief Strongly-typed domain model and value objects for video stabilization.
#pragma once

#include <cstddef>
#include <cstdint>

namespace Stabilizer {

/// @brief Video frame pixel format representation.
enum class PixelFormat : std::uint8_t {
    Grayscale8 = 0U, ///< 8-bit luma (Y)
    RGB24, ///< Packed 24-bit RGB
    RGBA32, ///< Packed 32-bit RGBA
    YUV420p ///< Planar YUV (Y evaluated directly)
};

/// @brief Operational stabilization mode.
enum class StabilizationMode : std::uint8_t {
    Disabled = 0U,
    TranslationOnly, ///< 2-DOF (X, Y translation)
    TranslationRotation, ///< 3-DOF Rigid (X, Y, Theta)
    Similarity, ///< 4-DOF (X, Y, Theta, Scale)
    Affine, ///< 6-DOF (X, Y, Theta, Scale, Shear)
    RollingShutterComp ///< Affine with row-wise correction
};

/// @brief Strongly-typed image dimension container enforcing invariant bounds.
class FrameDimensions {
public:
    constexpr FrameDimensions() noexcept = default;

    /// @brief Construct frame dimensions with optional stride.
    /// @param width Image width in pixels.
    /// @param height Image height in pixels.
    /// @param stride Row stride in bytes (defaults to width if 0).
    constexpr FrameDimensions(std::uint32_t width, std::uint32_t height, std::uint32_t stride = 0U) noexcept
        : width_ { width }
        , height_ { height }
        , stride_ { (stride == 0U) ? width : stride }
    {
    }

    /// @brief Image width in pixels.
    [[nodiscard]] constexpr std::uint32_t width() const noexcept
    {
        return width_;
    }

    /// @brief Image height in pixels.
    [[nodiscard]] constexpr std::uint32_t height() const noexcept
    {
        return height_;
    }

    /// @brief Row stride in bytes.
    [[nodiscard]] constexpr std::uint32_t stride() const noexcept
    {
        return stride_;
    }

    /// @brief Total byte buffer size required.
    [[nodiscard]] constexpr std::size_t totalBytes() const noexcept
    {
        return static_cast<std::size_t>(stride_) * static_cast<std::size_t>(height_);
    }

    /// @brief Check whether dimensions are within supported limits.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return (width_ >= 16U) && (width_ <= 7680U) && (height_ >= 16U) && (height_ <= 4320U) && (stride_ >= width_);
    }

    [[nodiscard]] constexpr bool operator==(const FrameDimensions& other) const noexcept
    {
        return (width_ == other.width_) && (height_ == other.height_) && (stride_ == other.stride_);
    }

    [[nodiscard]] constexpr bool operator!=(const FrameDimensions& other) const noexcept
    {
        return !(*this == other);
    }

private:
    std::uint32_t width_ { 0U };
    std::uint32_t height_ { 0U };
    std::uint32_t stride_ { 0U };
};

/// @brief Sub-pixel 2D point representation.
struct SubPixelPoint {
    float x { 0.0F };
    float y { 0.0F };

    /// @brief Check if point has non-negative coordinates.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return (x >= 0.0F) && (y >= 0.0F);
    }

    /// @brief Squared Euclidean distance to another point.
    [[nodiscard]] float distanceSquared(const SubPixelPoint& other) const noexcept
    {
        const float dx = x - other.x;
        const float dy = y - other.y;
        return (dx * dx) + (dy * dy);
    }
};

/// @brief Inter-frame 2D displacement vector.
struct MotionVector {
    float dx { 0.0F };
    float dy { 0.0F };

    /// @brief Squared magnitude of motion vector.
    [[nodiscard]] float magnitudeSquared() const noexcept
    {
        return (dx * dx) + (dy * dy);
    }
};

/// @brief Invariant-enforced margin crop ratio [0.0 .. 0.30].
class CropRatio {
public:
    constexpr CropRatio() noexcept = default;

    /// @brief Construct crop ratio clamped within allowable bounds.
    /// @param val Fractional ratio of frame dimension.
    explicit constexpr CropRatio(float val) noexcept
        : value_ { (val < 0.0F) ? 0.0F : ((val > 0.30F) ? 0.30F : val) }
    {
    }

    /// @brief Underlying float value.
    [[nodiscard]] constexpr float value() const noexcept
    {
        return value_;
    }

private:
    float value_ { 0.05F }; // Default 5% border margin
};

/// @brief Invariant-enforced trajectory smoothing factor [0.01 .. 1.0].
class SmoothingFactor {
public:
    constexpr SmoothingFactor() noexcept = default;

    /// @brief Construct smoothing factor clamped within [0.01 .. 1.0].
    /// @param val Filter momentum or response coefficient.
    explicit constexpr SmoothingFactor(float val) noexcept
        : value_ { (val < 0.01F) ? 0.01F : ((val > 1.0F) ? 1.0F : val) }
    {
    }

    /// @brief Underlying float value.
    [[nodiscard]] constexpr float value() const noexcept
    {
        return value_;
    }

private:
    float value_ { 0.85F }; // Default 85% momentum
};

/// @brief Affine 2D Transformation (2x3 Matrix).
/// Represented as:
/// [ a11  a12  tx ]
/// [ a21  a22  ty ]
struct AffineTransform2D {
    float a11 { 1.0F };
    float a12 { 0.0F };
    float tx { 0.0F };
    float a21 { 0.0F };
    float a22 { 1.0F };
    float ty { 0.0F };

    /// @brief Return identity affine transformation.
    [[nodiscard]] static constexpr AffineTransform2D identity() noexcept
    {
        return { 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F };
    }

    /// @brief Construct pure translation affine transformation.
    [[nodiscard]] static constexpr AffineTransform2D translation(float dx, float dy) noexcept
    {
        return { 1.0F, 0.0F, dx, 0.0F, 1.0F, dy };
    }

    /// @brief Transform a sub-pixel point using this matrix.
    [[nodiscard]] SubPixelPoint transform(const SubPixelPoint& pt) const noexcept
    {
        return { (a11 * pt.x) + (a12 * pt.y) + tx, (a21 * pt.x) + (a22 * pt.y) + ty };
    }

    /// @brief Compose transformation with next: result = next * this.
    [[nodiscard]] AffineTransform2D compose(const AffineTransform2D& next) const noexcept
    {
        return { (next.a11 * a11) + (next.a12 * a21), (next.a11 * a12) + (next.a12 * a22),
            (next.a11 * tx) + (next.a12 * ty) + next.tx, (next.a21 * a11) + (next.a22 * a21),
            (next.a21 * a12) + (next.a22 * a22), (next.a21 * tx) + (next.a22 * ty) + next.ty };
    }

    /// @brief Invert the affine transformation analytically.
    /// @param out Receives the inverted matrix if non-singular.
    /// @return true if invertible; false if determinant is near zero.
    [[nodiscard]] bool invert(AffineTransform2D& out) const noexcept
    {
        const float det = (a11 * a22) - (a12 * a21);
        if ((det > -1e-6F) && (det < 1e-6F)) {
            return false;
        }
        const float invDet = 1.0F / det;
        out.a11 = a22 * invDet;
        out.a12 = -a12 * invDet;
        out.tx = ((a12 * ty) - (a22 * tx)) * invDet;
        out.a21 = -a21 * invDet;
        out.a22 = a11 * invDet;
        out.ty = ((a21 * tx) - (a11 * ty)) * invDet;
        return true;
    }
};

/// @brief Runtime stabilization performance and tracking metrics.
struct StabilizerStatus {
    std::uint32_t trackedFeatures { 0U };
    std::uint32_t inlierMatches { 0U };
    float confidence { 0.0F };
    float currentJitterPixels { 0.0F };
    std::uint32_t processingTimeUs { 0U };
    bool isDegenerate { false };
};

} // namespace Stabilizer
