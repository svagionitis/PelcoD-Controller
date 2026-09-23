#pragma once

/// @file KlvStreamScanner.h
/// @brief Continuous bitstream scanner and sliding-window reassembler for STANAG 4609 KLV packets.

#include "KlvParser.h"
#include "KlvTypes.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace Klv {

/// @class KlvStreamScanner
/// @brief Ingests arbitrary chunked bitstream feeds, searches for Universal Labels, and dispatches validated KLV packets.
class KlvStreamScanner {
public:
    /// @brief Callback invoked when a complete, validated raw KLV packet is identified.
    using RawPacketCallback = std::function<void(const std::uint8_t* packet, std::size_t size)>;

    /// @brief Callback invoked when a KLV packet has been successfully parsed into a UasDatalinkMessage.
    using MessageCallback = std::function<void(const UasDatalinkMessage& message)>;

    /// @brief Default constructor.
    KlvStreamScanner() = default;

    /// @brief Sets the callback for raw validated KLV packet buffers.
    /// @param[in] callback Callback function.
    void setRawPacketCallback(RawPacketCallback callback);

    /// @brief Sets the callback for successfully deserialized UasDatalinkMessages.
    /// @param[in] callback Callback function.
    void setMessageCallback(MessageCallback callback);

    /// @brief Sets the maximum allowed buffer size before auto-pruning orphaned bytes.
    /// @param[in] maxBytes Maximum buffer capacity in bytes (default 1 MB).
    void setMaxBufferSize(std::size_t maxBytes) noexcept;

    /// @brief Processes a newly received chunk of streaming bytes.
    /// @param[in] data Pointer to the new byte buffer.
    /// @param[in] size Number of bytes in the chunk.
    /// @return Number of validated KLV packets successfully extracted and dispatched in this call.
    std::size_t processBytes(const std::uint8_t* data, std::size_t size);

    /// @brief Clears internal buffers and resets scanner state.
    void reset();

    /// @brief Returns the current number of buffered bytes waiting for complete packet assembly.
    [[nodiscard]] std::size_t bufferedBytes() const noexcept;

private:
    std::vector<std::uint8_t> m_buffer;
    std::size_t m_maxBufferSize { 1024U * 1024U }; // 1 MB default
    RawPacketCallback m_rawPacketCallback;
    MessageCallback m_messageCallback;

    std::size_t scanAndDispatch();
};

} // namespace Klv
