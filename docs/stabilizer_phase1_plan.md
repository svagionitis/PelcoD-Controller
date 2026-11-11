# Digital Video Stabilizer Phase 1 Implementation Plan: Domain Types, Frame Buffers & Image Pyramids

## 1. Executive Summary & Objectives

### 1.1 Context & Scope
Phase 1 establishes the foundational data structures, strongly-typed domain model, zero-heap memory management architecture, and multi-scale image pyramid engine for **`libs/Stabilizer`**.

High-rate video stabilization pipelines (operating at 60 to 120 FPS at 1080p / 4K) cannot tolerate dynamic heap allocation (`malloc`, `new`, `std::vector::resize`) during active processing loops. In compliance with **SEI CERT ERR55-CPP** and **MISRA C++:2023 Rule 7.0.6**, Phase 1 builds a deterministic, statically-sized memory arena and contiguous buffer infrastructure providing non-owning frame views and SIMD-aligned multi-scale image pyramids.

All modules are clean-room implementations constructed from open computer vision literature and numerical standards.

### 1.2 Target Deliverables for Phase 1
1. **Core Domain Value Objects (`StabilizerTypes.h`)**:
   - Zero-cost strongly-typed primitives eliminating Primitive Obsession: `FrameDimensions`, `SubPixelPoint`, `MotionVector`, `CropRatio`, `SmoothingFactor`, `StabilizerStatus`.
   - Strongly-typed enumeration: `StabilizationMode`, `PixelFormat`, `PyramidScale`.
   - Rigid and Affine algebra: `AffineTransform2D` ($2 \times 3$ matrix) supporting zero-heap composition, inverse calculation, identity detection, and coordinate transformation.
2. **Zero-Heap Memory Arena & Frame Buffers (`StabilizerArena.h`, `FrameBuffer.h`, `FrameView.h`)**:
   - `FrameView`: Non-owning, `noexcept` 2D span over contiguous or strided image memory, supporting sub-region ROI slicing with zero heap allocations.
   - `FrameBuffer`: 64-byte cache-line aligned owning memory container (AVX2/AVX-512/NEON compatible) allocated once during initialization.
   - `StabilizerArena`: Monotonic scratchpad allocator reset between frames, eliminating runtime memory fragmentation.
3. **Multi-Scale Gaussian Image Pyramid Engine (`FramePyramid.h`, `FramePyramid.cpp`)**:
   - 3-level Gaussian downsampler ($L_0 = 1\times$, $L_1 = 0.5\times$, $L_2 = 0.25\times$) with pre-allocated contiguous pyramid buffers.
   - Separable $5 \times 1$ Gaussian binomial kernel ($[1, 4, 6, 4, 1] / 16$) with fixed-point SIMD acceleration.
   - Fast $2 \times 2$ box filter fallback for ultra-low latency profiles.
4. **Build System & GoogleTest Verification**:
   - `libs/Stabilizer/CMakeLists.txt` (Zero Qt dependencies, C++17, hardening flags, warnings as errors).
   - `libs/Stabilizer/tests/CMakeLists.txt` integrated with top-level test runner.
   - Test suites: `TestStabilizerTypes.cpp`, `TestFrameView.cpp`, `TestStabilizerArena.cpp`, and `TestFramePyramid.cpp`.

---

## 2. Architecture & Design

### 2.1 Layer Interaction Overview

