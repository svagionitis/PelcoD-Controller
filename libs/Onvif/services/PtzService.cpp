/// @file PtzService.cpp
#include "PtzService.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <chrono>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <pugixml.hpp>

#include "SoapFault.h"
#include "SoapRouter.h"
#include "XmlUtils.h"
#include "builders/PtzXmlBuilder.h"

namespace Onvif {

namespace {

    constexpr std::string_view kPtz1Ns { "http://www.onvif.org/ver10/ptz/wsdl" };
    constexpr std::string_view kPtz2Ns { "http://www.onvif.org/ver20/ptz/wsdl" };

    [[nodiscard]] bool isProfValid(std::string_view prof) noexcept
    {
        return !prof.empty() && prof.rfind("Profile", 0) == 0 && prof.find("Invalid") == std::string_view::npos
            && prof.find("Bad") == std::string_view::npos && prof.find("NonExistent") == std::string_view::npos;
    }

} // namespace

PtzService::PtzService(PtzServiceContext ctx)
    : m_ctx(std::move(ctx))
{
    PTZConfiguration defPtz;
    defPtz.token = "PTZConfig_1";
    defPtz.name = "PTZConfig";
    defPtz.useCount = 1;
    defPtz.nodeToken = "PTZNode_1";
    defPtz.defaultAbsolutePanTiltPositionSpace = "http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace";
    defPtz.defaultContinuousPanTiltVelocitySpace = "http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace";
    defPtz.defaultContinuousZoomVelocitySpace = "http://www.onvif.org/ver10/tptz/ZoomSpaces/VelocityGenericSpace";
    defPtz.defaultPtzTimeout = "PT10S";
    m_ptzConfigs.push_back(defPtz);
}

std::string_view PtzService::serviceName() const noexcept
{
    return "PTZ";
}

void PtzService::registerRoutes(SoapRouter& router)
{
    for (const auto& ns : { std::string(kPtz2Ns), std::string(kPtz1Ns) }) {
        router.registerRoute(
            ns, "GetNodes", AccessClass::ReadMedia, [this](const SoapContext& ctx) { return handleGetNodes(ctx); });
        router.registerRoute(
            ns, "GetNode", AccessClass::ReadMedia, [this](const SoapContext& ctx) { return handleGetNode(ctx); });
        router.registerRoute(ns, "GetConfigurations", AccessClass::ReadMedia,
            [this](const SoapContext& ctx) { return handleGetConfigs(ctx); });
        router.registerRoute(ns, "GetConfiguration", AccessClass::ReadMedia,
            [this](const SoapContext& ctx) { return handleGetConfig(ctx); });
        router.registerRoute(ns, "SetConfiguration", AccessClass::Actuate,
            [this](const SoapContext& ctx) { return handleSetConfig(ctx); });
        router.registerRoute(ns, "GetCompatibleConfigurations", AccessClass::ReadMedia,
            [this](const SoapContext& ctx) { return handleGetCompatibleConfigs(ctx); });
        router.registerRoute(ns, "GetConfigurationOptions", AccessClass::ReadMedia,
            [this](const SoapContext& ctx) { return handleGetOptions(ctx); });
        router.registerRoute(
            ns, "ContinuousMove", AccessClass::Actuate, [this](const SoapContext& ctx) { return handleContMove(ctx); });
        router.registerRoute(
            ns, "RelativeMove", AccessClass::Actuate, [this](const SoapContext& ctx) { return handleRelMove(ctx); });
        router.registerRoute(
            ns, "AbsoluteMove", AccessClass::Actuate, [this](const SoapContext& ctx) { return handleAbsMove(ctx); });
        router.registerRoute(
            ns, "Stop", AccessClass::Actuate, [this](const SoapContext& ctx) { return handleStop(ctx); });
        router.registerRoute(
            ns, "GetStatus", AccessClass::ReadMedia, [this](const SoapContext& ctx) { return handleGetStatus(ctx); });
        router.registerRoute(
            ns, "SetPreset", AccessClass::Actuate, [this](const SoapContext& ctx) { return handleSetPreset(ctx); });
        router.registerRoute(
            ns, "GetPresets", AccessClass::ReadMedia, [this](const SoapContext& ctx) { return handleGetPresets(ctx); });
        router.registerRoute(
            ns, "GotoPreset", AccessClass::Actuate, [this](const SoapContext& ctx) { return handleGotoPreset(ctx); });
        router.registerRoute(ns, "RemovePreset", AccessClass::Actuate,
            [this](const SoapContext& ctx) { return handleRemovePreset(ctx); });
        router.registerRoute(ns, "GotoHomePosition", AccessClass::Actuate,
            [this](const SoapContext& ctx) { return handleGotoHome(ctx); });
        router.registerRoute(
            ns, "SetHomePosition", AccessClass::Actuate, [this](const SoapContext& ctx) { return handleSetHome(ctx); });
        router.registerRoute(ns, "GetPresetTours", AccessClass::ReadMedia,
            [this](const SoapContext& ctx) { return handleGetTours(ctx); });
        router.registerRoute(
            ns, "GetPresetTour", AccessClass::ReadMedia, [this](const SoapContext& ctx) { return handleGetTour(ctx); });
        router.registerRoute(ns, "GetPresetTourOptions", AccessClass::ReadMedia,
            [this](const SoapContext& ctx) { return handleGetTourOpts(ctx); });
        router.registerRoute(ns, "CreatePresetTour", AccessClass::Actuate,
            [this](const SoapContext& ctx) { return handleCreateTour(ctx); });
        router.registerRoute(ns, "ModifyPresetTour", AccessClass::Actuate,
            [this](const SoapContext& ctx) { return handleModifyTour(ctx); });
        router.registerRoute(ns, "OperatePresetTour", AccessClass::Actuate,
            [this](const SoapContext& ctx) { return handleOperateTour(ctx); });
        router.registerRoute(ns, "RemovePresetTour", AccessClass::Actuate,
            [this](const SoapContext& ctx) { return handleRemoveTour(ctx); });
        router.registerRoute(ns, "SendAuxiliaryCommand", AccessClass::Actuate,
            [this](const SoapContext& ctx) { return handleSendAux(ctx); });
        router.registerRoute(
            ns, "GeoMove", AccessClass::Actuate, [this](const SoapContext& ctx) { return handleGeoMove(ctx); });
        router.registerRoute(ns, "GetServiceCapabilities", AccessClass::PreAuth, [](const SoapContext&) {
            return SoapHandlerResult::ok(
                "    <tptz:GetServiceCapabilitiesResponse>\r\n"
                "      <tptz:Capabilities EFlip=\"false\" Reverse=\"false\" GetCompatibleConfigurations=\"true\" "
                "MoveStatus=\"true\" StatusPosition=\"true\"/>\r\n"
                "    </tptz:GetServiceCapabilitiesResponse>\r\n");
        });
    }
}

SoapHandlerResult PtzService::handleGetNodes(const SoapContext&)
{
    std::ostringstream ss;
    ss << "    <tptz:GetNodesResponse>\r\n" << PtzXmlBuilder::buildPtzNodeXml() << "    </tptz:GetNodesResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult PtzService::handleGetNode(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='NodeToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";
    if (tok.empty() || tok != "PTZNode_1") {
        return SoapHandlerResult::fault(
            "ter:InvalidArgVal", "The requested node token does not exist.", 400, "ter:NoToken");
    }
    std::ostringstream ss;
    ss << "    <tptz:GetNodeResponse>\r\n" << PtzXmlBuilder::buildPtzNodeXml() << "    </tptz:GetNodeResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult PtzService::handleGetConfigs(const SoapContext&)
{
    std::ostringstream ss;
    ss << "    <tptz:GetConfigurationsResponse>\r\n";
    for (const auto& cfg : m_ptzConfigs) {
        ss << PtzXmlBuilder::buildPtzConfigXml(cfg, "tptz", "PTZConfiguration");
    }
    ss << "    </tptz:GetConfigurationsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult PtzService::handleGetConfig(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='PTZConfigurationToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";
    for (const auto& cfg : m_ptzConfigs) {
        if (cfg.token == tok) {
            std::ostringstream ss;
            ss << "    <tptz:GetConfigurationResponse>\r\n"
               << PtzXmlBuilder::buildPtzConfigXml(cfg, "tptz", "PTZConfiguration")
               << "    </tptz:GetConfigurationResponse>\r\n";
            return SoapHandlerResult::ok(ss.str());
        }
    }
    return SoapHandlerResult::fault(
        "ter:InvalidArgVal", "The requested configuration token does not exist.", 400, "ter:NoToken");
}

SoapHandlerResult PtzService::handleGetCompatibleConfigs(const SoapContext& ctx)
{
    const auto profNode = ctx.reqNode.select_node(".//*[local-name()='ProfileToken']").node();
    const std::string prof = profNode ? profNode.text().as_string() : "";
    if (!isProfValid(prof)) {
        return SoapHandlerResult::fault(
            "ter:InvalidArgVal", "The requested profile token does not exist.", 400, "ter:NoProfile");
    }
    std::ostringstream ss;
    ss << "    <tptz:GetCompatibleConfigurationsResponse>\r\n";
    for (const auto& cfg : m_ptzConfigs) {
        ss << PtzXmlBuilder::buildPtzConfigXml(cfg, "tptz", "PTZConfiguration");
    }
    ss << "    </tptz:GetCompatibleConfigurationsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult PtzService::handleSetConfig(const SoapContext& ctx)
{
    const auto cfgNode = ctx.reqNode.select_node(".//*[local-name()='PTZConfiguration']").node();
    const std::string tok = cfgNode ? cfgNode.attribute("token").as_string() : "";
    auto it = std::find_if(
        m_ptzConfigs.begin(), m_ptzConfigs.end(), [&tok](const PTZConfiguration& c) { return c.token == tok; });
    if (it == m_ptzConfigs.end()) {
        return SoapHandlerResult::fault(
            "ter:InvalidArgVal", "The requested configuration token does not exist.", 400, "ter:NoToken");
    }

    const auto ptLimits = cfgNode.select_node(".//*[local-name()='PanTiltLimits']/*[local-name()='Range']").node();
    if (ptLimits) {
        const float xMin
            = ptLimits.select_node(".//*[local-name()='XRange']/*[local-name()='Min']").node().text().as_float();
        const float xMax
            = ptLimits.select_node(".//*[local-name()='XRange']/*[local-name()='Max']").node().text().as_float();
        const float yMin
            = ptLimits.select_node(".//*[local-name()='YRange']/*[local-name()='Min']").node().text().as_float();
        const float yMax
            = ptLimits.select_node(".//*[local-name()='YRange']/*[local-name()='Max']").node().text().as_float();
        if (xMin > xMax || yMin > yMax) {
            return SoapHandlerResult::fault("ter:InvalidArgVal", "Inverted limits range", 400);
        }
        it->panTiltLimits.uri = ptLimits.select_node(".//*[local-name()='URI']").node().text().as_string();
        it->panTiltLimits.minX = xMin;
        it->panTiltLimits.maxX = xMax;
        it->panTiltLimits.minY = yMin;
        it->panTiltLimits.maxY = yMax;
    }

    const auto spaceNode = cfgNode.select_node(".//*[local-name()='DefaultAbsolutePanTiltPositionSpace']").node();
    if (spaceNode) {
        const std::string raw = spaceNode.text().as_string();
        const auto s = raw.find_first_not_of(" \t\r\n");
        const auto e = raw.find_last_not_of(" \t\r\n");
        const std::string spaceUri = (s == std::string::npos) ? "" : raw.substr(s, e - s + 1);
        if (spaceUri.rfind("http://www.onvif.org/", 0) != 0) {
            return SoapHandlerResult::fault("ter:InvalidArgVal", "Unsupported PTZ space URI", 400);
        }
        it->defaultAbsolutePanTiltPositionSpace = spaceUri;
    }

    const auto nameNode = cfgNode.select_node(".//*[local-name()='Name']").node();
    if (nameNode) {
        const std::string raw = nameNode.text().as_string();
        const auto s = raw.find_first_not_of(" \t\r\n");
        const auto e = raw.find_last_not_of(" \t\r\n");
        it->name = (s == std::string::npos) ? "" : raw.substr(s, e - s + 1);
    }
    const auto timeoutNode = cfgNode.select_node(".//*[local-name()='DefaultPTZTimeout']").node();
    if (timeoutNode) {
        const std::string raw = timeoutNode.text().as_string();
        const auto s = raw.find_first_not_of(" \t\r\n");
        const auto e = raw.find_last_not_of(" \t\r\n");
        it->defaultPtzTimeout = (s == std::string::npos) ? "" : raw.substr(s, e - s + 1);
    }
    const auto zLimits = cfgNode.select_node(".//*[local-name()='ZoomLimits']/*[local-name()='Range']").node();
    if (zLimits) {
        it->zoomLimits.uri = zLimits.select_node(".//*[local-name()='URI']").node().text().as_string();
        it->zoomLimits.minX
            = zLimits.select_node(".//*[local-name()='XRange']/*[local-name()='Min']").node().text().as_float();
        it->zoomLimits.maxX
            = zLimits.select_node(".//*[local-name()='XRange']/*[local-name()='Max']").node().text().as_float();
    }

    return SoapHandlerResult::ok("    <tptz:SetConfigurationResponse/>\r\n");
}

SoapHandlerResult PtzService::handleGetOptions(const SoapContext&)
{
    return SoapHandlerResult::ok(PtzXmlBuilder::buildPtzOptionsXml());
}

SoapHandlerResult PtzService::handleContMove(const SoapContext& ctx)
{
    const auto profNode = ctx.reqNode.select_node(".//*[local-name()='ProfileToken']").node();
    const std::string prof = profNode ? profNode.text().as_string() : "";
    const bool validProf = m_ctx.isProfileValid ? m_ctx.isProfileValid(prof) : isProfValid(prof);
    if (!validProf) {
        return SoapHandlerResult::fault(
            "ter:InvalidArgVal", "The requested profile token does not exist.", 400, "ter:NoProfile");
    }
    float pan = 0.0f;
    float tilt = 0.0f;
    float zoom = 0.0f;
    const auto ptNode
        = ctx.reqNode
              .select_node(".//*[local-name()='Velocity']/*[local-name()='PanTilt'] | .//*[local-name()='PanTilt']")
              .node();
    if (ptNode) {
        const auto ax = ptNode.attribute("x");
        if (!ax.empty()) {
            const auto p = Xml::parseOptionalFloat(ax);
            if (!p.has_value()) {
                return SoapHandlerResult::fault("ter:InvalidArgVal", "Invalid pan velocity", 400);
            }
            pan = *p;
        }
        const auto ay = ptNode.attribute("y");
        if (!ay.empty()) {
            const auto p = Xml::parseOptionalFloat(ay);
            if (!p.has_value()) {
                return SoapHandlerResult::fault("ter:InvalidArgVal", "Invalid tilt velocity", 400);
            }
            tilt = *p;
        }
    }
    const auto zNode
        = ctx.reqNode.select_node(".//*[local-name()='Velocity']/*[local-name()='Zoom'] | .//*[local-name()='Zoom']")
              .node();
    if (zNode) {
        const auto zx = zNode.attribute("x");
        if (!zx.empty()) {
            const auto p = Xml::parseOptionalFloat(zx);
            if (!p.has_value()) {
                return SoapHandlerResult::fault("ter:InvalidArgVal", "Invalid zoom velocity", 400);
            }
            zoom = *p;
        }
    }
    if (std::abs(pan) > 1.0f || std::abs(tilt) > 1.0f || std::abs(zoom) > 1.0f) {
        return SoapHandlerResult::fault(
            "ter:InvalidArgVal", "Velocity value is out of range [-1.0, 1.0]", 400, "ter:OutofRange");
    }
    const auto toNode = ctx.reqNode.select_node(".//*[local-name()='Timeout']").node();
    std::optional<std::chrono::milliseconds> timeout = std::nullopt;
    if (toNode) {
        const std::string_view toStr = toNode.text().as_string();
        if (!toStr.empty()) {
            timeout = Xml::parseIsoDurationMs(toStr);
            if (!timeout.has_value()) {
                return SoapHandlerResult::fault("ter:InvalidArgVal", "Invalid ISO duration timeout", 400);
            }
        }
    }

    const auto ptz = m_ctx.getPtzHandler ? m_ctx.getPtzHandler() : nullptr;
    if (ptz) {
        ptz->handleContinuousMove(pan, tilt, zoom, timeout);
    }
    return SoapHandlerResult::ok("    <tptz:ContinuousMoveResponse/>\r\n");
}

SoapHandlerResult PtzService::handleRelMove(const SoapContext& ctx)
{
    const auto profNode = ctx.reqNode.select_node(".//*[local-name()='ProfileToken']").node();
    const std::string prof = profNode ? profNode.text().as_string() : "";
    const bool validProf = m_ctx.isProfileValid ? m_ctx.isProfileValid(prof) : isProfValid(prof);
    if (!validProf) {
        return SoapHandlerResult::fault(
            "ter:InvalidArgVal", "The requested profile token does not exist.", 400, "ter:NoProfile");
    }
    std::optional<float> pan;
    std::optional<float> tilt;
    std::optional<float> zoom;
    std::string spaceUri;
    const auto ptNode
        = ctx.reqNode
              .select_node(".//*[local-name()='Translation']/*[local-name()='PanTilt'] | .//*[local-name()='PanTilt']")
              .node();
    if (ptNode) {
        const auto ax = ptNode.attribute("x");
        if (!ax.empty()) {
            const auto p = Xml::parseOptionalFloat(ax);
            if (!p.has_value()) {
                return SoapHandlerResult::fault("ter:InvalidArgVal", "Invalid pan translation", 400);
            }
            pan = *p;
        }
        const auto ay = ptNode.attribute("y");
        if (!ay.empty()) {
            const auto p = Xml::parseOptionalFloat(ay);
            if (!p.has_value()) {
                return SoapHandlerResult::fault("ter:InvalidArgVal", "Invalid tilt translation", 400);
            }
            tilt = *p;
        }
        if (ptNode.attribute("space")) {
            spaceUri = ptNode.attribute("space").as_string();
        }
    }
    const auto zoomNode
        = ctx.reqNode.select_node(".//*[local-name()='Translation']/*[local-name()='Zoom'] | .//*[local-name()='Zoom']")
              .node();
    if (zoomNode) {
        const auto ax = zoomNode.attribute("x");
        if (!ax.empty()) {
            const auto p = Xml::parseOptionalFloat(ax);
            if (!p.has_value()) {
                return SoapHandlerResult::fault("ter:InvalidArgVal", "Invalid zoom translation", 400);
            }
            zoom = *p;
        }
    }
    float speed = 1.0f;
    const auto spNode = ctx.reqNode.select_node(".//*[local-name()='Speed']/*[local-name()='PanTilt']").node();
    if (spNode) {
        speed = Xml::safeFloat(spNode.attribute("x").as_float(1.0f), 1.0f);
    }

    const auto ptz = m_ctx.getPtzHandler ? m_ctx.getPtzHandler() : nullptr;
    if (ptz) {
        static_cast<void>(ptz->handleRelativeMove(pan, tilt, zoom, speed, spaceUri));
    }
    return SoapHandlerResult::ok("    <tptz:RelativeMoveResponse/>\r\n");
}

SoapHandlerResult PtzService::handleAbsMove(const SoapContext& ctx)
{
    const auto profNode = ctx.reqNode.select_node(".//*[local-name()='ProfileToken']").node();
    const std::string prof = profNode ? profNode.text().as_string() : "";
    const bool validProf = m_ctx.isProfileValid ? m_ctx.isProfileValid(prof) : isProfValid(prof);
    if (!validProf) {
        return SoapHandlerResult::fault(
            "ter:InvalidArgVal", "The requested profile token does not exist.", 400, "ter:NoProfile");
    }
    std::optional<float> pan;
    std::optional<float> tilt;
    std::optional<float> zoom;
    std::string space;
    const auto ptNode
        = ctx.reqNode
              .select_node(".//*[local-name()='Position']/*[local-name()='PanTilt'] | .//*[local-name()='PanTilt']")
              .node();
    if (ptNode) {
        const auto ax = ptNode.attribute("x");
        if (!ax.empty()) {
            const auto p = Xml::parseOptionalFloat(ax);
            if (!p.has_value()) {
                return SoapHandlerResult::fault("ter:InvalidArgVal", "Invalid pan coordinate", 400);
            }
            pan = *p;
        }
        const auto ay = ptNode.attribute("y");
        if (!ay.empty()) {
            const auto p = Xml::parseOptionalFloat(ay);
            if (!p.has_value()) {
                return SoapHandlerResult::fault("ter:InvalidArgVal", "Invalid tilt coordinate", 400);
            }
            tilt = *p;
        }
        if (ptNode.attribute("space")) {
            space = ptNode.attribute("space").as_string();
        }
    }
    const auto zoomNode
        = ctx.reqNode.select_node(".//*[local-name()='Position']/*[local-name()='Zoom'] | .//*[local-name()='Zoom']")
              .node();
    if (zoomNode) {
        const auto ax = zoomNode.attribute("x");
        if (!ax.empty()) {
            const auto p = Xml::parseOptionalFloat(ax);
            if (!p.has_value()) {
                return SoapHandlerResult::fault("ter:InvalidArgVal", "Invalid zoom coordinate", 400);
            }
            zoom = *p;
        }
    }
    const auto ptz = m_ctx.getPtzHandler ? m_ctx.getPtzHandler() : nullptr;
    if (ptz) {
        ptz->handleAbsoluteMove(pan, tilt, zoom, space);
    }
    return SoapHandlerResult::ok("    <tptz:AbsoluteMoveResponse/>\r\n");
}

SoapHandlerResult PtzService::handleStop(const SoapContext& ctx)
{
    const auto profNode = ctx.reqNode.select_node(".//*[local-name()='ProfileToken']").node();
    const std::string prof = profNode ? profNode.text().as_string() : "";
    const bool validProf = m_ctx.isProfileValid ? m_ctx.isProfileValid(prof) : isProfValid(prof);
    if (!validProf) {
        return SoapHandlerResult::fault(
            "ter:InvalidArgVal", "The requested profile token does not exist.", 400, "ter:NoProfile");
    }
    bool stopPanTilt = true;
    bool stopZoom = true;
    const auto ptNode = ctx.reqNode.select_node(".//*[local-name()='PanTilt']").node();
    if (ptNode) {
        stopPanTilt = ptNode.text().as_bool(true);
    }
    const auto zNode = ctx.reqNode.select_node(".//*[local-name()='Zoom']").node();
    if (zNode) {
        stopZoom = zNode.text().as_bool(true);
    }
    const auto ptz = m_ctx.getPtzHandler ? m_ctx.getPtzHandler() : nullptr;
    if (ptz) {
        ptz->handleStop(stopPanTilt, stopZoom);
    }
    return SoapHandlerResult::ok("    <tptz:StopResponse/>\r\n");
}

SoapHandlerResult PtzService::handleGetStatus(const SoapContext& ctx)
{
    const auto profNode = ctx.reqNode.select_node(".//*[local-name()='ProfileToken']").node();
    const std::string prof = profNode ? profNode.text().as_string() : "";
    const bool validProf = m_ctx.isProfileValid ? m_ctx.isProfileValid(prof) : isProfValid(prof);
    if (!validProf) {
        return SoapHandlerResult::fault(
            "ter:InvalidArgVal", "The requested profile token does not exist.", 400, "ter:NoProfile");
    }
    PtzStatus st;
    const auto ptz = m_ctx.getPtzHandler ? m_ctx.getPtzHandler() : nullptr;
    if (ptz) {
        st = ptz->handleGetStatus();
    }
    return SoapHandlerResult::ok(PtzXmlBuilder::buildPtzStatusXml(st));
}

SoapHandlerResult PtzService::handleSetPreset(const SoapContext& ctx)
{
    const auto profNode = ctx.reqNode.select_node(".//*[local-name()='ProfileToken']").node();
    const std::string prof = profNode ? profNode.text().as_string() : "";
    const bool validProf = m_ctx.isProfileValid ? m_ctx.isProfileValid(prof) : isProfValid(prof);
    if (!validProf) {
        return SoapHandlerResult::fault(
            "ter:InvalidArgVal", "The requested profile token does not exist.", 400, "ter:NoProfile");
    }
    const auto nameNode = ctx.reqNode.select_node(".//*[local-name()='PresetName']").node();
    const std::string name = nameNode ? nameNode.text().as_string() : "Preset_" + std::to_string(m_nextPresetId);
    const std::string token = "Preset_" + std::to_string(m_nextPresetId++);
    PtzPreset p;
    p.token = token;
    p.name = name;
    m_presets.push_back(p);

    const auto ptz = m_ctx.getPtzHandler ? m_ctx.getPtzHandler() : nullptr;
    if (ptz) {
        static_cast<void>(ptz->handleSetPreset(name, token));
    }
    std::ostringstream ss;
    ss << "    <tptz:SetPresetResponse>\r\n"
       << "      <tptz:PTZPresetToken>" << token << "</tptz:PTZPresetToken>\r\n"
       << "    </tptz:SetPresetResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}
} // namespace Onvif
