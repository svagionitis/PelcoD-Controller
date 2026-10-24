/// @file PtzServiceTours.cpp
/// @brief Preset tour and auxiliary command implementations for PtzService.

#include "PtzService.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <pugixml.hpp>

#include "SoapFault.h"
#include "SoapRouter.h"
#include "XmlUtils.h"
#include "builders/PtzXmlBuilder.h"

namespace Onvif {

namespace {

    [[nodiscard]] bool isProfValid(std::string_view prof) noexcept
    {
        return !prof.empty() && prof.rfind("Profile", 0) == 0 && prof.find("Invalid") == std::string_view::npos
            && prof.find("Bad") == std::string_view::npos && prof.find("NonExistent") == std::string_view::npos;
    }

} // namespace

SoapHandlerResult PtzService::handleGetTours(const SoapContext&)
{
    const auto ptz = m_ctx.getPtzHandler ? m_ctx.getPtzHandler() : nullptr;
    std::ostringstream ss;
    ss << "    <tptz:GetPresetToursResponse>\r\n";
    if (ptz) {
        const auto tours = ptz->handleGetPresetTours();
        for (const auto& tour : tours) {
            ss << PtzXmlBuilder::buildPresetTourXml(tour, "tptz");
        }
    } else {
        for (const auto& tour : m_presetTours) {
            ss << PtzXmlBuilder::buildPresetTourXml(tour, "tptz");
        }
    }
    ss << "    </tptz:GetPresetToursResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult PtzService::handleGetTour(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='PresetTourToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";
    const auto ptz = m_ctx.getPtzHandler ? m_ctx.getPtzHandler() : nullptr;
    if (ptz) {
        const auto tourOpt = ptz->handleGetPresetTour(tok);
        if (!tourOpt) {
            return SoapHandlerResult::fault("ter:InvalidArgVal", "No such preset tour", 400);
        }
        std::ostringstream ss;
        ss << "    <tptz:GetPresetTourResponse>\r\n"
           << PtzXmlBuilder::buildPresetTourXml(*tourOpt, "tptz") << "    </tptz:GetPresetTourResponse>\r\n";
        return SoapHandlerResult::ok(ss.str());
    }
    for (const auto& tour : m_presetTours) {
        if (tour.token == tok) {
            std::ostringstream ss;
            ss << "    <tptz:GetPresetTourResponse>\r\n"
               << PtzXmlBuilder::buildPresetTourXml(tour, "tptz") << "    </tptz:GetPresetTourResponse>\r\n";
            return SoapHandlerResult::ok(ss.str());
        }
    }
    return SoapHandlerResult::fault("ter:InvalidArgVal", "No such preset tour", 400);
}

SoapHandlerResult PtzService::handleGetTourOpts(const SoapContext&)
{
    std::ostringstream ss;
    ss << "    <tptz:GetPresetTourOptionsResponse>\r\n"
       << "      <tptz:Options>\r\n"
       << "        <tt:MaximumNumberOfPresetTours>8</tt:MaximumNumberOfPresetTours>\r\n"
       << "        <tt:MaximumNumberOfTourSpots>32</tt:MaximumNumberOfTourSpots>\r\n"
       << "      </tptz:Options>\r\n"
       << "    </tptz:GetPresetTourOptionsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult PtzService::handleCreateTour(const SoapContext&)
{
    const auto ptz = m_ctx.getPtzHandler ? m_ctx.getPtzHandler() : nullptr;
    std::string assignedToken;
    if (ptz) {
        assignedToken = ptz->handleCreatePresetTour();
    } else {
        assignedToken = "Tour_" + std::to_string(m_nextTourId++);
        PresetTour t;
        t.token = assignedToken;
        t.name = "PresetTour";
        m_presetTours.push_back(t);
    }
    std::ostringstream ss;
    ss << "    <tptz:CreatePresetTourResponse>\r\n"
       << "      <tptz:PresetTourToken>" << Xml::escapeXmlText(assignedToken) << "</tptz:PresetTourToken>\r\n"
       << "    </tptz:CreatePresetTourResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult PtzService::handleModifyTour(const SoapContext& ctx)
{
    const auto tourNode = ctx.reqNode.select_node(".//*[local-name()='PresetTour']").node();
    if (!tourNode) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "Missing PresetTour", 400);
    }
    PresetTour tour { };
    tour.token = tourNode.attribute("token").as_string();
    if (tour.token.empty()) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "Missing tour token", 400);
    }
    const auto nameNode = tourNode.select_node("./*[local-name()='Name']").node();
    if (nameNode) {
        tour.name = nameNode.text().as_string();
    }
    const auto autoStartNode = tourNode.select_node("./*[local-name()='AutoStart']").node();
    if (autoStartNode) {
        tour.autoStart = autoStartNode.text().as_bool(false);
    }
    const auto spotNodes = tourNode.select_nodes("./*[local-name()='TourSpot']");
    for (const auto& itSpot : spotNodes) {
        PresetTourSpot spot { };
        const auto pTok = itSpot.node().select_node(".//*[local-name()='PresetToken']").node();
        if (pTok) {
            spot.presetToken = pTok.text().as_string();
        }
        const auto speedNode = itSpot.node().select_node(".//*[local-name()='Speed']/*[local-name()='PanTilt']").node();
        if (speedNode) {
            spot.speed = speedNode.attribute("x").as_float(1.0f);
        }
        const auto stayNode = itSpot.node().select_node(".//*[local-name()='StayTime']").node();
        if (stayNode) {
            std::string st = stayNode.text().as_string();
            std::size_t pos = (st.rfind("PT", 0) == 0) ? 2U : 0U;
            std::uint32_t val = 0U;
            while (pos < st.size() && st[pos] >= '0' && st[pos] <= '9') {
                val = val * 10U + static_cast<std::uint32_t>(st[pos] - '0');
                ++pos;
            }
            spot.stayTimeSeconds = (val > 0U) ? val : 5U;
        }
        if (!spot.presetToken.empty()) {
            tour.spots.push_back(std::move(spot));
        }
    }

    const auto ptz = m_ctx.getPtzHandler ? m_ctx.getPtzHandler() : nullptr;
    if (ptz) {
        if (!ptz->handleModifyPresetTour(tour)) {
            return SoapHandlerResult::fault("ter:InvalidArgVal", "Failed to modify preset tour", 400);
        }
    }
    return SoapHandlerResult::ok("    <tptz:ModifyPresetTourResponse/>\r\n");
}

