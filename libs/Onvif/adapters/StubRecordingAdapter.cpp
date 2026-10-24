#include "StubRecordingAdapter.h"

#include <cstdint>

#include <algorithm>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace Onvif {

StubRecordingAdapter::StubRecordingAdapter()
{
    RecordingConfig defRec;
    defRec.recordingToken = "Rec_Main";
    defRec.content = "Continuous Surveillance Recording";
    defRec.sourceToken = "VideoSource_1";
    defRec.maximumRetentionTime = "P30D";

    RecordingTrack vidTrack;
    vidTrack.trackToken = "Track_Video_1";
    vidTrack.trackType = RecordingTrackType::Video;
    vidTrack.description = "Main H.264 Video Track";
    defRec.tracks.push_back(vidTrack);

    RecordingTrack audTrack;
    audTrack.trackToken = "Track_Audio_1";
    audTrack.trackType = RecordingTrackType::Audio;
    audTrack.description = "AAC Audio Track";
    defRec.tracks.push_back(audTrack);
    m_recordings.push_back(defRec);

    RecordingJob defJob;
    defJob.jobToken = "Job_Continuous";
    defJob.recordingToken = "Rec_Main";
    defJob.mode = RecordingJobMode::Active;
    defJob.priority = 5;
    defJob.sourceToken = "VideoSource_1";
    m_recordingJobs.push_back(defJob);

    m_replayConfig.sessionTimeout = "PT60S";
}

// =========================================================================
// IRecordingHandler Implementation
// =========================================================================

std::string StubRecordingAdapter::handleCreateRecording(const RecordingConfig& config)
{
    std::scoped_lock lock(m_mutex);
    RecordingConfig rec = config;
    if (rec.recordingToken.empty()) {
        rec.recordingToken = "Rec_" + std::to_string(m_recordings.size() + 1U);
    }
    m_recordings.push_back(rec);
    return rec.recordingToken;
}

std::vector<RecordingConfig> StubRecordingAdapter::handleGetRecordings()
{
    std::scoped_lock lock(m_mutex);
    return m_recordings;
}

std::optional<RecordingConfig> StubRecordingAdapter::handleGetRecordingConfig(const std::string& recordingToken)
{
    std::scoped_lock lock(m_mutex);
    for (const auto& r : m_recordings) {
        if (r.recordingToken == recordingToken) {
            return r;
        }
    }
    return std::nullopt;
}

bool StubRecordingAdapter::handleSetRecordingConfig(const std::string& recordingToken, const RecordingConfig& config)
{
    std::scoped_lock lock(m_mutex);
    for (auto& r : m_recordings) {
        if (r.recordingToken == recordingToken) {
            r.sourceToken = config.sourceToken;
            r.content = config.content;
            r.maximumRetentionTime = config.maximumRetentionTime;
            return true;
        }
    }
    return false;
}

bool StubRecordingAdapter::handleDeleteRecording(const std::string& recordingToken)
{
    std::scoped_lock lock(m_mutex);
    const auto origSize = m_recordings.size();
    m_recordings.erase(std::remove_if(m_recordings.begin(), m_recordings.end(),
                           [&recordingToken](const RecordingConfig& r) { return r.recordingToken == recordingToken; }),
        m_recordings.end());
    return m_recordings.size() < origSize;
}

std::vector<RecordingJob> StubRecordingAdapter::handleGetRecordingJobs()
{
    std::scoped_lock lock(m_mutex);
    return m_recordingJobs;
}

std::string StubRecordingAdapter::handleCreateRecordingJob(const RecordingJob& job)
{
    std::scoped_lock lock(m_mutex);
    RecordingJob j = job;
    if (j.jobToken.empty()) {
        j.jobToken = "Job_" + std::to_string(m_recordingJobs.size() + 1U);
    }
    m_recordingJobs.push_back(j);
    return j.jobToken;
}

bool StubRecordingAdapter::handleSetRecordingJobMode(const std::string& jobToken, RecordingJobMode mode)
{
    std::scoped_lock lock(m_mutex);
    for (auto& j : m_recordingJobs) {
        if (j.jobToken == jobToken) {
            j.mode = mode;
            return true;
        }
    }
    return false;
}

bool StubRecordingAdapter::handleDeleteRecordingJob(const std::string& jobToken)
{
    std::scoped_lock lock(m_mutex);
    const auto origSize = m_recordingJobs.size();
    m_recordingJobs.erase(std::remove_if(m_recordingJobs.begin(), m_recordingJobs.end(),
                              [&jobToken](const RecordingJob& j) { return j.jobToken == jobToken; }),
        m_recordingJobs.end());
    return m_recordingJobs.size() < origSize;
}

RecordingSummary StubRecordingAdapter::handleGetRecordingSummary()
{
    std::scoped_lock lock(m_mutex);
    RecordingSummary sum { };
    sum.numberRecordings = static_cast<int>(m_recordings.size());
    sum.dataFrom = "2026-01-01T00:00:00Z";
    sum.dataUntil = "2026-09-17T00:00:00Z";
    sum.totalStorageBytes = 1073741824ULL;
    return sum;
}

