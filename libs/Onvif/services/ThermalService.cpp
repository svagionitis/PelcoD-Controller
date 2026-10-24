/// @file ThermalService.cpp
/// @brief Modular ONVIF Thermal & Radiometry Service dispatcher.

#include "ThermalService.h"

#include <iomanip>
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
    constexpr std::string_view kThermalNs { "http://www.onvif.org/ver10/thermal/wsdl" };
}

ThermalService::ThermalService(ThermalServiceContext ctx)
    : m_ctx(std::move(ctx))
{
    m_radiometry.emissivity = 0.95F;
    m_radiometry.distance = 5.0F;

    ColorPalette p1;
    p1.token = "Palette_1";
    p1.name = "WhiteHot";
    p1.isDefault = true;
    m_palettes.push_back(p1);

    ColorPalette p2;
    p2.token = "Palette_2";
    p2.name = "BlackHot";
    p2.isDefault = false;
    m_palettes.push_back(p2);

    ColorPalette p3;
    p3.token = "Ironbow";
    p3.name = "Ironbow";
    p3.isDefault = false;
    m_palettes.push_back(p3);
}

std::string_view ThermalService::serviceName() const noexcept
{
    return "Thermal";
}

void ThermalService::registerRoutes(SoapRouter& router)
{
    const std::string ns { kThermalNs };

    router.registerRoute(ns, "GetServiceCapabilities", AccessClass::PreAuth, [](const SoapContext&) {
        return SoapHandlerResult::ok(
            "    <tth:GetServiceCapabilitiesResponse>\r\n"
            "      <tth:Capabilities Radiometry=\"true\" ColorPalette=\"true\" NUC=\"true\" Cooler=\"false\"/>\r\n"
            "    </tth:GetServiceCapabilitiesResponse>\r\n");
    });
    router.registerRoute(ns, "GetRadiometryConfigurationOptions", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetRadiometryOpts(ctx); });
    router.registerRoute(ns, "GetRadiometryConfiguration", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetRadiometry(ctx); });
    router.registerRoute(ns, "SetRadiometryConfiguration", AccessClass::Actuate,
        [this](const SoapContext& ctx) { return handleSetRadiometry(ctx); });
    router.registerRoute(ns, "GetRadiometrySpots", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetSpots(ctx); });
    router.registerRoute(
        ns, "SetRadiometrySpots", AccessClass::Actuate, [this](const SoapContext& ctx) { return handleSetSpots(ctx); });
    router.registerRoute(ns, "GetRadiometryBoxes", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetBoxes(ctx); });
    router.registerRoute(
        ns, "SetRadiometryBoxes", AccessClass::Actuate, [this](const SoapContext& ctx) { return handleSetBoxes(ctx); });
    router.registerRoute(ns, "GetColorPalettes", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetPalettes(ctx); });
    router.registerRoute(
        ns, "SetColorPalette", AccessClass::Actuate, [this](const SoapContext& ctx) { return handleSetPalette(ctx); });
    router.registerRoute(
        ns, "TriggerNUC", AccessClass::Actuate, [this](const SoapContext& ctx) { return handleTriggerNuc(ctx); });
    router.registerRoute(
        ns, "ManualNUC", AccessClass::Actuate, [this](const SoapContext& ctx) { return handleTriggerNuc(ctx); });
}

SoapHandlerResult ThermalService::handleGetRadiometryOpts(const SoapContext&)
{
    return SoapHandlerResult::ok("    <tth:GetRadiometryConfigurationOptionsResponse>\r\n"
                                 "      <tth:Options>\r\n"
                                 "        <tth:EmissivityRange Min=\"0.01\" Max=\"1.00\"/>\r\n"
                                 "        <tth:DistanceRange Min=\"0.1\" Max=\"1000.0\"/>\r\n"
                                 "        <tth:ReflectedTemperatureRange Min=\"-50.0\" Max=\"1500.0\"/>\r\n"
                                 "        <tth:AtmosphericTemperatureRange Min=\"-50.0\" Max=\"100.0\"/>\r\n"
                                 "        <tth:RelativeHumidityRange Min=\"0.0\" Max=\"100.0\"/>\r\n"
                                 "        <tth:WindowTransmissionRange Min=\"0.01\" Max=\"1.00\"/>\r\n"
                                 "      </tth:Options>\r\n"
                                 "    </tth:GetRadiometryConfigurationOptionsResponse>\r\n");
}

