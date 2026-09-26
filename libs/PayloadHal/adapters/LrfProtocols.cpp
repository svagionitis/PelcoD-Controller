/// @file LrfProtocols.cpp
/// @brief Implementation of NMEA-0183, ASCII, and Binary protocol parsers for LRF hardware.

#include "LrfProtocols.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace PayloadHal {

namespace {

std::string trimString(const std::string& str)
{
    const auto first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::vector<std::string> splitTokens(const std::string& str, char delim)
{
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delim)) {
        tokens.push_back(trimString(item));
    }
    return tokens;
}

} // namespace

// =============================================================================
// NMEA-0183 LRF Parser
// =============================================================================

std::uint8_t NmeaLrfParser::computeNmeaChecksum(const std::string& sentence)
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
    std::ostringstream ss;
    ss << "$" << body << "*" << std::uppercase << std::hex << std::setfill('0') << std::setw(2)
       << static_cast<int>(cs) << "\r\n";
    return ss.str();
}

std::vector<LrfTargetMeasurement> NmeaLrfParser::parseIncomingBytes(const std::uint8_t* data, std::size_t length)
{
    std::vector<LrfTargetMeasurement> results;
    if (data == nullptr || length == 0) {
        return results;
    }

    m_rxBuffer.append(reinterpret_cast<const char*>(data), length);

    std::size_t newlinePos = 0;
    while ((newlinePos = m_rxBuffer.find('\n')) != std::string::npos) {
        std::string line = m_rxBuffer.substr(0, newlinePos);
        m_rxBuffer.erase(0, newlinePos + 1);

        line = trimString(line);
        if (line.empty()) {
            continue;
        }

        const auto dollarPos = line.find('$');
        const auto starPos = line.rfind('*');
        if (dollarPos == std::string::npos || starPos == std::string::npos || starPos <= dollarPos) {
            continue;
        }

        const std::string body = line.substr(dollarPos + 1, starPos - dollarPos - 1);
        const std::string csStr = line.substr(starPos + 1, 2);
        if (csStr.size() < 2) {
            continue;
        }

        int expectedCs = 0;
        try {
            expectedCs = std::stoi(csStr, nullptr, 16);
        } catch (...) {
            continue;
        }

        const std::uint8_t calculatedCs = computeNmeaChecksum(body);
        if (calculatedCs != static_cast<std::uint8_t>(expectedCs)) {
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

        // Pattern 1: $GPLRF,<dist>,<unit>,<status>
        // e.g. $GPLRF,1250.50,M,OK*2C
        if (tokens.size() >= 3 && (tokens[2] == "M" || tokens[2] == "FT" || tokens[2] == "m")) {
            try {
                distanceMeters = std::stod(tokens[1]);
                if (tokens[2] == "FT") {
                    distanceMeters *= 0.3048;
                }
                valid = (distanceMeters > 0.0);
                if (tokens.size() >= 4 && (tokens[3] == "FAIL" || tokens[3] == "ERR" || tokens[3] == "NO_TARGET")) {
                    valid = false;
                }
            } catch (...) {
                valid = false;
            }
        }
        // Pattern 2: $PLRF,D,<dist>
        else if (tokens.size() >= 3 && tokens[1] == "D") {
            try {
                distanceMeters = std::stod(tokens[2]);
                valid = (distanceMeters > 0.0);
            } catch (...) {
                valid = false;
            }
        }
        // Pattern 3: $GPLRF,<dist>
        else if (tokens.size() >= 2) {
            try {
                distanceMeters = std::stod(tokens[1]);
                valid = (distanceMeters > 0.0);
            } catch (...) {
                valid = false;
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

    return results;
}

void NmeaLrfParser::reset()
{
    m_rxBuffer.clear();
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

    m_rxBuffer.append(reinterpret_cast<const char*>(data), length);

    std::size_t newlinePos = 0;
    while ((newlinePos = m_rxBuffer.find('\n')) != std::string::npos) {
        std::string line = m_rxBuffer.substr(0, newlinePos);
        m_rxBuffer.erase(0, newlinePos + 1);

        line = trimString(line);
        if (line.empty()) {
            continue;
        }

        std::string upperLine = line;
        std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(), ::toupper);

        bool valid = false;
        double distanceMeters = 0.0;

        if (upperLine.find("NO_TARGET") != std::string::npos || upperLine.find("ERROR") != std::string::npos
            || upperLine.find("FAIL") != std::string::npos) {
            valid = false;
        } else {
            // Check for prefix "R:", "DIST:", "D,", etc.
            std::string numPart = line;
            if (upperLine.rfind("DIST:", 0) == 0) {
                numPart = trimString(line.substr(5));
            } else if (upperLine.rfind("R:", 0) == 0) {
                numPart = trimString(line.substr(2));
            } else if (upperLine.rfind("D,", 0) == 0) {
                numPart = trimString(line.substr(2));
            }

            // Remove trailing 'm' or 'M'
            if (!numPart.empty() && (numPart.back() == 'm' || numPart.back() == 'M')) {
                numPart.pop_back();
                numPart = trimString(numPart);
            }

            try {
                distanceMeters = std::stod(numPart);
                valid = (distanceMeters > 0.0);
            } catch (...) {
                valid = false;
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

    return results;
}

void AsciiLrfParser::reset()
{
    m_rxBuffer.clear();
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

std::uint16_t BinaryLrfParser::computeCrc16(const std::uint8_t* data, std::size_t length) noexcept
{
    std::uint16_t crc = 0xFFFF;
    for (std::size_t i = 0; i < length; ++i) {
        crc ^= static_cast<std::uint16_t>(data[i]) << 8;
        for (int b = 0; b < 8; ++b) {
            if ((crc & 0x8000) != 0) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

namespace {
std::vector<std::uint8_t> buildBinaryPacket(std::uint8_t cmd, const std::vector<std::uint8_t>& payload)
{
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
} // namespace

std::vector<LrfTargetMeasurement> BinaryLrfParser::parseIncomingBytes(const std::uint8_t* data, std::size_t length)
{
    std::vector<LrfTargetMeasurement> results;
    if (data == nullptr || length == 0) {
        return results;
    }

    m_rxBuffer.insert(m_rxBuffer.end(), data, data + length);

    while (m_rxBuffer.size() >= 6) {
        // Sync search
        if (m_rxBuffer[0] != kSyncByte1 || m_rxBuffer[1] != kSyncByte2) {
            m_rxBuffer.erase(m_rxBuffer.begin());
            continue;
        }

        const std::uint8_t msgId = m_rxBuffer[2];
        const std::uint8_t payloadLen = m_rxBuffer[3];
        const std::size_t totalFrameLen = 4 + payloadLen + 2;

        if (m_rxBuffer.size() < totalFrameLen) {
            break; // Await more incoming stream bytes
        }

        const std::uint16_t expectedCrc = (static_cast<std::uint16_t>(m_rxBuffer[4 + payloadLen]) << 8)
            | static_cast<std::uint16_t>(m_rxBuffer[5 + payloadLen]);

        const std::uint16_t computedCrc = computeCrc16(m_rxBuffer.data(), 4 + payloadLen);
        if (computedCrc != expectedCrc) {
            // CRC mismatch: drop first sync byte to re-sync
            m_rxBuffer.erase(m_rxBuffer.begin());
            continue;
        }

        // Valid frame!
        if (msgId == kMsgEchoReport && payloadLen >= 5) {
            const std::uint8_t status = m_rxBuffer[4];
            const std::uint32_t distMm = (static_cast<std::uint32_t>(m_rxBuffer[5]) << 24)
                | (static_cast<std::uint32_t>(m_rxBuffer[6]) << 16) | (static_cast<std::uint32_t>(m_rxBuffer[7]) << 8)
                | static_cast<std::uint32_t>(m_rxBuffer[8]);

            double quality = 0.95;
            if (payloadLen >= 6) {
                quality = static_cast<double>(m_rxBuffer[9]) / 255.0;
            }

            double tempC = 25.0;
            if (payloadLen >= 7) {
                tempC = static_cast<double>(static_cast<std::int8_t>(m_rxBuffer[10]));
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

        m_rxBuffer.erase(m_rxBuffer.begin(), m_rxBuffer.begin() + totalFrameLen);
    }

    return results;
}

void BinaryLrfParser::reset()
{
    m_rxBuffer.clear();
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