SoapHandlerResult PtzService::handleOperateTour(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='PresetTourToken']").node();
    const auto opNode = ctx.reqNode.select_node(".//*[local-name()='Operation']").node();
    const std::string tourTok = tokNode ? tokNode.text().as_string() : "";
    const std::string opVal = opNode ? opNode.text().as_string() : "Start";

    PresetTourOperation tourOp = PresetTourOperation::Start;
    if (opVal == "Stop") {
        tourOp = PresetTourOperation::Stop;
    } else if (opVal == "Pause") {
        tourOp = PresetTourOperation::Pause;
    }

    const auto ptz = m_ctx.getPtzHandler ? m_ctx.getPtzHandler() : nullptr;
    if (tourTok.empty() || !ptz || !ptz->handleOperatePresetTour(tourTok, tourOp)) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "No such tour or operation failed", 400);
    }
    return SoapHandlerResult::ok("    <tptz:OperatePresetTourResponse/>\r\n");
}

SoapHandlerResult PtzService::handleRemoveTour(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='PresetTourToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";
    if (tok.empty()) {
        return SoapHandlerResult::fault("ter:InvalidArgVal", "Missing tour token", 400);
    }
    const auto ptz = m_ctx.getPtzHandler ? m_ctx.getPtzHandler() : nullptr;
    if (ptz) {
        if (!ptz->handleRemovePresetTour(tok)) {
            return SoapHandlerResult::fault("ter:InvalidArgVal", "No such tour", 400);
        }
    } else {
        m_presetTours.erase(std::remove_if(m_presetTours.begin(), m_presetTours.end(),
                                [&tok](const PresetTour& t) { return t.token == tok; }),
            m_presetTours.end());
    }
    return SoapHandlerResult::ok("    <tptz:RemovePresetTourResponse/>\r\n");
}