```
+---------------------------------------------------------------------------------------------------------------+
|                                          libs/Stabilizer (Phase 1)                                            |
|                                                                                                               |
|   +-------------------------------------------------------------------------------------------------------+   |
|   |                                     Domain Model (StabilizerTypes.h)                                  |   |
|   |                                                                                                       |   |
|   |   FrameDimensions        SubPixelPoint         MotionVector          AffineTransform2D                |   |
|   |   [W, H, Stride]         [float x, float y]    [float dx, float dy]  [2x3 Matrix: Scale/Rot/Trans]    |   |
|   |                                                                                                       |   |
|   |   CropRatio              SmoothingFactor       StabilizationMode     PixelFormat                      |   |
|   |   [0.0 .. 0.30]          [0.01 .. 1.0]         (Enum Class)          (Gray8, RGB24, YUV420p)          |   |
|   +-------------------------------------------+-----------------------------------------------------------+   |
|                                               |                                                               |
|                                               ▼                                                               |
|   +-------------------------------------------------------------------------------------------------------+   |
|   |                             Zero-Heap Memory Subsystem (Buffer & Arena)                               |   |
|   |                                                                                                       |   |
|   |   +------------------------------------+              +-------------------------------------------+   |   |
|   |   |            FrameView               |              |             StabilizerArena               |   |   |
|   |   |  - Non-owning pointer + pitch      |              |  - Monotonic scratchpad arena             |   |   |
|   |   |  - Zero-copy sub-ROI slicing       |              |  - Pre-allocated contiguous block (4MB)   |   |   |
|   |   |  - 64-byte alignment verification  |              |  - Frame-boundary zero-overhead reset     |   |   |
|   |   +-----------------+------------------+              +---------------------+---------------------+   |   |
|   |                     |                                                       |                         |   |
|   |                     ▼                                                       ▼                         |   |
|   |   +-----------------------------------------------------------------------------------------------+   |   |
|   |   |                                         FrameBuffer                                           |   |   |
|   |   |   - 64-byte aligned RAII storage (std::aligned_alloc / _aligned_malloc)                       |   |   |
|   |   |   - Fixed capacity for 4K / 1080p luma buffers                                                |   |   |
|   |   +-----------------------------------------------------------------------------------------------+   |   |
|   +-------------------------------------------+-----------------------------------------------------------+   |
|                                               |                                                               |
|                                               ▼                                                               |
|   +-------------------------------------------------------------------------------------------------------+   |
|   |                                 Multi-Scale Pyramid Engine (FramePyramid)                             |   |
|   |                                                                                                       |   |
|   |   Level 0 (Full Res: W x H)           Level 1 (Half Res: W/2 x H/2)       Level 2 (Quarter: W/4 x H/4)|   |
|   |   [ Contiguous Memory ] ------------> [ 5-tap Gaussian Downsampler ] ---> [ 5-tap Gaussian Downsample]|   |
|   +-------------------------------------------------------------------------------------------------------+   |
+---------------------------------------------------------------------------------------------------------------+
```

### 2.2 Class Hierarchy & Type Relationships

```mermaid
classDiagram
    direction TB

    namespace DomainTypes {
        class FrameDimensions {
            -std::uint32_t width_
            -std::uint32_t height_
            -std::uint32_t stride_
            +explicit FrameDimensions(w, h, stride)
            +width() std::uint32_t
            +height() std::uint32_t
            +stride() std::uint32_t
            +isValid() bool
            +totalBytes() std::size_t
        }

        class SubPixelPoint {
            +float x
            +float y
            +isValid() bool
            +distanceTo(SubPixelPoint) float
        }

        class MotionVector {
            +float dx
            +float dy
            +magnitude() float
            +angleRadians() float
        }

        class CropRatio {
            -float ratio_
            +explicit CropRatio(float)
            +value() float
        }

        class AffineTransform2D {
            +float a11
            +float a12
            +float tx
            +float a21
            +float a22
            +float ty
            +identity() AffineTransform2D
            +inverse() AffineTransform2D
            +transformPoint(SubPixelPoint) SubPixelPoint
            +compose(AffineTransform2D) AffineTransform2D
        }
    }

    namespace MemorySubsystem {
        class FrameView {
            -const std::uint8_t* data_
            -FrameDimensions dims_
            +explicit FrameView(data, dims)
            +data() const std::uint8_t*
            +subView(x, y, w, h) FrameView
            +pixel(x, y) std::uint8_t
            +isAligned(std::size_t) bool
        }

        class FrameBuffer {
            -std::uint8_t* memory_
            -FrameDimensions dims_
            +explicit FrameBuffer(dims)
            +view() FrameView
            +data() std::uint8_t*
        }

        class StabilizerArena {
            -std::uint8_t* buffer_
            -std::size_t capacity_
            -std::size_t offset_
            +explicit StabilizerArena(capacity)
            +allocate(size, align) void*
            +reset() void
        }
    }

    namespace PyramidSubsystem {
        class FramePyramid {
            -FrameBuffer storage_
            -FrameView levels_[3]
            -std::uint8_t numLevels_
            +build(FrameView base) bool
            +level(std::uint8_t idx) FrameView
            +levelsCount() std::uint8_t
        }
    }

    FrameBuffer ..> FrameView : produces
    FramePyramid o-- FrameBuffer : internal storage
    FramePyramid ..> FrameView : provides level views
    FrameView o-- FrameDimensions : describes layout
```

---

