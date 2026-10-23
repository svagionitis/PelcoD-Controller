#pragma once

/// @file IService.h
/// @brief Abstract interface for modular ONVIF service dispatchers.

#include <string_view>

namespace Onvif {

class SoapRouter;

/// @class IService
/// @brief Abstract interface for modular ONVIF service dispatchers.
/// @details Each concrete ONVIF domain service (Device, Media, PTZ, Imaging, Events, etc.)
///          implements this interface to register its operations declaratively with SoapRouter.
class IService {
public:
    virtual ~IService() = default;

    /// @brief Registers all SOAP operations supported by this service into the router.
    /// @param[in,out] router Target SoapRouter instance.
    virtual void registerRoutes(SoapRouter& router) = 0;

    /// @brief Gets the canonical service name (e.g. "Device", "Media", "PTZ").
    /// @return String view of service name.
    [[nodiscard]] virtual std::string_view serviceName() const noexcept = 0;
};

} // namespace Onvif
