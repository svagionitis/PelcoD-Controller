/// @file ImagingService.cpp
#include "ImagingService.h"

#include <sstream>
#include <string>
#include <string_view>

#include <pugixml.hpp>

#include "SoapFault.h"
#include "SoapRouter.h"
#include "XmlUtils.h"

namespace Onvif {

namespace {
    constexpr std::string_view kImagingNs { "http://www.onvif.org/ver20/imaging/wsdl" };
}

ImagingService::ImagingService(ImagingServiceContext ctx)
    : m_ctx(std::move(ctx))
{
    m_settings.brightness = 50.0F;
    m_settings.colorSaturation = 50.0F;
    m_settings.contrast = 50.0F;
    m_settings.sharpness = 50.0F;
    m_settings.autoFocusMode = "AUTO";

    m_presets = { { "Preset_Clear", "Clear", "Clear Daylight" }, { "Preset_BW", "B/W", "Night Vision B/W" } };
}

std::string_view ImagingService::serviceName() const noexcept
{
    return "Imaging";
}

void ImagingService::registerRoutes(SoapRouter& router)
{
    const std::string ns { kImagingNs };

    router.registerRoute(ns, "GetImagingSettings", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetSettings(ctx); });
    router.registerRoute(ns, "SetImagingSettings", AccessClass::Actuate,
        [this](const SoapContext& ctx) { return handleSetSettings(ctx); });
    router.registerRoute(
        ns, "GetOptions", AccessClass::ReadMedia, [this](const SoapContext& ctx) { return handleGetOptions(ctx); });
    router.registerRoute(
        ns, "Move", AccessClass::Actuate, [this](const SoapContext& ctx) { return handleMoveFocus(ctx); });
    router.registerRoute(
        ns, "Stop", AccessClass::Actuate, [this](const SoapContext& ctx) { return handleStopFocus(ctx); });
    router.registerRoute(
        ns, "GetStatus", AccessClass::ReadMedia, [this](const SoapContext& ctx) { return handleGetStatus(ctx); });
    router.registerRoute(
        ns, "GetPresets", AccessClass::ReadMedia, [this](const SoapContext& ctx) { return handleGetPresets(ctx); });
    router.registerRoute(ns, "GetCurrentPreset", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetCurPreset(ctx); });
    router.registerRoute(ns, "SetCurrentPreset", AccessClass::Actuate,
        [this](const SoapContext& ctx) { return handleSetCurPreset(ctx); });
    router.registerRoute(ns, "GetServiceCapabilities", AccessClass::PreAuth, [](const SoapContext&) {
        return SoapHandlerResult::ok(
            "    <timg:GetServiceCapabilitiesResponse>\r\n"
            "      <timg:Capabilities ImageStabilization=\"false\" Presets=\"true\" AdaptablePreset=\"false\"/>\r\n"
            "    </timg:GetServiceCapabilitiesResponse>\r\n");
    });
}

