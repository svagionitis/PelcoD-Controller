/// @file MacroScript.cpp
/// @brief Implementation of MacroScript serialization, deserialization, and validation.

#include "MacroScript.h"

#include "PelcoDFrame.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace PelcoD {

namespace {

[[nodiscard]] std::string trim(const std::string& s)
{
    const auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1U);
}

[[nodiscard]] std::string escapeJsonString(const std::string& input)
{
    std::string out {};
    out.reserve(input.size() + 16U);
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

[[nodiscard]] bool isValidHexToken(std::string_view tok) noexcept
{
    if (tok.empty()) {
        return false;
    }
    if (tok.size() > 2U && tok[0] == '0' && (tok[1] == 'x' || tok[1] == 'X')) {
        tok.remove_prefix(2U);
    }
    if (tok.empty() || tok.size() > 2U) {
        return false;
    }
    for (const char c : tok) {
        if (std::isxdigit(static_cast<unsigned char>(c)) == 0) {
            return false;
        }
    }
    return true;
}

void appendUtf8(std::uint32_t cp, std::string& out)
{
    if (cp <= 0x7FU) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FFU) {
        out.push_back(static_cast<char>(0xC0U | ((cp >> 6U) & 0x1FU)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
    } else if (cp <= 0xFFFFU) {
        out.push_back(static_cast<char>(0xE0U | ((cp >> 12U) & 0x0FU)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
    } else if (cp <= 0x10FFFFU) {
        out.push_back(static_cast<char>(0xF0U | ((cp >> 18U) & 0x07U)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 12U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
    }
}

enum class JsonType : std::uint8_t {
    Null = 0U,
    Bool = 1U,
    Number = 2U,
    String = 3U,
    Array = 4U,
    Object = 5U
};

struct JsonValue {
    JsonType type { JsonType::Null };
    bool boolVal { false };
    double numVal { 0.0 };
    std::string strVal {};
    std::vector<JsonValue> arrVal {};
    std::vector<std::pair<std::string, JsonValue>> objVal {};

    [[nodiscard]] const JsonValue* find(std::string_view key) const noexcept
    {
        if (type != JsonType::Object) {
            return nullptr;
        }
        for (const auto& kv : objVal) {
            if (kv.first == key) {
                return &kv.second;
            }
        }
        return nullptr;
    }
};

class JsonParser {
public:
    explicit JsonParser(std::string_view json)
        : m_src { json }
        , m_pos { 0U }
    {
    }

    [[nodiscard]] JsonValue parse()
    {
        skipWhitespace();
        if (m_pos >= m_src.size()) {
            throw std::runtime_error("Empty JSON input");
        }
        JsonValue root { parseValue() };
        skipWhitespace();
        if (m_pos < m_src.size()) {
            throw std::runtime_error("Unexpected trailing character at offset " + std::to_string(m_pos));
        }
        return root;
    }

private:
    std::string_view m_src {};
    std::size_t m_pos { 0U };

    void skipWhitespace() noexcept
    {
        while (m_pos < m_src.size()) {
            const char c { m_src[m_pos] };
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                ++m_pos;
            } else {
                break;
            }
        }
    }

    [[nodiscard]] char peek() const noexcept
    {
        if (m_pos < m_src.size()) {
            return m_src[m_pos];
        }
        return '\0';
    }

    [[nodiscard]] char get()
    {
        if (m_pos >= m_src.size()) {
            throw std::runtime_error("Unexpected end of JSON input");
        }
        return m_src[m_pos++];
    }

    [[nodiscard]] JsonValue parseValue()
    {
        skipWhitespace();
        const char c { peek() };
        if (c == '{') {
            return parseObject();
        }
        if (c == '[') {
            return parseArray();
        }
        if (c == '"') {
            return parseString();
        }
        if (c == 't' || c == 'f') {
            return parseBool();
        }
        if (c == 'n') {
            return parseNull();
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            return parseNumber();
        }
        throw std::runtime_error("Unexpected token '" + std::string(1U, c) + "' at offset " + std::to_string(m_pos));
    }

    [[nodiscard]] JsonValue parseObject()
    {
        (void)get(); // consume '{'
        JsonValue val {};
        val.type = JsonType::Object;

        skipWhitespace();
        if (peek() == '}') {
            (void)get();
            return val;
        }

        while (true) {
            skipWhitespace();
            if (peek() != '"') {
                throw std::runtime_error("Expected string key in object at offset " + std::to_string(m_pos));
            }
            JsonValue keyVal { parseString() };
            skipWhitespace();
            if (peek() != ':') {
                throw std::runtime_error("Expected ':' after object key at offset " + std::to_string(m_pos));
            }
            (void)get(); // consume ':'

            JsonValue memberVal { parseValue() };
            val.objVal.emplace_back(std::move(keyVal.strVal), std::move(memberVal));

            skipWhitespace();
            const char nextC { peek() };
            if (nextC == '}') {
                (void)get();
                break;
            }
            if (nextC == ',') {
                (void)get();
                continue;
            }
            throw std::runtime_error("Expected ',' or '}' in object at offset " + std::to_string(m_pos));
        }

        return val;
    }

    [[nodiscard]] JsonValue parseArray()
    {
        (void)get(); // consume '['
        JsonValue val {};
        val.type = JsonType::Array;

        skipWhitespace();
        if (peek() == ']') {
            (void)get();
            return val;
        }

        while (true) {
            val.arrVal.push_back(parseValue());
            skipWhitespace();
            const char nextC { peek() };
            if (nextC == ']') {
                (void)get();
                break;
            }
            if (nextC == ',') {
                (void)get();
                continue;
            }
            throw std::runtime_error("Expected ',' or ']' in array at offset " + std::to_string(m_pos));
        }

        return val;
    }

    [[nodiscard]] JsonValue parseString()
    {
        (void)get(); // consume opening '"'
        std::string s {};
        while (m_pos < m_src.size()) {
            const char c { m_src[m_pos++] };
            if (c == '"') {
                JsonValue val {};
                val.type = JsonType::String;
                val.strVal = std::move(s);
                return val;
            }
            if (c == '\\') {
                if (m_pos >= m_src.size()) {
                    throw std::runtime_error("Unterminated escape sequence in string");
                }
                const char esc { m_src[m_pos++] };
                switch (esc) {
                case '"':
                    s.push_back('"');
                    break;
                case '\\':
                    s.push_back('\\');
                    break;
                case '/':
                    s.push_back('/');
                    break;
                case 'b':
                    s.push_back('\b');
                    break;
                case 'f':
                    s.push_back('\f');
                    break;
                case 'n':
                    s.push_back('\n');
                    break;
                case 'r':
                    s.push_back('\r');
                    break;
                case 't':
                    s.push_back('\t');
                    break;
                case 'u': {
                    if (m_pos + 4U > m_src.size()) {
                        throw std::runtime_error("Incomplete \\uXXXX escape sequence");
                    }
                    std::uint32_t cp { 0U };
                    for (std::size_t i { 0U }; i < 4U; ++i) {
                        const char hc { m_src[m_pos++] };
                        cp <<= 4U;
                        if (hc >= '0' && hc <= '9') {
                            cp |= static_cast<std::uint32_t>(hc - '0');
                        } else if (hc >= 'a' && hc <= 'f') {
                            cp |= static_cast<std::uint32_t>(hc - 'a' + 10);
                        } else if (hc >= 'A' && hc <= 'F') {
                            cp |= static_cast<std::uint32_t>(hc - 'A' + 10);
                        } else {
                            throw std::runtime_error("Invalid hex digit in \\uXXXX escape");
                        }
                    }
                    appendUtf8(cp, s);
                    break;
                }
                default:
                    throw std::runtime_error("Invalid escape character '\\" + std::string(1U, esc) + "' in string");
                }
            } else {
                s.push_back(c);
            }
        }
        throw std::runtime_error("Unterminated string literal");
    }

    [[nodiscard]] JsonValue parseNumber()
    {
        const std::size_t startPos { m_pos };
        if (peek() == '-') {
            (void)get();
        }
        if (peek() < '0' || peek() > '9') {
            throw std::runtime_error("Expected digit in number at offset " + std::to_string(m_pos));
        }
        while (peek() >= '0' && peek() <= '9') {
            (void)get();
        }
        if (peek() == '.') {
            (void)get();
            if (peek() < '0' || peek() > '9') {
                throw std::runtime_error("Expected digit after decimal point at offset " + std::to_string(m_pos));
            }
            while (peek() >= '0' && peek() <= '9') {
                (void)get();
            }
        }
        if (peek() == 'e' || peek() == 'E') {
            (void)get();
            if (peek() == '+' || peek() == '-') {
                (void)get();
            }
            if (peek() < '0' || peek() > '9') {
                throw std::runtime_error("Expected digit in exponent at offset " + std::to_string(m_pos));
            }
            while (peek() >= '0' && peek() <= '9') {
                (void)get();
            }
        }

        const std::string_view numSub { m_src.substr(startPos, m_pos - startPos) };
        try {
            const double d { std::stod(std::string(numSub)) };
            JsonValue val {};
            val.type = JsonType::Number;
            val.numVal = d;
            return val;
        } catch (...) {
            throw std::runtime_error("Invalid floating point number format at offset " + std::to_string(startPos));
        }
    }

    [[nodiscard]] JsonValue parseBool()
    {
        if (m_src.substr(m_pos, 4U) == "true") {
            m_pos += 4U;
            JsonValue val {};
            val.type = JsonType::Bool;
            val.boolVal = true;
            return val;
        }
        if (m_src.substr(m_pos, 5U) == "false") {
            m_pos += 5U;
            JsonValue val {};
            val.type = JsonType::Bool;
            val.boolVal = false;
            return val;
        }
        throw std::runtime_error("Invalid boolean literal at offset " + std::to_string(m_pos));
    }

    [[nodiscard]] JsonValue parseNull()
    {
        if (m_src.substr(m_pos, 4U) == "null") {
            m_pos += 4U;
            JsonValue val {};
            val.type = JsonType::Null;
            return val;
        }
        throw std::runtime_error("Invalid null literal at offset " + std::to_string(m_pos));
    }
};

} // namespace

MacroSequence MacroSerializer::fromJson(const std::string& json)
{
    JsonParser parser { json };
    JsonValue root { parser.parse() };

    if (root.type != JsonType::Object) {
        throw std::runtime_error("Root JSON value must be an object");
    }

    MacroSequence seq {};
    if (const auto* nameVal = root.find("name"); nameVal != nullptr) {
        if (nameVal->type != JsonType::String) {
            throw std::runtime_error("'name' property must be a string");
        }
        seq.name = nameVal->strVal;
    }

    if (const auto* descVal = root.find("description"); descVal != nullptr) {
        if (descVal->type != JsonType::String) {
            throw std::runtime_error("'description' property must be a string");
        }
        seq.description = descVal->strVal;
    }

    if (const auto* repVal = root.find("repeatCount"); repVal != nullptr) {
        if (repVal->type != JsonType::Number) {
            throw std::runtime_error("'repeatCount' property must be a number");
        }
        if (std::isnan(repVal->numVal) || repVal->numVal < 0.0 || repVal->numVal > static_cast<double>(UINT32_MAX)) {
            throw std::runtime_error("'repeatCount' value out of allowable range [0, 4294967295]");
        }
        seq.repeatCount = static_cast<std::uint32_t>(repVal->numVal);
    }

    const auto* stepsVal { root.find("steps") };
    if (stepsVal == nullptr) {
        throw std::runtime_error("Macro JSON missing required 'steps' array");
    }
    if (stepsVal->type != JsonType::Array) {
        throw std::runtime_error("'steps' property must be a JSON array");
    }

    for (std::size_t i { 0U }; i < stepsVal->arrVal.size(); ++i) {
        const auto& stepItem { stepsVal->arrVal[i] };
        if (stepItem.type != JsonType::Object) {
            throw std::runtime_error("Step " + std::to_string(i + 1U) + " must be a JSON object");
        }

        MacroStep step {};
        if (const auto* lbl = stepItem.find("label"); lbl != nullptr) {
            if (lbl->type != JsonType::String) {
                throw std::runtime_error("Step " + std::to_string(i + 1U) + " 'label' must be a string");
            }
            step.label = lbl->strVal;
        }

        const auto* hexVal { stepItem.find("hex") };
        if (hexVal == nullptr || hexVal->type != JsonType::String) {
            throw std::runtime_error("Step " + std::to_string(i + 1U) + " missing 'hex' string property");
        }

        step.frame = PelcoDFrame::fromHexString(hexVal->strVal);
        if (step.frame.empty()) {
            throw std::runtime_error("Step " + std::to_string(i + 1U) + " contains invalid or empty hex payload");
        }

        if (const auto* dly = stepItem.find("delayMs"); dly != nullptr) {
            if (dly->type != JsonType::Number) {
                throw std::runtime_error("Step " + std::to_string(i + 1U) + " 'delayMs' must be a number");
            }
            if (std::isnan(dly->numVal) || dly->numVal < 0.0 || dly->numVal > static_cast<double>(UINT32_MAX)) {
                throw std::runtime_error("Step " + std::to_string(i + 1U) + " 'delayMs' value out of allowable range [0, 4294967295]");
            }
            step.delayMs = static_cast<std::uint32_t>(dly->numVal);
        }

        if (const auto* exp = stepItem.find("expectResponse"); exp != nullptr) {
            if (exp->type != JsonType::Bool) {
                throw std::runtime_error("Step " + std::to_string(i + 1U) + " 'expectResponse' must be a boolean");
            }
            step.expectResponse = exp->boolVal;
        }

        seq.steps.push_back(std::move(step));
    }

    return seq;
}

std::string MacroSerializer::toJson(const MacroSequence& sequence)
{
    std::ostringstream ss {};
    ss << "{\n";
    ss << "  \"name\": \"" << escapeJsonString(sequence.name) << "\",\n";
    ss << "  \"description\": \"" << escapeJsonString(sequence.description) << "\",\n";
    ss << "  \"repeatCount\": " << sequence.repeatCount << ",\n";
    ss << "  \"steps\": [\n";

    for (std::size_t i { 0U }; i < sequence.steps.size(); ++i) {
        const auto& step { sequence.steps[i] };
        ss << "    {\n";
        ss << "      \"label\": \"" << escapeJsonString(step.label) << "\",\n";
        ss << "      \"hex\": \"" << PelcoDFrame::toHexString(step.frame) << "\",\n";
        ss << "      \"delayMs\": " << step.delayMs << ",\n";
        ss << "      \"expectResponse\": " << (step.expectResponse ? "true" : "false") << "\n";
        ss << "    }" << (i + 1U < sequence.steps.size() ? "," : "") << "\n";
    }

    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

MacroSequence MacroSerializer::fromScript(const std::string& script)
{
    MacroSequence seq {};
    std::istringstream ss { script };
    std::string line {};
    std::size_t lineNum { 0U };

    while (std::getline(ss, line)) {
        ++lineNum;
        std::string trimmed { trim(line) };
        if (trimmed.empty()) {
            continue;
        }

        // Meta headers in comments: # Name: My Macro
        if (trimmed.rfind("# Name:", 0) == 0) {
            seq.name = trim(trimmed.substr(7U));
            continue;
        }
        if (trimmed.rfind("# Description:", 0) == 0) {
            seq.description = trim(trimmed.substr(14U));
            continue;
        }
        if (trimmed.rfind("# Repeat:", 0) == 0) {
            const std::string repStr { trim(trimmed.substr(9U)) };
            try {
                seq.repeatCount = static_cast<std::uint32_t>(std::stoul(repStr));
            } catch (...) {
                throw std::runtime_error("Invalid # Repeat value on line " + std::to_string(lineNum));
            }
            continue;
        }

        // Full line comments
        if (trimmed.front() == '#') {
            continue;
        }

        // Parse line: HEX_BYTES [delayMs] [# comment]
        std::string comment {};
        const auto hashPos = trimmed.find('#');
        if (hashPos != std::string::npos) {
            comment = trim(trimmed.substr(hashPos + 1U));
            trimmed = trim(trimmed.substr(0U, hashPos));
        }

        std::istringstream lineStream { trimmed };
        std::vector<std::string> tokens {};
        std::string tok {};
        while (lineStream >> tok) {
            tokens.push_back(tok);
        }

        if (tokens.empty()) {
            continue;
        }

        std::uint32_t delay { 100U };
        bool hasExplicitDelay { false };

        if (tokens.size() > 1U) {
            const std::string& lastTok { tokens.back() };
            if (lastTok.front() == '@') {
                std::string numPart { lastTok.substr(1U) };
                if (numPart.size() >= 2U && (numPart.rfind("ms") == numPart.size() - 2U || numPart.rfind("MS") == numPart.size() - 2U)) {
                    numPart = numPart.substr(0U, numPart.size() - 2U);
                }
                try {
                    delay = static_cast<std::uint32_t>(std::stoul(numPart));
                    hasExplicitDelay = true;
                } catch (...) {
                    throw std::runtime_error("Invalid explicit delay token '" + lastTok + "' on line " + std::to_string(lineNum));
                }
            } else if (lastTok.size() >= 3U && (lastTok.rfind("ms") == lastTok.size() - 2U || lastTok.rfind("MS") == lastTok.size() - 2U)) {
                std::string numPart { lastTok.substr(0U, lastTok.size() - 2U) };
                try {
                    delay = static_cast<std::uint32_t>(std::stoul(numPart));
                    hasExplicitDelay = true;
                } catch (...) {
                    throw std::runtime_error("Invalid explicit delay token '" + lastTok + "' on line " + std::to_string(lineNum));
                }
            } else if (std::all_of(lastTok.begin(), lastTok.end(), [](char c) {
                           return std::isdigit(static_cast<unsigned char>(c)) != 0;
                       })) {
                const std::size_t hexCountWithoutLast { tokens.size() - 1U };
                if (hexCountWithoutLast == PelcoDFrame::GeneralResponseSize ||
                    hexCountWithoutLast == PelcoDFrame::StandardFrameSize ||
                    hexCountWithoutLast == PelcoDFrame::QueryResponseSize) {
                    try {
                        delay = static_cast<std::uint32_t>(std::stoul(lastTok));
                        hasExplicitDelay = true;
                    } catch (...) {
                        throw std::runtime_error("Invalid numeric delay token on line " + std::to_string(lineNum));
                    }
                } else if (tokens.size() == PelcoDFrame::GeneralResponseSize ||
                           tokens.size() == PelcoDFrame::StandardFrameSize ||
                           tokens.size() == PelcoDFrame::QueryResponseSize) {
                    hasExplicitDelay = false;
                } else {
                    try {
                        delay = static_cast<std::uint32_t>(std::stoul(lastTok));
                        hasExplicitDelay = true;
                    } catch (...) {
                        hasExplicitDelay = false;
                    }
                }
            }
        }

        const std::size_t hexTokenCount { hasExplicitDelay ? (tokens.size() - 1U) : tokens.size() };
        std::string hexPart {};
        for (std::size_t i { 0U }; i < hexTokenCount; ++i) {
            if (!isValidHexToken(tokens[i])) {
                throw std::runtime_error("Invalid hex token '" + tokens[i] + "' on line " + std::to_string(lineNum));
            }
            if (!hexPart.empty()) {
                hexPart += ' ';
            }
            hexPart += tokens[i];
        }

        std::vector<std::uint8_t> bytes { PelcoDFrame::fromHexString(hexPart) };
        if (bytes.empty()) {
            throw std::runtime_error("Failed to parse hex payload on line " + std::to_string(lineNum));
        }

        MacroStep step {};
        step.label = comment;
        step.frame = std::move(bytes);
        step.delayMs = delay;
        step.expectResponse = false;
        seq.steps.push_back(std::move(step));
    }

    return seq;
}

std::string MacroSerializer::toScript(const MacroSequence& sequence)
{
    std::ostringstream ss {};
    ss << "# Name: " << sequence.name << "\n";
    if (!sequence.description.empty()) {
        ss << "# Description: " << sequence.description << "\n";
    }
    ss << "# Repeat: " << sequence.repeatCount << "\n";
    ss << "# Syntax: HEX_BYTES DELAY_MS # COMMENT\n\n";

    for (const auto& step : sequence.steps) {
        const std::string hex { PelcoDFrame::toHexString(step.frame) };
        ss << hex << "  " << step.delayMs;
        if (!step.label.empty()) {
            ss << "  # " << step.label;
        }
        ss << "\n";
    }

    return ss.str();
}

bool MacroSerializer::validate(const MacroSequence& sequence, std::string* errorMsg)
{
    try {
        if (sequence.steps.empty()) {
            if (errorMsg != nullptr) {
                *errorMsg = "Macro sequence contains no steps";
            }
            return false;
        }

        for (std::size_t i { 0U }; i < sequence.steps.size(); ++i) {
            const auto& step { sequence.steps[i] };
            const std::size_t len { step.frame.size() };
            if (len == 0U) {
                if (errorMsg != nullptr) {
                    *errorMsg = "Step " + std::to_string(i + 1U) + " has an empty byte payload";
                }
                return false;
            }

            if (len != PelcoDFrame::GeneralResponseSize &&
                len != PelcoDFrame::StandardFrameSize &&
                len != PelcoDFrame::QueryResponseSize) {
                if (errorMsg != nullptr) {
                    *errorMsg = "Step " + std::to_string(i + 1U) + " has invalid frame size (" +
                                std::to_string(len) + " bytes; expected 4, 7, or 18)";
                }
                return false;
            }

            if (!PelcoDFrame::isValidFrame(step.frame)) {
                if (errorMsg != nullptr) {
                    *errorMsg = "Step " + std::to_string(i + 1U) + " contains invalid Pelco-D checksum";
                }
                return false;
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

} // namespace PelcoD
