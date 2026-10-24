#pragma once

/// @file StubRecordingAdapter.h
/// @brief In-memory test and simulation adapter for ONVIF Profile G services.

#include <cstdint>

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "OnvifServerTypes.h"

namespace Onvif {

/// @class StubRecordingAdapter
/// @brief In-memory reference provider for Profile G Recording, Search, and Replay services.
/// @details Implements IRecordingHandler, ISearchHandler, and IReplayHandler per ISP.
class StubRecordingAdapter : public IRecordingHandler, public ISearchHandler, public IReplayHandler {
public:
    /// @brief Constructs adapter pre-populated with reference recording and job configurations.
    StubRecordingAdapter();

    ~StubRecordingAdapter() override = default;

    StubRecordingAdapter(const StubRecordingAdapter&) = delete;
    StubRecordingAdapter& operator=(const StubRecordingAdapter&) = delete;
    StubRecordingAdapter(StubRecordingAdapter&&) = delete;
    StubRecordingAdapter& operator=(StubRecordingAdapter&&) = delete;

    // =========================================================================
    // IRecordingHandler Implementation
    // =========================================================================

    /// @brief Creates a new edge recording container.
    /// @param[in] config Recording container properties.
    /// @return Assigned recording token string.
    [[nodiscard]] std::string handleCreateRecording(const RecordingConfig& config) override;

    /// @brief Retrieves all configured recording containers.
    /// @return Vector of RecordingConfig structures.
    [[nodiscard]] std::vector<RecordingConfig> handleGetRecordings() override;

    /// @brief Retrieves configuration attributes for a specific recording container.
    /// @param[in] recordingToken Recording token.
    /// @return RecordingConfig or nullopt if not found.
    [[nodiscard]] std::optional<RecordingConfig> handleGetRecordingConfig(const std::string& recordingToken) override;

    /// @brief Updates configuration attributes of an existing recording container.
    /// @param[in] recordingToken Recording token.
    /// @param[in] config Updated configuration.
    /// @return True on success.
    bool handleSetRecordingConfig(const std::string& recordingToken, const RecordingConfig& config) override;

    /// @brief Deletes a recording container by token.
    /// @param[in] recordingToken Recording token to delete.
    /// @return True on success.
    [[nodiscard]] bool handleDeleteRecording(const std::string& recordingToken) override;

    /// @brief Retrieves list of active automated recording jobs.
    /// @return Vector of RecordingJob structures.
    [[nodiscard]] std::vector<RecordingJob> handleGetRecordingJobs() override;

    /// @brief Creates a new automated recording job.
    /// @param[in] job Recording job definition.
    /// @return Assigned job token string.
    [[nodiscard]] std::string handleCreateRecordingJob(const RecordingJob& job) override;

    /// @brief Modifies recording job mode.
    /// @param[in] jobToken Job token.
    /// @param[in] mode Desired mode.
    /// @return True on success.
    [[nodiscard]] bool handleSetRecordingJobMode(const std::string& jobToken, RecordingJobMode mode) override;

    /// @brief Deletes a recording job by token.
    /// @param[in] jobToken Job token to delete.
    /// @return True on success.
    [[nodiscard]] bool handleDeleteRecordingJob(const std::string& jobToken) override;

    /// @brief Retrieves aggregated storage volume and time window metrics.
    /// @return RecordingSummary structure.
    [[nodiscard]] RecordingSummary handleGetRecordingSummary() override;

    /// @brief Retrieves all tracks associated with a recording.
    /// @param[in] recordingToken Recording token.
    /// @return Vector of RecordingTrack structures.
    [[nodiscard]] std::vector<RecordingTrack> handleGetTracks(const std::string& recordingToken) override;

    /// @brief Creates a track within a recording container.
    /// @param[in] recordingToken Recording token.
    /// @param[in] track Track parameters.
    /// @return Assigned track token string.
    [[nodiscard]] std::string handleCreateTrack(
        const std::string& recordingToken, const RecordingTrack& track) override;

    /// @brief Deletes a track from a recording container.
    /// @param[in] recordingToken Recording token.
    /// @param[in] trackToken Track token to delete.
    /// @return True on success.
    [[nodiscard]] bool handleDeleteTrack(const std::string& recordingToken, const std::string& trackToken) override;

    // =========================================================================
    // ISearchHandler Implementation
    // =========================================================================

    /// @brief Initiates historical recording search query.
    /// @param[in] scope Search scope.
    /// @param[in] maxMatches Maximum match count.
    /// @param[in] keepAliveTime Session keepalive string.
    /// @return Unique search session token string.
    [[nodiscard]] std::string handleFindRecordings(
        const std::string& scope, int maxMatches, const std::string& keepAliveTime) override;

    /// @brief Polls results for an active recording search query.
    /// @param[in] searchToken Search session token.
    /// @return Vector of RecordingSearchResult matches.
    [[nodiscard]] std::vector<RecordingSearchResult> handleGetRecordingSearch(const std::string& searchToken) override;

    /// @brief Initiates historical recorded events search query.
    /// @param[in] startUtc Start timestamp.
    /// @param[in] endUtc End timestamp.
    /// @param[in] maxMatches Maximum match count.
    /// @return Unique search session token string.
    [[nodiscard]] std::string handleFindEvents(
        const std::string& startUtc, const std::string& endUtc, int maxMatches) override;

    /// @brief Polls results for an active event search query.
    /// @param[in] searchToken Search session token.
    /// @return Vector of RecordedEventResult matches.
    [[nodiscard]] std::vector<RecordedEventResult> handleGetEventSearchResults(const std::string& searchToken) override;

    /// @brief Closes an active search query session.
    /// @param[in] searchToken Search session token.
    /// @return True on success.
    [[nodiscard]] bool handleEndSearch(const std::string& searchToken) override;

    // =========================================================================
    // IReplayHandler Implementation
    // =========================================================================

    /// @brief Generates an RTSP replay URI for playback of a recorded track.
    /// @param[in] recordingToken Target recording token.
    /// @param[in] trackToken Target track token.
    /// @return RTSP stream replay URI.
    [[nodiscard]] std::string handleGetReplayUri(
        const std::string& recordingToken, const std::string& trackToken) override;

    /// @brief Retrieves current replay session configuration.
    /// @return ReplayConfiguration structure.
    [[nodiscard]] ReplayConfiguration handleGetReplayConfiguration() override;

    /// @brief Configures replay session parameters.
    /// @param[in] config Updated replay parameters.
    /// @return True on success.
    [[nodiscard]] bool handleSetReplayConfiguration(const ReplayConfiguration& config) override;

private:
    mutable std::mutex m_mutex { };
    std::vector<RecordingConfig> m_recordings { };
    std::vector<RecordingJob> m_recordingJobs { };
    ReplayConfiguration m_replayConfig { };
    std::map<std::string, std::vector<RecordingSearchResult>> m_recordingSearches { };
    std::map<std::string, std::vector<RecordedEventResult>> m_eventSearches { };
    std::uint32_t m_nextSearchSessionId { 1U };
};

} // namespace Onvif
