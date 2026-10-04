#pragma once

/// @file TelemetryTimeIndex.h
/// @brief High-performance time index linking video timestamps with KLV telemetry.

#include "StanagScrubTypes.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Klv {

/// @class TelemetryTimeIndex
/// @brief Time-indexed multi-stream lookup engine for STANAG 4609 playback and scrubbing.
class TelemetryTimeIndex {
public:
    /// @brief Default constructor.
    TelemetryTimeIndex() = default;

    /// @brief Destructor.
    ~TelemetryTimeIndex() = default;

    /// @brief Inserts a video frame entry into the timeline.
    /// @param[in] entry Video frame indexing structure.
    void addVideoEntry(const VideoIndexEntry& entry);

    /// @brief Inserts a KLV metadata entry into the timeline.
    /// @param[in] entry KLV indexing structure.
    void addKlvEntry(const KlvIndexEntry& entry);

    /// @brief Finalizes timeline, sorts entries, and builds keyframe acceleration tables.
    void finalize();

    /// @brief Clears all indexed entries and resets time base.
    void clear() noexcept;

    /// @brief Locates the nearest preceding keyframe for a given PTS.
    /// @param[in] pts 90 kHz presentation timestamp.
    /// @return VideoIndexEntry of nearest preceding keyframe, or nullopt if none.
    [[nodiscard]] std::optional<VideoIndexEntry> findPrecedingKeyframe(std::uint64_t pts) const noexcept;

    /// @brief Locates video frame entry closest to requested presentation timestamp.
    /// @param[in] pts 90 kHz presentation timestamp.
    /// @return Closest VideoIndexEntry.
    [[nodiscard]] std::optional<VideoIndexEntry> findVideoFrame(std::uint64_t pts) const noexcept;

    /// @brief Locates telemetry entry closest to presentation timestamp within tolerance.
    /// @param[in] pts 90 kHz presentation timestamp.
    /// @param[in] maxDeltaUs Maximum allowed skew in microseconds (default 50,000 us).
    /// @return Closest KlvIndexEntry if within tolerance.
    [[nodiscard]] std::optional<KlvIndexEntry> findTelemetry(std::uint64_t pts,
                                                            std::uint64_t maxDeltaUs = 50000U) const noexcept;

    /// @brief Locates bounding pair of telemetry entries for interpolation.
    /// @param[in] pts Target 90 kHz presentation timestamp.
    /// @param[out] before Entry immediately prior to or at pts.
    /// @param[out] after Entry immediately following pts.
    /// @return True if valid bounding pair was found.
    [[nodiscard]] bool findTelemetryBounds(std::uint64_t pts,
                                           KlvIndexEntry& before,
                                           KlvIndexEntry& after) const noexcept;

    /// @brief Retrieves a contiguous window of telemetry entries for spatial trajectory rendering.
    /// @param[in] startPts Beginning of window.
    /// @param[in] endPts End of window.
    /// @return Vector of KLV entries within the requested interval.
    [[nodiscard]] std::vector<KlvIndexEntry> findWindow(std::uint64_t startPts,
                                                       std::uint64_t endPts) const;

    /// @brief Converts 90 kHz PTS to presentation time in seconds relative to origin.
    /// @param[in] pts 90 kHz presentation timestamp.
    /// @return Elapsed seconds.
    [[nodiscard]] double ptsToSeconds(std::uint64_t pts) const noexcept;

    /// @brief Converts elapsed presentation seconds to 90 kHz PTS ticks.
    /// @param[in] seconds Presentation time in seconds.
    /// @return 90 kHz PTS ticks.
    [[nodiscard]] std::uint64_t secondsToPts(double seconds) const noexcept;

    /// @brief Converts 90 kHz PTS ticks to absolute microsecond UTC epoch timestamp.
    /// @param[in] pts 90 kHz PTS ticks.
    /// @return UTC timestamp in microseconds since epoch.
    [[nodiscard]] std::uint64_t ptsToUtc(std::uint64_t pts) const noexcept;

    /// @brief Converts UTC microsecond epoch timestamp to 90 kHz PTS ticks.
    /// @param[in] utcUs UTC epoch in microseconds.
    /// @return 90 kHz PTS ticks.
    [[nodiscard]] std::uint64_t utcToPts(std::uint64_t utcUs) const noexcept;

    /// @brief Returns total number of indexed video frames.
    [[nodiscard]] std::size_t videoFrameCount() const noexcept;

    /// @brief Returns total number of indexed keyframes.
    [[nodiscard]] std::size_t keyframeCount() const noexcept;

    /// @brief Returns total number of indexed KLV packets.
    [[nodiscard]] std::size_t klvPacketCount() const noexcept;

    /// @brief Returns total timeline duration in seconds.
    [[nodiscard]] double durationSeconds() const noexcept;

    /// @brief Checks whether the timeline index is empty.
    [[nodiscard]] bool empty() const noexcept;

    /// @brief Checks whether index is finalized.
    [[nodiscard]] bool isFinalized() const noexcept;

    /// @brief Returns base PTS origin.
    [[nodiscard]] std::uint64_t basePts() const noexcept;

    /// @brief Returns base UTC timestamp in microseconds.
    [[nodiscard]] std::uint64_t baseUtcUs() const noexcept;

private:
    std::vector<VideoIndexEntry> m_videoEntries {};
    std::vector<KlvIndexEntry> m_klvEntries {};
    std::vector<std::size_t> m_keyframeIndices {};

    std::uint64_t m_basePts { 0U };
    std::uint64_t m_baseUtcUs { 0U };
    double m_durationSeconds { 0.0 };
    bool m_finalized { false };
};

} // namespace Klv
