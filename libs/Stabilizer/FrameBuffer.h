/// @file FrameBuffer.h
/// @brief 64-byte aligned RAII frame memory container.
#pragma once

#include <cstddef>
#include <cstdint>

#include "FrameView.h"
#include "StabilizerTypes.h"

namespace Stabilizer {

/// @brief 64-byte aligned RAII frame buffer container.
class FrameBuffer {
public:
    FrameBuffer() noexcept = default;
    explicit FrameBuffer(FrameDimensions dims);
    ~FrameBuffer() noexcept;

    FrameBuffer(const FrameBuffer&) = delete;
    FrameBuffer& operator=(const FrameBuffer&) = delete;

    FrameBuffer(FrameBuffer&& other) noexcept;
    FrameBuffer& operator=(FrameBuffer&& other) noexcept;

    /// @brief Raw mutable buffer pointer.
    [[nodiscard]] std::uint8_t* data() noexcept
    {
        return data_;
    }

    /// @brief Raw const buffer pointer.
    [[nodiscard]] const std::uint8_t* data() const noexcept
    {
        return data_;
    }

    /// @brief Image dimensions.
    [[nodiscard]] const FrameDimensions& dimensions() const noexcept
    {
        return dims_;
    }

    /// @brief Check whether memory allocation succeeded and is valid.
    [[nodiscard]] bool isValid() const noexcept
    {
        return (data_ != nullptr) && dims_.isValid();
    }

    /// @brief Construct a non-owning FrameView over this buffer.
    [[nodiscard]] FrameView view() const noexcept
    {
        return FrameView { data_, dims_ };
    }

    /// @brief Mutable row pointer.
    /// @param y Row index [0 .. height-1].
    [[nodiscard]] std::uint8_t* rowPtr(std::uint32_t y) noexcept
    {
        return data_ + (static_cast<std::size_t>(y) * dims_.stride());
    }

    /// @brief Const row pointer.
    /// @param y Row index [0 .. height-1].
    [[nodiscard]] const std::uint8_t* rowPtr(std::uint32_t y) const noexcept
    {
        return data_ + (static_cast<std::size_t>(y) * dims_.stride());
    }

    /// @brief Reset all buffer bytes to zero.
    void fillZero() noexcept;

private:
    void release() noexcept;

    std::uint8_t* data_ { nullptr };
    FrameDimensions dims_ { };
    std::size_t allocatedSize_ { 0U };
};

} // namespace Stabilizer
