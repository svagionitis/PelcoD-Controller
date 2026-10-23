/// @file OnvifServerAuth.cpp
/// @brief Authentication, authorization, and SOAP response dispatching for OnvifServer::Impl.

#include "OnvifServer.h"
#include "OnvifServerImpl.h"

#include <cstddef>
#include <cstdint>
#include <ctime>

#include <chrono>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

#include <pugixml.hpp>

#include "AccessPolicy.h"
#include "Authenticator.h"
#include "SoapFault.h"
#include "UsernameToken.h"

namespace Onvif {

namespace {

    std::string wrapSoapResponse(const std::string& bodyXml)
    {
        std::ostringstream oss;
        oss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
            << "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
            << "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
            << "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
            << "xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\" "
            << "xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" "
            << "xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" "
            << "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\" "
            << "xmlns:tan=\"http://www.onvif.org/ver20/analytics/wsdl\" "
            << "xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\" "
            << "xmlns:trc=\"http://www.onvif.org/ver10/recording/wsdl\" "
            << "xmlns:tse=\"http://www.onvif.org/ver10/search/wsdl\" "
            << "xmlns:trp=\"http://www.onvif.org/ver10/replay/wsdl\" "
            << "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\" "
            << "xmlns:wsa=\"http://www.w3.org/2005/08/addressing\" "
            << "xmlns:tns1=\"http://www.onvif.org/ver10/topics\" "
            << "xmlns:ter=\"http://www.onvif.org/ver10/error\" "
            << "xmlns:tt=\"http://www.onvif.org/ver10/schema\">\r\n"
            << "  <SOAP-ENV:Body>\r\n"
            << bodyXml << "  </SOAP-ENV:Body>\r\n"
            << "</SOAP-ENV:Envelope>\r\n";
        return oss.str();
    }

    [[nodiscard]] bool isOp(const std::string& opName, const std::string& target)
    {
        if (opName == target) {
            return true;
        }
        if (opName.length() > target.length() && opName.rfind(":" + target) == opName.length() - target.length() - 1) {
            return true;
        }
        return false;
    }

    [[nodiscard]] const char* outcomeLabel(AuthOutcome outcome) noexcept
    {
        switch (outcome) {
        case AuthOutcome::Success:
            return "success";
        case AuthOutcome::NoCredentials:
            return "no-credentials";
        case AuthOutcome::InvalidCredentials:
            return "invalid-credentials";
        case AuthOutcome::Replay:
            return "replay";
        case AuthOutcome::Stale:
            return "stale";
        case AuthOutcome::Unsupported:
        default:
            return "unsupported";
        }
    }

    [[nodiscard]] std::string clipForLog(std::string_view text)
    {
        constexpr std::size_t kMaxLogField { 96U };
        return std::string { text.substr(0U, kMaxLogField) };
    }

    std::string formatUtc(const std::chrono::system_clock::time_point& tp)
    {
        const std::time_t t = std::chrono::system_clock::to_time_t(tp);
        std::tm tmUtc { };
#ifdef _WIN32
        gmtime_s(&tmUtc, &t);
#else
        gmtime_r(&t, &tmUtc);
#endif
        char buf[32] { 0 };
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmUtc);
        return std::string(buf);
    }

} // namespace

bool OnvifServer::Impl::authorize(const httplib::Request& req, httplib::Response& res, std::string_view serviceName,
    const std::string& opName, const pugi::xml_document& doc)
{
    if (!config.auth.enabled) {
        lastAuthPrincipal = Principal { "admin", OnvifUserLevel::Administrator };
        return true;
    }
    const AccessClass required { AccessPolicy::classify(serviceName, opName) };

    if (provState.load() == ProvisioningState::Unprovisioned) {
        if ((required == AccessClass::PreAuth) || isOp(opName, "GetDeviceInformation")) {
            lastAuthPrincipal = Principal { "", OnvifUserLevel::Anonymous };
            return true;
        }
        if (isOp(opName, "CreateUsers")) {
            lastAuthPrincipal = Principal { "", OnvifUserLevel::Anonymous };
            return true;
        }
        sendSoapResponse(res, SoapFault::deviceUnprovisioned(), 400);
        return false;
    }

    if (required == AccessClass::PreAuth) {
        lastAuthPrincipal = Principal { "", OnvifUserLevel::Anonymous };
        return true;
    }

    const std::string authorization { req.get_header_value("Authorization") };
    AuthInput input { };
    input.method = req.method;
    input.uri = req.target.empty() ? req.path : req.target;
    input.authorization = authorization;
    input.securityHeader = findSecurityHeader(doc);
    input.remoteAddr = req.remote_addr;

    const std::string what { std::string { serviceName } + "/" + clipForLog(opName) };
    const AuthResult result { authenticator->authenticate(input) };
    if (result.outcome != AuthOutcome::Success) {
        rejectAuth(req, res, result.outcome, what);
        return false;
    }
    if (!AccessPolicy::permits(result.principal.level, required)) {
        rejectForbidden(req, res, result.principal, what);
        return false;
    }
    lastAuthPrincipal = result.principal;
    return true;
}

