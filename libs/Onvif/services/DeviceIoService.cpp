/// @file DeviceIoService.cpp
#include "DeviceIoService.h"

#include <cstddef>
#include <cstdint>

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <pugixml.hpp>

#include "SoapFault.h"
#include "SoapRouter.h"
#include "XmlUtils.h"

namespace Onvif {

namespace {
    constexpr std::string_view kDeviceIoNs { "http://www.onvif.org/ver10/deviceIO/wsdl" };
}

DeviceIoService::DeviceIoService(DeviceIoServiceContext ctx)
    : m_ctx(std::move(ctx))
    , m_relays(m_ctx.defaultRelays)
    , m_inputs(m_ctx.defaultInputs)
{
    if (m_relays.empty()) {
        RelayOutputConfig r;
        r.token = "Relay_1";
        r.mode = RelayMode::Monostable;
        r.delayTimeSeconds = 1.0f;
        r.idleState = RelayIdleState::Open;
        r.logicalState = RelayLogicalState::Inactive;
        m_relays.push_back(r);
    }
}

std::string_view DeviceIoService::serviceName() const noexcept
{
    return "DeviceIO";
}

void DeviceIoService::registerRoutes(SoapRouter& router)
{
    const std::string ns { kDeviceIoNs };

    router.registerRoute(ns, "GetServiceCapabilities", AccessClass::PreAuth, [](const SoapContext&) {
        return SoapHandlerResult::ok("    <tmd:GetServiceCapabilitiesResponse>\r\n"
                                     "      <tmd:Capabilities VideoSources=\"1\" VideoOutputs=\"0\" AudioSources=\"0\" "
                                     "AudioOutputs=\"0\" RelayOutputs=\"1\" DigitalInputs=\"1\"/>\r\n"
                                     "    </tmd:GetServiceCapabilitiesResponse>\r\n");
    });
    router.registerRoute(
        ns, "GetRelayOutputs", AccessClass::ReadMedia, [this](const SoapContext& ctx) { return handleGetRelays(ctx); });
    router.registerRoute(ns, "GetRelayOutputOptions", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetRelayOptions(ctx); });
    router.registerRoute(ns, "SetRelayOutputSettings", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleSetRelaySettings(ctx); });
    router.registerRoute(ns, "SetRelayOutputState", AccessClass::Actuate,
        [this](const SoapContext& ctx) { return handleSetRelayState(ctx); });
    router.registerRoute(ns, "GetDigitalInputs", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetInputs(ctx); });
    router.registerRoute(ns, "GetDigitalInputConfigurationOptions", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetInputOptions(ctx); });
    router.registerRoute(ns, "GetVideoSources", AccessClass::ReadMedia, [](const SoapContext&) {
        return SoapHandlerResult::ok(
            "    <tmd:GetVideoSourcesResponse>\r\n"
            "      <tmd:VideoSources token=\"VideoSource_1\">\r\n"
            "        <tt:Framerate>30.0</tt:Framerate>\r\n"
            "        <tt:Resolution><tt:Width>1920</tt:Width><tt:Height>1080</tt:Height></tt:Resolution>\r\n"
            "      </tmd:VideoSources>\r\n"
            "    </tmd:GetVideoSourcesResponse>\r\n");
    });
    router.registerRoute(ns, "GetVideoOutputs", AccessClass::ReadMedia,
        [](const SoapContext&) { return SoapHandlerResult::ok("    <tmd:GetVideoOutputsResponse/>\r\n"); });
    router.registerRoute(ns, "GetAudioSources", AccessClass::ReadMedia,
        [](const SoapContext&) { return SoapHandlerResult::ok("    <tmd:GetAudioSourcesResponse/>\r\n"); });
    router.registerRoute(ns, "GetAudioOutputs", AccessClass::ReadMedia,
        [](const SoapContext&) { return SoapHandlerResult::ok("    <tmd:GetAudioOutputsResponse/>\r\n"); });
}