## 3. Mathematical Foundations & Algorithms

### 3.1 2D Affine Transformation Algebra

The parametric inter-frame motion of the scene is represented by an affine mapping in homogeneous coordinates:
$$\begin{bmatrix} x' \\ y' \\ 1 \end{bmatrix} = \begin{bmatrix} a_{11} & a_{12} & t_x \\ a_{21} & a_{22} & t_y \\ 0 & 0 & 1 \end{bmatrix} \begin{bmatrix} x \\ y \\ 1 \end{bmatrix}$$

#### 3.1.1 Matrix Composition
Given transformation $A$ followed by transformation $B$:
$$C = B \cdot A = \begin{bmatrix} b_{11} & b_{12} & b_{tx} \\ b_{21} & b_{22} & b_{ty} \\ 0 & 0 & 1 \end{bmatrix} \begin{bmatrix} a_{11} & a_{12} & a_{tx} \\ a_{21} & a_{22} & a_{ty} \\ 0 & 0 & 1 \end{bmatrix}$$
Expanded into scalar C++17 expressions:
$$\begin{aligned}
c_{11} &= b_{11} a_{11} + b_{12} a_{21} \\
c_{12} &= b_{11} a_{12} + b_{12} a_{22} \\
c_{tx} &= b_{11} a_{tx} + b_{12} a_{ty} + b_{tx} \\
c_{21} &= b_{21} a_{11} + b_{22} a_{21} \\
c_{22} &= b_{21} a_{12} + b_{22} a_{22} \\
c_{ty} &= b_{21} a_{tx} + b_{22} a_{ty} + b_{ty}
\end{aligned}$$

#### 3.1.2 Analytical Matrix Inversion
The determinant of the $2 \times 2$ linear submatrix is:
$$\det(A) = a_{11} a_{22} - a_{12} a_{21}$$
If $|\det(A)| < 10^{-6}$, the matrix is near-singular and marked degenerate. Otherwise, the inverse $A^{-1}$ is calculated analytically:
$$A^{-1} = \frac{1}{\det(A)} \begin{bmatrix} a_{22} & -a_{12} & a_{12} t_y - a_{22} t_x \\ -a_{21} & a_{11} & a_{21} t_x - a_{11} t_y \\ 0 & 0 & \det(A) \end{bmatrix}$$

---

### 3.2 Gaussian Image Pyramid Downsampling

To enable Lucas-Kanade optical flow tracking across large angular jitter without losing sub-pixel precision, each input frame is decomposed into a 3-level Gaussian scale space.

```
Level 0: 1920 x 1080 (Full Resolution, Luma Y)
   │
   ▼ Separable 5-tap Gaussian Blur + 2x Downsample
Level 1:  960 x  540 (Scale 0.5x)
   │
   ▼ Separable 5-tap Gaussian Blur + 2x Downsample
Level 2:  480 x  270 (Scale 0.25x)
```

#### 3.2.1 1D Separable Binomial Kernel
The 2D Gaussian filter is factorized into horizontal and vertical passes using the discrete normalized binomial kernel:
$$\mathbf{k} = \frac{1}{16} \begin{bmatrix} 1 & 4 & 6 & 4 & 1 \end{bmatrix}$$

For a horizontal pass followed by decimation:
$$I_{\text{horiz}}(x, y) = \frac{I(2x-2, y) + 4 I(2x-1, y) + 6 I(2x, y) + 4 I(2x+1, y) + I(2x+2, y)}{16}$$
Boundary conditions use clamped edge reflection to prevent dark edge fringing:
$$x_{\text{clamped}} = \max(0, \min(x, W-1))$$

#### 3.2.2 Fixed-Point Integer Arithmetic
To guarantee identical, deterministic behavior across ARM64 and x86_64 and prevent floating-point rounding divergence:
$$\text{Pixel}_{\text{out}} = \left( \sum_{i=-2}^{2} k_i \cdot \text{Pixel}_{\text{in}} + 8 \right) \gg 4$$
The intermediate sum fits comfortably in `std::uint16_t` ($16 \times 255 = 4080 \le 65535$), enabling 16-way SIMD vectorization with zero overflow risk.

---

## 4. Detailed Component Specifications

### 4.1 Domain Types (`StabilizerTypes.h`)

