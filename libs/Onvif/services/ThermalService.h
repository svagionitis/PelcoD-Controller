#pragma once

/// @file ThermalService.h
/// @brief Modular ONVIF Thermal & Radiometry Service dispatcher.

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

class IThermalHandler;

/// @struct ThermalServiceContext
/// @brief External port handlers and environment injected into ThermalService.
struct ThermalServiceContext {
    std::function<std::shared_ptr<IThermalHandler>()> getThermalHandler { };
};

/// @class ThermalService
/// @brief Modular service implementation for ONVIF Thermal & Radiometry extension.
class ThermalService : public IService {
public:
    explicit ThermalService(ThermalServiceContext ctx);
    ~ThermalService() override = default;

    ThermalService(const ThermalService&) = delete;
    ThermalService& operator=(const ThermalService&) = delete;
    ThermalService(ThermalService&&) = default;
    ThermalService& operator=(ThermalService&&) = default;

    void registerRoutes(SoapRouter& router) override;
    [[nodiscard]] std::string_view serviceName() const noexcept override;

private:
    SoapHandlerResult handleGetRadiometry(const SoapContext& ctx);
    SoapHandlerResult handleSetRadiometry(const SoapContext& ctx);
    SoapHandlerResult handleGetRadiometryOpts(const SoapContext& ctx);
    SoapHandlerResult handleGetSpots(const SoapContext& ctx);
    SoapHandlerResult handleSetSpots(const SoapContext& ctx);
    SoapHandlerResult handleGetBoxes(const SoapContext& ctx);
    SoapHandlerResult handleSetBoxes(const SoapContext& ctx);
    SoapHandlerResult handleGetPalettes(const SoapContext& ctx);
    SoapHandlerResult handleSetPalette(const SoapContext& ctx);
    SoapHandlerResult handleTriggerNuc(const SoapContext& ctx);

    ThermalServiceContext m_ctx;
    RadiometryConfig m_radiometry { };
    std::vector<RadiometrySpot> m_spots { };
    std::vector<RadiometryBox> m_boxes { };
    std::vector<ColorPalette> m_palettes { };
    std::string m_activePalette { "WhiteHot" };
};

} // namespace Onvif
