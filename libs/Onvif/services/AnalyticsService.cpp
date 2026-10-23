/// @file AnalyticsService.cpp
#include "AnalyticsService.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <pugixml.hpp>

#include "SoapFault.h"
#include "SoapRouter.h"
#include "XmlUtils.h"

namespace Onvif {

namespace {
    constexpr std::string_view kAnalyticsNs { "http://www.onvif.org/ver20/analytics/wsdl" };

    std::vector<AnalyticsRule> parseAnalyticsRules(const pugi::xml_node& reqNode)
    {
        std::vector<AnalyticsRule> rules;
        for (auto rNode : reqNode.select_nodes(".//*[local-name()='Rule']")) {
            AnalyticsRule rule { };
            rule.name = rNode.node().attribute("Name").as_string();
            rule.type = rNode.node().attribute("Type").as_string();
            for (auto si : rNode.node().select_nodes(".//*[local-name()='SimpleItem']")) {
                const std::string sName = si.node().attribute("Name").as_string();
                const std::string sVal = si.node().attribute("Value").as_string();
                if (sName == "Direction") {
                    rule.direction = sVal;
                } else if (sName == "DwellTime") {
                    try {
                        rule.dwellTimeSeconds = std::stod(sVal);
                    } catch (...) {
                    }
                } else if (sName == "Sensitivity") {
                    try {
                        rule.sensitivity = std::stoi(sVal);
                    } catch (...) {
                    }
                } else if (sName == "MinConfidence") {
                    try {
                        rule.minConfidence = std::stof(sVal);
                    } catch (...) {
                    }
                } else if (sName == "Enabled") {
                    rule.enabled = (sVal == "true" || sVal == "1");
                } else if (sName == "Classes") {
                    std::stringstream ss(sVal);
                    std::string c;
                    while (std::getline(ss, c, ',')) {
                        if (!c.empty()) {
                            rule.objectClasses.push_back(c);
                        }
                    }
                }
            }
            const std::vector<Point2D> pts = Xml::parsePoint2DList(rNode.node());
            if (!pts.empty()) {
                if (rule.type.find("Line") != std::string::npos && pts.size() >= 2) {
                    rule.lineStart = pts[0];
                    rule.lineEnd = pts[1];
                } else {
                    rule.polygon = pts;
                }
            }
            rules.push_back(std::move(rule));
        }
        return rules;
    }

    void serializeRuleToXml(std::ostringstream& ss, const AnalyticsRule& rule)
    {
        ss << "      <tan:Rule Name=\"" << Xml::escapeXmlAttr(rule.name) << "\" Type=\""
           << Xml::escapeXmlAttr(rule.type) << "\">\r\n"
           << "        <tan:Parameters>\r\n";
        if (rule.type.find("Line") != std::string::npos) {
            ss << "          <tt:SimpleItem Name=\"Direction\" Value=\"" << Xml::escapeXmlAttr(rule.direction)
               << "\"/>\r\n"
               << "          <tt:ElementItem Name=\"Segment\">\r\n"
               << "            <tt:Point x=\"" << std::fixed << std::setprecision(4) << rule.lineStart.x << "\" y=\""
               << std::fixed << std::setprecision(4) << rule.lineStart.y << "\"/>\r\n"
               << "            <tt:Point x=\"" << std::fixed << std::setprecision(4) << rule.lineEnd.x << "\" y=\""
               << std::fixed << std::setprecision(4) << rule.lineEnd.y << "\"/>\r\n"
               << "          </tt:ElementItem>\r\n";
        } else if (rule.type.find("Field") != std::string::npos || rule.type.find("Loitering") != std::string::npos) {
            if (rule.type.find("Loitering") != std::string::npos) {
                ss << "          <tt:SimpleItem Name=\"DwellTime\" Value=\"" << std::fixed << std::setprecision(2)
                   << rule.dwellTimeSeconds << "\"/>\r\n";
            }
            if (!rule.polygon.empty()) {
                ss << "          <tt:ElementItem Name=\"Field\">\r\n"
                   << "            <tt:Polygon>\r\n";
                for (const auto& pt : rule.polygon) {
                    ss << "              <tt:Point x=\"" << std::fixed << std::setprecision(4) << pt.x << "\" y=\""
                       << std::fixed << std::setprecision(4) << pt.y << "\"/>\r\n";
                }
                ss << "            </tt:Polygon>\r\n"
                   << "          </tt:ElementItem>\r\n";
            }
        } else if (rule.type.find("CellMotion") != std::string::npos) {
            ss << "          <tt:SimpleItem Name=\"Sensitivity\" Value=\"" << rule.sensitivity << "\"/>\r\n";
        }

        if (!rule.objectClasses.empty()) {
            std::string classesJoined;
            for (size_t i = 0; i < rule.objectClasses.size(); ++i) {
                if (i > 0) {
                    classesJoined += ",";
                }
                classesJoined += rule.objectClasses[i];
            }
            ss << "          <tt:SimpleItem Name=\"Classes\" Value=\"" << Xml::escapeXmlAttr(classesJoined)
               << "\"/>\r\n"
               << "          <tt:SimpleItem Name=\"MinConfidence\" Value=\"" << std::fixed << std::setprecision(2)
               << rule.minConfidence << "\"/>\r\n";
        }

        ss << "          <tt:SimpleItem Name=\"Enabled\" Value=\"" << (rule.enabled ? "true" : "false") << "\"/>\r\n"
           << "        </tan:Parameters>\r\n"
           << "      </tan:Rule>\r\n";
    }

