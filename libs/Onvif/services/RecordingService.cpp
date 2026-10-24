/// @file RecordingService.cpp
/// @brief Modular ONVIF Profile G Recording service implementation.

#include "RecordingService.h"

#include <cstdint>

#include <algorithm>
#include <chrono>
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
    constexpr std::string_view kRecordingNs { "http://www.onvif.org/ver10/recording/wsdl" };
    constexpr std::string_view kSearchNs { "http://www.onvif.org/ver10/search/wsdl" };
    constexpr std::string_view kReplayNs { "http://www.onvif.org/ver10/replay/wsdl" };
}

RecordingService::RecordingService(RecordingServiceContext ctx)
    : m_ctx(std::move(ctx))
{
    RecordingConfig defRec;
    defRec.recordingToken = "Rec_Main";
    defRec.content = "Continuous Recording";
    defRec.sourceToken = "VideoSource_1";
    defRec.maximumRetentionTime = "P30D";
    defRec.tracks.push_back({ "Track_Video_1", RecordingTrackType::Video, "Primary Video Track" });
    m_recordings.push_back(defRec);

    RecordingJob defJob;
    defJob.jobToken = "Job_Continuous";
    defJob.recordingToken = "Rec_Main";
    defJob.mode = RecordingJobMode::Active;
    defJob.priority = 5;
    defJob.sourceToken = "Profile_1";
    m_jobs.push_back(defJob);

    m_replayConfig.sessionTimeout = "PT30S";
}

std::string_view RecordingService::serviceName() const noexcept
{
    return "Recording";
}

void RecordingService::registerRoutes(SoapRouter& router)
{
    const std::string recNs { kRecordingNs };
    const std::string seaNs { kSearchNs };
    const std::string repNs { kReplayNs };

    router.registerRoute(recNs, "GetServiceCapabilities", AccessClass::PreAuth, [](const SoapContext&) {
        return SoapHandlerResult::ok("    <trc:GetServiceCapabilitiesResponse>\r\n"
                                     "      <trc:Capabilities DynamicRecordings=\"true\" DynamicTracks=\"true\" "
                                     "Encoding=\"H264\" MaxRecordings=\"16\" MaxRecordingJobs=\"16\"/>\r\n"
                                     "    </trc:GetServiceCapabilitiesResponse>\r\n");
    });
    router.registerRoute(recNs, "GetRecordings", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetRecordings(ctx); });
    router.registerRoute(recNs, "GetRecordingConfiguration", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetRecConfig(ctx); });
    router.registerRoute(recNs, "SetRecordingConfiguration", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleSetRecConfig(ctx); });
    router.registerRoute(recNs, "CreateRecording", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleCreateRecord(ctx); });
    router.registerRoute(recNs, "DeleteRecording", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleDeleteRecord(ctx); });
    router.registerRoute(recNs, "CreateTrack", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleCreateTrack(ctx); });
    router.registerRoute(recNs, "GetTrackConfiguration", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetTrackConfig(ctx); });
    router.registerRoute(recNs, "SetTrackConfiguration", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleSetTrackConfig(ctx); });
    router.registerRoute(recNs, "DeleteTrack", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleDeleteTrack(ctx); });
    router.registerRoute(recNs, "GetRecordingJobs", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetRecordJobs(ctx); });
    router.registerRoute(recNs, "CreateRecordingJob", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleCreateRecJob(ctx); });
    router.registerRoute(recNs, "SetRecordingJobMode", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleSetJobMode(ctx); });
    router.registerRoute(recNs, "DeleteRecordingJob", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleDeleteRecJob(ctx); });
    router.registerRoute(recNs, "GetRecordingSummary", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetRecSummary(ctx); });

    router.registerRoute(seaNs, "GetServiceCapabilities", AccessClass::PreAuth, [](const SoapContext&) {
        return SoapHandlerResult::ok("    <tse:GetServiceCapabilitiesResponse>\r\n"
                                     "      <tse:Capabilities MetadataSearch=\"true\" General=\"true\"/>\r\n"
                                     "    </tse:GetServiceCapabilitiesResponse>\r\n");
    });
    router.registerRoute(seaNs, "FindRecordings", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleFindRecordings(ctx); });
    router.registerRoute(seaNs, "GetRecordingSearchResults", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetRecSearch(ctx); });
    router.registerRoute(
        seaNs, "FindEvents", AccessClass::ReadMedia, [this](const SoapContext& ctx) { return handleFindEvents(ctx); });
    router.registerRoute(seaNs, "GetEventSearchResults", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetEventSearch(ctx); });
    router.registerRoute(
        seaNs, "EndSearch", AccessClass::ReadMedia, [this](const SoapContext& ctx) { return handleEndSearch(ctx); });

    router.registerRoute(repNs, "GetServiceCapabilities", AccessClass::PreAuth, [](const SoapContext&) {
        return SoapHandlerResult::ok(
            "    <trp:GetServiceCapabilitiesResponse>\r\n"
            "      <trp:Capabilities ReversePlayback=\"false\" SessionTimeoutRange=\"PT10S PT60S\"/>\r\n"
            "    </trp:GetServiceCapabilitiesResponse>\r\n");
    });
    router.registerRoute(repNs, "GetReplayUri", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetReplayUri(ctx); });
    router.registerRoute(repNs, "GetReplayConfiguration", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetReplayConfig(ctx); });
    router.registerRoute(repNs, "SetReplayConfiguration", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleSetReplayConfig(ctx); });
}

