#pragma once

/// @file XmlUtils.h
/// @brief XML traversal and namespace-agnostic element lookup utilities for ONVIF.

#include "OnvifTypes.h"
#include <ostream>
#include <pugixml.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace Onvif::Xml {

/// @brief Sanitizes a string by filtering out illegal XML 1.0 control characters.
/// @details XML 1.0 §2.2 specifies valid characters:
///          #x9 | #xA | #xD | [#x20-#xD7FF] | [#xE000-#xFFFD] | [#x10000-#x10FFFF].
///          Control characters in [0x00, 0x08], 0x0B, 0x0C, and [0x0E, 0x1F] are stripped.
/// @param[in] input Raw input string to sanitize.
/// @return Sanitized string adhering strictly to XML 1.0 character sets.
[[nodiscard]] std::string sanitizeXml(std::string_view input);

/// @brief Escapes predefined XML entities in a string for safe embedding in text or attributes.
/// @details Replaces '&' -> "&amp;", '<' -> "&lt;", '>' -> "&gt;", '"' -> "&quot;", '\'' -> "&apos;".
///          Also filters illegal XML 1.0 control characters.
/// @param[in] input String to escape.
/// @return XML-safe escaped string.
[[nodiscard]] std::string escapeXml(std::string_view input);

/// @brief Escapes predefined XML entities specifically for element text nodes.
/// @details Replaces '&' -> "&amp;", '<' -> "&lt;", '>' -> "&gt;". Quotes are preserved.
///          Also filters illegal XML 1.0 control characters.
/// @param[in] input String to escape.
/// @return XML-safe escaped element text string.
[[nodiscard]] std::string escapeXmlText(std::string_view input);

/// @brief Escapes predefined XML entities specifically for XML attributes.
/// @details Replaces '&' -> "&amp;", '<' -> "&lt;", '>' -> "&gt;", '"' -> "&quot;", '\'' -> "&apos;".
///          Also filters illegal XML 1.0 control characters.
/// @param[in] input String to escape.
/// @return XML-safe escaped attribute string.
[[nodiscard]] std::string escapeXmlAttr(std::string_view input);

/// @brief Writes a safe XML element with escaped content to an output stream.
/// @param[in,out] os Destination output stream.
/// @param[in] qualifiedTag XML tag name (e.g. "tt:Name").
/// @param[in] content Raw text content to escape and enclose.
void writeXmlTag(std::ostream& os, std::string_view qualifiedTag, std::string_view content);

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

/// @brief Parses a 2D coordinate point from an XML node, supporting case-insensitive 'x'/'X' and 'y'/'Y' attributes.
/// @details Checks for attribute 'x' then 'X', falling back to defaultX if neither is present.
///          Checks for attribute 'y' then 'Y', falling back to defaultY if neither is present.
/// @param[in] node XML node containing coordinate attributes.
/// @param[in] defaultX Default X coordinate value if attribute is absent.
/// @param[in] defaultY Default Y coordinate value if attribute is absent.
/// @return Point2D populated with parsed or default values.
/// @note Thread-safe; operates strictly on the provided immutable XML node.
[[nodiscard]] Point2D parsePoint2D(const pugi::xml_node& node, float defaultX = 0.0f, float defaultY = 0.0f);

/// @brief Parses a sequence of Point2D elements from immediate or descendant Point elements.
/// @details Searches immediate children matching local name "Point". If none are found,
///          recursively collects all descendant "Point" elements to support nested structures.
/// @param[in] parent Parent XML node containing point elements.
/// @return Vector of parsed Point2D coordinates.
/// @note Thread-safe; operates strictly on the provided immutable XML node.
[[nodiscard]] std::vector<Point2D> parsePoint2DList(const pugi::xml_node& parent);

} // namespace Onvif::Xml