    void serializeModuleToXml(std::ostringstream& ss, const AnalyticsModule& mod)
    {
        ss << "      <tan:AnalyticsModule Name=\"" << Xml::escapeXmlAttr(mod.name) << "\" Type=\""
           << Xml::escapeXmlAttr(mod.type) << "\">\r\n"
           << "        <tan:Parameters>\r\n";
        for (const auto& [k, v] : mod.parameters) {
            ss << "          <tt:SimpleItem Name=\"" << Xml::escapeXmlAttr(k) << "\" Value=\"" << Xml::escapeXmlAttr(v)
               << "\"/>\r\n";
        }
        ss << "        </tan:Parameters>\r\n"
           << "      </tan:AnalyticsModule>\r\n";
    }
} // namespace

AnalyticsService::AnalyticsService(AnalyticsServiceContext ctx)
    : m_ctx(std::move(ctx))
{
}

std::string_view AnalyticsService::serviceName() const noexcept
{
    return "Analytics";
}

void AnalyticsService::registerRoutes(SoapRouter& router)
{
    const std::string ns { kAnalyticsNs };

    router.registerRoute(ns, "GetServiceCapabilities", AccessClass::PreAuth, [](const SoapContext&) {
        return SoapHandlerResult::ok("    <tan:GetServiceCapabilitiesResponse>\r\n"
                                     "      <tan:Capabilities RuleSupport=\"true\" AnalyticsModuleSupport=\"true\" "
                                     "CellBasedSceneDescriptionSupported=\"false\"/>\r\n"
                                     "    </tan:GetServiceCapabilitiesResponse>\r\n");
    });
    router.registerRoute(ns, "GetSupportedRules", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetSupportedRules(ctx); });
    router.registerRoute(
        ns, "GetRules", AccessClass::ReadMedia, [this](const SoapContext& ctx) { return handleGetRules(ctx); });
    router.registerRoute(
        ns, "CreateRules", AccessClass::WriteSystem, [this](const SoapContext& ctx) { return handleCreateRules(ctx); });
    router.registerRoute(
        ns, "ModifyRules", AccessClass::WriteSystem, [this](const SoapContext& ctx) { return handleModifyRules(ctx); });
    router.registerRoute(
        ns, "DeleteRules", AccessClass::WriteSystem, [this](const SoapContext& ctx) { return handleDeleteRules(ctx); });
    router.registerRoute(ns, "GetSupportedAnalyticsModules", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetSupportedModules(ctx); });
    router.registerRoute(ns, "GetAnalyticsModules", AccessClass::ReadMedia,
        [this](const SoapContext& ctx) { return handleGetModules(ctx); });
    router.registerRoute(ns, "CreateAnalyticsModules", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleCreateModules(ctx); });
    router.registerRoute(ns, "ModifyAnalyticsModules", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleModifyModules(ctx); });
    router.registerRoute(ns, "DeleteAnalyticsModules", AccessClass::WriteSystem,
        [this](const SoapContext& ctx) { return handleDeleteModules(ctx); });
}