SoapHandlerResult RecordingService::handleGetRecordings(const SoapContext&)
{
    std::vector<RecordingConfig> recs = m_recordings;
    const auto h = m_ctx.getRecordingHandler ? m_ctx.getRecordingHandler() : nullptr;
    if (h) {
        recs = h->handleGetRecordings();
    }
    std::ostringstream ss;
    ss << "    <trc:GetRecordingsResponse>\r\n";
    for (const auto& r : recs) {
        ss << "      <trc:RecordingItem token=\"" << Xml::escapeXmlAttr(r.recordingToken) << "\">\r\n"
           << "        <tt:Source><tt:SourceToken>" << Xml::escapeXmlText(r.sourceToken)
           << "</tt:SourceToken></tt:Source>\r\n"
           << "        <tt:Content>" << Xml::escapeXmlText(r.content) << "</tt:Content>\r\n"
           << "        <tt:Tracks>\r\n";
        for (const auto& trk : r.tracks) {
            ss << "          <tt:TrackInformation>\r\n"
               << "            <tt:TrackToken>" << Xml::escapeXmlText(trk.trackToken) << "</tt:TrackToken>\r\n"
               << "            <tt:TrackType>" << Xml::escapeXmlText(recordingTrackTypeToString(trk.trackType))
               << "</tt:TrackType>\r\n"
               << "            <tt:Description>" << Xml::escapeXmlText(trk.description) << "</tt:Description>\r\n"
               << "          </tt:TrackInformation>\r\n";
        }
        ss << "        </tt:Tracks>\r\n"
           << "      </trc:RecordingItem>\r\n";
    }
    ss << "    </trc:GetRecordingsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult RecordingService::handleGetRecConfig(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='RecordingToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";
    const auto h = m_ctx.getRecordingHandler ? m_ctx.getRecordingHandler() : nullptr;
    if (h) {
        const auto cfg = h->handleGetRecordingConfig(tok);
        if (cfg) {
            std::ostringstream ss;
            ss << "    <trc:GetRecordingConfigurationResponse>\r\n"
               << "      <trc:RecordingConfiguration>\r\n"
               << "        <tt:Content>" << Xml::escapeXmlText(cfg->content) << "</tt:Content>\r\n"
               << "        <tt:MaximumRetentionTime>" << Xml::escapeXmlText(cfg->maximumRetentionTime)
               << "</tt:MaximumRetentionTime>\r\n"
               << "      </trc:RecordingConfiguration>\r\n"
               << "    </trc:GetRecordingConfigurationResponse>\r\n";
            return SoapHandlerResult::ok(ss.str());
        }
    }
    for (const auto& r : m_recordings) {
        if (r.recordingToken == tok) {
            std::ostringstream ss;
            ss << "    <trc:GetRecordingConfigurationResponse>\r\n"
               << "      <trc:RecordingConfiguration>\r\n"
               << "        <tt:Content>" << Xml::escapeXmlText(r.content) << "</tt:Content>\r\n"
               << "        <tt:MaximumRetentionTime>" << Xml::escapeXmlText(r.maximumRetentionTime)
               << "</tt:MaximumRetentionTime>\r\n"
               << "      </trc:RecordingConfiguration>\r\n"
               << "    </trc:GetRecordingConfigurationResponse>\r\n";
            return SoapHandlerResult::ok(ss.str());
        }
    }
    return SoapHandlerResult::fault("ter:InvalidArgVal", "No such recording", 400);
}

