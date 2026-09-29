#include "N2kFastPacketAssembler.h"

#include <algorithm>

namespace Nmea::N2k {

N2kFastPacketAssembler::N2kFastPacketAssembler()
{
    // Populate standard NMEA 2000 Fast Packet PGNs
    m_fastPacketPgns = {
        126996U, // Product Information
        126998U, // Configuration Information
        129029U, // GNSS Position Data
        129038U, // AIS Class A Position Report
        129039U, // AIS Class B Position Report
        129040U, // AIS Class B Extended Position Report
        129041U, // AIS Aids to Navigation (AtoN) Report
        129794U, // AIS Class A Static and Voyage Related Data
        129809U, // AIS Class B "CS" Static Data Part A
        129810U // AIS Class B "CS" Static Data Part B
    };
}

bool N2kFastPacketAssembler::isFastPacketPgn(std::uint32_t pgn) const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_fastPacketPgns.find(pgn) != m_fastPacketPgns.end();
}

void N2kFastPacketAssembler::registerFastPacketPgn(std::uint32_t pgn)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_fastPacketPgns.insert(pgn);
}

std::optional<N2kMessage> N2kFastPacketAssembler::processCanFrame(const CanFrame& frame)
{
    const N2kHeader header = N2kHeader::fromCanId(frame.id);
    const auto now = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(m_mutex);

    // If single frame (not registered as Fast Packet), return message directly
    if (m_fastPacketPgns.find(header.pgn) == m_fastPacketPgns.end()) {
        N2kMessage msg {};
        msg.header = header;
        msg.timestamp = now;
        const std::size_t count = std::min<std::size_t>(frame.dlc, 8U);
        msg.payload.assign(frame.data.begin(), frame.data.begin() + static_cast<std::ptrdiff_t>(count));
        return msg;
    }

    // Fast Packet multi-frame protocol
    if (frame.dlc < 2U) {
        return std::nullopt;
    }

    const std::uint8_t seqCounter = static_cast<std::uint8_t>((frame.data[0] >> 3) & 0x1FU);
    const std::uint8_t frameCounter = static_cast<std::uint8_t>(frame.data[0] & 0x07U);
    const std::uint64_t sessionKey = makeSessionKey(header.sourceAddress, header.pgn);

    if (frameCounter == 0U) {
        // First frame of Fast Packet sequence
        const std::size_t totalBytes = frame.data[1];
        if (totalBytes == 0U || totalBytes > 250U) {
            // Invalid total byte count
            m_sessions.erase(sessionKey);
            return std::nullopt;
        }

        FastPacketSession session {};
        session.sequenceCounter = seqCounter;
        session.expectedFrame = 1U;
        session.totalBytes = totalBytes;
        session.startTime = now;
        session.buffer.reserve(totalBytes);

        const std::size_t chunk = std::min<std::size_t>(static_cast<std::size_t>(frame.dlc - 2U), totalBytes);
        session.buffer.insert(
            session.buffer.end(), frame.data.begin() + 2, frame.data.begin() + 2 + static_cast<std::ptrdiff_t>(chunk));

        if (session.buffer.size() >= session.totalBytes) {
            // Single-frame complete Fast Packet (small payload <= 6 bytes)
            N2kMessage msg {};
            msg.header = header;
            msg.timestamp = now;
            msg.payload = std::move(session.buffer);
            m_sessions.erase(sessionKey);
            return msg;
        }

        m_sessions[sessionKey] = std::move(session);
        return std::nullopt;
    }

    // Subsequent frame (frameCounter > 0)
    auto it = m_sessions.find(sessionKey);
    if (it == m_sessions.end()) {
        // Missing start frame or timed out
        return std::nullopt;
    }

    FastPacketSession& session = it->second;

    // Check sequence counter match
    if (session.sequenceCounter != seqCounter) {
        // Disjoint sequence counter, discard corrupted session
        m_sessions.erase(it);
        return std::nullopt;
    }

    // Check frame order counter (with wrap-around modulo 8)
    if (frameCounter != (session.expectedFrame & 0x07U)) {
        // Out-of-order or dropped frame
        m_sessions.erase(it);
        return std::nullopt;
    }

    const std::size_t remaining = session.totalBytes - session.buffer.size();
    const std::size_t available = static_cast<std::size_t>(frame.dlc - 1U);
    const std::size_t chunk = std::min<std::size_t>(available, remaining);

    session.buffer.insert(
        session.buffer.end(), frame.data.begin() + 1, frame.data.begin() + 1 + static_cast<std::ptrdiff_t>(chunk));
    session.expectedFrame = static_cast<std::uint8_t>((session.expectedFrame + 1U) & 0xFFU);

    if (session.buffer.size() >= session.totalBytes) {
        N2kMessage msg {};
        msg.header = header;
        msg.timestamp = now;
        msg.payload = std::move(session.buffer);
        m_sessions.erase(it);
        return msg;
    }

    return std::nullopt;
}

void N2kFastPacketAssembler::pruneTimedOutSessions(std::chrono::milliseconds timeout)
{
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto it = m_sessions.begin(); it != m_sessions.end();) {
        if ((now - it->second.startTime) > timeout) {
            it = m_sessions.erase(it);
        } else {
            ++it;
        }
    }
}

void N2kFastPacketAssembler::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessions.clear();
}

} // namespace Nmea::N2k
