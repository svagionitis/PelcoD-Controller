#pragma once

/// @file DeviceIoService.h
/// @brief Modular ONVIF DeviceIO Service (tmd) dispatcher.

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include "IService.h"
#include "OnvifServerTypes.h"
#include "OnvifTypes.h"
#include "SoapRouter.h"

namespace Onvif {

class IDeviceIoHandler;

/// @struct DeviceIoServiceContext
/// @brief External port handlers and environment injected into DeviceIoService.
struct DeviceIoServiceContext {
    std::function<std::shared_ptr<IDeviceIoHandler>()> getDeviceIoHandler { };
    std::vector<RelayOutputConfig> defaultRelays { };
    std::vector<DigitalInputConfig> defaultInputs { };
};

/// @class DeviceIoService
/// @brief Modular service implementation for ONVIF DeviceIO (tmd WSDL).
class DeviceIoService : public IService {
public:
    /// @brief Constructs DeviceIoService with injected environment.
    /// @param[in] ctx DeviceIO service dependencies.
    explicit DeviceIoService(DeviceIoServiceContext ctx);
    ~DeviceIoService() override = default;

    DeviceIoService(const DeviceIoService&) = delete;
    DeviceIoService& operator=(const DeviceIoService&) = delete;
    DeviceIoService(DeviceIoService&&) = default;
    DeviceIoService& operator=(DeviceIoService&&) = default;

    /// @brief Registers all tmd operations with the SoapRouter.
    /// @param[in,out] router Target SoapRouter instance.
    void registerRoutes(SoapRouter& router) override;

    /// @brief Canonical service name ("DeviceIO").
    /// @return String view of service name.
    [[nodiscard]] std::string_view serviceName() const noexcept override;

private:
    SoapHandlerResult handleGetRelays(const SoapContext& ctx);
    SoapHandlerResult handleGetRelayOptions(const SoapContext& ctx);
    SoapHandlerResult handleSetRelaySettings(const SoapContext& ctx);
    SoapHandlerResult handleSetRelayState(const SoapContext& ctx);
    SoapHandlerResult handleGetInputs(const SoapContext& ctx);
    SoapHandlerResult handleGetInputOptions(const SoapContext& ctx);

    DeviceIoServiceContext m_ctx { };
    std::vector<RelayOutputConfig> m_relays { };
    std::vector<DigitalInputConfig> m_inputs { };
};

} // namespace Onvif
