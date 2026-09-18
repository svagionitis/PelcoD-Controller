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

} // namespace PelcoD::Onvif::Xml