SoapHandlerResult DeviceIoService::handleGetRelays(const SoapContext&)
{
    auto relays = m_relays;
    const auto io = m_ctx.getDeviceIoHandler ? m_ctx.getDeviceIoHandler() : nullptr;
    if (io) {
        relays = io->handleGetRelayOutputs();
    }
    std::ostringstream ss;
    ss << "    <tmd:GetRelayOutputsResponse>\r\n";
    for (const auto& r : relays) {
        ss << "      <tmd:RelayOutputs token=\"" << Xml::escapeXmlAttr(r.token) << "\">\r\n"
           << "        <tt:Properties>\r\n"
           << "          <tt:Mode>" << (r.mode == RelayMode::Bistable ? "Bistable" : "Monostable") << "</tt:Mode>\r\n"
           << "          <tt:DelayTime>PT" << static_cast<int>(r.delayTimeSeconds) << "S</tt:DelayTime>\r\n"
           << "          <tt:IdleState>" << (r.idleState == RelayIdleState::Closed ? "closed" : "open")
           << "</tt:IdleState>\r\n"
           << "        </tt:Properties>\r\n"
           << "        <tt:LogicalState>" << (r.logicalState == RelayLogicalState::Active ? "active" : "inactive")
           << "</tt:LogicalState>\r\n"
           << "      </tmd:RelayOutputs>\r\n";
    }
    ss << "    </tmd:GetRelayOutputsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult DeviceIoService::handleGetRelayOptions(const SoapContext& ctx)
{
    const auto tokenNode = ctx.reqNode.select_node(".//*[local-name()='RelayOutputToken']").node();
    const std::string token = tokenNode ? tokenNode.text().as_string() : "Relay_1";
    std::ostringstream ss;
    ss << "    <tmd:GetRelayOutputOptionsResponse>\r\n"
       << "      <tmd:RelayOutputOptions token=\"" << Xml::escapeXmlAttr(token) << "\">\r\n"
       << "        <tt:Mode>Bistable</tt:Mode>\r\n"
       << "        <tt:Mode>Monostable</tt:Mode>\r\n"
       << "        <tt:DelayTimes>\r\n"
       << "          <tt:Min>PT0S</tt:Min>\r\n"
       << "          <tt:Max>PT300S</tt:Max>\r\n"
       << "        </tt:DelayTimes>\r\n"
       << "        <tt:Discrete>false</tt:Discrete>\r\n"
       << "      </tmd:RelayOutputOptions>\r\n"
       << "    </tmd:GetRelayOutputOptionsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult DeviceIoService::handleSetRelaySettings(const SoapContext& ctx)
{
    const auto tokenNode = ctx.reqNode.select_node(".//*[local-name()='RelayOutputToken']").node();
    const std::string token = tokenNode ? tokenNode.text().as_string() : "";

    RelayOutputConfig updated;
    updated.token = token;

    const auto modeNode = ctx.reqNode.select_node(".//*[local-name()='Mode']").node();
    if (modeNode) {
        const std::string m = modeNode.text().as_string();
        updated.mode = (m == "Bistable") ? RelayMode::Bistable : RelayMode::Monostable;
    }

    const auto delayNode = ctx.reqNode.select_node(".//*[local-name()='DelayTime']").node();
    if (delayNode) {
        std::string dt = delayNode.text().as_string();
        std::size_t pos = (dt.rfind("PT", 0) == 0) ? 2U : 0U;
        std::uint32_t val = 0U;
        while (pos < dt.size() && dt[pos] >= '0' && dt[pos] <= '9') {
            val = val * 10U + static_cast<std::uint32_t>(dt[pos] - '0');
            ++pos;
        }
        updated.delayTimeSeconds = static_cast<float>(val > 0U ? val : 1U);
    }

    const auto idleNode = ctx.reqNode.select_node(".//*[local-name()='IdleState']").node();
    if (idleNode) {
        const std::string idl = idleNode.text().as_string();
        updated.idleState = (idl == "closed" || idl == "Closed") ? RelayIdleState::Closed : RelayIdleState::Open;
    }

    const auto io = m_ctx.getDeviceIoHandler ? m_ctx.getDeviceIoHandler() : nullptr;
    if (io) {
        static_cast<void>(io->handleSetRelayOutputSettings(token, updated));
    }
    for (auto& r : m_relays) {
        if (r.token == token) {
            r.mode = updated.mode;
            r.delayTimeSeconds = updated.delayTimeSeconds;
            r.idleState = updated.idleState;
            break;
        }
    }
    return SoapHandlerResult::ok("    <tmd:SetRelayOutputSettingsResponse/>\r\n");
}

SoapHandlerResult DeviceIoService::handleSetRelayState(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='RelayOutputToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";
    const auto stateNode = ctx.reqNode.select_node(".//*[local-name()='LogicalState']").node();
    const std::string state = stateNode ? stateNode.text().as_string() : "inactive";
    const auto st = (state == "active" || state == "Active") ? RelayLogicalState::Active : RelayLogicalState::Inactive;

    const auto io = m_ctx.getDeviceIoHandler ? m_ctx.getDeviceIoHandler() : nullptr;
    if (io) {
        static_cast<void>(io->handleSetRelayOutputState(tok, st));
    }
    for (auto& r : m_relays) {
        if (r.token == tok) {
            r.logicalState = st;
            break;
        }
    }
    return SoapHandlerResult::ok("    <tmd:SetRelayOutputStateResponse/>\r\n");
}

SoapHandlerResult DeviceIoService::handleGetInputs(const SoapContext&)
{
    auto inputs = m_inputs;
    const auto io = m_ctx.getDeviceIoHandler ? m_ctx.getDeviceIoHandler() : nullptr;
    if (io) {
        inputs = io->handleGetDigitalInputs();
    }
    std::ostringstream ss;
    ss << "    <tmd:GetDigitalInputsResponse>\r\n";
    for (const auto& in : inputs) {
        ss << "      <tmd:DigitalInputs token=\"" << Xml::escapeXmlAttr(in.token) << "\">\r\n"
           << "        <tt:IdleState>Open</tt:IdleState>\r\n"
           << "      </tmd:DigitalInputs>\r\n";
    }
    ss << "    </tmd:GetDigitalInputsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult DeviceIoService::handleGetInputOptions(const SoapContext& ctx)
{
    const auto tokenNode = ctx.reqNode.select_node(".//*[local-name()='Token']").node();
    const std::string token = tokenNode ? tokenNode.text().as_string() : "Input_1";

    std::ostringstream ss;
    ss << "    <tmd:GetDigitalInputConfigurationOptionsResponse>\r\n"
       << "      <tmd:DigitalInputOptions token=\"" << Xml::escapeXmlAttr(token) << "\">\r\n"
       << "        <tt:IdleState>open</tt:IdleState>\r\n"
       << "        <tt:IdleState>closed</tt:IdleState>\r\n"
       << "      </tmd:DigitalInputOptions>\r\n"
       << "    </tmd:GetDigitalInputConfigurationOptionsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

} // namespace Onvif
