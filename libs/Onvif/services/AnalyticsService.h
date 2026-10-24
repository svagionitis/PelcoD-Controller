#pragma once

/// @file AnalyticsService.h
/// @brief Modular ONVIF Analytics Service (tan) dispatcher.

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include "IService.h"
#include "OnvifServerTypes.h"
#include "OnvifTypes.h"
#include "SoapRouter.h"

namespace Onvif {

class IAnalyticsHandler;

/// @struct AnalyticsServiceContext
/// @brief External port handlers and environment injected into AnalyticsService.
struct AnalyticsServiceContext {
    std::function<std::shared_ptr<IAnalyticsHandler>()> getAnalyticsHandler { };
};

/// @class AnalyticsService
/// @brief Modular service implementation for ONVIF Analytics (tan WSDL).
class AnalyticsService : public IService {
public:
    /// @brief Constructs AnalyticsService with injected environment.
    /// @param[in] ctx Analytics service dependencies.
    explicit AnalyticsService(AnalyticsServiceContext ctx);
    ~AnalyticsService() override = default;

    AnalyticsService(const AnalyticsService&) = delete;
    AnalyticsService& operator=(const AnalyticsService&) = delete;
    AnalyticsService(AnalyticsService&&) = default;
    AnalyticsService& operator=(AnalyticsService&&) = default;

    /// @brief Registers all tan operations with the SoapRouter.
    /// @param[in,out] router Target SoapRouter instance.
    void registerRoutes(SoapRouter& router) override;

    /// @brief Canonical service name ("Analytics").
    /// @return String view of service name.
    [[nodiscard]] std::string_view serviceName() const noexcept override;

private:
    SoapHandlerResult handleGetSupportedRules(const SoapContext& ctx);
    SoapHandlerResult handleGetRules(const SoapContext& ctx);
    SoapHandlerResult handleCreateRules(const SoapContext& ctx);
    SoapHandlerResult handleModifyRules(const SoapContext& ctx);
    SoapHandlerResult handleDeleteRules(const SoapContext& ctx);
    SoapHandlerResult handleGetSupportedModules(const SoapContext& ctx);
    SoapHandlerResult handleGetModules(const SoapContext& ctx);
    SoapHandlerResult handleCreateModules(const SoapContext& ctx);
    SoapHandlerResult handleModifyModules(const SoapContext& ctx);
    SoapHandlerResult handleDeleteModules(const SoapContext& ctx);

    AnalyticsServiceContext m_ctx { };
    std::vector<AnalyticsRule> m_rules { };
    std::vector<AnalyticsModule> m_modules { };
};

} // namespace Onvif
