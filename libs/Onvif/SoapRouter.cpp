/// @file SoapRouter.cpp
/// @brief Implementation of declarative SOAP operation routing engine and access gatekeeper.

#include "SoapRouter.h"

#include <cstddef>
#include <cstdint>

#include <exception>
#include <string>
#include <string_view>
#include <utility>

#include <pugixml.hpp>

#include "AccessPolicy.h"
#include "OnvifAuthTypes.h"
#include "OnvifTypes.h"
#include "SoapFault.h"

namespace Onvif {

void SoapRouter::registerRoute(std::string nsUri, std::string localName, AccessClass accessClass, SoapHandlerFn handler)
{
    OpKey key { };
    key.namespaceUri = std::move(nsUri);
    key.localName = std::move(localName);
    registerRoute(key, accessClass, std::move(handler));
}

void SoapRouter::registerRoute(const OpKey& key, AccessClass accessClass, SoapHandlerFn handler)
{
    RouteEntry entry { };
    entry.accessClass = accessClass;
    entry.handler = std::move(handler);
    m_routes[key] = std::move(entry);
}

bool SoapRouter::hasRoute(const std::string& nsUri, const std::string& localName) const noexcept
{
    const OpKey key { nsUri, localName };
    return m_routes.find(key) != m_routes.end();
}

std::size_t SoapRouter::routeCount() const noexcept
{
    return m_routes.size();
}

void SoapRouter::clearRoutes() noexcept
{
    m_routes.clear();
}

std::string_view SoapRouter::extractPrefix(std::string_view qualifiedName) noexcept
{
    const auto pos = qualifiedName.find(':');
    if (pos == std::string_view::npos) {
        return { };
    }
    return qualifiedName.substr(0U, pos);
}

std::string_view SoapRouter::extractLocalName(std::string_view qualifiedName) noexcept
{
    const auto pos = qualifiedName.rfind(':');
    if (pos == std::string_view::npos) {
        return qualifiedName;
    }
    return qualifiedName.substr(pos + 1U);
}

std::string SoapRouter::resolveNamespace(const pugi::xml_node& node, std::string_view prefix) const
{
    if (!node) {
        return { };
    }

    std::string attrName { };
    if (prefix.empty()) {
        attrName = "xmlns";
    } else {
        attrName = "xmlns:" + std::string(prefix);
    }

    pugi::xml_node curr = node;
    while (curr) {
        const auto attr = curr.attribute(attrName.c_str());
        if (attr) {
            const char* val = attr.value();
            if (val != nullptr && *val != '\0') {
                return std::string(val);
            }
        }
        curr = curr.parent();
    }

    return { };
}

OpKey SoapRouter::extractOpKey(const pugi::xml_node& node, std::string_view fallbackNs) const
{
    if (!node) {
        return { };
    }

    const std::string_view rawName = node.name();
    const std::string_view pfx = extractPrefix(rawName);
    const std::string_view local = extractLocalName(rawName);

    std::string nsUri = resolveNamespace(node, pfx);
    if (nsUri.empty() && !fallbackNs.empty()) {
        nsUri = std::string(fallbackNs);
    }

    OpKey key { };
    key.namespaceUri = std::move(nsUri);
    key.localName = std::string(local);
    return key;
}

DispatchResult SoapRouter::dispatch(
    const std::string& nsUri, const std::string& localName, const SoapContext& ctx) const
{
    const OpKey key { nsUri, localName };
    const auto it = m_routes.find(key);
    if (it == m_routes.end()) {
        DispatchResult res { };
        res.status = DispatchStatus::ActionNotSupported;
        res.httpStatusCode = 500;
        res.responseXml = SoapFault::actionNotSupported("Action Not Supported: " + localName);
        return res;
    }

    const auto& entry = it->second;
    if (!AccessPolicy::permits(ctx.principal.level, entry.accessClass)) {
        DispatchResult res { };
        res.status = DispatchStatus::NotAuthorized;
        res.httpStatusCode = (ctx.principal.level == OnvifUserLevel::Anonymous) ? 401 : 400;
        res.responseXml = SoapFault::notAuthorized();
        return res;
    }

    if (!entry.handler) {
        DispatchResult res { };
        res.status = DispatchStatus::InternalFault;
        res.httpStatusCode = 500;
        res.responseXml = SoapFault::receiver("ter:Action", "", "Handler not implemented");
        return res;
    }

    try {
        const auto handlerRes = entry.handler(ctx);
        DispatchResult res { };
        if (handlerRes.success) {
            res.status = DispatchStatus::Success;
            res.httpStatusCode = handlerRes.httpStatusCode;
            res.responseXml = handlerRes.responseBody;
        } else {
            res.status = DispatchStatus::BadRequest;
            res.httpStatusCode = handlerRes.httpStatusCode;
            const std::string subcode = handlerRes.faultSubcode.empty() ? "ter:InvalidArgVal" : handlerRes.faultSubcode;
            const std::string reason = handlerRes.faultReason.empty() ? "Operation failed" : handlerRes.faultReason;
            res.responseXml = SoapFault::sender(subcode, "", reason);
        }
        return res;
    } catch (const std::exception& e) {
        DispatchResult res { };
        res.status = DispatchStatus::InternalFault;
        res.httpStatusCode = 500;
        res.responseXml = SoapFault::receiver("ter:Action", "", e.what());
        return res;
    } catch (...) {
        DispatchResult res { };
        res.status = DispatchStatus::InternalFault;
        res.httpStatusCode = 500;
        res.responseXml = SoapFault::receiver("ter:Action", "", "Internal handler error");
        return res;
    }
}

DispatchResult SoapRouter::dispatch(
    const pugi::xml_node& reqNode, const SoapContext& ctx, std::string_view fallbackNs) const
{
    if (!reqNode) {
        DispatchResult res { };
        res.status = DispatchStatus::BadRequest;
        res.httpStatusCode = 400;
        res.responseXml = SoapFault::sender("ter:InvalidArgVal", "", "Missing request body node");
        return res;
    }

    const auto key = extractOpKey(reqNode, fallbackNs);
    return dispatch(key.namespaceUri, key.localName, ctx);
}

} // namespace Onvif