bool OnvifServer::Impl::authorizeHttp(const httplib::Request& req, httplib::Response& res, std::string_view what)
{
    if (!config.auth.enabled) {
        return true;
    }
    if (provState.load() == ProvisioningState::Unprovisioned) {
        res.status = 400;
        res.set_content(SoapFault::deviceUnprovisioned(), "application/soap+xml; charset=utf-8");
        return false;
    }
    const std::string authorization { req.get_header_value("Authorization") };
    AuthInput input { };
    input.method = req.method;
    input.uri = req.target.empty() ? req.path : req.target;
    input.authorization = authorization;
    input.remoteAddr = req.remote_addr;

    const AuthResult result { authenticator->authenticate(input) };
    if (result.outcome != AuthOutcome::Success) {
        rejectAuth(req, res, result.outcome, what);
        return false;
    }
    if (!AccessPolicy::permits(result.principal.level, AccessClass::ReadMedia)) {
        rejectForbidden(req, res, result.principal, what);
        return false;
    }
    return true;
}

void OnvifServer::Impl::rejectAuth(
    const httplib::Request& req, httplib::Response& res, AuthOutcome outcome, std::string_view what)
{
    appendAuthFailLog(req.remote_addr, what, outcomeLabel(outcome));

    if (outcome != AuthOutcome::NoCredentials) {
        const auto now { std::chrono::steady_clock::now() };
        authenticator->recordFailure(req.remote_addr, now);
        const std::chrono::milliseconds delay { authenticator->penalty(req.remote_addr, now) };
        if (delay.count() > 0) {
            std::this_thread::sleep_for(delay);
        }
    }

    const bool challenge { (outcome == AuthOutcome::NoCredentials) || (outcome == AuthOutcome::Stale) };
    if (challenge) {
        for (const auto& value : authenticator->challenges(outcome == AuthOutcome::Stale)) {
            static_cast<void>(res.headers.emplace("WWW-Authenticate", value));
        }
    }
    sendSoapResponse(res, SoapFault::notAuthorized(), challenge ? 401 : 400);
}

void OnvifServer::Impl::rejectForbidden(
    const httplib::Request& req, httplib::Response& res, const Principal& who, std::string_view what)
{
    appendAuthFailLog(req.remote_addr, what, "insufficient-privilege user=" + clipForLog(who.username));
    sendSoapResponse(res, SoapFault::notAuthorized(), 400);
}

void OnvifServer::Impl::appendAuthFailLog(const std::string& remoteAddr, std::string_view what, std::string_view reason)
{
    std::scoped_lock lock(logMutex);
    const std::string timestamp = formatUtc(std::chrono::system_clock::now());
    std::ostringstream entry;
    entry << "[" << timestamp << "] [" << remoteAddr << "] AUTH_FAIL " << what << " (" << reason << ")";
    accessLogs.push_back(entry.str());
    if (accessLogs.size() > 500) {
        accessLogs.pop_front();
    }
}

void OnvifServer::Impl::appendAccessLog(
    std::string_view serviceName, const std::string& opName, const std::string& remoteAddr)
{
    std::scoped_lock lock(logMutex);
    const std::string timestamp = formatUtc(std::chrono::system_clock::now());
    std::ostringstream entry;
    entry << "[" << timestamp << "] [" << remoteAddr << "] " << serviceName << "Service: " << opName;
    accessLogs.push_back(entry.str());
    if (accessLogs.size() > 500) {
        accessLogs.pop_front();
    }
}

void OnvifServer::Impl::sendSoapResponse(httplib::Response& res, const std::string& bodyXml, int status)
{
    res.status = status;
    res.set_content(wrapSoapResponse(bodyXml), "application/soap+xml; charset=utf-8");
}

} // namespace Onvif
