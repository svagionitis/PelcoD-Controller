#pragma once

/// @file NmeaGateway.h
/// @brief Bidirectional gateway converting between NMEA 0183 sentences and NMEA 2000 CAN frames.

#include "NmeaSentenceBuilder.h"
#include "NmeaSentenceParser.h"
#include "NmeaTypes.h"
#include "n2k/N2kDecoder.h"
#include "n2k/N2kEncoder.h"
#include "n2k/N2kFastPacketAssembler.h"
#include "n2k/N2kTypes.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Nmea::Gateway {

/// @brief Gateway operational configuration.
struct GatewayConfig {
    bool enableN2kTo0183 { true }; ///< Enable CAN -> NMEA 0183 translation
    bool enable0183ToN2k { true }; ///< Enable NMEA 0183 -> CAN translation
    std::uint8_t defaultCanSource { 0x23U }; ///< Outbound CAN source node address
    std::string defaultTalkerId { "GP" }; ///< Outbound NMEA 0183 talker identifier
    std::chrono::milliseconds defaultDecimationInterval { 100 }; ///< Default rate limit per PGN/sentence
};

/// @brief Gateway traffic and conversion metrics.
struct GatewayStats {
    std::uint64_t n2kFramesReceived { 0U };
    std::uint64_t n2kMessagesDecoded { 0U };
    std::uint64_t sentencesEmitted { 0U };
    std::uint64_t sentencesReceived { 0U };
    std::uint64_t sentencesParsed { 0U };
    std::uint64_t n2kFramesEmitted { 0U };
    std::uint64_t throttledDrops { 0U };
};

/// @class NmeaGateway
/// @brief Bidirectional bridge converting between NMEA 0183 serial/network sentences and NMEA 2000 CAN frames.
/// @details Features rate decimation to protect slow serial lines (e.g. 4800 baud) from high-frequency CAN floods,
///          Fast Packet reassembly, and thread-safe callbacks.
class NmeaGateway {
public:
    using SentenceOutputCallback = std::function<void(std::string_view sentence)>;
    using CanFrameOutputCallback = std::function<void(const N2k::CanFrame& frame)>;

    explicit NmeaGateway(const GatewayConfig& config = {});
    virtual ~NmeaGateway() = default;

    // Non-copyable, non-movable
    NmeaGateway(const NmeaGateway&) = delete;
    NmeaGateway& operator=(const NmeaGateway&) = delete;
    NmeaGateway(NmeaGateway&&) = delete;
    NmeaGateway& operator=(NmeaGateway&&) = delete;

    // --- Configuration ---

    /// @brief Updates gateway configuration.
    void setConfig(const GatewayConfig& config);

    /// @brief Retrieves current gateway configuration.
    [[nodiscard]] GatewayConfig config() const;

    /// @brief Sets minimum interval between sentence emissions for a specific PGN.
    /// @param[in] pgn Parameter Group Number (e.g. 129025).
    /// @param[in] minInterval Minimum elapsed time between emissions.
    void setPgnDecimation(std::uint32_t pgn, std::chrono::milliseconds minInterval);

    /// @brief Sets minimum interval between CAN transmissions for a specific sentence ID (e.g. "GGA", "HDT").
    /// @param[in] sentenceId 3-character sentence mnemonic.
    /// @param[in] minInterval Minimum elapsed time between transmissions.
    void setSentenceDecimation(std::string_view sentenceId, std::chrono::milliseconds minInterval);

    // --- Callback Registration ---

    /// @brief Sets callback for outgoing NMEA 0183 sentences synthesized from N2K.
    void setSentenceOutputCallback(SentenceOutputCallback cb);

    /// @brief Sets callback for outgoing NMEA 2000 CAN frames synthesized from NMEA 0183.
    void setCanFrameOutputCallback(CanFrameOutputCallback cb);

    // --- Ingestion Handlers ---

    /// @brief Ingests an inbound NMEA 2000 CAN frame.
    /// @param[in] frame Raw 29-bit CAN frame.
    void onCanFrame(const N2k::CanFrame& frame);

    /// @brief Ingests an assembled NMEA 2000 message.
    /// @param[in] msg Reassembled N2K message.
    void onN2kMessage(const N2k::N2kMessage& msg);

    /// @brief Ingests an inbound NMEA 0183 sentence.
    /// @param[in] sentence Full NMEA sentence (with or without checksum).
    void onSentence(std::string_view sentence);

    // --- Telemetry Cache / State Accessors ---

    [[nodiscard]] std::optional<N2k::PositionRapid> lastPosition() const;
    [[nodiscard]] std::optional<N2k::CogSogRapid> lastCogSog() const;
    [[nodiscard]] std::optional<N2k::VesselHeading> lastHeading() const;
    [[nodiscard]] std::optional<N2k::Attitude> lastAttitude() const;
    [[nodiscard]] std::optional<N2k::WindData> lastWind() const;

    /// @brief Retrieves traffic and conversion statistics.
    [[nodiscard]] GatewayStats stats() const;

    /// @brief Resets traffic statistics to zero.
    void resetStats();

private:
    void processDecodedN2k(const N2k::N2kMessage& msg);
    [[nodiscard]] bool shouldEmitPgn(std::uint32_t pgn, std::chrono::steady_clock::time_point now);
    [[nodiscard]] bool shouldEmitSentence(std::string_view id, std::chrono::steady_clock::time_point now);

    mutable std::mutex m_mutex {};
    GatewayConfig m_config {};
    GatewayStats m_stats {};

    SentenceOutputCallback m_sentenceCallback {};
    CanFrameOutputCallback m_canCallback {};

    N2k::N2kFastPacketAssembler m_assembler {};

    // Telemetry state cache for combining PGNs (e.g. 129025 pos + 129026 cog/sog for RMC)
    std::optional<N2k::PositionRapid> m_lastPos {};
    std::optional<N2k::CogSogRapid> m_lastCogSog {};
    std::optional<N2k::VesselHeading> m_lastHeading {};
    std::optional<N2k::Attitude> m_lastAttitude {};
    std::optional<N2k::WindData> m_lastWind {};

    // Rate decimation tracking
    std::unordered_map<std::uint32_t, std::chrono::milliseconds> m_pgnIntervals {};
    std::unordered_map<std::uint32_t, std::chrono::steady_clock::time_point> m_pgnLastEmitted {};

    std::unordered_map<std::string, std::chrono::milliseconds> m_sentenceIntervals {};
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_sentenceLastEmitted {};
};

} // namespace Nmea::Gateway
