/// @file StabilizerArena.cpp
/// @brief High-speed zero-fragmentation monotonic scratchpad allocator implementation.
#include "StabilizerArena.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <utility>

#if defined(_MSC_VER) || defined(__MINGW32__)
#include <malloc.h>
#endif

namespace Stabilizer {

namespace {

    constexpr std::size_t BaseAlignmentBytes = 64U;

    [[nodiscard]] std::uint8_t* allocateAlignedBlock(std::size_t bytes) noexcept
    {
        if (bytes == 0U) {
            return nullptr;
        }
#if defined(_MSC_VER) || defined(__MINGW32__)
        return static_cast<std::uint8_t*>(_aligned_malloc(bytes, BaseAlignmentBytes));
#else
        void* ptr = nullptr;
        if (posix_memalign(&ptr, BaseAlignmentBytes, bytes) != 0) {
            return nullptr;
        }
        return static_cast<std::uint8_t*>(ptr);
#endif
    }

    void freeAlignedBlock(std::uint8_t* ptr) noexcept
    {
        if (ptr == nullptr) {
            return;
        }
#if defined(_MSC_VER) || defined(__MINGW32__)
        _aligned_free(ptr);
#else
        std::free(ptr);
#endif
    }

} // namespace

StabilizerArena::StabilizerArena(std::size_t capacityBytes)
    : capacity_ { capacityBytes }
    , offset_ { 0U }
{
    if (capacity_ > 0U) {
        memory_ = allocateAlignedBlock(capacity_);
    }
}

StabilizerArena::~StabilizerArena() noexcept
{
    release();
}

StabilizerArena::StabilizerArena(StabilizerArena&& other) noexcept
    : memory_ { other.memory_ }
    , capacity_ { other.capacity_ }
    , offset_ { other.offset_ }
{
    other.memory_ = nullptr;
    other.capacity_ = 0U;
    other.offset_ = 0U;
}

StabilizerArena& StabilizerArena::operator=(StabilizerArena&& other) noexcept
{
    if (this != &other) {
        release();
        memory_ = other.memory_;
        capacity_ = other.capacity_;
        offset_ = other.offset_;

        other.memory_ = nullptr;
        other.capacity_ = 0U;
        other.offset_ = 0U;
    }
    return *this;
}

void* StabilizerArena::allocate(std::size_t size, std::size_t alignment) noexcept
{
    if ((memory_ == nullptr) || (size == 0U)) {
        return nullptr;
    }
    const std::size_t actualAlignment = (alignment == 0U) ? 1U : alignment;
    const std::uintptr_t currentAddress = reinterpret_cast<std::uintptr_t>(memory_ + offset_);
    const std::uintptr_t alignmentMask = static_cast<std::uintptr_t>(actualAlignment - 1U);
    const std::uintptr_t alignedAddress = (currentAddress + alignmentMask) & ~alignmentMask;

    const std::size_t padding = static_cast<std::size_t>(alignedAddress - currentAddress);
    if ((offset_ + padding + size) > capacity_) {
        return nullptr; // Arena exhausted
    }

    offset_ += padding + size;
    return reinterpret_cast<void*>(alignedAddress);
}

void StabilizerArena::release() noexcept
{
    if (memory_ != nullptr) {
        freeAlignedBlock(memory_);
        memory_ = nullptr;
    }
    capacity_ = 0U;
    offset_ = 0U;
}

} // namespace Stabilizer
