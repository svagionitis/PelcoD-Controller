#pragma once

/// @file SoapRouter.h
/// @brief Declarative SOAP operation routing engine and access-level gatekeeper.

#include <cstddef>
#include <cstdint>

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <pugixml.hpp>

#include "AccessPolicy.h"
#include "OnvifAuthTypes.h"
#include "OnvifTypes.h"

namespace Onvif {

/// @struct OpKey
/// @brief Hashable key identifying a target SOAP operation by schema namespace and element name.
struct OpKey {
    std::string namespaceUri { }; ///< Target XML schema namespace URI.
    std::string localName { }; ///< Target SOAP operation element name (without prefix).

    /// @brief Equality operator for hash map lookups.
    [[nodiscard]] bool operator==(const OpKey& other) const noexcept
    {
        return localName == other.localName && namespaceUri == other.namespaceUri;
    }
};

/// @struct OpKeyHash
/// @brief Deterministic 64-bit hash functor for OpKey.
struct OpKeyHash {
    [[nodiscard]] std::size_t operator()(const OpKey& key) const noexcept
    {
        const std::size_t h1 = std::hash<std::string> { }(key.namespaceUri);
        const std::size_t h2 = std::hash<std::string> { }(key.localName);
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6U) + (h1 >> 2U));
    }
};

/// @struct SoapContext
/// @brief Invocation context supplied to dispatched operation handlers.
struct SoapContext {
    pugi::xml_node reqNode { }; ///< Parsed XML node of the SOAP operation request.
    Principal principal { }; ///< Authenticated identity and user privilege level.
    std::string peerAddress { }; ///< Client IP address string.
    std::string requestUrl { }; ///< Target HTTP request URI path.
    std::string serviceName { }; ///< Human-readable service name (e.g. "Device", "PTZ").
    std::string host { }; ///< Resolved host candidate for URI construction.
    bool isUnprovisioned { false }; ///< True when server is in unprovisioned commissioning state.
};

/// @enum DispatchStatus
/// @brief Status outcome of a SOAP dispatch attempt.
enum class DispatchStatus : std::uint8_t {
    Success = 0U, ///< Handler executed successfully.
    ActionNotSupported, ///< Operation or namespace not recognized.
    NotAuthorized, ///< Caller lacks required AccessClass privilege.
    BadRequest, ///< Malformed request payload.
    InternalFault ///< Handler threw exception or returned failure.
};

/// @struct SoapHandlerResult
/// @brief Result payload produced by a SOAP operation handler.
struct SoapHandlerResult {
    bool success { true }; ///< True if operation succeeded.
    std::int32_t httpStatusCode { 200 }; ///< Target HTTP response status code.
    std::string responseBody { }; ///< Serialized XML response fragment (Body child).
    std::string faultSubcode { }; ///< Optional ONVIF subcode if failed (e.g. "ter:InvalidArgVal").
    std::string faultSecondarySubcode { }; ///< Optional secondary subcode (e.g. "ter:NoToken").
    std::string faultReason { }; ///< Human-readable explanation if failed.

    /// @brief Factory creating a successful operation result.
    /// @param[in] body Serialized XML payload of response.
    /// @param[in] code HTTP status code (defaults to 200).
    /// @return Formatted successful SoapHandlerResult.
    [[nodiscard]] static SoapHandlerResult ok(std::string body, std::int32_t code = 200)
    {
        SoapHandlerResult res { };
        res.success = true;
        res.httpStatusCode = code;
        res.responseBody = std::move(body);
        return res;
    }

    /// @brief Factory creating a fault operation result.
    /// @param[in] subcode Qualified fault subcode (e.g. "ter:InvalidArgVal").
    /// @param[in] reason Human-readable explanation.
    /// @param[in] code HTTP status code (defaults to 400).
    /// @param[in] secondary Optional secondary subcode (e.g. "ter:NoToken").
    /// @return Formatted fault SoapHandlerResult.
    [[nodiscard]] static SoapHandlerResult fault(
        std::string subcode, std::string reason, std::int32_t code = 400, std::string secondary = "")
    {
        SoapHandlerResult res { };
        res.success = false;
        res.httpStatusCode = code;
        res.faultSubcode = std::move(subcode);
        res.faultSecondarySubcode = std::move(secondary);
        res.faultReason = std::move(reason);
        return res;
    }
};

/// @brief Functor type for operation handlers.
using SoapHandlerFn = std::function<SoapHandlerResult(const SoapContext& ctx)>;

/// @struct RouteEntry
/// @brief Route metadata associating an operation with access policy and callback.
struct RouteEntry {
    AccessClass accessClass { AccessClass::Unrecoverable }; ///< Required ONVIF access class.
    SoapHandlerFn handler { }; ///< Handler callback functor.
};

