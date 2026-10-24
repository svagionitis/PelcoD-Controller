#pragma once

/// @file MediaService.h
/// @brief Modular ONVIF Media Service (trt / tr2) dispatcher.

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

class IOsdHandler;
class IMaskHandler;
class IVideoSourceModeHandler;
class IMetadataHandler;

/// @struct MediaServiceContext
/// @brief External port handlers and environment injected into MediaService.
struct MediaServiceContext {
    OnvifServerConfig config { };
    std::function<std::shared_ptr<IOsdHandler>()> getOsdHandler { };
    std::function<std::shared_ptr<IMaskHandler>()> getMaskHandler { };
    std::function<std::shared_ptr<IVideoSourceModeHandler>()> getVideoSourceModeHandler { };
    std::function<std::shared_ptr<IMetadataHandler>()> getMetadataHandler { };
    std::function<std::string(const SoapContext&)> resolveHost { };
};

/// @class MediaService
/// @brief Modular service implementation for ONVIF Media (trt / tr2 WSDL).
class MediaService : public IService {
public:
    /// @brief Constructs MediaService with injected environment.
    /// @param[in] ctx Media service dependencies.
    explicit MediaService(MediaServiceContext ctx);
    ~MediaService() override = default;

    MediaService(const MediaService&) = delete;
    MediaService& operator=(const MediaService&) = delete;
    MediaService(MediaService&&) = default;
    MediaService& operator=(MediaService&&) = default;

    /// @brief Registers all trt and tr2 operations with the SoapRouter.
    /// @param[in,out] router Target SoapRouter instance.
    void registerRoutes(SoapRouter& router) override;

    /// @brief Canonical service name ("Media").
    /// @return String view of service name.
    [[nodiscard]] std::string_view serviceName() const noexcept override;

private:
    SoapHandlerResult handleGetProfiles(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetProfile(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleCreateProfile(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleDeleteProfile(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetVideoSources(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetVideoConfigs(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetVideoConfig(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetVideoSourceConfigs(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetVideoSourceConfig(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleSetVideoEncoderConfig(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetVideoOptions(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetStreamUri(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetSnapshotUri(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleAddVideoEncoder(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleRemoveVideoEncoder(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleAddVideoSourceConfig(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleRemoveVideoSourceConfig(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleAddPTZConfig(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleRemovePTZConfig(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetCompatibleVsc(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetCompatibleVec(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetGuaranteedEncoder(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetOSDs(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetOSD(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleCreateOSD(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleDeleteOSD(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleSetOSD(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetOSDOptions(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetMasks(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetMask(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleCreateMask(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleDeleteMask(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleSetMask(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetMaskOptions(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetVideoModes(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleSetVideoMode(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetMetaConfigs(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetMetaConfig(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleSetMetaConfig(const SoapContext& ctx, const std::string& prefix);
    SoapHandlerResult handleGetMetaOptions(const SoapContext& ctx, const std::string& prefix);

    MediaServiceContext m_ctx { };
    std::vector<ServerMediaProfile> m_profiles { };
    std::vector<VideoSourceConfig> m_videoSources { };
    std::vector<VideoEncoderConfig> m_videoEncoders { };
    std::vector<MetadataConfiguration> m_metadataConfigs { };
    std::map<std::string, OsdConfig> m_osds { };
    std::vector<PrivacyMask> m_masks { };
    MaskOptions m_maskOptions { };
    std::uint32_t m_nextProfileId { 2U };
    std::uint32_t m_nextOsdId { 2U };
    std::uint32_t m_nextMaskId { 2U };
};

} // namespace Onvif