SoapHandlerResult RecordingService::handleSetRecConfig(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='RecordingToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";
    const auto contentNode = ctx.reqNode.select_node(".//*[local-name()='Content']").node();
    const auto retNode = ctx.reqNode.select_node(".//*[local-name()='MaximumRetentionTime']").node();

    RecordingConfig cfg;
    cfg.recordingToken = tok;
    cfg.content = contentNode ? contentNode.text().as_string() : "";
    cfg.maximumRetentionTime = retNode ? retNode.text().as_string() : "";

    const auto h = m_ctx.getRecordingHandler ? m_ctx.getRecordingHandler() : nullptr;
    if (h) {
        static_cast<void>(h->handleSetRecordingConfig(tok, cfg));
    }
    for (auto& r : m_recordings) {
        if (r.recordingToken == tok) {
            if (contentNode)
                r.content = cfg.content;
            if (retNode)
                r.maximumRetentionTime = cfg.maximumRetentionTime;
            break;
        }
    }
    return SoapHandlerResult::ok("    <trc:SetRecordingConfigurationResponse/>\r\n");
}

SoapHandlerResult RecordingService::handleCreateRecord(const SoapContext& ctx)
{
    const auto srcNode = ctx.reqNode.select_node(".//*[local-name()='SourceId' or local-name()='SourceToken']").node();
    const auto contentNode = ctx.reqNode.select_node(".//*[local-name()='Content']").node();
    const auto retNode = ctx.reqNode.select_node(".//*[local-name()='MaximumRetentionTime']").node();

    RecordingConfig cfg;
    cfg.recordingToken = "Rec_" + std::to_string(m_recordings.size() + 1);
    cfg.sourceToken = srcNode ? srcNode.text().as_string() : "VideoSource_1";
    cfg.content = contentNode ? contentNode.text().as_string() : "Surveillance Recording";
    cfg.maximumRetentionTime = retNode ? retNode.text().as_string() : "P30D";

    std::string assignedToken;
    const auto h = m_ctx.getRecordingHandler ? m_ctx.getRecordingHandler() : nullptr;
    if (h) {
        assignedToken = h->handleCreateRecording(cfg);
    }
    if (assignedToken.empty()) {
        assignedToken = cfg.recordingToken;
    }
    cfg.recordingToken = assignedToken;
    m_recordings.push_back(cfg);

    std::ostringstream ss;
    ss << "    <trc:CreateRecordingResponse>\r\n"
       << "      <trc:RecordingToken>" << Xml::escapeXmlText(assignedToken) << "</trc:RecordingToken>\r\n"
       << "    </trc:CreateRecordingResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult RecordingService::handleDeleteRecord(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='RecordingToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";

    const auto h = m_ctx.getRecordingHandler ? m_ctx.getRecordingHandler() : nullptr;
    if (h) {
        static_cast<void>(h->handleDeleteRecording(tok));
    }
    m_recordings.erase(std::remove_if(m_recordings.begin(), m_recordings.end(),
                           [&tok](const RecordingConfig& r) { return r.recordingToken == tok; }),
        m_recordings.end());
    return SoapHandlerResult::ok("    <trc:DeleteRecordingResponse/>\r\n");
}

