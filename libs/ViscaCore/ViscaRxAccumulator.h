#pragma once

#include "ViscaFrame.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <vector>

namespace Visca {

/// @class ViscaRxAccumulator
/// @brief Thread-safe streaming byte accumulator and framer for VISCA packets.
/// @details Processes incoming byte chunks from a transport, isolates packets delimited
/// by the 0xFF terminator, validates packet size and headers, and emits valid @ref ViscaFrame instances.
class ViscaRxAccumulator {
public:
    /// @brief Callback function signature for notifying when a valid frame is received.
    using FrameCallback = std::function<void(const ViscaFrame&)>;

    /// @brief Default constructor.
    ViscaRxAccumulator() = default;

    /// @brief Destructor.
    ~ViscaRxAccumulator() = default;

    // Non-copyable, non-movable due to synchronization primitive (std::mutex)
    ViscaRxAccumulator(const ViscaRxAccumulator&) = delete;
    ViscaRxAccumulator& operator=(const ViscaRxAccumulator&) = delete;
    ViscaRxAccumulator(ViscaRxAccumulator&&) = delete;
    ViscaRxAccumulator& operator=(ViscaRxAccumulator&&) = delete;

    /// @brief Sets the callback invoked whenever a complete, valid VISCA frame is extracted.
    /// @param[in] callback Callable accepting a const reference to @ref ViscaFrame.
    void setFrameCallback(FrameCallback callback);

    /// @brief Feeds incoming raw bytes into the accumulator for framing.
    /// @param[in] data Pointer to raw byte buffer.
    /// @param[in] length Number of bytes in buffer.
    void addData(const std::uint8_t* data, std::size_t length);

    /// @brief Feeds incoming raw bytes into the accumulator for framing.
    /// @param[in] data Vector containing raw byte sequence.
    void addData(const std::vector<std::uint8_t>& data);

    /// @brief Retrieves and removes the oldest framed packet from the internal queue.
    /// @return Optional containing @ref ViscaFrame if available, std::nullopt if queue is empty.
    [[nodiscard]] std::optional<ViscaFrame> popFrame();

    /// @brief Checks if there are any framed packets pending in the queue.
    /// @return True if at least one frame is queued, false otherwise.
    [[nodiscard]] bool hasFrames() const;

    /// @brief Returns the count of pending framed packets in the queue.
    /// @return Number of queued frames.
    [[nodiscard]] std::size_t pendingFrameCount() const;

    /// @brief Clears all internal accumulation buffers and queued frames.
    void clear();

private:
    /// @brief Internal helper to scan the buffer for 0xFF delimiters and extract frames.
    /// @note Must be called with @ref m_mutex held.
    void processBufferLocked();

    mutable std::mutex m_mutex {};
    std::vector<std::uint8_t> m_buffer {};
    std::deque<ViscaFrame> m_frameQueue {};
    FrameCallback m_callback { nullptr };

    static constexpr std::size_t kMaxAccumulatorBufferSize { 512U };
};

} // namespace Visca
