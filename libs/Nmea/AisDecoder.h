#pragma once

/// @file AisDecoder.h
/// @brief Multi-sentence assembler and message decoder for ITU-R M.1371 !AIVDM and !AIVDO messages.

#include "AisBitReader.h"
#include "AisTypes.h"
#include "NmeaSentenceParser.h"

#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace Nmea {

/// @class AisDecoder
/// @brief Thread-safe decoder reconstructing and deserializing multi-part and single-part AIS messages.
class AisDecoder {
public:
    /// @brief Default constructor initializing default fragment TTL (5.0 seconds).
    AisDecoder();
    ~AisDecoder() = default;

    // Non-copyable, non-movable for address stability and thread safety
    AisDecoder(const AisDecoder&) = delete;
    AisDecoder& operator=(const AisDecoder&) = delete;
    AisDecoder(AisDecoder&&) = delete;
    AisDecoder& operator=(AisDecoder&&) = delete;

    /// @brief Ingests an !AIVDM or !AIVDO sentence and decodes vessel telemetry if message is complete.
    /// @param[in] sentence Complete NMEA/AIS sentence string.
    /// @param[out] outTarget Deserialized vessel target state snapshot.
    /// @param[in] verifyChecksum True to enforce 8-bit XOR checksum validation.
    /// @return True if a complete AIS message was successfully assembled and parsed.
    [[nodiscard]] bool decodeSentence(
        std::string_view sentence, AisVesselTarget& outTarget, bool verifyChecksum = true);

    /// @brief Decodes a pre-assembled armored payload directly without multi-part reassembly.
    /// @param[in] armoredPayload 6-bit armored ASCII string.
    /// @param[in] fillBits Trailing padding bits count (0-5).
    /// @param[out] outTarget Deserialized vessel target.
    /// @return True if message was recognized and parsed successfully.
    [[nodiscard]] static bool decodePayload(
        std::string_view armoredPayload, std::size_t fillBits, AisVesselTarget& outTarget) noexcept;

    /// @brief Decodes Types 1, 2, and 3 (Class A Position Report).
    [[nodiscard]] static bool decodeClassAPosition(AisBitReader& reader, AisVesselTarget& target) noexcept;

    /// @brief Decodes Type 5 (Class A Static and Voyage Data).
    [[nodiscard]] static bool decodeClassAStatic(AisBitReader& reader, AisVesselTarget& target) noexcept;

    /// @brief Decodes Type 18 (Standard Class B Position Report).
    [[nodiscard]] static bool decodeClassBPosition(AisBitReader& reader, AisVesselTarget& target) noexcept;

    /// @brief Decodes Type 19 (Extended Class B Position Report).
    [[nodiscard]] static bool decodeClassBExtendedPosition(AisBitReader& reader, AisVesselTarget& target) noexcept;

    /// @brief Decodes Type 24 (Class B Static Data Report, Parts A and B).
    [[nodiscard]] static bool decodeClassBStatic(AisBitReader& reader, AisVesselTarget& target) noexcept;

    /// @brief Clears pending multi-part fragments.
    void reset();

    /// @brief Sets maximum expiration age for incomplete multi-part sentences.
    /// @param[in] ttl Timeout duration.
    void setFragmentTtl(std::chrono::milliseconds ttl) noexcept;

private:
    struct MultiPartFragment {
        std::uint8_t totalParts { 0U };
        std::uint8_t lastPart { 0U };
        std::string combinedPayload {};
        std::size_t fillBits { 0U };
        std::chrono::steady_clock::time_point lastSeen { std::chrono::steady_clock::now() };
    };

    void pruneStaleFragmentsLocked(std::chrono::steady_clock::time_point now);

    mutable std::mutex m_mutex {};
    std::map<std::pair<char, std::uint8_t>, MultiPartFragment> m_fragments {};
    std::chrono::milliseconds m_fragmentTtl { 5000 };
};

} // namespace Nmea