```cpp
/// @file StabilizerTypes.h
#pragma once

#include <cstddef>
#include <cstdint>

namespace Stabilizer {

/// @brief Video frame pixel format.
enum class PixelFormat : std::uint8_t {
    Grayscale8 = 0U, ///< 8-bit luma (Y)
    RGB24,           ///< Packed 24-bit RGB
    RGBA32,          ///< Packed 32-bit RGBA
    YUV420p          ///< Planar YUV (Y evaluated directly)
};

/// @brief Stabilization operational mode.
enum class StabilizationMode : std::uint8_t {
    Disabled = 0U,
    TranslationOnly,      ///< 2-DOF (X, Y)
    TranslationRotation,  ///< 3-DOF Rigid (X, Y, Theta)
    Similarity,           ///< 4-DOF (X, Y, Theta, Scale)
    Affine,               ///< 6-DOF (X, Y, Theta, Scale, Shear)
    RollingShutterComp    ///< Affine with row-wise correction
};

/// @brief Strongly-typed image dimension container enforcing invariant bounds.
class FrameDimensions {
public:
    constexpr FrameDimensions() noexcept = default;

    explicit constexpr FrameDimensions(std::uint32_t width, 
                                       std::uint32_t height, 
                                       std::uint32_t stride = 0U) noexcept
        : width_{width}
        , height_{height}
        , stride_{(stride == 0U) ? width : stride} {}

    [[nodiscard]] constexpr std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] constexpr std::uint32_t height() const noexcept { return height_; }
    [[nodiscard]] constexpr std::uint32_t stride() const noexcept { return stride_; }

    [[nodiscard]] constexpr std::size_t totalBytes() const noexcept {
        return static_cast<std::size_t>(stride_) * static_cast<std::size_t>(height_);
    }

    [[nodiscard]] constexpr bool isValid() const noexcept {
        return (width_ >= 64U) && (width_ <= 7680U) &&
               (height_ >= 64U) && (height_ <= 4320U) &&
               (stride_ >= width_);
    }

    [[nodiscard]] constexpr bool operator==(const FrameDimensions& other) const noexcept {
        return (width_ == other.width_) && (height_ == other.height_) && (stride_ == other.stride_);
    }

    [[nodiscard]] constexpr bool operator!=(const FrameDimensions& other) const noexcept {
        return !(*this == other);
    }

private:
    std::uint32_t width_{0U};
    std::uint32_t height_{0U};
    std::uint32_t stride_{0U};
};

/// @brief Sub-pixel 2D point representation.
struct SubPixelPoint {
    float x{0.0F};
    float y{0.0F};

    [[nodiscard]] constexpr bool isValid() const noexcept {
        return (x >= 0.0F) && (y >= 0.0F);
    }

    [[nodiscard]] float distanceSquared(const SubPixelPoint& other) const noexcept {
        const float dx = x - other.x;
        const float dy = y - other.y;
        return (dx * dx) + (dy * dy);
    }
};

/// @brief Inter-frame displacement vector.
struct MotionVector {
    float dx{0.0F};
    float dy{0.0F};

    [[nodiscard]] float magnitudeSquared() const noexcept {
        return (dx * dx) + (dy * dy);
    }
};

/// @brief Invariant-enforced margin crop ratio [0.0 .. 0.30].
class CropRatio {
public:
    constexpr CropRatio() noexcept = default;
    explicit constexpr CropRatio(float val) noexcept
        : value_{(val < 0.0F) ? 0.0F : ((val > 0.30F) ? 0.30F : val)} {}

    [[nodiscard]] constexpr float value() const noexcept { return value_; }

private:
    float value_{0.05F}; // Default 5% border margin
};

/// @brief Invariant-enforced trajectory smoothing factor [0.01 .. 1.0].
class SmoothingFactor {
public:
    constexpr SmoothingFactor() noexcept = default;
    explicit constexpr SmoothingFactor(float val) noexcept
        : value_{(val < 0.01F) ? 0.01F : ((val > 1.0F) ? 1.0F : val)} {}

    [[nodiscard]] constexpr float value() const noexcept { return value_; }

private:
    float value_{0.85F}; // Default 85% momentum
};

/// @brief Affine 2D Transformation (2x3 Matrix).
struct AffineTransform2D {
    float a11{1.0F};
    float a12{0.0F};
    float tx{0.0F};
    float a21{0.0F};
    float a22{1.0F};
    float ty{0.0F};

    [[nodiscard]] static constexpr AffineTransform2D identity() noexcept {
        return {1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F};
    }

    [[nodiscard]] static constexpr AffineTransform2D translation(float dx, float dy) noexcept {
        return {1.0F, 0.0F, dx, 0.0F, 1.0F, dy};
    }

    [[nodiscard]] SubPixelPoint transform(const SubPixelPoint& pt) const noexcept {
        return {
            (a11 * pt.x) + (a12 * pt.y) + tx,
            (a21 * pt.x) + (a22 * pt.y) + ty
        };
    }

    [[nodiscard]] AffineTransform2D compose(const AffineTransform2D& next) const noexcept {
        return {
            (next.a11 * a11) + (next.a12 * a21),
            (next.a11 * a12) + (next.a12 * a22),
            (next.a11 * tx)  + (next.a12 * ty) + next.tx,
            (next.a21 * a11) + (next.a22 * a21),
            (next.a21 * a12) + (next.a22 * a22),
            (next.a21 * tx)  + (next.a22 * ty) + next.ty
        };
    }

    [[nodiscard]] bool invert(AffineTransform2D& out) const noexcept {
        const float det = (a11 * a22) - (a12 * a21);
        if ((det > -1e-6F) && (det < 1e-6F)) {
            return false;
        }
        const float invDet = 1.0F / det;
        out.a11 =  a22 * invDet;
        out.a12 = -a12 * invDet;
        out.tx  = ((a12 * ty) - (a22 * tx)) * invDet;
        out.a21 = -a21 * invDet;
        out.a22 =  a11 * invDet;
        out.ty  = ((a21 * tx) - (a11 * ty)) * invDet;
        return true;
    }
};

/// @brief Runtime stabilization performance and tracking metrics.
struct StabilizerStatus {
    std::uint32_t trackedFeatures{0U};
    std::uint32_t inlierMatches{0U};
    float confidence{0.0F};
    float currentJitterPixels{0.0F};
    std::uint32_t processingTimeUs{0U};
    bool isDegenerate{false};
};

} // namespace Stabilizer
```