SoapHandlerResult ImagingService::handleGetSettings(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";

    ImagingSettings settings = m_settings;
    const auto imaging = m_ctx.getImagingHandler ? m_ctx.getImagingHandler() : nullptr;
    if (imaging) {
        settings = imaging->handleGetImagingSettings(tok);
    }
    std::ostringstream ss;
    ss << "    <timg:GetImagingSettingsResponse>\r\n"
       << "      <timg:ImagingSettings>\r\n"
       << "        <tt:Brightness>" << settings.brightness << "</tt:Brightness>\r\n"
       << "        <tt:ColorSaturation>" << settings.colorSaturation << "</tt:ColorSaturation>\r\n"
       << "        <tt:Contrast>" << settings.contrast << "</tt:Contrast>\r\n"
       << "        <tt:Sharpness>" << settings.sharpness << "</tt:Sharpness>\r\n"
       << "        <tt:Focus>\r\n"
       << "          <tt:AutoFocusMode>" << Xml::escapeXmlText(settings.autoFocusMode) << "</tt:AutoFocusMode>\r\n"
       << "          <tt:DefaultSpeed>1.0</tt:DefaultSpeed>\r\n"
       << "        </tt:Focus>\r\n"
       << "      </timg:ImagingSettings>\r\n"
       << "    </timg:GetImagingSettingsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult ImagingService::handleSetSettings(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";

    const auto bNode = ctx.reqNode.select_node(".//*[local-name()='Brightness']").node();
    if (bNode) {
        m_settings.brightness = bNode.text().as_float(50.0F);
    }
    const auto cNode = ctx.reqNode.select_node(".//*[local-name()='Contrast']").node();
    if (cNode) {
        m_settings.contrast = cNode.text().as_float(50.0F);
    }
    const auto csNode = ctx.reqNode.select_node(".//*[local-name()='ColorSaturation']").node();
    if (csNode) {
        m_settings.colorSaturation = csNode.text().as_float(50.0F);
    }
    const auto sNode = ctx.reqNode.select_node(".//*[local-name()='Sharpness']").node();
    if (sNode) {
        m_settings.sharpness = sNode.text().as_float(50.0F);
    }
    const auto irNode = ctx.reqNode.select_node(".//*[local-name()='IrCutFilter']").node();
    if (irNode) {
        m_settings.irCutFilter = irNode.text().as_string();
    }
    const auto blcModeNode
        = ctx.reqNode.select_node(".//*[local-name()='BacklightCompensation']/*[local-name()='Mode']").node();
    if (blcModeNode) {
        m_settings.backlightCompensation = (std::string_view(blcModeNode.text().as_string()) == "ON");
    }
    const auto blcLevelNode
        = ctx.reqNode.select_node(".//*[local-name()='BacklightCompensation']/*[local-name()='Level']").node();
    if (blcLevelNode) {
        m_settings.backlightLevel = blcLevelNode.text().as_float(0.0F);
    }
    const auto wdrModeNode
        = ctx.reqNode.select_node(".//*[local-name()='WideDynamicRange']/*[local-name()='Mode']").node();
    if (wdrModeNode) {
        m_settings.wideDynamicRange = (std::string_view(wdrModeNode.text().as_string()) == "ON");
    }
    const auto wdrLevelNode
        = ctx.reqNode.select_node(".//*[local-name()='WideDynamicRange']/*[local-name()='Level']").node();
    if (wdrLevelNode) {
        m_settings.wdrLevel = wdrLevelNode.text().as_float(0.0F);
    }
    const auto afNode = ctx.reqNode.select_node(".//*[local-name()='Focus']/*[local-name()='AutoFocusMode']").node();
    if (afNode) {
        m_settings.autoFocusMode = afNode.text().as_string();
    }
    const auto expModeNode = ctx.reqNode.select_node(".//*[local-name()='Exposure']/*[local-name()='Mode']").node();
    if (expModeNode) {
        m_settings.exposure.mode = expModeNode.text().as_string();
    }
    const auto irisNode = ctx.reqNode.select_node(".//*[local-name()='Exposure']/*[local-name()='Iris']").node();
    if (irisNode) {
        m_settings.exposure.iris = irisNode.text().as_float(50.0F);
    }

    const auto imaging = m_ctx.getImagingHandler ? m_ctx.getImagingHandler() : nullptr;
    if (imaging) {
        static_cast<void>(imaging->handleSetImagingSettings(tok, m_settings));
    }
    return SoapHandlerResult::ok("    <timg:SetImagingSettingsResponse/>\r\n");
}

SoapHandlerResult ImagingService::handleGetOptions(const SoapContext&)
{
    std::ostringstream ss;
    ss << "    <timg:GetOptionsResponse>\r\n"
       << "      <timg:ImagingOptions>\r\n"
       << "        <tt:Brightness><tt:Min>0.0</tt:Min><tt:Max>100.0</tt:Max></tt:Brightness>\r\n"
       << "        <tt:ColorSaturation><tt:Min>0.0</tt:Min><tt:Max>100.0</tt:Max></tt:ColorSaturation>\r\n"
       << "        <tt:Contrast><tt:Min>0.0</tt:Min><tt:Max>100.0</tt:Max></tt:Contrast>\r\n"
       << "        <tt:Sharpness><tt:Min>0.0</tt:Min><tt:Max>100.0</tt:Max></tt:Sharpness>\r\n"
       << "        <tt:Focus>\r\n"
       << "          <tt:AutoFocusModes>AUTO</tt:AutoFocusModes>\r\n"
       << "          <tt:AutoFocusModes>MANUAL</tt:AutoFocusModes>\r\n"
       << "          <tt:DefaultSpeed><tt:Min>0.0</tt:Min><tt:Max>1.0</tt:Max></tt:DefaultSpeed>\r\n"
       << "        </tt:Focus>\r\n"
       << "      </timg:ImagingOptions>\r\n"
       << "    </timg:GetOptionsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult ImagingService::handleMoveFocus(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";

    FocusMove move;
    const auto contNode = ctx.reqNode.select_node(".//*[local-name()='Continuous']/*[local-name()='Speed']").node();
    if (contNode) {
        move.mode = FocusMoveMode::Continuous;
        move.continuousSpeed = contNode.text().as_float(0.0F);
    }
    const auto absNode = ctx.reqNode.select_node(".//*[local-name()='Absolute']/*[local-name()='Position']").node();
    if (absNode) {
        move.mode = FocusMoveMode::Absolute;
        move.absolutePosition = absNode.text().as_float(0.0F);
    }
    const auto relNode = ctx.reqNode.select_node(".//*[local-name()='Relative']/*[local-name()='Distance']").node();
    if (relNode) {
        move.mode = FocusMoveMode::Relative;
        move.relativeDistance = relNode.text().as_float(0.0F);
    }

    const auto imaging = m_ctx.getImagingHandler ? m_ctx.getImagingHandler() : nullptr;
    if (imaging) {
        static_cast<void>(imaging->handleMoveFocusAdvanced(tok, move));
    }
    return SoapHandlerResult::ok("    <timg:MoveResponse/>\r\n");
}

