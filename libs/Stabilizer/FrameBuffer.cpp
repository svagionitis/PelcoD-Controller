/// @file FrameBuffer.cpp
/// @brief 64-byte aligned RAII frame memory container implementation.
#include "FrameBuffer.h"

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

    constexpr std::size_t AlignmentBytes = 64U;

    [[nodiscard]] std::uint8_t* allocateMemory(std::size_t bytes) noexcept
    {
        if (bytes == 0U) {
            return nullptr;
        }
#if defined(_MSC_VER) || defined(__MINGW32__)
        return static_cast<std::uint8_t*>(_aligned_malloc(bytes, AlignmentBytes));
#else
        void* ptr = nullptr;
        if (posix_memalign(&ptr, AlignmentBytes, bytes) != 0) {
            return nullptr;
        }
        return static_cast<std::uint8_t*>(ptr);
#endif
    }

    void freeMemory(std::uint8_t* ptr) noexcept
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

FrameBuffer::FrameBuffer(FrameDimensions dims)
    : dims_ { dims }
    , allocatedSize_ { dims.totalBytes() }
{
    if (dims_.isValid() && (allocatedSize_ > 0U)) {
        data_ = allocateMemory(allocatedSize_);
        if (data_ != nullptr) {
            std::memset(data_, 0, allocatedSize_);
        }
    }
}

FrameBuffer::~FrameBuffer() noexcept
{
    release();
}

FrameBuffer::FrameBuffer(FrameBuffer&& other) noexcept
    : data_ { other.data_ }
    , dims_ { other.dims_ }
    , allocatedSize_ { other.allocatedSize_ }
{
    other.data_ = nullptr;
    other.dims_ = FrameDimensions { };
    other.allocatedSize_ = 0U;
}

FrameBuffer& FrameBuffer::operator=(FrameBuffer&& other) noexcept
{
    if (this != &other) {
        release();
        data_ = other.data_;
        dims_ = other.dims_;
        allocatedSize_ = other.allocatedSize_;

        other.data_ = nullptr;
        other.dims_ = FrameDimensions { };
        other.allocatedSize_ = 0U;
    }
    return *this;
}

void FrameBuffer::fillZero() noexcept
{
    if ((data_ != nullptr) && (allocatedSize_ > 0U)) {
        std::memset(data_, 0, allocatedSize_);
    }
}

void FrameBuffer::release() noexcept
{
    if (data_ != nullptr) {
        freeMemory(data_);
        data_ = nullptr;
    }
    dims_ = FrameDimensions { };
    allocatedSize_ = 0U;
}

} // namespace Stabilizer