std::vector<RecordingTrack> StubRecordingAdapter::handleGetTracks(const std::string& recordingToken)
{
    std::scoped_lock lock(m_mutex);
    for (const auto& r : m_recordings) {
        if (r.recordingToken == recordingToken) {
            return r.tracks;
        }
    }
    return { };
}

std::string StubRecordingAdapter::handleCreateTrack(const std::string& recordingToken, const RecordingTrack& track)
{
    std::scoped_lock lock(m_mutex);
    for (auto& r : m_recordings) {
        if (r.recordingToken == recordingToken) {
            RecordingTrack t = track;
            if (t.trackToken.empty()) {
                t.trackToken = "Track_" + std::to_string(r.tracks.size() + 1U);
            }
            r.tracks.push_back(t);
            return t.trackToken;
        }
    }
    return { };
}

bool StubRecordingAdapter::handleDeleteTrack(const std::string& recordingToken, const std::string& trackToken)
{
    std::scoped_lock lock(m_mutex);
    for (auto& r : m_recordings) {
        if (r.recordingToken == recordingToken) {
            const auto origSize = r.tracks.size();
            r.tracks.erase(std::remove_if(r.tracks.begin(), r.tracks.end(),
                               [&trackToken](const RecordingTrack& t) { return t.trackToken == trackToken; }),
                r.tracks.end());
            return r.tracks.size() < origSize;
        }
    }
    return false;
}

// =========================================================================
// ISearchHandler Implementation
// =========================================================================

std::string StubRecordingAdapter::handleFindRecordings(
    const std::string& /*scope*/, std::int32_t /*maxMatches*/, const std::string& /*keepAliveTime*/)
{
    std::scoped_lock lock(m_mutex);
    const std::string token = "SearchSession_" + std::to_string(m_nextSearchSessionId++);
    std::vector<RecordingSearchResult> results;
    for (const auto& r : m_recordings) {
        for (const auto& trk : r.tracks) {
            RecordingSearchResult resItem;
            resItem.recordingToken = r.recordingToken;
            resItem.trackToken = trk.trackToken;
            resItem.earliestTime = "2026-01-01T00:00:00Z";
            resItem.latestTime = "2026-09-17T00:00:00Z";
            resItem.searchState = "Completed";
            results.push_back(resItem);
        }
    }
    m_recordingSearches[token] = results;
    return token;
}

std::vector<RecordingSearchResult> StubRecordingAdapter::handleGetRecordingSearch(const std::string& searchToken)
{
    std::scoped_lock lock(m_mutex);
    const auto it = m_recordingSearches.find(searchToken);
    if (it != m_recordingSearches.end()) {
        return it->second;
    }
    return { };
}

std::string StubRecordingAdapter::handleFindEvents(
    const std::string& startUtc, const std::string& /*endUtc*/, std::int32_t /*maxMatches*/)
{
    std::scoped_lock lock(m_mutex);
    const std::string token = "EventSearch_" + std::to_string(m_nextSearchSessionId++);
    std::vector<RecordedEventResult> events;
    RecordedEventResult ev;
    ev.recordingToken = "Rec_Main";
    ev.eventTime = startUtc.empty() ? "2026-09-17T00:00:00Z" : startUtc;
    ev.topic = "tns1:VideoAnalytics/Motion";
    ev.source = "VideoSource_1";
    ev.data = "State=true";
    events.push_back(ev);
    m_eventSearches[token] = events;
    return token;
}

std::vector<RecordedEventResult> StubRecordingAdapter::handleGetEventSearchResults(const std::string& searchToken)
{
    std::scoped_lock lock(m_mutex);
    const auto it = m_eventSearches.find(searchToken);
    if (it != m_eventSearches.end()) {
        return it->second;
    }
    return { };
}

bool StubRecordingAdapter::handleEndSearch(const std::string& searchToken)
{
    std::scoped_lock lock(m_mutex);
    m_recordingSearches.erase(searchToken);
    m_eventSearches.erase(searchToken);
    return true;
}

// =========================================================================
// IReplayHandler Implementation
// =========================================================================

std::string StubRecordingAdapter::handleGetReplayUri(const std::string& recordingToken, const std::string& trackToken)
{
    return "rtsp://127.0.0.1:8554/replay?recording=" + recordingToken + "&track=" + trackToken;
}

ReplayConfiguration StubRecordingAdapter::handleGetReplayConfiguration()
{
    std::scoped_lock lock(m_mutex);
    return m_replayConfig;
}

bool StubRecordingAdapter::handleSetReplayConfiguration(const ReplayConfiguration& config)
{
    std::scoped_lock lock(m_mutex);
    m_replayConfig = config;
    return true;
}

} // namespace Onvif