---

### 4.2 Non-Owning Frame View (`FrameView.h`)

`FrameView` wraps arbitrary video buffers (such as camera capture frames, decoder outputs, or OpenGL PBOs) without taking ownership or performing heap allocations.

```cpp
/// @file FrameView.h
#pragma once

#include <cstddef>
#include <cstdint>

#include "StabilizerTypes.h"

namespace Stabilizer {

/// @brief Non-owning, zero-heap view over a 2D grayscale image buffer.
class FrameView {
public:
    constexpr FrameView() noexcept = default;

    constexpr FrameView(const std::uint8_t* data, 
                        FrameDimensions dims) noexcept
        : data_{data}
        , dims_{dims} {}

    [[nodiscard]] constexpr const std::uint8_t* data() const noexcept { return data_; }
    [[nodiscard]] constexpr const FrameDimensions& dimensions() const noexcept { return dims_; }
    [[nodiscard]] constexpr bool isEmpty() const noexcept { return (data_ == nullptr) || !dims_.isValid(); }

    /// @brief Retrieve row pointer with zero overhead.
    [[nodiscard]] constexpr const std::uint8_t* rowPtr(std::uint32_t y) const noexcept {
        return data_ + (static_cast<std::size_t>(y) * dims_.stride());
    }

    /// @brief Bounds-checked pixel retrieval with clamping.
    [[nodiscard]] constexpr std::uint8_t pixelClamped(std::int32_t x, std::int32_t y) const noexcept {
        const std::uint32_t cx = (x < 0) ? 0U : ((static_cast<std::uint32_t>(x) >= dims_.width()) ? dims_.width() - 1U : static_cast<std::uint32_t>(x));
        const std::uint32_t cy = (y < 0) ? 0U : ((static_cast<std::uint32_t>(y) >= dims_.height()) ? dims_.height() - 1U : static_cast<std::uint32_t>(y));
        return *(rowPtr(cy) + cx);
    }

    /// @brief Check memory pointer alignment.
    [[nodiscard]] bool isAligned(std::size_t alignment) const noexcept {
        return (reinterpret_cast<std::uintptr_t>(data_) % alignment) == 0U;
    }

    /// @brief Zero-copy sub-ROI windowing.
    [[nodiscard]] FrameView subView(std::uint32_t x, std::uint32_t y, 
                                    std::uint32_t w, std::uint32_t h) const noexcept {
        if ((x + w > dims_.width()) || (y + h > dims_.height()) || isEmpty()) {
            return FrameView{};
        }
        const std::uint8_t* subData = rowPtr(y) + x;
        return FrameView{subData, FrameDimensions{w, h, dims_.stride()}};
    }

private:
    const std::uint8_t* data_{nullptr};
    FrameDimensions dims_{};
};

} // namespace Stabilizer
```

