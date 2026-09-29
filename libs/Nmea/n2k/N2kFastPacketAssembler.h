#pragma once

#include "N2kTypes.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Nmea::N2k {

/// @class N2kFastPacketAssembler
/// @brief Reassembles ISO 11783-3 / NMEA 2000 multi-frame Fast Packet CAN sequences.
/// @details Handles both standard single-frame (<= 8 byte) PGNs and multi-frame Fast Packet
///          protocols (e.g., PGN 129029, 129038, 129039, 129794) across multiple CAN transmitting nodes.
/// @note Thread-safe for multi-threaded CAN frame ingestion.
class N2kFastPacketAssembler {
public:
    /// @brief Constructs an assembler and initializes standard known Fast Packet PGNs.
    N2kFastPacketAssembler();

    /// @brief Default virtual destructor.
    ~N2kFastPacketAssembler() = default;

    // Non-copyable, movable
    N2kFastPacketAssembler(const N2kFastPacketAssembler&) = delete;
    N2kFastPacketAssembler& operator=(const N2kFastPacketAssembler&) = delete;
    N2kFastPacketAssembler(N2kFastPacketAssembler&&) noexcept = default;
    N2kFastPacketAssembler& operator=(N2kFastPacketAssembler&&) noexcept = default;

    /// @brief Checks if a PGN is registered as a Fast Packet multi-frame PGN.
    /// @param[in] pgn Parameter Group Number to query.
    /// @return True if registered as Fast Packet, false otherwise.
    [[nodiscard]] bool isFastPacketPgn(std::uint32_t pgn) const noexcept;

    /// @brief Registers a PGN to be handled by the Fast Packet multi-frame state machine.
    /// @param[in] pgn Parameter Group Number to register.
    void registerFastPacketPgn(std::uint32_t pgn);

    /// @brief Processes an incoming 29-bit CAN frame.
    /// @details For single-frame PGNs, immediately returns the completed N2kMessage.
    ///          For Fast Packet PGNs, reassembles frames until total payload is accumulated.
    /// @param[in] frame Raw CAN frame (ID, DLC, payload bytes).
    /// @return Completed N2kMessage if assembly completed, std::nullopt if frame was an intermediate fragment.
    [[nodiscard]] std::optional<N2kMessage> processCanFrame(const CanFrame& frame);

    /// @brief Prunes incomplete reassembly sessions that have timed out.
    /// @param[in] timeout Maximum allowed age for an open Fast Packet assembly session.
    void pruneTimedOutSessions(std::chrono::milliseconds timeout = std::chrono::milliseconds(500));

    /// @brief Clears all ongoing reassembly sessions.
    void clear();

private:
    struct FastPacketSession {
        std::uint8_t sequenceCounter { 0U };
        std::uint8_t expectedFrame { 0U };
        std::size_t totalBytes { 0U };
        std::vector<std::uint8_t> buffer {};
        std::chrono::steady_clock::time_point startTime {};
    };

    [[nodiscard]] static std::uint64_t makeSessionKey(std::uint8_t sourceAddress, std::uint32_t pgn) noexcept
    {
        return (static_cast<std::uint64_t>(sourceAddress) << 32) | static_cast<std::uint64_t>(pgn);
    }

    mutable std::mutex m_mutex {};
    std::unordered_set<std::uint32_t> m_fastPacketPgns {};
    std::unordered_map<std::uint64_t, FastPacketSession> m_sessions {};
};

} // namespace Nmea::N2k
