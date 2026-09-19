/// @file MacroScript.cpp
/// @brief Implementation of MacroScript serialization, deserialization, and validation.

#include "MacroScript.h"

#include "PelcoDFrame.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace PelcoD {

namespace {

std::string trim(const std::string& s)
{
    const auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string escapeJsonString(const std::string& input)
{
    std::string out;
    out.reserve(input.size() + 16);
    for (const char c : input) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

std::string extractJsonStringField(const std::string& json, const std::string& fieldName, const std::string& defaultValue = "")
{
    const std::string pattern = "\"" + fieldName + "\"";
    const auto pos = json.find(pattern);
    if (pos == std::string::npos) {
        return defaultValue;
    }
    const auto colonPos = json.find(':', pos + pattern.size());
    if (colonPos == std::string::npos) {
        return defaultValue;
    }
    const auto quoteStart = json.find('"', colonPos + 1);
    if (quoteStart == std::string::npos) {
        return defaultValue;
    }
    std::string val;
    bool escaped = false;
    for (std::size_t i = quoteStart + 1; i < json.size(); ++i) {
        const char c = json[i];
        if (escaped) {
            switch (c) {
            case '"':
                val += '"';
                break;
            case '\\':
                val += '\\';
                break;
            case 'n':
                val += '\n';
                break;
            case 'r':
                val += '\r';
                break;
            case 't':
                val += '\t';
                break;
            default:
                val += c;
                break;
            }
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            return val;
        } else {
            val += c;
        }
    }
    return defaultValue;
}

std::uint32_t extractJsonUIntField(const std::string& json, const std::string& fieldName, std::uint32_t defaultValue = 0)
{
    const std::string pattern = "\"" + fieldName + "\"";
    const auto pos = json.find(pattern);
    if (pos == std::string::npos) {
        return defaultValue;
    }
    const auto colonPos = json.find(':', pos + pattern.size());
    if (colonPos == std::string::npos) {
        return defaultValue;
    }
    const auto numStart = json.find_first_of("0123456789", colonPos + 1);
    if (numStart == std::string::npos) {
        return defaultValue;
    }
    const auto numEnd = json.find_first_not_of("0123456789", numStart);
    const std::string numStr = (numEnd == std::string::npos) ? json.substr(numStart) : json.substr(numStart, numEnd - numStart);
    try {
        return static_cast<std::uint32_t>(std::stoul(numStr));
    } catch (...) {
        return defaultValue;
    }
}

bool extractJsonBoolField(const std::string& json, const std::string& fieldName, bool defaultValue = false)
{
    const std::string pattern = "\"" + fieldName + "\"";
    const auto pos = json.find(pattern);
    if (pos == std::string::npos) {
        return defaultValue;
    }
    const auto colonPos = json.find(':', pos + pattern.size());
    if (colonPos == std::string::npos) {
        return defaultValue;
    }
    const auto valStart = json.find_first_not_of(" \t\r\n", colonPos + 1);
    if (valStart == std::string::npos) {
        return defaultValue;
    }
    if (json.compare(valStart, 4, "true") == 0) {
        return true;
    }
    if (json.compare(valStart, 5, "false") == 0) {
        return false;
    }
    return defaultValue;
}

} // namespace

MacroSequence MacroSerializer::fromJson(const std::string& json)
{
    MacroSequence seq;
    seq.name = extractJsonStringField(json, "name", "Untitled Macro");
    seq.description = extractJsonStringField(json, "description", "");
    seq.repeatCount = extractJsonUIntField(json, "repeatCount", 1U);

    // Locate "steps": [ ... ]
    const auto stepsPos = json.find("\"steps\"");
    if (stepsPos == std::string::npos) {
        return seq;
    }
    const auto arrayStart = json.find('[', stepsPos);
    if (arrayStart == std::string::npos) {
        return seq;
    }
    const auto arrayEnd = json.find(']', arrayStart);
    if (arrayEnd == std::string::npos) {
        return seq;
    }

    std::size_t cur = arrayStart + 1;
    while (cur < arrayEnd) {
        const auto objStart = json.find('{', cur);
        if (objStart == std::string::npos || objStart >= arrayEnd) {
            break;
        }
        const auto objEnd = json.find('}', objStart);
        if (objEnd == std::string::npos || objEnd > arrayEnd) {
            break;
        }

        const std::string stepJson = json.substr(objStart, objEnd - objStart + 1);
        MacroStep step;
        step.label = extractJsonStringField(stepJson, "label", "");
        const std::string hexStr = extractJsonStringField(stepJson, "hex", "");
        step.frame = PelcoDFrame::fromHexString(hexStr);
        step.delayMs = extractJsonUIntField(stepJson, "delayMs", 100U);
        step.expectResponse = extractJsonBoolField(stepJson, "expectResponse", false);

        if (!step.frame.empty()) {
            seq.steps.push_back(std::move(step));
        }

        cur = objEnd + 1;
    }

    return seq;
}

std::string MacroSerializer::toJson(const MacroSequence& sequence)
{
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"name\": \"" << escapeJsonString(sequence.name) << "\",\n";
    ss << "  \"description\": \"" << escapeJsonString(sequence.description) << "\",\n";
    ss << "  \"repeatCount\": " << sequence.repeatCount << ",\n";
    ss << "  \"steps\": [\n";

    for (std::size_t i = 0; i < sequence.steps.size(); ++i) {
        const auto& step = sequence.steps[i];
        ss << "    {\n";
        ss << "      \"label\": \"" << escapeJsonString(step.label) << "\",\n";
        ss << "      \"hex\": \"" << PelcoDFrame::toHexString(step.frame) << "\",\n";
        ss << "      \"delayMs\": " << step.delayMs << ",\n";
        ss << "      \"expectResponse\": " << (step.expectResponse ? "true" : "false") << "\n";
        ss << "    }" << (i + 1 < sequence.steps.size() ? "," : "") << "\n";
    }

    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

MacroSequence MacroSerializer::fromScript(const std::string& script)
{
    MacroSequence seq;
    std::istringstream ss(script);
    std::string line;

    while (std::getline(ss, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty()) {
            continue;
        }

        // Meta headers in comments: # Name: My Macro
        if (trimmed.rfind("# Name:", 0) == 0) {
            seq.name = trim(trimmed.substr(7));
            continue;
        }
        if (trimmed.rfind("# Description:", 0) == 0) {
            seq.description = trim(trimmed.substr(14));
            continue;
        }
        if (trimmed.rfind("# Repeat:", 0) == 0) {
            try {
                seq.repeatCount = static_cast<std::uint32_t>(std::stoul(trim(trimmed.substr(9))));
            } catch (...) {
            }
            continue;
        }

        // Full line comments
        if (trimmed.front() == '#') {
            continue;
        }

        // Parse line: HEX_BYTES [delayMs] [# comment]
        std::string comment;
        const auto hashPos = trimmed.find('#');
        if (hashPos != std::string::npos) {
            comment = trim(trimmed.substr(hashPos + 1));
            trimmed = trim(trimmed.substr(0, hashPos));
        }

        std::istringstream lineStream(trimmed);
        std::vector<std::string> tokens;
        std::string tok;
        while (lineStream >> tok) {
            tokens.push_back(tok);
        }

        if (tokens.empty()) {
            continue;
        }

        // Check if the last token is a delay in milliseconds (pure numeric)
        std::uint32_t delay = 100U;
        bool hasExplicitDelay = false;
        if (tokens.size() > 1) {
            const std::string& lastTok = tokens.back();
            if (std::all_of(lastTok.begin(), lastTok.end(), [](unsigned char c) { return std::isdigit(c); })) {
                try {
                    delay = static_cast<std::uint32_t>(std::stoul(lastTok));
                    hasExplicitDelay = true;
                } catch (...) {
                }
            }
        }

        std::size_t hexTokenCount = hasExplicitDelay ? (tokens.size() - 1) : tokens.size();
        std::string hexPart;
        for (std::size_t i = 0; i < hexTokenCount; ++i) {
            if (!hexPart.empty()) {
                hexPart += " ";
            }
            hexPart += tokens[i];
        }

        std::vector<std::uint8_t> bytes = PelcoDFrame::fromHexString(hexPart);
        if (!bytes.empty()) {
            MacroStep step;
            step.label = comment;
            step.frame = std::move(bytes);
            step.delayMs = delay;
            step.expectResponse = false;
            seq.steps.push_back(std::move(step));
        }
    }

    return seq;
}

std::string MacroSerializer::toScript(const MacroSequence& sequence)
{
    std::ostringstream ss;
    ss << "# Name: " << sequence.name << "\n";
    if (!sequence.description.empty()) {
        ss << "# Description: " << sequence.description << "\n";
    }
    ss << "# Repeat: " << sequence.repeatCount << "\n";
    ss << "# Syntax: HEX_BYTES DELAY_MS # COMMENT\n\n";

    for (const auto& step : sequence.steps) {
        std::string hex = PelcoDFrame::toHexString(step.frame);
        ss << hex << "  " << step.delayMs;
        if (!step.label.empty()) {
            ss << "  # " << step.label;
        }
        ss << "\n";
    }

    return ss.str();
}

bool MacroSerializer::validate(const MacroSequence& sequence, std::string* errorMsg) noexcept
{
    if (sequence.steps.empty()) {
        if (errorMsg) {
            *errorMsg = "Macro sequence contains no steps";
        }
        return false;
    }

    for (std::size_t i = 0; i < sequence.steps.size(); ++i) {
        const auto& step = sequence.steps[i];
        if (step.frame.empty()) {
            if (errorMsg) {
                *errorMsg = "Step " + std::to_string(i + 1) + " has an empty byte payload";
            }
            return false;
        }

        // Standard Pelco-D frame is 7 bytes starting with 0xFF
        if (step.frame.size() == PelcoDFrame::StandardFrameSize) {
            if (!PelcoDFrame::isValidFrame(step.frame)) {
                if (errorMsg) {
                    *errorMsg = "Step " + std::to_string(i + 1) + " contains invalid Pelco-D checksum";
                }
                return false;
            }
        }
    }

    return true;
}

} // namespace PelcoD
