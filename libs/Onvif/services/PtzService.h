#pragma once

/// @file PtzService.h
/// @brief Modular ONVIF PTZ Service (tptz) dispatcher.

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

class IPtzHandler;

/// @struct PtzServiceContext
/// @brief External port handlers and environment injected into PtzService.
struct PtzServiceContext {
    std::function<std::shared_ptr<IPtzHandler>()> getPtzHandler { };
    std::function<bool(std::string_view)> isProfileValid { };
};

/// @class PtzService
/// @brief Modular service implementation for ONVIF PTZ (tptz WSDL).
class PtzService : public IService {
public:
    explicit PtzService(PtzServiceContext ctx);
    ~PtzService() override = default;

    PtzService(const PtzService&) = delete;
    PtzService& operator=(const PtzService&) = delete;
    PtzService(PtzService&&) = default;
    PtzService& operator=(PtzService&&) = default;

    void registerRoutes(SoapRouter& router) override;
    [[nodiscard]] std::string_view serviceName() const noexcept override;

private:
    SoapHandlerResult handleGetNodes(const SoapContext& ctx);
    SoapHandlerResult handleGetNode(const SoapContext& ctx);
    SoapHandlerResult handleGetConfigs(const SoapContext& ctx);
    SoapHandlerResult handleGetConfig(const SoapContext& ctx);
    SoapHandlerResult handleSetConfig(const SoapContext& ctx);
    SoapHandlerResult handleGetCompatibleConfigs(const SoapContext& ctx);
    SoapHandlerResult handleGetOptions(const SoapContext& ctx);
    SoapHandlerResult handleContMove(const SoapContext& ctx);
    SoapHandlerResult handleRelMove(const SoapContext& ctx);
    SoapHandlerResult handleAbsMove(const SoapContext& ctx);
    SoapHandlerResult handleStop(const SoapContext& ctx);
    SoapHandlerResult handleGetStatus(const SoapContext& ctx);
    SoapHandlerResult handleSetPreset(const SoapContext& ctx);
    SoapHandlerResult handleGetPresets(const SoapContext& ctx);
    SoapHandlerResult handleGotoPreset(const SoapContext& ctx);
    SoapHandlerResult handleRemovePreset(const SoapContext& ctx);
    SoapHandlerResult handleGotoHome(const SoapContext& ctx);
    SoapHandlerResult handleSetHome(const SoapContext& ctx);
    SoapHandlerResult handleGetTours(const SoapContext& ctx);
    SoapHandlerResult handleGetTour(const SoapContext& ctx);
    SoapHandlerResult handleGetTourOpts(const SoapContext& ctx);
    SoapHandlerResult handleCreateTour(const SoapContext& ctx);
    SoapHandlerResult handleModifyTour(const SoapContext& ctx);
    SoapHandlerResult handleOperateTour(const SoapContext& ctx);
    SoapHandlerResult handleRemoveTour(const SoapContext& ctx);
    SoapHandlerResult handleSendAux(const SoapContext& ctx);
    SoapHandlerResult handleGeoMove(const SoapContext& ctx);

    PtzServiceContext m_ctx;
    std::vector<PTZConfiguration> m_ptzConfigs { };
    std::vector<PtzPreset> m_presets { };
    std::vector<PresetTour> m_presetTours { };
    std::uint32_t m_nextPresetId { 1U };
    std::uint32_t m_nextTourId { 1U };
};

} // namespace Onvif
