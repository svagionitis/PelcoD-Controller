#pragma once

/// @file DeviceService.h
/// @brief Modular ONVIF Device Management (tds) service dispatcher.

#include <cstddef>
#include <cstdint>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "IService.h"
#include "OnvifAuthTypes.h"
#include "OnvifServerTypes.h"
#include "OnvifTypes.h"
#include "SoapRouter.h"

namespace Onvif {

class ScopeStore;
class CredentialStore;
class IDeviceManagementHandler;
class IVideoSourceModeHandler;

/// @struct DeviceServiceContext
/// @brief Environment and external port providers injected into DeviceService.
struct DeviceServiceContext {
    OnvifServerConfig config { };
    std::shared_ptr<ScopeStore> scopeStore { };
    std::shared_ptr<CredentialStore> credStore { };
    std::function<std::shared_ptr<IDeviceManagementHandler>()> getDeviceHandler { };
    std::function<std::shared_ptr<IVideoSourceModeHandler>()> getVideoSourceModeHandler { };
    std::function<DiscoveryMode()> getDiscoveryMode { };
    std::function<void(DiscoveryMode)> setDiscoveryMode { };
    std::function<void(ProvisioningState)> setProvisioningState { };
    std::function<std::string(const SoapContext&)> resolveHost { };
    std::function<void(const std::string&, const std::string&)> logMessage { };
    std::function<std::vector<NetworkProtocolConfig>()> getNetProtocols { };
    std::function<bool(const std::vector<NetworkProtocolConfig>&)> setNetProtocols { };
    std::function<std::string(SystemLogType)> getSystemLog { };
    std::function<std::size_t()> getRelayOutputCount { };
    std::function<bool()> isProfileGSupported { };
    std::function<bool()> isThermalSupported { };
};

/// @class DeviceService
/// @brief Modular service implementation for ONVIF Device Management (tds WSDL).
class DeviceService : public IService {
public:
    /// @brief Constructs DeviceService with injected environment.
    /// @param[in] ctx Device service dependencies and state callbacks.
    explicit DeviceService(DeviceServiceContext ctx);
    ~DeviceService() override = default;

    DeviceService(const DeviceService&) = delete;
    DeviceService& operator=(const DeviceService&) = delete;
    DeviceService(DeviceService&&) = default;
    DeviceService& operator=(DeviceService&&) = default;

    /// @brief Registers all tds operations with the SoapRouter.
    void registerRoutes(SoapRouter& router) override;

    /// @brief Canonical service name.
    [[nodiscard]] std::string_view serviceName() const noexcept override;

private:
    SoapHandlerResult handleGetDeviceInfo(const SoapContext& ctx);
    SoapHandlerResult handleGetCaps(const SoapContext& ctx);
    SoapHandlerResult handleGetServices(const SoapContext& ctx);
    SoapHandlerResult handleGetSysDate(const SoapContext& ctx);
    SoapHandlerResult handleSetSysDate(const SoapContext& ctx);
    SoapHandlerResult handleReboot(const SoapContext& ctx);
    SoapHandlerResult handleGetUsers(const SoapContext& ctx);
    SoapHandlerResult handleCreateUsers(const SoapContext& ctx);
    SoapHandlerResult handleDeleteUsers(const SoapContext& ctx);
    SoapHandlerResult handleSetUser(const SoapContext& ctx);
    SoapHandlerResult handleGetScopes(const SoapContext& ctx);
    SoapHandlerResult handleAddScopes(const SoapContext& ctx);
    SoapHandlerResult handleSetScopes(const SoapContext& ctx);
    SoapHandlerResult handleRemoveScopes(const SoapContext& ctx);
    SoapHandlerResult handleGetNetIfaces(const SoapContext& ctx);
    SoapHandlerResult handleSetNetIfaces(const SoapContext& ctx);
    SoapHandlerResult handleGetNetProtos(const SoapContext& ctx);
    SoapHandlerResult handleSetNetProtos(const SoapContext& ctx);
    SoapHandlerResult handleGetGateway(const SoapContext& ctx);
    SoapHandlerResult handleSetGateway(const SoapContext& ctx);
    SoapHandlerResult handleGetDns(const SoapContext& ctx);
    SoapHandlerResult handleSetDns(const SoapContext& ctx);
    SoapHandlerResult handleGetNtp(const SoapContext& ctx);
    SoapHandlerResult handleSetNtp(const SoapContext& ctx);
    SoapHandlerResult handleGetHostname(const SoapContext& ctx);
    SoapHandlerResult handleSetHostname(const SoapContext& ctx);
    SoapHandlerResult handleGetDiscMode(const SoapContext& ctx);
    SoapHandlerResult handleSetDiscMode(const SoapContext& ctx);
    SoapHandlerResult handleGetWsdlUrl(const SoapContext& ctx);
    SoapHandlerResult handleGetEndpointRef(const SoapContext& ctx);
    SoapHandlerResult handleGetSystemLog(const SoapContext& ctx);
    SoapHandlerResult handleGetSysSupport(const SoapContext& ctx);
    SoapHandlerResult handleGetSysBackup(const SoapContext& ctx);
    SoapHandlerResult handleRestoreSys(const SoapContext& ctx);
    SoapHandlerResult handleGetCerts(const SoapContext& ctx);
    SoapHandlerResult handleGetCertInfo(const SoapContext& ctx);
    SoapHandlerResult handleCreateCert(const SoapContext& ctx);
    SoapHandlerResult handleGetPkcs10(const SoapContext& ctx);
    SoapHandlerResult handleLoadCerts(const SoapContext& ctx);
    SoapHandlerResult handleDeleteCerts(const SoapContext& ctx);
    SoapHandlerResult handleGetClientCert(const SoapContext& ctx);
    SoapHandlerResult handleSetClientCert(const SoapContext& ctx);
    SoapHandlerResult handleGetGeoLoc(const SoapContext& ctx);
    SoapHandlerResult handleSetGeoLoc(const SoapContext& ctx);
    SoapHandlerResult handleDeleteGeoLoc(const SoapContext& ctx);
    SoapHandlerResult handleGetVideoModes(const SoapContext& ctx);
    SoapHandlerResult handleSetVideoMode(const SoapContext& ctx);
    SoapHandlerResult handleFactoryDefault(const SoapContext& ctx);

    DeviceServiceContext m_ctx;
    SystemDateTimeConfig m_dateTime { };
    std::string m_hostname { "PelcoD-Bridge" };
    std::string m_gateway { "192.168.1.1" };
    DnsConfig m_dns { };
    NtpConfig m_ntp { };
    std::vector<NetworkInterfaceConfig> m_netInterfaces { };
    std::vector<OnvifCertificate> m_certs { };
    ClientCertificateMode m_clientCertMode { ClientCertificateMode::Off };
};

} // namespace Onvif