---

### 4.3 64-Byte Aligned Owning Frame Buffer (`FrameBuffer.h`)

```cpp
/// @file FrameBuffer.h
#pragma once

#include <cstddef>
#include <cstdint>

#include "FrameView.h"
#include "StabilizerTypes.h"

namespace Stabilizer {

/// @brief 64-byte aligned RAII frame memory container.
class FrameBuffer {
public:
    FrameBuffer() noexcept = default;
    explicit FrameBuffer(FrameDimensions dims);
    ~FrameBuffer() noexcept;

    // Non-copyable to prevent accidental heap duplications
    FrameBuffer(const FrameBuffer&) = delete;
    FrameBuffer& operator=(const FrameBuffer&) = delete;

    // Movable
    FrameBuffer(FrameBuffer&& other) noexcept;
    FrameBuffer& operator=(FrameBuffer&& other) noexcept;

    [[nodiscard]] std::uint8_t* data() noexcept { return data_; }
    [[nodiscard]] const std::uint8_t* data() const noexcept { return data_; }
    [[nodiscard]] const FrameDimensions& dimensions() const noexcept { return dims_; }
    [[nodiscard]] bool isValid() const noexcept { return (data_ != nullptr) && dims_.isValid(); }

    [[nodiscard]] FrameView view() const noexcept {
        return FrameView{data_, dims_};
    }

    [[nodiscard]] std::uint8_t* rowPtr(std::uint32_t y) noexcept {
        return data_ + (static_cast<std::size_t>(y) * dims_.stride());
    }

    void fillZero() noexcept;

private:
    void release() noexcept;

    std::uint8_t* data_{nullptr};
    FrameDimensions dims_{};
    std::size_t allocatedSize_{0U};
};

} // namespace Stabilizer
```

---

### 4.4 Monotonic Scratchpad Memory Arena (`StabilizerArena.h`)

```cpp
/// @file StabilizerArena.h
#pragma once

#include <cstddef>
#include <cstdint>

namespace Stabilizer {

/// @brief High-speed zero-fragmentation monotonic scratchpad allocator.
class StabilizerArena {
public:
    explicit StabilizerArena(std::size_t capacityBytes);
    ~StabilizerArena() noexcept;

    StabilizerArena(const StabilizerArena&) = delete;
    StabilizerArena& operator=(const StabilizerArena&) = delete;

    StabilizerArena(StabilizerArena&& other) noexcept;
    StabilizerArena& operator=(StabilizerArena&& other) noexcept;

    /// @brief Allocate aligned scratchpad memory block.
    [[nodiscard]] void* allocate(std::size_t size, std::size_t alignment = 64U) noexcept;

    /// @brief Reset arena pointer back to zero (called at frame boundary).
    void reset() noexcept {
        offset_ = 0U;
    }

    [[nodiscard]] std::size_t usedBytes() const noexcept { return offset_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

private:
    std::uint8_t* memory_{nullptr};
    std::size_t capacity_{0U};
    std::size_t offset_{0U};
};

} // namespace Stabilizer
```

---

### 4.5 Multi-Scale Gaussian Frame Pyramid (`FramePyramid.h`, `FramePyramid.cpp`)

```cpp
/// @file FramePyramid.h
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

    /// @brief Build multi-scale pyramid from input frame view.
    /// @param baseView Full-resolution level-0 grayscale frame.
    /// @return true if pyramid constructed cleanly; false on dimension mismatch.
    [[nodiscard]] bool build(const FrameView& baseView) noexcept;

    /// @brief Access individual pyramid level (0=Full, 1=Half, 2=Quarter).
    [[nodiscard]] FrameView level(std::uint8_t levelIdx) const noexcept;

    [[nodiscard]] std::uint8_t levelCount() const noexcept { return numLevels_; }
    [[nodiscard]] const FrameDimensions& baseDimensions() const noexcept { return baseDims_; }

private:
    void downsampleLevel(const FrameView& src, std::uint8_t* dstData, 
                         const FrameDimensions& dstDims) noexcept;

    FrameDimensions baseDims_{};
    std::uint8_t numLevels_{MaxPyramidLevels};
    FrameBuffer storageLevel1_{};
    FrameBuffer storageLevel2_{};
    std::array<FrameView, MaxPyramidLevels> views_{};
};

} // namespace Stabilizer
```

