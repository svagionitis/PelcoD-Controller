#pragma once

/// @file ServiceRegistry.h
/// @brief Type-safe ONVIF service registry and DOM-based response builder.
/// @details Encapsulates service descriptors, standard ONVIF specification versions,
///          and WSDL-compliant XML generation for GetServices (Finding C4).

#include <functional>
#include <pugixml.hpp>
#include <string>
#include <vector>

namespace Onvif {

/// @struct ServiceVersion
/// @brief Represents an ONVIF service version pair (Major.Minor).
struct ServiceVersion {
    int major { 2 };
    int minor { 0 };
};

/// @struct ServiceDescriptor
/// @brief Metadata and capability configuration for an advertised ONVIF service.
struct ServiceDescriptor {
    std::string nameSpace {};
    std::string xAddrPath {};
    ServiceVersion version { 2, 0 };
    bool enabled { true };
    std::function<void(pugi::xml_node&)> capabilitiesWriter {};
};

/// @class ServiceRegistry
/// @brief Manages advertised ONVIF service endpoints and generates conforming responses.
class ServiceRegistry {
public:
    /// @brief Default constructor.
    ServiceRegistry() = default;

    /// @brief Default destructor.
    ~ServiceRegistry() = default;

    /// @brief Registers an ONVIF service descriptor into the registry.
    /// @param desc Service descriptor to register.
    void addService(const ServiceDescriptor& desc);

    /// @brief Removes all registered service descriptors.
    void clear() noexcept;

    /// @brief Retrieves the list of currently registered service descriptors.
    /// @return Const reference to vector of service descriptors.
    [[nodiscard]] const std::vector<ServiceDescriptor>& getServices() const noexcept;

    /// @brief Extracts the IncludeCapability boolean flag from a GetServices SOAP request.
    /// @param reqNode Root or container node of the GetServices request.
    /// @return True if capabilities were requested, false otherwise.
    [[nodiscard]] static bool parseIncludeCap(const pugi::xml_node& reqNode);

    /// @brief Builds a well-formed GetServicesResponse XML string using pugixml.
    /// @details Guarantees matching tags, XML schema element ordering (Namespace ->
    ///          XAddr -> [Capabilities] -> Version), and proper XML escaping.
    /// @param host Sanitized host IP or hostname.
    /// @param port Listening TCP port.
    /// @param includeCap Whether to embed service capabilities.
    /// @return Serialized XML snippet representing tds:GetServicesResponse.
    [[nodiscard]] std::string buildServicesXml(const std::string& host, int port, bool includeCap) const;

    /// @brief Factory creating a pre-configured registry with standard ONVIF versions.
    /// @param thermalEnabled Whether thermal service should be advertised.
    /// @param profileGEnabled Whether Profile G services (Recording/Search/Replay) should be advertised.
    /// @return Fully populated ServiceRegistry instance.
    [[nodiscard]] static ServiceRegistry createDefault(bool thermalEnabled, bool profileGEnabled);

private:
    std::vector<ServiceDescriptor> m_services {};
};

} // namespace Onvif