SoapHandlerResult AnalyticsService::handleGetSupportedRules(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='ConfigurationToken']").node();
    const std::string configToken = tokNode ? tokNode.text().as_string() : "";

    std::vector<AnalyticsRuleDescription> descs;
    const auto analyticsHandler = m_ctx.getAnalyticsHandler ? m_ctx.getAnalyticsHandler() : nullptr;
    if (analyticsHandler) {
        descs = analyticsHandler->handleGetSupportedRules(configToken);
    }
    if (descs.empty()) {
        descs = {
            { "tt:LineDetector", { "Segment", "Direction", "Classes", "MinConfidence", "Enabled" } },
            { "tt:FieldDetector", { "Field", "Classes", "MinConfidence", "Enabled" } },
            { "tt:LoiteringDetector", { "Field", "DwellTime", "Classes", "MinConfidence", "Enabled" } },
            { "tt:CellMotionDetector", { "Sensitivity", "Enabled" } },
        };
    }
    std::ostringstream ss;
    ss << "    <tan:GetSupportedRulesResponse>\r\n"
       << "      <tan:SupportedRules>\r\n";
    for (const auto& desc : descs) {
        ss << "        <tan:RuleDescription Name=\"" << desc.ruleType << "\">\r\n"
           << "          <tan:Messages IsProperty=\"true\">\r\n"
           << "            <tt:Source>\r\n"
           << "              <tt:SimpleItemDescription Name=\"VideoAnalyticsConfigurationToken\" "
              "Type=\"tt:ReferenceToken\"/>\r\n"
           << "            </tt:Source>\r\n"
           << "            <tt:Data>\r\n"
           << "              <tt:SimpleItemDescription Name=\"State\" Type=\"xs:boolean\"/>\r\n"
           << "            </tt:Data>\r\n"
           << "          </tan:Messages>\r\n";
        for (const auto& param : desc.supportedParameters) {
            ss << "          <tan:Parameters>\r\n"
               << "            <tt:SimpleItemDescription Name=\"" << param << "\" Type=\"xs:string\"/>\r\n"
               << "          </tan:Parameters>\r\n";
        }
        ss << "        </tan:RuleDescription>\r\n";
    }
    ss << "      </tan:SupportedRules>\r\n"
       << "    </tan:GetSupportedRulesResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult AnalyticsService::handleGetRules(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='ConfigurationToken']").node();
    const std::string configToken = tokNode ? tokNode.text().as_string() : "";
    std::vector<AnalyticsRule> rules;
    const auto analyticsHandler = m_ctx.getAnalyticsHandler ? m_ctx.getAnalyticsHandler() : nullptr;
    if (analyticsHandler) {
        rules = analyticsHandler->handleGetRules(configToken);
    } else {
        rules = m_rules;
    }
    std::ostringstream ss;
    ss << "    <tan:GetRulesResponse>\r\n";
    for (const auto& r : rules) {
        serializeRuleToXml(ss, r);
    }
    ss << "    </tan:GetRulesResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult AnalyticsService::handleCreateRules(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='ConfigurationToken']").node();
    const std::string configToken = tokNode ? tokNode.text().as_string() : "";
    std::vector<AnalyticsRule> newRules = parseAnalyticsRules(ctx.reqNode);
    const auto analyticsHandler = m_ctx.getAnalyticsHandler ? m_ctx.getAnalyticsHandler() : nullptr;
    if (analyticsHandler) {
        analyticsHandler->handleCreateRules(configToken, newRules);
    } else {
        for (auto& r : newRules) {
            m_rules.push_back(std::move(r));
        }
    }
    return SoapHandlerResult::ok("    <tan:CreateRulesResponse/>\r\n");
}

