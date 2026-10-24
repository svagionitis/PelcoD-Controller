#pragma once

/// @file RecordingService.h
/// @brief Modular ONVIF Profile G (Recording, Search, Replay) service dispatcher.

#include <cstddef>
#include <cstdint>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "IService.h"
#include "OnvifServerTypes.h"
#include "OnvifTypes.h"
#include "SoapRouter.h"

namespace Onvif {

class IRecordingHandler;
class ISearchHandler;
class IReplayHandler;

/// @struct RecordingServiceContext
/// @brief External port handlers and environment injected into RecordingService.
struct RecordingServiceContext {
    std::function<std::shared_ptr<IRecordingHandler>()> getRecordingHandler { };
    std::function<std::shared_ptr<ISearchHandler>()> getSearchHandler { };
    std::function<std::shared_ptr<IReplayHandler>()> getReplayHandler { };
    std::function<std::string(const SoapContext&)> resolveHost { };
};

/// @class RecordingService
/// @brief Modular service implementation for ONVIF Profile G services.
class RecordingService : public IService {
public:
    /// @brief Constructs RecordingService with injected environment.
    /// @param[in] ctx Profile G recording/search/replay dependencies.
    explicit RecordingService(RecordingServiceContext ctx);
    ~RecordingService() override = default;

    RecordingService(const RecordingService&) = delete;
    RecordingService& operator=(const RecordingService&) = delete;
    RecordingService(RecordingService&&) = default;
    RecordingService& operator=(RecordingService&&) = default;

    /// @brief Registers all Profile G operations with the SoapRouter.
    /// @param[in,out] router Target SoapRouter instance.
    void registerRoutes(SoapRouter& router) override;

    /// @brief Canonical service name ("Recording").
    /// @return String view of service name.
    [[nodiscard]] std::string_view serviceName() const noexcept override;

private:
    SoapHandlerResult handleGetRecordings(const SoapContext& ctx);
    SoapHandlerResult handleGetRecConfig(const SoapContext& ctx);
    SoapHandlerResult handleSetRecConfig(const SoapContext& ctx);
    SoapHandlerResult handleCreateRecord(const SoapContext& ctx);
    SoapHandlerResult handleDeleteRecord(const SoapContext& ctx);
    SoapHandlerResult handleCreateTrack(const SoapContext& ctx);
    SoapHandlerResult handleGetTrackConfig(const SoapContext& ctx);
    SoapHandlerResult handleSetTrackConfig(const SoapContext& ctx);
    SoapHandlerResult handleDeleteTrack(const SoapContext& ctx);
    SoapHandlerResult handleGetRecordJobs(const SoapContext& ctx);
    SoapHandlerResult handleCreateRecJob(const SoapContext& ctx);
    SoapHandlerResult handleSetJobMode(const SoapContext& ctx);
    SoapHandlerResult handleDeleteRecJob(const SoapContext& ctx);
    SoapHandlerResult handleGetRecSummary(const SoapContext& ctx);

    SoapHandlerResult handleFindRecordings(const SoapContext& ctx);
    SoapHandlerResult handleGetRecSearch(const SoapContext& ctx);
    SoapHandlerResult handleFindEvents(const SoapContext& ctx);
    SoapHandlerResult handleGetEventSearch(const SoapContext& ctx);
    SoapHandlerResult handleEndSearch(const SoapContext& ctx);

    SoapHandlerResult handleGetReplayUri(const SoapContext& ctx);
    SoapHandlerResult handleGetReplayConfig(const SoapContext& ctx);
    SoapHandlerResult handleSetReplayConfig(const SoapContext& ctx);

    RecordingServiceContext m_ctx { };
    std::vector<RecordingConfig> m_recordings { };
    std::vector<RecordingJob> m_jobs { };
    std::map<std::string, std::vector<RecordingSearchResult>> m_recSearches { };
    std::map<std::string, std::vector<RecordedEventResult>> m_eventSearches { };
    ReplayConfiguration m_replayConfig { };
    std::uint32_t m_nextSearchId { 1U };
};

} // namespace Onvif
