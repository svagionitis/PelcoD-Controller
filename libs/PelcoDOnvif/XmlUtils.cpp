/// @file XmlUtils.cpp
/// @brief Implementation of XML traversal and namespace-agnostic element lookup utilities.

#include "XmlUtils.h"

namespace PelcoD::Onvif::Xml {

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

Point2D parsePoint2D(const pugi::xml_node& node, float defaultX, float defaultY)
{
    Point2D pt {};
    const auto attrX = node.attribute("x");
    pt.x = !attrX.empty() ? attrX.as_float(defaultX) : node.attribute("X").as_float(defaultX);

    const auto attrY = node.attribute("y");
    pt.y = !attrY.empty() ? attrY.as_float(defaultY) : node.attribute("Y").as_float(defaultY);

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

} // namespace PelcoD::Onvif::Xml
