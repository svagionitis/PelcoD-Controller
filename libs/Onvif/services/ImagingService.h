#pragma once

/// @file ImagingService.h
/// @brief Modular ONVIF Imaging Service (timg) dispatcher.

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "IService.h"
#include "OnvifServerTypes.h"
#include "OnvifTypes.h"
#include "SoapRouter.h"

namespace Onvif {

class IImagingHandler;

/// @struct ImagingServiceContext
/// @brief External port handlers and environment injected into ImagingService.
struct ImagingServiceContext {
    std::function<std::shared_ptr<IImagingHandler>()> getImagingHandler { };
};

/// @class ImagingService
/// @brief Modular service implementation for ONVIF Imaging (timg WSDL).
class ImagingService : public IService {
public:
    /// @brief Constructs ImagingService with injected environment.
    /// @param[in] ctx Imaging service dependencies.
    explicit ImagingService(ImagingServiceContext ctx);
    ~ImagingService() override = default;

    ImagingService(const ImagingService&) = delete;
    ImagingService& operator=(const ImagingService&) = delete;
    ImagingService(ImagingService&&) = default;
    ImagingService& operator=(ImagingService&&) = default;

    /// @brief Registers all timg operations with the SoapRouter.
    /// @param[in,out] router Target SoapRouter instance.
    void registerRoutes(SoapRouter& router) override;

    /// @brief Canonical service name ("Imaging").
    /// @return String view of service name.
    [[nodiscard]] std::string_view serviceName() const noexcept override;

private:
    SoapHandlerResult handleGetSettings(const SoapContext& ctx);
    SoapHandlerResult handleSetSettings(const SoapContext& ctx);
    SoapHandlerResult handleGetOptions(const SoapContext& ctx);
    SoapHandlerResult handleMoveFocus(const SoapContext& ctx);
    SoapHandlerResult handleStopFocus(const SoapContext& ctx);
    SoapHandlerResult handleGetStatus(const SoapContext& ctx);
    SoapHandlerResult handleGetPresets(const SoapContext& ctx);
    SoapHandlerResult handleGetCurPreset(const SoapContext& ctx);
    SoapHandlerResult handleSetCurPreset(const SoapContext& ctx);

    ImagingServiceContext m_ctx { };
    ImagingSettings m_settings { };
    FocusStatus20 m_focusStatus { };
    std::vector<ImagingPreset> m_presets { };
    std::string m_currentPresetToken { "Preset_Clear" };
};

} // namespace Onvif