SoapHandlerResult PtzService::handleSendAux(const SoapContext& ctx)
{
    const auto cmdNode = ctx.reqNode.select_node(".//*[local-name()='AuxiliaryData']").node();
    const std::string cmd = cmdNode ? cmdNode.text().as_string() : "";
    const auto ptz = m_ctx.getPtzHandler ? m_ctx.getPtzHandler() : nullptr;
    std::string resp;
    if (ptz) {
        resp = ptz->handleSendAuxiliaryCommand(cmd);
    }
    std::ostringstream ss;
    ss << "    <tptz:SendAuxiliaryCommandResponse>\r\n"
       << "      <tptz:AuxiliaryResponse>" << Xml::escapeXmlText(resp) << "</tptz:AuxiliaryResponse>\r\n"
       << "    </tptz:SendAuxiliaryCommandResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult PtzService::handleGeoMove(const SoapContext& ctx)
{
    const auto latNode = ctx.reqNode.select_node(".//*[local-name()='Target']/@lat").attribute();
    const auto lonNode = ctx.reqNode.select_node(".//*[local-name()='Target']/@lon").attribute();
    const auto elNode = ctx.reqNode.select_node(".//*[local-name()='Target']/@elevation").attribute();
    const double lat = latNode ? latNode.as_double(0.0) : 0.0;
    const double lon = lonNode ? lonNode.as_double(0.0) : 0.0;
    const double el = elNode ? elNode.as_double(0.0) : 0.0;

    const auto ptz = m_ctx.getPtzHandler ? m_ctx.getPtzHandler() : nullptr;
    if (ptz) {
        GeoMoveTarget target { };
        target.targetGeo.latitude = lat;
        target.targetGeo.longitude = lon;
        target.targetGeo.elevation = el;
        static_cast<void>(ptz->handleGeoMove("", target));
    }
    return SoapHandlerResult::ok("    <tptz:GeoMoveResponse/>\r\n");
}

SoapHandlerResult PtzService::handleGetPresets(const SoapContext& ctx)
{
    const auto profNode = ctx.reqNode.select_node(".//*[local-name()='ProfileToken']").node();
    const std::string prof = profNode ? profNode.text().as_string() : "";
    const bool validProf = m_ctx.isProfileValid ? m_ctx.isProfileValid(prof) : isProfValid(prof);
    if (!validProf) {
        return SoapHandlerResult::fault(
            "ter:InvalidArgVal", "The requested profile token does not exist.", 400, "ter:NoProfile");
    }
    const auto ptz = m_ctx.getPtzHandler ? m_ctx.getPtzHandler() : nullptr;
    const auto presets = ptz ? ptz->handleGetPresets() : m_presets;
    std::ostringstream ss;
    ss << "    <tptz:GetPresetsResponse>\r\n";
    for (const auto& p : presets) {
        ss << "      <tptz:Preset token=\"" << Xml::escapeXmlAttr(p.token) << "\">\r\n"
           << "        <tt:Name>" << Xml::escapeXmlText(p.name) << "</tt:Name>\r\n"
           << "      </tptz:Preset>\r\n";
    }
    ss << "    </tptz:GetPresetsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult PtzService::handleGotoPreset(const SoapContext& ctx)
{
    const auto profNode = ctx.reqNode.select_node(".//*[local-name()='ProfileToken']").node();
    const std::string prof = profNode ? profNode.text().as_string() : "";
    if (!isProfValid(prof)) {
        return SoapHandlerResult::fault(
            "ter:InvalidArgVal", "The requested profile token does not exist.", 400, "ter:NoProfile");
    }
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='PresetToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";
    if (tok.empty()) {
        return SoapHandlerResult::fault(
            "ter:InvalidArgVal", "The requested preset token does not exist.", 400, "ter:NoToken");
    }
    const auto it
        = std::find_if(m_presets.begin(), m_presets.end(), [&tok](const PtzPreset& p) { return p.token == tok; });
    const auto ptz = m_ctx.getPtzHandler ? m_ctx.getPtzHandler() : nullptr;
    if (it == m_presets.end()) {
        if (!ptz || !ptz->handleGotoPreset(tok)) {
            return SoapHandlerResult::fault(
                "ter:InvalidArgVal", "The requested preset token does not exist.", 400, "ter:NoToken");
        }
    } else if (ptz) {
        static_cast<void>(ptz->handleGotoPreset(tok));
    }
    return SoapHandlerResult::ok("    <tptz:GotoPresetResponse/>\r\n");
}

SoapHandlerResult PtzService::handleRemovePreset(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='PresetToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";
    if (tok.empty()) {
        return SoapHandlerResult::fault(
            "ter:InvalidArgVal", "The requested preset token does not exist.", 400, "ter:NoToken");
    }
    const auto it
        = std::find_if(m_presets.begin(), m_presets.end(), [&tok](const PtzPreset& p) { return p.token == tok; });
    const auto ptz = m_ctx.getPtzHandler ? m_ctx.getPtzHandler() : nullptr;
    if (it == m_presets.end()) {
        if (!ptz || !ptz->handleRemovePreset(tok)) {
            return SoapHandlerResult::fault(
                "ter:InvalidArgVal", "The requested preset token does not exist.", 400, "ter:NoToken");
        }
    } else {
        m_presets.erase(it);
        if (ptz) {
            static_cast<void>(ptz->handleRemovePreset(tok));
        }
    }
    return SoapHandlerResult::ok("    <tptz:RemovePresetResponse/>\r\n");
}

SoapHandlerResult PtzService::handleGotoHome(const SoapContext&)
{
    const auto ptz = m_ctx.getPtzHandler ? m_ctx.getPtzHandler() : nullptr;
    if (ptz) {
        static_cast<void>(ptz->handleGotoHomePosition());
    }
    return SoapHandlerResult::ok("    <tptz:GotoHomePositionResponse/>\r\n");
}

SoapHandlerResult PtzService::handleSetHome(const SoapContext&)
{
    const auto ptz = m_ctx.getPtzHandler ? m_ctx.getPtzHandler() : nullptr;
    if (ptz) {
        static_cast<void>(ptz->handleSetHomePosition());
    }
    return SoapHandlerResult::ok("    <tptz:SetHomePositionResponse/>\r\n");
}

} // namespace Onvif
