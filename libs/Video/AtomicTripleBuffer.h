#pragma once

/// @file AtomicTripleBuffer.h
/// @brief Lock-free, zero-allocation, cache-line-aligned triple buffering primitive.

#include <array>
#include <atomic>
#include <cstdint>

namespace Video {

/// @class AtomicTripleBuffer
/// @brief Lock-free, zero-allocation frame exchange primitive between a Producer and Consumer.
///
/// Implements an atomic 3-buffer state machine:
/// - Buffer Slot A: Producer (Write) buffer (owned exclusively by producer thread).
/// - Buffer Slot B: Consumer (Read) buffer (owned exclusively by consumer thread).
/// - Buffer Slot C: Shared intermediate buffer swapped atomically via single-word atomic exchange/CAS.
///
/// Thread Safety:
/// - Exactly one Producer thread and one Consumer thread can operate concurrently without locks.
/// - Guarantees zero data tearing, monotonic frame ordering, and zero dynamic heap allocations.
///
/// @tparam T Buffer slot type (e.g. FrameBufferSlot).
template <typename T>
class AtomicTripleBuffer {
public:
    /// @brief Default constructor initializing slot indices.
    AtomicTripleBuffer()
        : m_slots {}
        , m_writeIndex(0U)
        , m_readIndex(1U)
        , m_sharedState(2U) // Shared index 2, dirty flag = 0
    {
    }

    /// @brief Destructor.
    ~AtomicTripleBuffer() = default;

    // Non-copyable, non-movable for thread-safety and address stability
    AtomicTripleBuffer(const AtomicTripleBuffer&) = delete;
    AtomicTripleBuffer& operator=(const AtomicTripleBuffer&) = delete;
    AtomicTripleBuffer(AtomicTripleBuffer&&) = delete;
    AtomicTripleBuffer& operator=(AtomicTripleBuffer&&) = delete;

    /// @brief Accesses the producer's active write buffer slot.
    /// @return Mutable reference to current write buffer.
    [[nodiscard]] T& getWriteBuffer() noexcept
    {
        return m_slots[m_writeIndex];
    }

    /// @brief Publishes the current write buffer atomically, marking a new frame available.
    void publishWriteBuffer() noexcept
    {
        // New state: index = m_writeIndex, dirty flag = 1 (bit 2)
        const auto newState = static_cast<std::uint8_t>(m_writeIndex | (1U << 2U));
        const std::uint8_t oldState = m_sharedState.exchange(newState, std::memory_order_acq_rel);
        // The old intermediate slot becomes the new write slot
        m_writeIndex = static_cast<std::uint8_t>(oldState & 0x03U);
    }

    /// @brief Accesses the consumer's active read buffer slot.
    /// @return Const reference to the current read buffer.
    [[nodiscard]] const T& getReadBuffer() const noexcept
    {
        return m_slots[m_readIndex];
    }

    /// @brief Accesses the consumer's active read buffer slot (mutable).
    /// @return Mutable reference to current read buffer.
    [[nodiscard]] T& getReadBuffer() noexcept
    {
        return m_slots[m_readIndex];
    }

    /// @brief Attempts to swap the read buffer with the latest published frame.
    /// @return True if a fresh frame was acquired, false if no new frame was available.
    bool swapReadBuffer() noexcept
    {
        std::uint8_t curr = m_sharedState.load(std::memory_order_acquire);
        while (true) {
            // Check if dirty bit (bit 2) is set
            if ((curr & (1U << 2U)) == 0U) {
                return false;
            }
            // New state: index = m_readIndex, dirty flag = 0
            const auto newState = static_cast<std::uint8_t>(m_readIndex & 0x03U);
            if (m_sharedState.compare_exchange_weak(
                    curr, newState, std::memory_order_acq_rel, std::memory_order_acquire)) {
                m_readIndex = static_cast<std::uint8_t>(curr & 0x03U);
                return true;
            }
        }
    }

    /// @brief Checks if a fresh frame is waiting in the intermediate slot.
    /// @return True if a new unread frame is available.
    [[nodiscard]] bool hasNewFrame() const noexcept
    {
        return (m_sharedState.load(std::memory_order_acquire) & (1U << 2U)) != 0U;
    }

    /// @brief Resets the index pointers and clears the new frame flag.
    void reset() noexcept
    {
        m_writeIndex = 0U;
        m_readIndex = 1U;
        m_sharedState.store(static_cast<std::uint8_t>(2U), std::memory_order_release);
    }

    /// @brief Provides direct access to the underlying 3 buffer slots for pre-allocation.
    /// @return Reference to slots array.
    [[nodiscard]] std::array<T, 3>& getSlots() noexcept
    {
        return m_slots;
    }

    /// @brief Provides const direct access to the underlying 3 buffer slots.
    /// @return Const reference to slots array.
    [[nodiscard]] const std::array<T, 3>& getSlots() const noexcept
    {
        return m_slots;
    }

private:
    std::array<T, 3> m_slots;

    std::uint8_t m_writeIndex { 0U };
    std::uint8_t m_readIndex { 1U };

    // Cache-line aligned atomic state byte: bits 0-1 = shared index (0..2), bit 2 = dirty flag (1 if new frame ready)
    alignas(64) std::atomic<std::uint8_t> m_sharedState { 2U };
};

} // namespace Video
