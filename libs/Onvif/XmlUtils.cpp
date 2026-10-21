#include "XmlUtils.h"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <pugixml.hpp>

namespace Onvif::Xml {

std::string sanitizeXml(std::string_view input)
{
    std::string out {};
    out.reserve(input.size());
    for (const char ch : input) {
        const auto uc = static_cast<unsigned char>(ch);
        if (uc >= 0x20 || uc == 0x09 || uc == 0x0A || uc == 0x0D) {
            out.push_back(ch);
        }
    }
    return out;
}

std::string escapeXml(std::string_view input)
{
    std::string out {};
    out.reserve(input.size() + (input.size() / 4));
    for (const char ch : input) {
        const auto uc = static_cast<unsigned char>(ch);
        if (uc < 0x20 && uc != 0x09 && uc != 0x0A && uc != 0x0D) {
            continue;
        }
        switch (ch) {
        case '&':
            out.append("&amp;");
            break;
        case '<':
            out.append("&lt;");
            break;
        case '>':
            out.append("&gt;");
            break;
        case '"':
            out.append("&quot;");
            break;
        case '\'':
            out.append("&apos;");
            break;
        default:
            out.push_back(ch);
            break;
        }
    }
    return out;
}

std::string escapeXmlText(std::string_view input)
{
    std::string out {};
    out.reserve(input.size() + (input.size() / 4));
    for (const char ch : input) {
        const auto uc = static_cast<unsigned char>(ch);
        if (uc < 0x20 && uc != 0x09 && uc != 0x0A && uc != 0x0D) {
            continue;
        }
        switch (ch) {
        case '&':
            out.append("&amp;");
            break;
        case '<':
            out.append("&lt;");
            break;
        case '>':
            out.append("&gt;");
            break;
        default:
            out.push_back(ch);
            break;
        }
    }
    return out;
}

std::string escapeXmlAttr(std::string_view input)
{
    return escapeXml(input);
}

void writeXmlTag(std::ostream& os, std::string_view qualifiedTag, std::string_view content)
{
    os << '<' << qualifiedTag << '>' << escapeXml(content) << "</" << qualifiedTag << '>';
}

std::string getLocalNodeName(const pugi::xml_node& node)
{
    const std::string name = node.name();
    const auto colonPos = name.find(':');
    return (colonPos != std::string::npos) ? name.substr(colonPos + 1) : name;
}

pugi::xml_node findNodeWithSuffix(const pugi::xml_node& parent, const std::string& suffix)
{
    for (const auto& child : parent.children()) {
        if (getLocalNodeName(child) == suffix) {
            return child;
        }
    }
    return {};
}

void collectNodesWithSuffix(
    const pugi::xml_node& parent, const std::string& suffix, std::vector<pugi::xml_node>& result)
{
    for (const auto& child : parent.children()) {
        if (getLocalNodeName(child) == suffix) {
            result.push_back(child);
        }
        collectNodesWithSuffix(child, suffix, result);
    }
}

pugi::xml_node findRecursiveNodeWithSuffix(const pugi::xml_node& parent, const std::string& suffix)
{
    std::vector<pugi::xml_node> matches {};
    collectNodesWithSuffix(parent, suffix, matches);
    if (!matches.empty()) {
        return matches.front();
    }
    return {};
}

float parseSafeFloat(const pugi::xml_attribute& attr, float fallback) noexcept
{
    if (attr.empty()) {
        return fallback;
    }
    const char* str = attr.value();
    char* end = nullptr;
    const float val = std::strtof(str, &end);
    if (end == str || !std::isfinite(val)) {
        return fallback;
    }
    return val;
}

std::optional<float> parseOptionalFloat(const pugi::xml_attribute& attr) noexcept
{
    if (attr.empty()) {
        return std::nullopt;
    }
    const char* str = attr.value();
    char* end = nullptr;
    const float val = std::strtof(str, &end);
    if (end == str || !std::isfinite(val)) {
        return std::nullopt;
    }
    return val;
}

Point2D parsePoint2D(const pugi::xml_node& node, float defaultX, float defaultY)
{
    Point2D pt {};
    const auto attrX = node.attribute("x");
    const auto attrXUpper = node.attribute("X");
    const auto& actualX = !attrX.empty() ? attrX : attrXUpper;
    pt.x = parseSafeFloat(actualX, defaultX);

    const auto attrY = node.attribute("y");
    const auto attrYUpper = node.attribute("Y");
    const auto& actualY = !attrY.empty() ? attrY : attrYUpper;
    pt.y = parseSafeFloat(actualY, defaultY);

    return pt;
}

std::vector<Point2D> parsePoint2DList(const pugi::xml_node& parent)
{
    std::vector<Point2D> points {};
    std::vector<pugi::xml_node> pointNodes {};

    for (const auto& child : parent.children()) {
        if (getLocalNodeName(child) == "Point") {
            pointNodes.push_back(child);
        }
    }

    if (pointNodes.empty()) {
        collectNodesWithSuffix(parent, "Point", pointNodes);
    }

    points.reserve(pointNodes.size());
    for (const auto& ptNode : pointNodes) {
        points.push_back(parsePoint2D(ptNode));
    }

    return points;
}

std::optional<std::chrono::milliseconds> parseIsoDurationMs(std::string_view isoDuration) noexcept
{
    if (isoDuration.empty() || isoDuration.front() != 'P') {
        return std::nullopt;
    }

    double totalSeconds { 0.0 };
    double currentVal { 0.0 };
    double fracMul { 0.0 };
    bool inTime { false };
    bool inFrac { false };

    for (std::size_t i { 1U }; i < isoDuration.size(); ++i) {
        const char c { isoDuration[i] };
        if (c == 'T') {
            inTime = true;
        } else if (c == '.' || c == ',') {
            inFrac = true;
            fracMul = 0.1;
        } else if (c >= '0' && c <= '9') {
            if (inFrac) {
                currentVal += static_cast<double>(c - '0') * fracMul;
                fracMul *= 0.1;
            } else {
                currentVal = (currentVal * 10.0) + static_cast<double>(c - '0');
            }
        } else if (c == 'D') {
            totalSeconds += currentVal * 86400.0;
            currentVal = 0.0;
            inFrac = false;
        } else if (c == 'H' && inTime) {
            totalSeconds += currentVal * 3600.0;
            currentVal = 0.0;
            inFrac = false;
        } else if (c == 'M' && inTime) {
            totalSeconds += currentVal * 60.0;
            currentVal = 0.0;
            inFrac = false;
        } else if (c == 'S' && inTime) {
            totalSeconds += currentVal;
            currentVal = 0.0;
            inFrac = false;
        }
    }

    if (totalSeconds > 0.0 && std::isfinite(totalSeconds)) {
        const auto ms = static_cast<std::int64_t>(totalSeconds * 1000.0);
        return std::chrono::milliseconds(ms);
    }
    return std::nullopt;
}

} // namespace Onvif::Xml