SoapHandlerResult AnalyticsService::handleModifyRules(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='ConfigurationToken']").node();
    const std::string configToken = tokNode ? tokNode.text().as_string() : "";
    std::vector<AnalyticsRule> modRules = parseAnalyticsRules(ctx.reqNode);
    const auto analyticsHandler = m_ctx.getAnalyticsHandler ? m_ctx.getAnalyticsHandler() : nullptr;
    if (analyticsHandler) {
        analyticsHandler->handleModifyRules(configToken, modRules);
    } else {
        for (const auto& mr : modRules) {
            for (auto& ir : m_rules) {
                if (ir.name == mr.name) {
                    ir = mr;
                    break;
                }
            }
        }
    }
    return SoapHandlerResult::ok("    <tan:ModifyRulesResponse/>\r\n");
}

SoapHandlerResult AnalyticsService::handleDeleteRules(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='ConfigurationToken']").node();
    const std::string configToken = tokNode ? tokNode.text().as_string() : "";
    std::vector<std::string> delNames;
    for (auto rn : ctx.reqNode.select_nodes(".//*[local-name()='RuleName']")) {
        delNames.push_back(rn.node().text().as_string());
    }
    const auto analyticsHandler = m_ctx.getAnalyticsHandler ? m_ctx.getAnalyticsHandler() : nullptr;
    if (analyticsHandler) {
        analyticsHandler->handleDeleteRules(configToken, delNames);
    } else {
        m_rules.erase(std::remove_if(m_rules.begin(), m_rules.end(),
                          [&](const AnalyticsRule& r) {
                              return std::find(delNames.begin(), delNames.end(), r.name) != delNames.end();
                          }),
            m_rules.end());
    }
    return SoapHandlerResult::ok("    <tan:DeleteRulesResponse/>\r\n");
}

SoapHandlerResult AnalyticsService::handleGetSupportedModules(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='ConfigurationToken']").node();
    const std::string configToken = tokNode ? tokNode.text().as_string() : "";
    std::vector<AnalyticsModuleDescription> descs;
    const auto analyticsHandler = m_ctx.getAnalyticsHandler ? m_ctx.getAnalyticsHandler() : nullptr;
    if (analyticsHandler) {
        descs = analyticsHandler->handleGetSupportedModules(configToken);
    }
    if (descs.empty()) {
        descs = {
            { "tt:ObjectClassificationModule", { "Classes", "MinConfidence" } },
            { "tt:MotionDetectionModule", { "Sensitivity" } },
        };
    }
    std::ostringstream ss;
    ss << "    <tan:GetSupportedAnalyticsModulesResponse>\r\n"
       << "      <tan:SupportedAnalyticsModules>\r\n";
    for (const auto& desc : descs) {
        ss << "        <tan:AnalyticsModuleDescription Name=\"" << desc.moduleType << "\">\r\n";
        for (const auto& p : desc.supportedParameters) {
            ss << "          <tan:Parameters>\r\n"
               << "            <tt:SimpleItemDescription Name=\"" << p << "\" Type=\"xs:string\"/>\r\n"
               << "          </tan:Parameters>\r\n";
        }
        ss << "        </tan:AnalyticsModuleDescription>\r\n";
    }
    ss << "      </tan:SupportedAnalyticsModules>\r\n"
       << "    </tan:GetSupportedAnalyticsModulesResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult AnalyticsService::handleGetModules(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='ConfigurationToken']").node();
    const std::string configToken = tokNode ? tokNode.text().as_string() : "";
    std::vector<AnalyticsModule> mods;
    const auto analyticsHandler = m_ctx.getAnalyticsHandler ? m_ctx.getAnalyticsHandler() : nullptr;
    if (analyticsHandler) {
        mods = analyticsHandler->handleGetAnalyticsModules(configToken);
    } else {
        mods = m_modules;
    }
    std::ostringstream ss;
    ss << "    <tan:GetAnalyticsModulesResponse>\r\n";
    for (const auto& m : mods) {
        serializeModuleToXml(ss, m);
    }
    ss << "    </tan:GetAnalyticsModulesResponse>\r\n";
    return SoapHandlerResult::ok(ss.str());
}

