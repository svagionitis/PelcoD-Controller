/// @file StabilizerArena.h
/// @brief High-speed zero-fragmentation monotonic scratchpad allocator.
#pragma once

#include <cstddef>
#include <cstdint>

namespace Stabilizer {

/// @brief Monotonic scratchpad memory arena for zero-heap real-time frame cycles.
class StabilizerArena {
public:
    StabilizerArena() noexcept = default;
    explicit StabilizerArena(std::size_t capacityBytes);
    ~StabilizerArena() noexcept;

    StabilizerArena(const StabilizerArena&) = delete;
    StabilizerArena& operator=(const StabilizerArena&) = delete;

    StabilizerArena(StabilizerArena&& other) noexcept;
    StabilizerArena& operator=(StabilizerArena&& other) noexcept;

    /// @brief Allocate aligned scratchpad memory block.
    /// @param size Bytes requested.
    /// @param alignment Memory alignment boundary (default 64 bytes).
    /// @return Pointer to aligned block, or nullptr if exhausted.
    [[nodiscard]] void* allocate(std::size_t size, std::size_t alignment = 64U) noexcept;

    /// @brief Reset arena pointer back to zero (called at frame boundary).
    void reset() noexcept
    {
        offset_ = 0U;
    }

    /// @brief Number of bytes currently allocated in this frame cycle.
    [[nodiscard]] std::size_t usedBytes() const noexcept
    {
        return offset_;
    }

    /// @brief Total capacity in bytes.
    [[nodiscard]] std::size_t capacity() const noexcept
    {
        return capacity_;
    }

    /// @brief Remaining available bytes.
    [[nodiscard]] std::size_t availableBytes() const noexcept
    {
        return (capacity_ > offset_) ? (capacity_ - offset_) : 0U;
    }

private:
    void release() noexcept;

    std::uint8_t* memory_ { nullptr };
    std::size_t capacity_ { 0U };
    std::size_t offset_ { 0U };
};

} // namespace Stabilizer