SoapHandlerResult ImagingService::handleStopFocus(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";
    const auto imaging = m_ctx.getImagingHandler ? m_ctx.getImagingHandler() : nullptr;
    if (imaging) {
        imaging->handleStopFocus(tok);
    }
    return SoapHandlerResult::ok("    <timg:StopResponse/>\r\n");
}

SoapHandlerResult ImagingService::handleGetStatus(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";
    FocusStatus20 status = m_focusStatus;
    const auto imaging = m_ctx.getImagingHandler ? m_ctx.getImagingHandler() : nullptr;
    if (imaging) {
        status = imaging->handleGetFocusStatus(tok);
    }
    std::ostringstream ss;
    ss << "    <timg:GetStatusResponse>\r\n"
       << "      <timg:Status>\r\n"
       << "        <tt:FocusStatus20>\r\n"
       << "          <tt:Position>" << status.position << "</tt:Position>\r\n"
       << "          <tt:MoveStatus>" << Xml::escapeXmlText(status.moveStatus) << "</tt:MoveStatus>\r\n"
       << "        </tt:FocusStatus20>\r\n"
       << "      </timg:Status>\r\n"
       << "    </timg:GetStatusResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult ImagingService::handleGetPresets(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "VideoSource_1";
    auto presets = m_presets;
    const auto imaging = m_ctx.getImagingHandler ? m_ctx.getImagingHandler() : nullptr;
    if (imaging) {
        presets = imaging->handleGetImagingPresets(tok);
    }
    std::ostringstream ss;
    ss << "    <timg:GetPresetsResponse>\r\n";
    for (const auto& p : presets) {
        ss << "      <timg:Preset token=\"" << Xml::escapeXmlAttr(p.token) << "\">\r\n"
           << "        <tt:Name>" << Xml::escapeXmlText(p.name) << "</tt:Name>\r\n"
           << "      </timg:Preset>\r\n";
    }
    ss << "    </timg:GetPresetsResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult ImagingService::handleGetCurPreset(const SoapContext&)
{
    std::ostringstream ss;
    ss << "    <timg:GetCurrentPresetResponse>\r\n"
       << "      <timg:CurrentPreset>\r\n"
       << "        <timg:Token>" << Xml::escapeXmlText(m_currentPresetToken) << "</timg:Token>\r\n"
       << "      </timg:CurrentPreset>\r\n"
       << "    </timg:GetCurrentPresetResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult ImagingService::handleSetCurPreset(const SoapContext& ctx)
{
    const auto vsNode = ctx.reqNode.select_node(".//*[local-name()='VideoSourceToken']").node();
    const std::string vsTok = vsNode ? vsNode.text().as_string() : "VideoSource_1";
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='PresetToken']").node();
    const std::string tok = tokNode ? tokNode.text().as_string() : "";
    m_currentPresetToken = tok;
    const auto imaging = m_ctx.getImagingHandler ? m_ctx.getImagingHandler() : nullptr;
    if (imaging) {
        static_cast<void>(imaging->handleSetCurrentImagingPreset(vsTok, tok));
    }
    return SoapHandlerResult::ok("    <timg:SetCurrentPresetResponse/>\r\n");
}

} // namespace Onvif
