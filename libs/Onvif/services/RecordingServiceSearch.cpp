/// @file RecordingServiceSearch.cpp
/// @brief Search and Replay service implementations for RecordingService.

#include "RecordingService.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <pugixml.hpp>

#include "SoapFault.h"
#include "SoapRouter.h"
#include "XmlUtils.h"

namespace Onvif {

namespace {

    static std::string formatIsoUtc(const std::chrono::system_clock::time_point& tp)
    {
        const std::time_t t = std::chrono::system_clock::to_time_t(tp);
        std::tm tmUtc { };
#ifdef _WIN32
        gmtime_s(&tmUtc, &t);
#else
        gmtime_r(&t, &tmUtc);
#endif
        char buf[32] { 0 };
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmUtc);
        return std::string(buf);
    }

} // namespace

SoapHandlerResult RecordingService::handleFindRecordings(const SoapContext&)
{
    std::string searchToken;
    std::vector<RecordingSearchResult> results;
    const auto searchHandler = m_ctx.getSearchHandler ? m_ctx.getSearchHandler() : nullptr;
    if (searchHandler) {
        searchToken = searchHandler->handleFindRecordings("", 10, "PT60S");
        results = searchHandler->handleGetRecordingSearch(searchToken);
    }
    if (searchToken.empty()) {
        searchToken = "SearchSession_" + std::to_string(m_nextSearchId++);
        for (const auto& r : m_recordings) {
            for (const auto& trk : r.tracks) {
                RecordingSearchResult resItem;
                resItem.recordingToken = r.recordingToken;
                resItem.trackToken = trk.trackToken;
                resItem.earliestTime = "2026-01-01T00:00:00Z";
                resItem.latestTime = formatIsoUtc(std::chrono::system_clock::now());
                resItem.searchState = "Completed";
                results.push_back(resItem);
            }
        }
        m_recSearches[searchToken] = results;
    }
    std::ostringstream ss;
    ss << "    <tse:FindRecordingsResponse>\r\n"
       << "      <tse:SearchToken>" << Xml::escapeXmlText(searchToken) << "</tse:SearchToken>\r\n"
       << "    </tse:FindRecordingsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult RecordingService::handleGetRecSearch(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='SearchToken']").node();
    const std::string searchToken = tokNode ? tokNode.text().as_string() : "";
    std::vector<RecordingSearchResult> results;
    const auto searchHandler = m_ctx.getSearchHandler ? m_ctx.getSearchHandler() : nullptr;
    if (searchHandler) {
        results = searchHandler->handleGetRecordingSearch(searchToken);
    }
    if (results.empty()) {
        auto it = m_recSearches.find(searchToken);
        if (it != m_recSearches.end()) {
            results = it->second;
        }
    }
    std::ostringstream ss;
    ss << "    <tse:GetRecordingSearchResultsResponse>\r\n"
       << "      <tse:ResultList SearchState=\"Completed\">\r\n";
    for (const auto& resItem : results) {
        ss << "        <tse:RecordingInformation>\r\n"
           << "          <tt:RecordingToken>" << Xml::escapeXmlText(resItem.recordingToken)
           << "</tt:RecordingToken>\r\n"
           << "          <tt:TrackToken>" << Xml::escapeXmlText(resItem.trackToken) << "</tt:TrackToken>\r\n"
           << "          <tt:EarliestRecording>" << Xml::escapeXmlText(resItem.earliestTime)
           << "</tt:EarliestRecording>\r\n"
           << "          <tt:LatestRecording>" << Xml::escapeXmlText(resItem.latestTime) << "</tt:LatestRecording>\r\n"
           << "          <tt:SearchState>" << Xml::escapeXmlText(resItem.searchState) << "</tt:SearchState>\r\n"
           << "        </tse:RecordingInformation>\r\n";
    }
    ss << "      </tse:ResultList>\r\n"
       << "    </tse:GetRecordingSearchResultsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult RecordingService::handleFindEvents(const SoapContext& ctx)
{
    const auto startNode = ctx.reqNode.select_node(".//*[local-name()='StartPoint']").node();
    const auto endNode = ctx.reqNode.select_node(".//*[local-name()='EndPoint']").node();
    const std::string startPt = startNode ? startNode.text().as_string() : "";
    const std::string endPt = endNode ? endNode.text().as_string() : "";
    std::string searchToken;
    const auto searchHandler = m_ctx.getSearchHandler ? m_ctx.getSearchHandler() : nullptr;
    if (searchHandler) {
        searchToken = searchHandler->handleFindEvents(startPt, endPt, 10);
    }
    if (searchToken.empty()) {
        searchToken = "EventSearch_" + std::to_string(m_nextSearchId++);
        std::vector<RecordedEventResult> evResults;
        RecordedEventResult ev1;
        ev1.recordingToken = "Rec_Main";
        ev1.eventTime = startPt.empty() ? formatIsoUtc(std::chrono::system_clock::now()) : startPt;
        ev1.topic = "tns1:VideoAnalytics/Motion";
        ev1.source = "VideoSource_1";
        ev1.data = "State=true";
        evResults.push_back(ev1);
        m_eventSearches[searchToken] = evResults;
    }
    std::ostringstream ss;
    ss << "    <tse:FindEventsResponse>\r\n"
       << "      <tse:SearchToken>" << Xml::escapeXmlText(searchToken) << "</tse:SearchToken>\r\n"
       << "    </tse:FindEventsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult RecordingService::handleGetEventSearch(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='SearchToken']").node();
    const std::string searchToken = tokNode ? tokNode.text().as_string() : "";
    std::vector<RecordedEventResult> events;
    const auto searchHandler = m_ctx.getSearchHandler ? m_ctx.getSearchHandler() : nullptr;
    if (searchHandler) {
        events = searchHandler->handleGetEventSearchResults(searchToken);
    }
    if (events.empty()) {
        auto it = m_eventSearches.find(searchToken);
        if (it != m_eventSearches.end()) {
            events = it->second;
        }
    }
    std::ostringstream ss;
    ss << "    <tse:GetEventSearchResultsResponse>\r\n"
       << "      <tse:ResultList SearchState=\"Completed\">\r\n";
    for (const auto& ev : events) {
        ss << "        <tse:EventInformation>\r\n"
           << "          <tt:RecordingToken>" << Xml::escapeXmlText(ev.recordingToken) << "</tt:RecordingToken>\r\n"
           << "          <tt:UtcTime>" << Xml::escapeXmlText(ev.eventTime) << "</tt:UtcTime>\r\n"
           << "          <tt:Topic>" << Xml::escapeXmlText(ev.topic) << "</tt:Topic>\r\n"
           << "          <tt:Source>" << Xml::escapeXmlText(ev.source) << "</tt:Source>\r\n"
           << "          <tt:Data>" << Xml::escapeXmlText(ev.data) << "</tt:Data>\r\n"
           << "        </tse:EventInformation>\r\n";
    }
    ss << "      </tse:ResultList>\r\n"
       << "    </tse:GetEventSearchResultsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult RecordingService::handleEndSearch(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='SearchToken']").node();
    const std::string searchToken = tokNode ? tokNode.text().as_string() : "";
    const auto searchHandler = m_ctx.getSearchHandler ? m_ctx.getSearchHandler() : nullptr;
    if (searchHandler) {
        static_cast<void>(searchHandler->handleEndSearch(searchToken));
    }
    m_recSearches.erase(searchToken);
    m_eventSearches.erase(searchToken);
    std::ostringstream ss;
    ss << "    <tse:EndSearchResponse>\r\n"
       << "      <tse:Endpoint>" << Xml::escapeXmlText(searchToken) << "</tse:Endpoint>\r\n"
       << "    </tse:EndSearchResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult RecordingService::handleGetReplayUri(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='RecordingToken']").node();
    const std::string recToken = tokNode ? tokNode.text().as_string() : "Rec_Main";
    std::string uri;
    const auto replayHandler = m_ctx.getReplayHandler ? m_ctx.getReplayHandler() : nullptr;
    if (replayHandler) {
        uri = replayHandler->handleGetReplayUri(recToken, "Track_Video_1");
    }
    if (uri.empty()) {
        const std::string host = m_ctx.resolveHost ? m_ctx.resolveHost(ctx) : "127.0.0.1";
        uri = "rtsp://" + host + ":8554/replay?recording=" + recToken;
    }
    std::ostringstream ss;
    ss << "    <trp:GetReplayUriResponse>\r\n"
       << "      <trp:Uri>" << Xml::escapeXmlText(uri) << "</trp:Uri>\r\n"
       << "    </trp:GetReplayUriResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult RecordingService::handleGetReplayConfig(const SoapContext&)
{
    ReplayConfiguration cfg = m_replayConfig;
    const auto replayHandler = m_ctx.getReplayHandler ? m_ctx.getReplayHandler() : nullptr;
    if (replayHandler) {
        cfg = replayHandler->handleGetReplayConfiguration();
    }
    std::ostringstream ss;
    ss << "    <trp:GetReplayConfigurationResponse>\r\n"
       << "      <trp:Configuration>\r\n"
       << "        <tt:SessionTimeout>" << Xml::escapeXmlText(cfg.sessionTimeout) << "</tt:SessionTimeout>\r\n"
       << "      </trp:Configuration>\r\n"
       << "    </trp:GetReplayConfigurationResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult RecordingService::handleSetReplayConfig(const SoapContext& ctx)
{
    const auto timeoutNode = ctx.reqNode.select_node(".//*[local-name()='SessionTimeout']").node();
    std::string timeoutStr = timeoutNode ? timeoutNode.text().as_string() : "PT60S";
    ReplayConfiguration cfg;
    cfg.sessionTimeout = timeoutStr;
    const auto replayHandler = m_ctx.getReplayHandler ? m_ctx.getReplayHandler() : nullptr;
    if (replayHandler) {
        static_cast<void>(replayHandler->handleSetReplayConfiguration(cfg));
    }
    m_replayConfig.sessionTimeout = timeoutStr;
    return SoapHandlerResult::ok("    <trp:SetReplayConfigurationResponse/>\r\n");
}

} // namespace Onvif
