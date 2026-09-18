#pragma once

/// @file XmlUtils.h
/// @brief XML traversal and namespace-agnostic element lookup utilities for ONVIF.

#include <pugixml.hpp>
#include <string>
#include <vector>

namespace PelcoD::Onvif::Xml {

/// @brief Extracts the local name of an XML node, stripping any namespace prefix.
/// @param[in] node XML node to inspect.
/// @return Local element name without prefix (e.g. "Body" from "s:Body").
[[nodiscard]] std::string getLocalNodeName(const pugi::xml_node& node);

/// @brief Searches immediate children of parent for a node whose local name matches suffix.
/// @param[in] parent Parent XML node to search.
/// @param[in] suffix Local element name to match.
/// @return Matching XML node or empty node if not found.
[[nodiscard]] pugi::xml_node findNodeWithSuffix(const pugi::xml_node& parent, const std::string& suffix);

/// @brief Recursively collects all descendant nodes matching suffix.
/// @param[in] parent Root XML node to search.
/// @param[in] suffix Local element name to match.
/// @param[out] result Vector where matched nodes are appended.
void collectNodesWithSuffix(
    const pugi::xml_node& parent, const std::string& suffix, std::vector<pugi::xml_node>& result);

/// @brief Recursively searches for the first descendant node matching suffix.
/// @param[in] parent Root XML node to search.
/// @param[in] suffix Local element name to match.
/// @return First matching descendant XML node or empty node if not found.
[[nodiscard]] pugi::xml_node findRecursiveNodeWithSuffix(const pugi::xml_node& parent, const std::string& suffix);

} // namespace PelcoD::Onvif::Xml
