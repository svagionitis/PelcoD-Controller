#pragma once

/// @file CircularByteRing.h
/// @brief SPSC lock-free circular byte ring with cacheline padding.

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

namespace PelcoD {

/// @struct ByteRegion
/// @brief Contiguous memory slice for zero-copy write.
struct ByteRegion {
    std::uint8_t* data { nullptr };
    std::size_t size { 0U };
};

/// @struct ConstByteRegion
/// @brief Contiguous memory slice for zero-copy read.
struct ConstByteRegion {
    const std::uint8_t* data { nullptr };
    std::size_t size { 0U };
};

/// @struct ZeroCopyWriteView
/// @brief Disjoint view containing up to two contiguous writable blocks.
struct ZeroCopyWriteView {
    ByteRegion first {};
    ByteRegion second {};

    [[nodiscard]] std::size_t totalSize() const noexcept
    {
        return first.size + second.size;
    }
};

/// @struct ZeroCopyReadView
/// @brief Disjoint view containing up to two contiguous readable blocks.
struct ZeroCopyReadView {
    ConstByteRegion first {};
    ConstByteRegion second {};

    [[nodiscard]] std::size_t totalSize() const noexcept
    {
        return first.size + second.size;
    }
};

/// @class CircularByteRing
/// @brief Single-Producer Single-Consumer lock-free ring buffer for byte streams.
/// @tparam Capacity Total capacity in bytes (must be a power of 2).
template <std::size_t Capacity = 65536U> class CircularByteRing {
    static_assert(Capacity >= 2U && (Capacity & (Capacity - 1U)) == 0U, "Capacity must be a power of 2");

public:
    static constexpr std::size_t Mask { Capacity - 1U };
    static constexpr std::size_t npos { static_cast<std::size_t>(-1) };

    CircularByteRing()
        : m_head(0U)
        , m_tail(0U)
    {
    }

    ~CircularByteRing() = default;

    // Non-copyable, non-movable for lock-free safety
    CircularByteRing(const CircularByteRing&) = delete;
    CircularByteRing& operator=(const CircularByteRing&) = delete;
    CircularByteRing(CircularByteRing&&) = delete;
    CircularByteRing& operator=(CircularByteRing&&) = delete;

    /// @brief Obtains zero-copy writable memory slices (Producer only).
    /// @return ZeroCopyWriteView with 1 or 2 contiguous regions.
    [[nodiscard]] ZeroCopyWriteView getWriteView() noexcept
    {
        const auto currentTail = m_tail.load(std::memory_order_relaxed);
        const auto currentHead = m_head.load(std::memory_order_acquire);
        const std::size_t used = currentTail - currentHead;
        const std::size_t freeBytes = Capacity - used;

        ZeroCopyWriteView view {};
        if (freeBytes == 0U) {
            return view;
        }

        const std::size_t tailIdx = currentTail & Mask;
        const std::size_t spaceToEnd = Capacity - tailIdx;
        const std::size_t firstSize = (freeBytes < spaceToEnd) ? freeBytes : spaceToEnd;
        const std::size_t secondSize = freeBytes - firstSize;

        view.first = ByteRegion { &m_buffer[tailIdx], firstSize };
        if (secondSize > 0U) {
            view.second = ByteRegion { &m_buffer[0U], secondSize };
        }
        return view;
    }

    /// @brief Commits written bytes into the ring (Producer only).
    /// @param[in] bytes Number of bytes written.
    void advanceWrite(std::size_t bytes) noexcept
    {
        const auto currentTail = m_tail.load(std::memory_order_relaxed);
        const auto currentHead = m_head.load(std::memory_order_acquire);
        const std::size_t maxWrite = Capacity - (currentTail - currentHead);
        const std::size_t actual = (bytes <= maxWrite) ? bytes : maxWrite;
        m_tail.store(currentTail + actual, std::memory_order_release);
    }

    /// @brief Obtains zero-copy readable memory slices (Consumer only).
    /// @return ZeroCopyReadView with 1 or 2 contiguous regions.
    [[nodiscard]] ZeroCopyReadView getReadView() const noexcept
    {
        const auto currentHead = m_head.load(std::memory_order_relaxed);
        const auto currentTail = m_tail.load(std::memory_order_acquire);
        const std::size_t available = currentTail - currentHead;

        ZeroCopyReadView view {};
        if (available == 0U) {
            return view;
        }

        const std::size_t headIdx = currentHead & Mask;
        const std::size_t spaceToEnd = Capacity - headIdx;
        const std::size_t firstSize = (available < spaceToEnd) ? available : spaceToEnd;
        const std::size_t secondSize = available - firstSize;

        view.first = ConstByteRegion { &m_buffer[headIdx], firstSize };
        if (secondSize > 0U) {
            view.second = ConstByteRegion { &m_buffer[0U], secondSize };
        }
        return view;
    }

    /// @brief Consumes and releases readable bytes (Consumer only).
    /// @param[in] bytes Number of bytes consumed.
    void advanceRead(std::size_t bytes) noexcept
    {
        const auto currentHead = m_head.load(std::memory_order_relaxed);
        const auto currentTail = m_tail.load(std::memory_order_acquire);
        const std::size_t maxRead = currentTail - currentHead;
        const std::size_t actual = (bytes <= maxRead) ? bytes : maxRead;
        m_head.store(currentHead + actual, std::memory_order_release);
    }

    /// @brief Scans buffer for a delimiter byte without advancing.
    /// @param[in] delimiter Byte to locate.
    /// @return Offset from readable start, or npos if not found.
    [[nodiscard]] std::size_t findByte(std::uint8_t delimiter) const noexcept
    {
        const auto view = getReadView();
        if (view.totalSize() == 0U) {
            return npos;
        }

        if (view.first.size > 0U) {
            const void* p = std::memchr(view.first.data, delimiter, view.first.size);
            if (p != nullptr) {
                const auto* ptr = static_cast<const std::uint8_t*>(p);
                return static_cast<std::size_t>(ptr - view.first.data);
            }
        }

        if (view.second.size > 0U) {
            const void* p = std::memchr(view.second.data, delimiter, view.second.size);
            if (p != nullptr) {
                const auto* ptr = static_cast<const std::uint8_t*>(p);
                return view.first.size + static_cast<std::size_t>(ptr - view.second.data);
            }
        }

        return npos;
    }

    /// @brief Peeks bytes from ring without advancing read cursor.
    /// @param[out] dest Target buffer.
    /// @param[in] count Number of bytes to peek.
    /// @return True if sufficient bytes exist.
    [[nodiscard]] bool peekBytes(std::uint8_t* dest, std::size_t count) const noexcept
    {
        if (dest == nullptr || count == 0U) {
            return false;
        }

        const auto view = getReadView();
        if (view.totalSize() < count) {
            return false;
        }

        const std::size_t copyFirst = (count < view.first.size) ? count : view.first.size;
        std::memcpy(dest, view.first.data, copyFirst);

        if (count > copyFirst) {
            const std::size_t copySecond = count - copyFirst;
            std::memcpy(dest + copyFirst, view.second.data, copySecond);
        }

        return true;
    }

    /// @brief Writes exact byte array into ring (Producer only).
    /// @param[in] src Source buffer.
    /// @param[in] count Byte count.
    /// @return True on success, false if full.
    [[nodiscard]] bool writeExact(const std::uint8_t* src, std::size_t count) noexcept
    {
        if (src == nullptr || count == 0U) {
            return false;
        }

        const auto view = getWriteView();
        if (view.totalSize() < count) {
            return false;
        }

        const std::size_t copyFirst = (count < view.first.size) ? count : view.first.size;
        std::memcpy(view.first.data, src, copyFirst);

        if (count > copyFirst) {
            const std::size_t copySecond = count - copyFirst;
            std::memcpy(view.second.data, src + copyFirst, copySecond);
        }

        advanceWrite(count);
        return true;
    }

    /// @brief Reads and consumes exact bytes from ring (Consumer only).
    /// @param[out] dest Destination buffer.
    /// @param[in] count Byte count.
    /// @return True on success, false if insufficient data.
    [[nodiscard]] bool readExact(std::uint8_t* dest, std::size_t count) noexcept
    {
        if (!peekBytes(dest, count)) {
            return false;
        }
        advanceRead(count);
        return true;
    }

    /// @brief Returns number of readable bytes.
    /// @return Available bytes count.
    [[nodiscard]] std::size_t availableRead() const noexcept
    {
        const auto currentHead = m_head.load(std::memory_order_relaxed);
        const auto currentTail = m_tail.load(std::memory_order_acquire);
        return currentTail - currentHead;
    }

    /// @brief Returns remaining writable byte capacity.
    /// @return Free capacity count.
    [[nodiscard]] std::size_t availableWrite() const noexcept
    {
        return Capacity - availableRead();
    }

    /// @brief Returns fixed total capacity.
    /// @return Capacity value.
    [[nodiscard]] constexpr std::size_t capacity() const noexcept
    {
        return Capacity;
    }

    /// @brief Resets read and write cursors.
    void clear() noexcept
    {
        m_head.store(0U, std::memory_order_relaxed);
        m_tail.store(0U, std::memory_order_relaxed);
    }

private:
    static constexpr std::size_t CacheLineSize { 64U };

    alignas(CacheLineSize) std::atomic<std::size_t> m_head;
    alignas(CacheLineSize) std::atomic<std::size_t> m_tail;
    alignas(CacheLineSize) std::array<std::uint8_t, Capacity> m_buffer;
};

} // namespace PelcoD