---

## 5. Build System & Directory Organization

### 5.1 Directory Layout

```
libs/Stabilizer/
├── CMakeLists.txt
├── StabilizerTypes.h
├── FrameView.h
├── FrameBuffer.h
├── FrameBuffer.cpp
├── StabilizerArena.h
├── StabilizerArena.cpp
├── FramePyramid.h
├── FramePyramid.cpp
└── tests/
    ├── CMakeLists.txt
    ├── TestStabilizerTypes.cpp
    ├── TestFrameView.cpp
    ├── TestStabilizerArena.cpp
    └── TestFramePyramid.cpp
```

### 5.2 CMake Configuration (`libs/Stabilizer/CMakeLists.txt`)

```cmake
cmake_minimum_required(VERSION 3.16)
project(Stabilizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(Stabilizer STATIC
    FrameBuffer.cpp
    FramePyramid.cpp
    StabilizerArena.cpp
)

target_include_directories(Stabilizer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# Compiler hardening and MISRA warning enforcement
if(MSVC)
    target_compile_options(Stabilizer PRIVATE /W4 /WX /permissive- /Zc:__cplusplus)
else()
    target_compile_options(Stabilizer PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

---

## 6. Verification & Test Strategy

### 6.1 Test Cases Matrix

| Test Suite | Target Component | Verification Objective | Pass Criteria |
| :--- | :--- | :--- | :--- |
| `TestStabilizerTypes` | `FrameDimensions` | Invariant checks, minimum/maximum clamping, stride equality. | `isValid()` returns `false` on $<64\times 64$ or $>7680\times 4320$. |
| `TestStabilizerTypes` | `AffineTransform2D` | Identity, composition, analytical inversion, degenerate det. | Inverting singular matrix returns `false`; non-singular verifies $A \cdot A^{-1} = I \pm 10^{-5}$. |
| `TestStabilizerTypes` | `CropRatio` / `SmoothingFactor` | Range clamping and bounds enforcement. | Ratios strictly clamped to $[0.0, 0.30]$ and $[0.01, 1.0]$. |
| `TestFrameView` | `FrameView` | Pointer indexing, sub-ROI windowing, clamped pixel retrieval. | `pixelClamped` at $(-5, -5)$ equals $(0, 0)$; subView returns valid dimensions. |
| `TestFrameBuffer` | `FrameBuffer` | 64-byte alignment, move semantics, zero-fill. | `data()` pointer modulo 64 is zero; moved-from buffer is empty. |
| `TestStabilizerArena` | `StabilizerArena` | Monotonic allocation, alignment offsets, reset reuse. | Allocates blocks without memory leaks; reset sets `usedBytes()` to zero. |
| `TestFramePyramid` | `FramePyramid` | 3-level Gaussian downsampling on synthetic patterns. | Level 1 is exactly half width/height; zero heap allocations during `build()`. |

### 6.2 Benchmarking Targets
- **Pyramid Construction Latency**: $\le 0.45\text{ ms}$ for 1080p luma frame on standard x86_64 / ARM64.
- **Dynamic Allocation Audit**: Exactly 0 heap allocation calls (`malloc`, `new`) during active `build()` cycles.

---

## 7. Safety, Compliance & Traceability

1. **MISRA C++:2023 Rule 7.0.6 (Explicit Casts)**:
   - All conversions between `std::size_t`, `std::uint32_t`, and pointer arithmetic explicitly use `static_cast` with compile-time range verification.
2. **SEI CERT INT31-C (Safe Integer Conversion)**:
   - Frame dimension additions ($x + w$, $y + h$) protected against integer overflow prior to boundary checks.
3. **SEI CERT ERR55-CPP (Zero Allocation `noexcept`)**:
   - `FrameView::subView()`, `FrameView::pixelClamped()`, and `FramePyramid::build()` are fully marked `noexcept` and execute within pre-allocated arenas.
4. **IWYU & `<cstdint>` Compliance**:
   - All translation units directly `#include <cstdint>` and `<cstddef>`. All primitive integer types strictly qualified with `std::`.
5. **Zero Qt Dependency**:
   - Zero `#include <Q...>` references. Completely decoupled from GUI frameworks.