/// @struct DispatchResult
/// @brief Comprehensive result returned by SoapRouter::dispatch().
struct DispatchResult {
    DispatchStatus status { DispatchStatus::Success }; ///< Dispatch status code.
    std::int32_t httpStatusCode { 200 }; ///< HTTP response code.
    std::string responseXml { }; ///< Serialized body fragment or SOAP Fault XML.
};

/// @class SoapRouter
/// @brief Thread-safe declarative SOAP operation router and access gatekeeper.
/// @details Routes incoming XML operations based on exact {namespaceUri, localName} matching.
///          Enforces ONVIF Core §5.9.4 access class policies prior to handler execution.
///          Returns standard SOAP 1.2 ActionNotSupported or NotAuthorized faults upon failures.
class SoapRouter {
public:
    SoapRouter() = default;
    ~SoapRouter() = default;

    SoapRouter(const SoapRouter&) = delete;
    SoapRouter& operator=(const SoapRouter&) = delete;
    SoapRouter(SoapRouter&&) = default;
    SoapRouter& operator=(SoapRouter&&) = default;

    /// @brief Registers an operation route with explicit namespace, local name, and access class.
    /// @param[in] nsUri Target XML schema namespace URI.
    /// @param[in] localName Operation element name without prefix.
    /// @param[in] accessClass Required ONVIF access class.
    /// @param[in] handler Callback function executing the operation.
    void registerRoute(std::string nsUri, std::string localName, AccessClass accessClass, SoapHandlerFn handler);

    /// @brief Registers an operation route using an OpKey struct.
    /// @param[in] key Operation identifier key.
    /// @param[in] accessClass Required ONVIF access class.
    /// @param[in] handler Callback function executing the operation.
    void registerRoute(const OpKey& key, AccessClass accessClass, SoapHandlerFn handler);

    /// @brief Checks whether an operation route is registered.
    /// @param[in] nsUri Target XML schema namespace URI.
    /// @param[in] localName Operation element name.
    /// @return True if registered, false otherwise.
    [[nodiscard]] bool hasRoute(const std::string& nsUri, const std::string& localName) const noexcept;

    /// @brief Returns the total number of registered routes.
    /// @return Count of active routes.
    [[nodiscard]] std::size_t routeCount() const noexcept;

    /// @brief Clears all registered operation routes.
    void clearRoutes() noexcept;

    /// @brief Dispatches an operation by explicit namespace and local name.
    /// @param[in] nsUri Resolved XML namespace URI.
    /// @param[in] localName Operation element local name.
    /// @param[in] ctx Execution context.
    /// @return Result containing status, HTTP code, and response XML.
    [[nodiscard]] DispatchResult dispatch(
        const std::string& nsUri, const std::string& localName, const SoapContext& ctx) const;

    /// @brief Dispatches an operation parsed directly from an XML request node.
    /// @param[in] reqNode Operation root XML node within SOAP Body.
    /// @param[in] ctx Execution context.
    /// @param[in] fallbackNs Default namespace URI to assume if element omits declaration.
    /// @return Result containing status, HTTP code, and response XML.
    [[nodiscard]] DispatchResult dispatch(
        const pugi::xml_node& reqNode, const SoapContext& ctx, std::string_view fallbackNs = { }) const;

    /// @brief Extracts OpKey from an XML node, resolving prefix and xmlns declarations.
    /// @param[in] node Target XML element to inspect.
    /// @param[in] fallbackNs Default namespace URI if no declaration is found.
    /// @return Resolved OpKey structure.
    [[nodiscard]] OpKey extractOpKey(const pugi::xml_node& node, std::string_view fallbackNs = { }) const;

    /// @brief Resolves the XML namespace URI for a given element node by inspecting prefix and xmlns.
    /// @param[in] node Target XML element to inspect.
    /// @param[in] prefix Namespace prefix (empty for default namespace).
    /// @return Resolved namespace URI string, or empty if unresolved.
    [[nodiscard]] std::string resolveNamespace(const pugi::xml_node& node, std::string_view prefix) const;

    /// @brief Extracts the local name from a qualified XML tag (stripping prefix if present).
    /// @param[in] qualifiedName Full XML tag name (e.g. "tds:GetCapabilities").
    /// @return Local name without prefix (e.g. "GetCapabilities").
    [[nodiscard]] static std::string_view extractLocalName(std::string_view qualifiedName) noexcept;

    /// @brief Extracts the prefix from a qualified XML tag (e.g. "tds" from "tds:GetCapabilities").
    /// @param[in] qualifiedName Full XML tag name.
    /// @return Namespace prefix, or empty if tag has no prefix.
    [[nodiscard]] static std::string_view extractPrefix(std::string_view qualifiedName) noexcept;

private:
    std::unordered_map<OpKey, RouteEntry, OpKeyHash> m_routes { };
};

} // namespace Onvif
