/// @file LrfProtocols.cpp
/// @brief Implementation of NMEA-0183, ASCII, and Binary protocol parsers for LRF hardware.

#include "LrfProtocols.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <string_view>

namespace PayloadHal {

namespace {

    std::string_view trimStringView(std::string_view str) noexcept
    {
        const auto first = str.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos) {
            return {};
        }
        const auto last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

    std::string trimString(std::string_view str)
    {
        return std::string(trimStringView(str));
    }

    std::vector<std::string> splitTokens(std::string_view str, char delim)
    {
        std::vector<std::string> tokens;
        if (str.empty()) {
            return tokens;
        }
        std::size_t start = 0;
        while (start <= str.size()) {
            const auto end = str.find(delim, start);
            if (end == std::string_view::npos) {
                tokens.push_back(trimString(str.substr(start)));
                break;
            }
            tokens.push_back(trimString(str.substr(start, end - start)));
            start = end + 1;
        }
        return tokens;
    }

    bool parseDoubleLocaleIndependent(std::string_view str, double& outVal) noexcept
    {
        const auto first = str.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos) {
            return false;
        }
        const auto last = str.find_last_not_of(" \t\r\n");
        str = str.substr(first, last - first + 1);

        if (!str.empty() && str.front() == '+') {
            str.remove_prefix(1);
        }
        if (str.empty()) {
            return false;
        }

        auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), outVal);
        return (ec == std::errc {} && ptr == str.data() + str.size());
    }

    std::string toUpperSafe(std::string_view str)
    {
        std::string upper(str);
        std::transform(upper.begin(), upper.end(), upper.begin(),
            [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return upper;
    }

} // namespace

// =============================================================================
// NMEA-0183 LRF Parser
// =============================================================================

std::uint8_t NmeaLrfParser::computeNmeaChecksum(std::string_view sentence)
{
    std::uint8_t cs { 0 };
    std::size_t start = 0;
    if (!sentence.empty() && sentence[0] == '$') {
        start = 1;
    }
    for (std::size_t i = start; i < sentence.size(); ++i) {
        if (sentence[i] == '*') {
            break;
        }
        cs ^= static_cast<std::uint8_t>(sentence[i]);
    }
    return cs;
}

std::string NmeaLrfParser::formatNmeaSentence(const std::string& body)
{
    const std::uint8_t cs = computeNmeaChecksum(body);
    constexpr char kHexDigits[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(1 + body.size() + 1 + 2 + 2);
    result.push_back('$');
    result.append(body);
    result.push_back('*');
    result.push_back(kHexDigits[(cs >> 4) & 0x0F]);
    result.push_back(kHexDigits[cs & 0x0F]);
    result.append("\r\n");
    return result;
}

std::vector<std::string> NmeaLrfParser::splitTokens(std::string_view str, char delim)
{
    return ::PayloadHal::splitTokens(str, delim);
}

std::vector<LrfTargetMeasurement> NmeaLrfParser::parseIncomingBytes(const std::uint8_t* data, std::size_t length)
{
    std::vector<LrfTargetMeasurement> results;
    if (data == nullptr || length == 0) {
        return results;
    }

    if (m_rxBuffer.size() + length > kMaxRxBufferSize) {
        const auto lastDollar = m_rxBuffer.rfind('$');
        if (lastDollar != std::string::npos && lastDollar > 0) {
            m_rxBuffer.erase(0, lastDollar);
        }
        if (m_rxBuffer.size() + length > kMaxRxBufferSize) {
            m_rxBuffer.clear();
        }
    }
    if (length > kMaxRxBufferSize) {
        data += (length - kMaxRxBufferSize);
        length = kMaxRxBufferSize;
    }

    m_rxBuffer.append(reinterpret_cast<const char*>(data), length);

    std::size_t processedUpTo = 0;
    std::size_t newlinePos = 0;
    while ((newlinePos = m_rxBuffer.find('\n', processedUpTo)) != std::string::npos) {
        const std::string_view rawLine(m_rxBuffer.data() + processedUpTo, newlinePos - processedUpTo);
        processedUpTo = newlinePos + 1;

        const std::string_view line = trimStringView(rawLine);
        if (line.empty()) {
            continue;
        }

        const auto dollarPos = line.find('$');
        const auto starPos = line.rfind('*');
        if (dollarPos == std::string_view::npos || starPos == std::string_view::npos || starPos <= dollarPos) {
            continue;
        }

        const std::string_view body = line.substr(dollarPos + 1, starPos - dollarPos - 1);
        const std::string_view csStr = line.substr(starPos + 1, 2);
        if (csStr.size() < 2) {
            continue;
        }

        std::uint8_t expectedCs = 0;
        const auto [pCs, ecCs] = std::from_chars(csStr.data(), csStr.data() + csStr.size(), expectedCs, 16);
        if (ecCs != std::errc {} || pCs != csStr.data() + csStr.size()) {
            continue;
        }

        const std::uint8_t calculatedCs = computeNmeaChecksum(body);
        if (calculatedCs != expectedCs) {
            continue; // Checksum failure
        }

        const auto tokens = splitTokens(body, ',');
        if (tokens.empty()) {
            continue;
        }

        const std::string& header = tokens[0];
        if (header != "GPLRF" && header != "PLRF") {
            continue; // Not an LRF sentence
        }

        double distanceMeters = 0.0;
        bool valid = false;

        // Check if any subsequent token indicates failure or error status
        bool hasError = false;
        for (std::size_t i = 1; i < tokens.size(); ++i) {
            if (tokens[i] == "FAIL" || tokens[i] == "ERR" || tokens[i] == "NO_TARGET") {
                hasError = true;
                break;
            }
        }

        // Pattern 1: $GPLRF,<dist>,<unit>,<status>
        // e.g. $GPLRF,1250.50,M,OK*2C or $GPLRF,1250.50,M,*2C
        if (tokens.size() >= 3 && (tokens[2] == "M" || tokens[2] == "FT" || tokens[2] == "m")) {
            if (parseDoubleLocaleIndependent(tokens[1], distanceMeters)) {
                if (tokens[2] == "FT") {
                    distanceMeters *= 0.3048;
                }
                valid = (distanceMeters > 0.0 && !hasError);
            }
        }
        // Pattern 2: $PLRF,D,<dist>
        else if (tokens.size() >= 3 && tokens[1] == "D") {
            if (parseDoubleLocaleIndependent(tokens[2], distanceMeters)) {
                valid = (distanceMeters > 0.0 && !hasError);
            }
        }
        // Pattern 3: $GPLRF,<dist> (or $GPLRF,<dist>,,<status>)
        else if (tokens.size() >= 2) {
            if (parseDoubleLocaleIndependent(tokens[1], distanceMeters)) {
                valid = (distanceMeters > 0.0 && !hasError);
            }
        }

        LrfTargetMeasurement meas {};
        meas.valid = valid;
        meas.slantRangeMeters = valid ? distanceMeters : 0.0;
        meas.signalQualityRatio = valid ? 0.95 : 0.0;
        meas.diodeTemperatureC = 25.0;
        meas.pulseCounter = ++m_pulseCounter;
        meas.timestamp = std::chrono::system_clock::now();

        results.push_back(meas);
    }

    if (processedUpTo > 0) {
        m_rxBuffer.erase(0, processedUpTo);
    }

    return results;
}

void NmeaLrfParser::reset()
{
    m_rxBuffer.clear();
}

std::size_t NmeaLrfParser::getRxBufferSize() const noexcept
{
    return m_rxBuffer.size();
}

std::vector<std::uint8_t> NmeaLrfParser::buildArmCommand()
{
    const std::string cmd = formatNmeaSentence("GPLRF,ARM");
    return { cmd.begin(), cmd.end() };
}

std::vector<std::uint8_t> NmeaLrfParser::buildDisarmCommand()
{
    const std::string cmd = formatNmeaSentence("GPLRF,DISARM");
    return { cmd.begin(), cmd.end() };
}

std::vector<std::uint8_t> NmeaLrfParser::buildFireCommand()
{
    const std::string cmd = formatNmeaSentence("GPLRF,FIRE");
    return { cmd.begin(), cmd.end() };
}

std::vector<std::uint8_t> NmeaLrfParser::buildContinuousCommand(LrfMode mode)
{
    int rate = 1;
    if (mode == LrfMode::Continuous5Hz) {
        rate = 5;
    } else if (mode == LrfMode::Continuous10Hz) {
        rate = 10;
    }
    const std::string cmd = formatNmeaSentence("GPLRF,RATE," + std::to_string(rate));
    return { cmd.begin(), cmd.end() };
}

std::vector<std::uint8_t> NmeaLrfParser::buildStopCommand()
{
    const std::string cmd = formatNmeaSentence("GPLRF,STOP");
    return { cmd.begin(), cmd.end() };
}

// =============================================================================
// ASCII Delimited LRF Parser
// =============================================================================

AsciiLrfParser::AsciiLrfParser(SerialLrfConfig config)
    : m_config(std::move(config))
{
}

std::vector<LrfTargetMeasurement> AsciiLrfParser::parseIncomingBytes(const std::uint8_t* data, std::size_t length)
{
    std::vector<LrfTargetMeasurement> results;
    if (data == nullptr || length == 0) {
        return results;
    }

    if (m_rxBuffer.size() + length > kMaxRxBufferSize) {
        const auto lastNewline = m_rxBuffer.rfind('\n');
        if (lastNewline != std::string::npos) {
            m_rxBuffer.erase(0, lastNewline + 1);
        }
        if (m_rxBuffer.size() + length > kMaxRxBufferSize) {
            m_rxBuffer.clear();
        }
    }
    if (length > kMaxRxBufferSize) {
        data += (length - kMaxRxBufferSize);
        length = kMaxRxBufferSize;
    }

    m_rxBuffer.append(reinterpret_cast<const char*>(data), length);

    std::size_t processedUpTo = 0;
    std::size_t newlinePos = 0;
    while ((newlinePos = m_rxBuffer.find('\n', processedUpTo)) != std::string::npos) {
        const std::string_view rawLine(m_rxBuffer.data() + processedUpTo, newlinePos - processedUpTo);
        processedUpTo = newlinePos + 1;

        const std::string_view line = trimStringView(rawLine);
        if (line.empty()) {
            continue;
        }

        const std::string upperLine = toUpperSafe(line);

        bool valid = false;
        double distanceMeters = 0.0;

        if (upperLine.find("NO_TARGET") != std::string::npos || upperLine.find("ERROR") != std::string::npos
            || upperLine.find("FAIL") != std::string::npos) {
            valid = false;
        } else {
            // Check for prefix "R:", "DIST:", "D,", etc.
            std::string_view numPart = line;
            if (upperLine.rfind("DIST:", 0) == 0) {
                numPart = trimStringView(line.substr(5));
            } else if (upperLine.rfind("R:", 0) == 0) {
                numPart = trimStringView(line.substr(2));
            } else if (upperLine.rfind("D,", 0) == 0) {
                numPart = trimStringView(line.substr(2));
            }

            // Remove trailing 'm' or 'M'
            if (!numPart.empty() && (numPart.back() == 'm' || numPart.back() == 'M')) {
                numPart.remove_suffix(1);
                numPart = trimStringView(numPart);
            }

            if (parseDoubleLocaleIndependent(numPart, distanceMeters)) {
                valid = (distanceMeters > 0.0);
            }
        }

        LrfTargetMeasurement meas {};
        meas.valid = valid;
        meas.slantRangeMeters = valid ? distanceMeters : 0.0;
        meas.signalQualityRatio = valid ? 0.90 : 0.0;
        meas.diodeTemperatureC = 25.0;
        meas.pulseCounter = ++m_pulseCounter;
        meas.timestamp = std::chrono::system_clock::now();

        results.push_back(meas);
    }

    if (processedUpTo > 0) {
        m_rxBuffer.erase(0, processedUpTo);
    }

    return results;
}

void AsciiLrfParser::reset()
{
    m_rxBuffer.clear();
}

std::size_t AsciiLrfParser::getRxBufferSize() const noexcept
{
    return m_rxBuffer.size();
}

std::vector<std::uint8_t> AsciiLrfParser::buildArmCommand()
{
    const std::string cmd = m_config.customArmCmd.empty() ? "ARM\r\n" : m_config.customArmCmd;
    return { cmd.begin(), cmd.end() };
}

std::vector<std::uint8_t> AsciiLrfParser::buildDisarmCommand()
{
    const std::string cmd = m_config.customDisarmCmd.empty() ? "DISARM\r\n" : m_config.customDisarmCmd;
    return { cmd.begin(), cmd.end() };
}

std::vector<std::uint8_t> AsciiLrfParser::buildFireCommand()
{
    const std::string cmd = m_config.customFireCmd.empty() ? "FIRE\r\n" : m_config.customFireCmd;
    return { cmd.begin(), cmd.end() };
}

std::vector<std::uint8_t> AsciiLrfParser::buildContinuousCommand(LrfMode mode)
{
    int rate = 1;
    if (mode == LrfMode::Continuous5Hz) {
        rate = 5;
    } else if (mode == LrfMode::Continuous10Hz) {
        rate = 10;
    }
    const std::string cmd = "RATE " + std::to_string(rate) + "\r\n";
    return { cmd.begin(), cmd.end() };
}

std::vector<std::uint8_t> AsciiLrfParser::buildStopCommand()
{
    const std::string cmd = "STOP\r\n";
    return { cmd.begin(), cmd.end() };
}

// =============================================================================
// Binary Framed LRF Parser
// =============================================================================

namespace {

    constexpr std::array<std::uint16_t, 256> generateCrc16CcittTable() noexcept
    {
        std::array<std::uint16_t, 256> table {};
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint16_t cur = static_cast<std::uint16_t>(i << 8U);
            for (int bit = 0; bit < 8; ++bit) {
                if ((cur & 0x8000U) != 0U) {
                    cur = static_cast<std::uint16_t>((cur << 1U) ^ 0x1021U);
                } else {
                    cur = static_cast<std::uint16_t>(cur << 1U);
                }
            }
            table[i] = cur;
        }
        return table;
    }

    constexpr auto kCrc16CcittTable = generateCrc16CcittTable();

} // namespace

std::uint16_t BinaryLrfParser::computeCrc16(const std::uint8_t* data, std::size_t length) noexcept
{
    if (data == nullptr || length == 0) {
        return 0xFFFFU;
    }

    std::uint16_t crc = 0xFFFFU;
    for (std::size_t i = 0; i < length; ++i) {
        const std::uint8_t byte = data[i];
        const std::uint8_t tableIdx = static_cast<std::uint8_t>((crc >> 8U) ^ byte);
        crc = static_cast<std::uint16_t>((crc << 8U) ^ kCrc16CcittTable[tableIdx]);
    }
    return crc;
}

std::vector<std::uint8_t> BinaryLrfParser::buildBinaryPacket(std::uint8_t cmd, const std::vector<std::uint8_t>& payload)
{
    if (payload.size() > kMaxPayloadLength) {
        return {};
    }

    std::vector<std::uint8_t> packet;
    packet.reserve(6 + payload.size());
    packet.push_back(BinaryLrfParser::kSyncByte1);
    packet.push_back(BinaryLrfParser::kSyncByte2);
    packet.push_back(cmd);
    packet.push_back(static_cast<std::uint8_t>(payload.size()));
    packet.insert(packet.end(), payload.begin(), payload.end());

    const std::uint16_t crc = BinaryLrfParser::computeCrc16(packet.data(), packet.size());
    packet.push_back(static_cast<std::uint8_t>((crc >> 8) & 0xFF));
    packet.push_back(static_cast<std::uint8_t>(crc & 0xFF));
    return packet;
}

std::vector<LrfTargetMeasurement> BinaryLrfParser::parseIncomingBytes(const std::uint8_t* data, std::size_t length)
{
    std::vector<LrfTargetMeasurement> results;
    if (data == nullptr || length == 0) {
        return results;
    }

    if (m_rxBuffer.size() + length > kMaxRxBufferSize) {
        auto it = std::adjacent_find(m_rxBuffer.begin(), m_rxBuffer.end(),
            [](std::uint8_t a, std::uint8_t b) { return a == kSyncByte1 && b == kSyncByte2; });
        if (it != m_rxBuffer.end() && it != m_rxBuffer.begin()) {
            m_rxBuffer.erase(m_rxBuffer.begin(), it);
        }
        if (m_rxBuffer.size() + length > kMaxRxBufferSize) {
            m_rxBuffer.clear();
        }
    }
    if (length > kMaxRxBufferSize) {
        data += (length - kMaxRxBufferSize);
        length = kMaxRxBufferSize;
    }

    m_rxBuffer.insert(m_rxBuffer.end(), data, data + length);

    std::size_t offset = 0;
    while (m_rxBuffer.size() - offset >= 6) {
        // Fast-forward to sync marker using adjacent_find
        if (m_rxBuffer[offset] != kSyncByte1 || m_rxBuffer[offset + 1] != kSyncByte2) {
            auto it = std::adjacent_find(m_rxBuffer.begin() + static_cast<std::ptrdiff_t>(offset), m_rxBuffer.end(),
                [](std::uint8_t a, std::uint8_t b) { return a == kSyncByte1 && b == kSyncByte2; });
            if (it == m_rxBuffer.end()) {
                if (m_rxBuffer.back() == kSyncByte1) {
                    offset = m_rxBuffer.size() - 1;
                } else {
                    offset = m_rxBuffer.size();
                }
                break;
            }
            offset = static_cast<std::size_t>(std::distance(m_rxBuffer.begin(), it));
            if (m_rxBuffer.size() - offset < 6) {
                break;
            }
        }

        const std::uint8_t msgId = m_rxBuffer[offset + 2];
        const std::uint8_t payloadLen = m_rxBuffer[offset + 3];

        if (msgId == kMsgEchoReport
            && (payloadLen < kMinEchoReportPayloadLength || payloadLen > kMaxEchoReportPayloadLength)) {
            // Invalid EchoReport payload length: skip first sync byte and continue searching
            ++offset;
            continue;
        }

        const std::size_t totalFrameLen = 4 + payloadLen + 2;

        if (m_rxBuffer.size() - offset < totalFrameLen) {
            break; // Await more incoming stream bytes
        }

        const std::uint16_t expectedCrc = (static_cast<std::uint16_t>(m_rxBuffer[offset + 4 + payloadLen]) << 8)
            | static_cast<std::uint16_t>(m_rxBuffer[offset + 5 + payloadLen]);

        const std::uint16_t computedCrc = computeCrc16(m_rxBuffer.data() + offset, 4 + payloadLen);
        if (computedCrc != expectedCrc) {
            // CRC mismatch: skip first sync byte to re-sync
            ++offset;
            continue;
        }

        // Valid frame!
        if (msgId == kMsgEchoReport && payloadLen >= kMinEchoReportPayloadLength) {
            const std::uint8_t status = m_rxBuffer[offset + 4];
            const std::uint32_t distMm = (static_cast<std::uint32_t>(m_rxBuffer[offset + 5]) << 24)
                | (static_cast<std::uint32_t>(m_rxBuffer[offset + 6]) << 16)
                | (static_cast<std::uint32_t>(m_rxBuffer[offset + 7]) << 8)
                | static_cast<std::uint32_t>(m_rxBuffer[offset + 8]);

            double quality = 0.95;
            if (payloadLen >= 6) {
                quality = static_cast<double>(m_rxBuffer[offset + 9]) / 255.0;
            }

            double tempC = 25.0;
            if (payloadLen >= 7) {
                tempC = static_cast<double>(static_cast<std::int8_t>(m_rxBuffer[offset + 10]));
            }

            LrfTargetMeasurement meas {};
            meas.valid = (status == 0 && distMm > 0);
            meas.slantRangeMeters = meas.valid ? (static_cast<double>(distMm) / 1000.0) : 0.0;
            meas.signalQualityRatio = meas.valid ? quality : 0.0;
            meas.diodeTemperatureC = tempC;
            meas.pulseCounter = ++m_pulseCounter;
            meas.timestamp = std::chrono::system_clock::now();

            results.push_back(meas);
        }

        offset += totalFrameLen;
    }

    if (offset > 0) {
        m_rxBuffer.erase(m_rxBuffer.begin(), m_rxBuffer.begin() + static_cast<std::ptrdiff_t>(offset));
    }

    return results;
}

void BinaryLrfParser::reset()
{
    m_rxBuffer.clear();
}

std::size_t BinaryLrfParser::getRxBufferSize() const noexcept
{
    return m_rxBuffer.size();
}

std::vector<std::uint8_t> BinaryLrfParser::buildArmCommand()
{
    return buildBinaryPacket(kCmdArm, {});
}

std::vector<std::uint8_t> BinaryLrfParser::buildDisarmCommand()
{
    return buildBinaryPacket(kCmdDisarm, {});
}

std::vector<std::uint8_t> BinaryLrfParser::buildFireCommand()
{
    return buildBinaryPacket(kCmdFireSingle, {});
}

std::vector<std::uint8_t> BinaryLrfParser::buildContinuousCommand(LrfMode mode)
{
    std::uint8_t rate = 1;
    if (mode == LrfMode::Continuous5Hz) {
        rate = 5;
    } else if (mode == LrfMode::Continuous10Hz) {
        rate = 10;
    }
    return buildBinaryPacket(kCmdContinuous, { rate });
}

std::vector<std::uint8_t> BinaryLrfParser::buildStopCommand()
{
    return buildBinaryPacket(kCmdStop, {});
}

// =============================================================================
// Factory Helper
// =============================================================================

std::unique_ptr<ILrfProtocolParser> createLrfParser(const SerialLrfConfig& config)
{
    switch (config.protocolType) {
    case LrfProtocolType::Nmea:
        return std::make_unique<NmeaLrfParser>();
    case LrfProtocolType::Ascii:
        return std::make_unique<AsciiLrfParser>(config);
    case LrfProtocolType::Binary:
        return std::make_unique<BinaryLrfParser>();
    }
    return std::make_unique<NmeaLrfParser>();
}

} // namespace PayloadHal