SoapHandlerResult AnalyticsService::handleCreateModules(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='ConfigurationToken']").node();
    const std::string configToken = tokNode ? tokNode.text().as_string() : "";
    std::vector<AnalyticsModule> newMods;
    for (auto mNode : ctx.reqNode.select_nodes(".//*[local-name()='AnalyticsModule']")) {
        AnalyticsModule mod;
        mod.name = mNode.node().attribute("Name").as_string();
        mod.type = mNode.node().attribute("Type").as_string();
        for (auto si : mNode.node().select_nodes(".//*[local-name()='SimpleItem']")) {
            mod.parameters[si.node().attribute("Name").as_string()] = si.node().attribute("Value").as_string();
        }
        newMods.push_back(std::move(mod));
    }
    const auto analyticsHandler = m_ctx.getAnalyticsHandler ? m_ctx.getAnalyticsHandler() : nullptr;
    if (analyticsHandler) {
        analyticsHandler->handleCreateAnalyticsModules(configToken, newMods);
    } else {
        for (auto& m : newMods) {
            m_modules.push_back(std::move(m));
        }
    }
    return SoapHandlerResult::ok("    <tan:CreateAnalyticsModulesResponse/>\r\n");
}

SoapHandlerResult AnalyticsService::handleModifyModules(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='ConfigurationToken']").node();
    const std::string configToken = tokNode ? tokNode.text().as_string() : "";
    std::vector<AnalyticsModule> modMods;
    for (auto mNode : ctx.reqNode.select_nodes(".//*[local-name()='AnalyticsModule']")) {
        AnalyticsModule mod;
        mod.name = mNode.node().attribute("Name").as_string();
        mod.type = mNode.node().attribute("Type").as_string();
        for (auto si : mNode.node().select_nodes(".//*[local-name()='SimpleItem']")) {
            mod.parameters[si.node().attribute("Name").as_string()] = si.node().attribute("Value").as_string();
        }
        modMods.push_back(std::move(mod));
    }
    const auto analyticsHandler = m_ctx.getAnalyticsHandler ? m_ctx.getAnalyticsHandler() : nullptr;
    if (analyticsHandler) {
        analyticsHandler->handleModifyAnalyticsModules(configToken, modMods);
    } else {
        for (const auto& mm : modMods) {
            for (auto& im : m_modules) {
                if (im.name == mm.name) {
                    im = mm;
                    break;
                }
            }
        }
    }
    return SoapHandlerResult::ok("    <tan:ModifyAnalyticsModulesResponse/>\r\n");
}

SoapHandlerResult AnalyticsService::handleDeleteModules(const SoapContext& ctx)
{
    const auto tokNode = ctx.reqNode.select_node(".//*[local-name()='ConfigurationToken']").node();
    const std::string configToken = tokNode ? tokNode.text().as_string() : "";
    std::vector<std::string> delNames;
    for (auto mn : ctx.reqNode.select_nodes(".//*[local-name()='AnalyticsModuleName']")) {
        delNames.push_back(mn.node().text().as_string());
    }
    const auto analyticsHandler = m_ctx.getAnalyticsHandler ? m_ctx.getAnalyticsHandler() : nullptr;
    if (analyticsHandler) {
        analyticsHandler->handleDeleteAnalyticsModules(configToken, delNames);
    } else {
        m_modules.erase(std::remove_if(m_modules.begin(), m_modules.end(),
                            [&](const AnalyticsModule& m) {
                                return std::find(delNames.begin(), delNames.end(), m.name) != delNames.end();
                            }),
            m_modules.end());
    }
    return SoapHandlerResult::ok("    <tan:DeleteAnalyticsModulesResponse/>\r\n");
}

} // namespace Onvif