SoapHandlerResult RecordingService::handleCreateTrack(const SoapContext& ctx)
{
    const auto recTokNode = ctx.reqNode.select_node(".//*[local-name()='RecordingToken']").node();
    const std::string recTok = recTokNode ? recTokNode.text().as_string() : "";
    const auto typeNode = ctx.reqNode.select_node(".//*[local-name()='TrackType']").node();
    const auto descNode = ctx.reqNode.select_node(".//*[local-name()='Description']").node();

    RecordingTrack trk;
    trk.trackToken = "Track_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count() % 100000);
    trk.trackType = recordingTrackTypeFromString(typeNode ? typeNode.text().as_string() : "Video");
    trk.description = descNode ? descNode.text().as_string() : "";

    std::string assignedToken;
    const auto h = m_ctx.getRecordingHandler ? m_ctx.getRecordingHandler() : nullptr;
    if (h) {
        assignedToken = h->handleCreateTrack(recTok, trk);
    }
    if (assignedToken.empty()) {
        assignedToken = trk.trackToken;
    }
    trk.trackToken = assignedToken;

    for (auto& r : m_recordings) {
        if (r.recordingToken == recTok) {
            r.tracks.push_back(trk);
            break;
        }
    }
    std::ostringstream ss;
    ss << "    <trc:CreateTrackResponse>\r\n"
       << "      <trc:TrackToken>" << Xml::escapeXmlText(assignedToken) << "</trc:TrackToken>\r\n"
       << "    </trc:CreateTrackResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult RecordingService::handleGetTrackConfig(const SoapContext& ctx)
{
    const auto recTokNode = ctx.reqNode.select_node(".//*[local-name()='RecordingToken']").node();
    const auto trkTokNode = ctx.reqNode.select_node(".//*[local-name()='TrackToken']").node();
    const std::string recTok = recTokNode ? recTokNode.text().as_string() : "";
    const std::string trkTok = trkTokNode ? trkTokNode.text().as_string() : "";

    for (const auto& r : m_recordings) {
        if (r.recordingToken == recTok) {
            for (const auto& trk : r.tracks) {
                if (trk.trackToken == trkTok) {
                    std::ostringstream ss;
                    ss << "    <trc:GetTrackConfigurationResponse>\r\n"
                       << "      <trc:TrackConfiguration>\r\n"
                       << "        <tt:TrackType>" << Xml::escapeXmlText(recordingTrackTypeToString(trk.trackType))
                       << "</tt:TrackType>\r\n"
                       << "        <tt:Description>" << Xml::escapeXmlText(trk.description) << "</tt:Description>\r\n"
                       << "      </trc:TrackConfiguration>\r\n"
                       << "    </trc:GetTrackConfigurationResponse>\r\n";
                    return SoapHandlerResult::ok(ss.str());
                }
            }
        }
    }
    return SoapHandlerResult::fault("ter:InvalidArgVal", "No such track", 400);
}

SoapHandlerResult RecordingService::handleSetTrackConfig(const SoapContext& ctx)
{
    const auto recTokNode = ctx.reqNode.select_node(".//*[local-name()='RecordingToken']").node();
    const auto trkTokNode = ctx.reqNode.select_node(".//*[local-name()='TrackToken']").node();
    const auto descNode = ctx.reqNode.select_node(".//*[local-name()='Description']").node();
    const std::string recTok = recTokNode ? recTokNode.text().as_string() : "";
    const std::string trkTok = trkTokNode ? trkTokNode.text().as_string() : "";

    for (auto& r : m_recordings) {
        if (r.recordingToken == recTok) {
            for (auto& trk : r.tracks) {
                if (trk.trackToken == trkTok && descNode) {
                    trk.description = descNode.text().as_string();
                    break;
                }
            }
        }
    }
    return SoapHandlerResult::ok("    <trc:SetTrackConfigurationResponse/>\r\n");
}

SoapHandlerResult RecordingService::handleDeleteTrack(const SoapContext& ctx)
{
    const auto recTokNode = ctx.reqNode.select_node(".//*[local-name()='RecordingToken']").node();
    const auto trkTokNode = ctx.reqNode.select_node(".//*[local-name()='TrackToken']").node();
    const std::string recTok = recTokNode ? recTokNode.text().as_string() : "";
    const std::string trkTok = trkTokNode ? trkTokNode.text().as_string() : "";

    const auto h = m_ctx.getRecordingHandler ? m_ctx.getRecordingHandler() : nullptr;
    if (h) {
        static_cast<void>(h->handleDeleteTrack(recTok, trkTok));
    }
    for (auto& r : m_recordings) {
        if (r.recordingToken == recTok) {
            r.tracks.erase(std::remove_if(r.tracks.begin(), r.tracks.end(),
                               [&trkTok](const RecordingTrack& t) { return t.trackToken == trkTok; }),
                r.tracks.end());
            break;
        }
    }
    return SoapHandlerResult::ok("    <trc:DeleteTrackResponse/>\r\n");
}

SoapHandlerResult RecordingService::handleGetRecordJobs(const SoapContext&)
{
    std::vector<RecordingJob> jobs = m_jobs;
    const auto h = m_ctx.getRecordingHandler ? m_ctx.getRecordingHandler() : nullptr;
    if (h) {
        jobs = h->handleGetRecordingJobs();
    }
    std::ostringstream ss;
    ss << "    <trc:GetRecordingJobsResponse>\r\n";
    for (const auto& j : jobs) {
        ss << "      <trc:JobItem token=\"" << Xml::escapeXmlAttr(j.jobToken) << "\">\r\n"
           << "        <trc:JobConfiguration>\r\n"
           << "          <trc:RecordingToken>" << Xml::escapeXmlText(j.recordingToken) << "</trc:RecordingToken>\r\n"
           << "          <trc:Mode>" << Xml::escapeXmlText(recordingJobModeToString(j.mode)) << "</trc:Mode>\r\n"
           << "          <trc:Priority>" << j.priority << "</trc:Priority>\r\n"
           << "          <trc:SourceToken>" << Xml::escapeXmlText(j.sourceToken) << "</trc:SourceToken>\r\n"
           << "        </trc:JobConfiguration>\r\n"
           << "      </trc:JobItem>\r\n";
    }
    ss << "    </trc:GetRecordingJobsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult RecordingService::handleCreateRecJob(const SoapContext& ctx)
{
    const auto recTokNode = ctx.reqNode.select_node(".//*[local-name()='RecordingToken']").node();
    const auto modeNode = ctx.reqNode.select_node(".//*[local-name()='Mode']").node();
    const auto prioNode = ctx.reqNode.select_node(".//*[local-name()='Priority']").node();
    const auto srcNode = ctx.reqNode.select_node(".//*[local-name()='SourceToken']").node();

    RecordingJob job;
    job.jobToken = "Job_" + std::to_string(m_jobs.size() + 1);
    job.recordingToken = recTokNode ? recTokNode.text().as_string() : "Rec_Main";
    job.mode = recordingJobModeFromString(modeNode ? modeNode.text().as_string() : "Active");
    job.priority = prioNode ? prioNode.text().as_int(1) : 1;
    job.sourceToken = srcNode ? srcNode.text().as_string() : "Profile_1";

    std::string assignedToken;
    const auto h = m_ctx.getRecordingHandler ? m_ctx.getRecordingHandler() : nullptr;
    if (h) {
        assignedToken = h->handleCreateRecordingJob(job);
    }
    if (assignedToken.empty()) {
        assignedToken = job.jobToken;
    }
    job.jobToken = assignedToken;
    m_jobs.push_back(job);

    std::ostringstream ss;
    ss << "    <trc:CreateRecordingJobResponse>\r\n"
       << "      <trc:JobToken>" << Xml::escapeXmlText(assignedToken) << "</trc:JobToken>\r\n"
       << "    </trc:CreateRecordingJobResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult RecordingService::handleSetJobMode(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='JobToken']").node();
    const auto modeNode = ctx.reqNode.select_node(".//*[local-name()='Mode']").node();
    const std::string jobTok = tokNode ? tokNode.text().as_string() : "";
    const RecordingJobMode mode = recordingJobModeFromString(modeNode ? modeNode.text().as_string() : "Active");

    const auto h = m_ctx.getRecordingHandler ? m_ctx.getRecordingHandler() : nullptr;
    if (h) {
        static_cast<void>(h->handleSetRecordingJobMode(jobTok, mode));
    }
    for (auto& j : m_jobs) {
        if (j.jobToken == jobTok) {
            j.mode = mode;
            break;
        }
    }
    return SoapHandlerResult::ok("    <trc:SetRecordingJobModeResponse/>\r\n");
}

SoapHandlerResult RecordingService::handleDeleteRecJob(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='JobToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";

    const auto h = m_ctx.getRecordingHandler ? m_ctx.getRecordingHandler() : nullptr;
    if (h) {
        static_cast<void>(h->handleDeleteRecordingJob(tok));
    }
    m_jobs.erase(
        std::remove_if(m_jobs.begin(), m_jobs.end(), [&tok](const RecordingJob& j) { return j.jobToken == tok; }),
        m_jobs.end());
    return SoapHandlerResult::ok("    <trc:DeleteRecordingJobResponse/>\r\n");
}

SoapHandlerResult RecordingService::handleGetRecSummary(const SoapContext&)
{
    RecordingSummary sum;
    const auto h = m_ctx.getRecordingHandler ? m_ctx.getRecordingHandler() : nullptr;
    if (h) {
        sum = h->handleGetRecordingSummary();
    } else {
        sum.dataFrom = "2026-01-01T00:00:00Z";
        sum.dataUntil = "2026-12-31T23:59:59Z";
        sum.numberRecordings = static_cast<int>(m_recordings.size());
        sum.totalStorageBytes = 1024ULL * 1024ULL * 1024ULL * 50ULL;
    }
    std::ostringstream ss;
    ss << "    <trc:GetRecordingSummaryResponse>\r\n"
       << "      <trc:Summary>\r\n"
       << "        <tt:DataFrom>" << Xml::escapeXmlText(sum.dataFrom) << "</tt:DataFrom>\r\n"
       << "        <tt:DataUntil>" << Xml::escapeXmlText(sum.dataUntil) << "</tt:DataUntil>\r\n"
       << "        <tt:NumberRecordings>" << sum.numberRecordings << "</tt:NumberRecordings>\r\n"
       << "        <tt:TotalRetentionTime>P30D</tt:TotalRetentionTime>\r\n"
       << "      </trc:Summary>\r\n"
       << "    </trc:GetRecordingSummaryResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

} // namespace Onvif