SoapHandlerResult ThermalService::handleGetRadiometry(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";
    RadiometryConfig cfg = m_radiometry;
    const auto handler = m_ctx.getThermalHandler ? m_ctx.getThermalHandler() : nullptr;
    if (handler) {
        cfg = handler->handleGetRadiometryConfig(tok);
    }
    std::ostringstream ss;
    ss << "    <tth:GetRadiometryConfigurationResponse>\r\n"
       << "      <tth:Configuration>\r\n"
       << "        <tth:VideoSourceToken>" << Xml::escapeXmlText(tok) << "</tth:VideoSourceToken>\r\n"
       << "        <tth:Emissivity>" << std::fixed << std::setprecision(2) << cfg.emissivity << "</tth:Emissivity>\r\n"
       << "        <tth:Distance>" << std::fixed << std::setprecision(1) << cfg.distance << "</tth:Distance>\r\n"
       << "        <tth:ReflectedTemperature>" << std::fixed << std::setprecision(1) << cfg.reflectedTemperature
       << "</tth:ReflectedTemperature>\r\n"
       << "        <tth:AtmosphericTemperature>" << std::fixed << std::setprecision(1) << cfg.atmosphericTemperature
       << "</tth:AtmosphericTemperature>\r\n"
       << "        <tth:RelativeHumidity>" << std::fixed << std::setprecision(1) << cfg.relativeHumidity
       << "</tth:RelativeHumidity>\r\n"
       << "        <tth:WindowTransmission>" << std::fixed << std::setprecision(2) << cfg.windowTransmission
       << "</tth:WindowTransmission>\r\n"
       << "      </tth:Configuration>\r\n"
       << "    </tth:GetRadiometryConfigurationResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult ThermalService::handleSetRadiometry(const SoapContext& ctx)
{
    const auto cfgNode = ctx.reqNode.select_node(".//*[local-name()='Configuration']").node();
    const auto tokNode
        = cfgNode ? cfgNode.select_node(".//*[local-name()='VideoSourceToken']").node() : pugi::xml_node { };
    const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";

    RadiometryConfig cfg = m_radiometry;
    if (cfgNode) {
        const auto emNode = cfgNode.select_node(".//*[local-name()='Emissivity']").node();
        if (emNode)
            cfg.emissivity = emNode.text().as_float(0.95F);
        const auto distNode = cfgNode.select_node(".//*[local-name()='Distance']").node();
        if (distNode)
            cfg.distance = distNode.text().as_float(5.0F);
        const auto refNode = cfgNode.select_node(".//*[local-name()='ReflectedTemperature']").node();
        if (refNode)
            cfg.reflectedTemperature = refNode.text().as_float(20.0F);
        const auto atmNode = cfgNode.select_node(".//*[local-name()='AtmosphericTemperature']").node();
        if (atmNode)
            cfg.atmosphericTemperature = atmNode.text().as_float(20.0F);
        const auto humNode = cfgNode.select_node(".//*[local-name()='RelativeHumidity']").node();
        if (humNode)
            cfg.relativeHumidity = humNode.text().as_float(50.0F);
        const auto winNode = cfgNode.select_node(".//*[local-name()='WindowTransmission']").node();
        if (winNode)
            cfg.windowTransmission = winNode.text().as_float(1.0F);
    }
    const auto handler = m_ctx.getThermalHandler ? m_ctx.getThermalHandler() : nullptr;
    if (handler) {
        static_cast<void>(handler->handleSetRadiometryConfig(tok, cfg));
    }
    m_radiometry = cfg;
    return SoapHandlerResult::ok("    <tth:SetRadiometryConfigurationResponse/>\r\n");
}

SoapHandlerResult ThermalService::handleGetSpots(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";
    std::vector<RadiometrySpot> spots = m_spots;
    const auto handler = m_ctx.getThermalHandler ? m_ctx.getThermalHandler() : nullptr;
    if (handler) {
        spots = handler->handleGetRadiometrySpots(tok);
    }
    std::ostringstream ss;
    ss << "    <tth:GetRadiometrySpotsResponse>\r\n";
    for (const auto& s : spots) {
        ss << "      <tth:Spot token=\"" << Xml::escapeXmlAttr(s.token) << "\">\r\n"
           << "        <tth:Position x=\"" << std::fixed << std::setprecision(4) << s.position.x << "\" y=\""
           << s.position.y << "\"/>\r\n"
           << "        <tth:Label>" << Xml::escapeXmlText(s.label) << "</tth:Label>\r\n"
           << "        <tth:Temperature>" << std::fixed << std::setprecision(1) << s.temperature
           << "</tth:Temperature>\r\n"
           << "      </tth:Spot>\r\n";
    }
    ss << "    </tth:GetRadiometrySpotsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult ThermalService::handleSetSpots(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";
    std::vector<RadiometrySpot> spots;
    for (const auto& spotNode : ctx.reqNode.select_nodes(".//*[local-name()='Spot']")) {
        RadiometrySpot s;
        s.token = spotNode.node().attribute("token").as_string();
        const auto pos = spotNode.node().select_node(".//*[local-name()='Position']").node();
        if (pos)
            s.position = Xml::parsePoint2D(pos, 0.5F, 0.5F);
        const auto lbl = spotNode.node().select_node(".//*[local-name()='Label']").node();
        if (lbl)
            s.label = lbl.text().as_string();
        const auto temp = spotNode.node().select_node(".//*[local-name()='Temperature']").node();
        if (temp)
            s.temperature = temp.text().as_float(0.0F);
        if (!s.token.empty())
            spots.push_back(std::move(s));
    }
    const auto handler = m_ctx.getThermalHandler ? m_ctx.getThermalHandler() : nullptr;
    if (handler) {
        static_cast<void>(handler->handleSetRadiometrySpots(tok, spots));
    }
    m_spots = spots;
    return SoapHandlerResult::ok("    <tth:SetRadiometrySpotsResponse/>\r\n");
}

SoapHandlerResult ThermalService::handleGetBoxes(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";
    std::vector<RadiometryBox> boxes = m_boxes;
    const auto handler = m_ctx.getThermalHandler ? m_ctx.getThermalHandler() : nullptr;
    if (handler) {
        boxes = handler->handleGetRadiometryBoxes(tok);
    }
    std::ostringstream ss;
    ss << "    <tth:GetRadiometryBoxesResponse>\r\n";
    for (const auto& b : boxes) {
        ss << "      <tth:Box token=\"" << Xml::escapeXmlAttr(b.token) << "\">\r\n"
           << "        <tth:TopLeft x=\"" << std::fixed << std::setprecision(4) << b.topLeft.x << "\" y=\""
           << b.topLeft.y << "\"/>\r\n"
           << "        <tth:BottomRight x=\"" << b.bottomRight.x << "\" y=\"" << b.bottomRight.y << "\"/>\r\n"
           << "        <tth:Label>" << Xml::escapeXmlText(b.label) << "</tth:Label>\r\n"
           << "        <tth:MinTemperature>" << std::fixed << std::setprecision(1) << b.minTemperature
           << "</tth:MinTemperature>\r\n"
           << "        <tth:MaxTemperature>" << b.maxTemperature << "</tth:MaxTemperature>\r\n"
           << "        <tth:AvgTemperature>" << b.avgTemperature << "</tth:AvgTemperature>\r\n"
           << "      </tth:Box>\r\n";
    }
    ss << "    </tth:GetRadiometryBoxesResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult ThermalService::handleSetBoxes(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";
    std::vector<RadiometryBox> boxes;
    for (const auto& boxNode : ctx.reqNode.select_nodes(".//*[local-name()='Box']")) {
        RadiometryBox b;
        b.token = boxNode.node().attribute("token").as_string();
        const auto tl = boxNode.node().select_node(".//*[local-name()='TopLeft']").node();
        if (tl)
            b.topLeft = Xml::parsePoint2D(tl, 0.0F, 0.0F);
        const auto br = boxNode.node().select_node(".//*[local-name()='BottomRight']").node();
        if (br)
            b.bottomRight = Xml::parsePoint2D(br, 1.0F, 1.0F);
        const auto lbl = boxNode.node().select_node(".//*[local-name()='Label']").node();
        if (lbl)
            b.label = lbl.text().as_string();
        const auto minT = boxNode.node().select_node(".//*[local-name()='MinTemperature']").node();
        if (minT)
            b.minTemperature = minT.text().as_float(0.0F);
        const auto maxT = boxNode.node().select_node(".//*[local-name()='MaxTemperature']").node();
        if (maxT)
            b.maxTemperature = maxT.text().as_float(0.0F);
        const auto avgT = boxNode.node().select_node(".//*[local-name()='AvgTemperature']").node();
        if (avgT)
            b.avgTemperature = avgT.text().as_float(0.0F);
        if (!b.token.empty())
            boxes.push_back(std::move(b));
    }
    const auto handler = m_ctx.getThermalHandler ? m_ctx.getThermalHandler() : nullptr;
    if (handler) {
        static_cast<void>(handler->handleSetRadiometryBoxes(tok, boxes));
    }
    m_boxes = boxes;
    return SoapHandlerResult::ok("    <tth:SetRadiometryBoxesResponse/>\r\n");
}

SoapHandlerResult ThermalService::handleGetPalettes(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";
    std::vector<ColorPalette> palettes = m_palettes;
    const auto handler = m_ctx.getThermalHandler ? m_ctx.getThermalHandler() : nullptr;
    if (handler) {
        palettes = handler->handleGetColorPalettes(tok);
    }
    std::ostringstream ss;
    ss << "    <tth:GetColorPalettesResponse>\r\n";
    for (const auto& p : palettes) {
        ss << "      <tth:Palette token=\"" << Xml::escapeXmlAttr(p.token) << "\" IsDefault=\""
           << (p.isDefault ? "true" : "false") << "\">\r\n"
           << "        <tth:Name>" << Xml::escapeXmlText(p.name) << "</tth:Name>\r\n"
           << "      </tth:Palette>\r\n";
    }
    ss << "    </tth:GetColorPalettesResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult ThermalService::handleSetPalette(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";
    const auto palNode = ctx.reqNode.select_node(".//*[local-name()='PaletteToken']").node();
    const std::string pal = palNode ? palNode.text().as_string() : "WhiteHot";
    const auto handler = m_ctx.getThermalHandler ? m_ctx.getThermalHandler() : nullptr;
    if (handler) {
        static_cast<void>(handler->handleSetColorPalette(tok, pal));
    }
    m_activePalette = pal;
    return SoapHandlerResult::ok("    <tth:SetColorPaletteResponse/>\r\n");
}

SoapHandlerResult ThermalService::handleTriggerNuc(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";
    const auto handler = m_ctx.getThermalHandler ? m_ctx.getThermalHandler() : nullptr;
    if (handler) {
        static_cast<void>(handler->handleTriggerNuc(tok));
    }
    return SoapHandlerResult::ok("    <tth:TriggerNUCResponse/>\r\n");
}

} // namespace Onvif
