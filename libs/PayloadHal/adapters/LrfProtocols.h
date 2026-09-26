#pragma once

/// @file LrfProtocols.h
/// @brief Multi-protocol wire framing and parser abstractions for Laser Range Finder (LRF) hardware.

#include "PayloadHal/ILaserRangeFinder.h"
#include "PayloadHal/PayloadTypes.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace PayloadHal {

/// @enum LrfProtocolType
/// @brief Supported hardware framing protocols for serial/stream Laser Range Finders.
enum class LrfProtocolType : std::uint8_t {
    Nmea,   ///< NMEA-0183 ASCII standard ($GPLRF, $PLRF sentences with checksum)
    Ascii,  ///< Delimited human-readable ASCII text (e.g. "R: 1250.5\r\n")
    Binary  ///< Framed binary packet with header, length, payload, and CRC-16
};

/// @struct SerialLrfConfig
/// @brief Operational and safety configuration parameters for serial LRF hardware.
struct SerialLrfConfig {
    LrfProtocolType protocolType { LrfProtocolType::Nmea }; ///< Active wire protocol parser

    // --- Safety Interlocks & Watchdog ---
    std::chrono::milliseconds autoDisarmTimeout { 30000 }; ///< Inactivity timeout to auto-disarm (ANSI Z136 eye-safety)
    bool enforceArmingInterlock { true };                  ///< If true, reject fire commands when !isArmed()

    // --- Range Gating Filters ---
    double minRangeMeters { 0.5 };     ///< Minimum distance threshold (meters) to reject near-field backscatter
    double maxRangeMeters { 30000.0 }; ///< Maximum operational range threshold (meters)

    // --- Continuous Triggering ---
    double continuousRateHz { 1.0 };   ///< Default pulse repetition rate in continuous mode (1.0 to 10.0 Hz)

    // --- Custom Protocol Overrides ---
    std::string customFireCmd {};      ///< Custom fire command string (for Ascii protocol)
    std::string customArmCmd {};       ///< Custom arm command string (for Ascii protocol)
    std::string customDisarmCmd {};    ///< Custom disarm command string (for Ascii protocol)
};

/// @class ILrfProtocolParser
/// @brief Abstract interface for parsing raw incoming stream bytes and constructing outgoing LRF commands.
class ILrfProtocolParser {
public:
    virtual ~ILrfProtocolParser() = default;

    /// @brief Ingests raw serial bytes, buffers fragment frames, and returns any complete parsed measurements.
    /// @param[in] data Pointer to raw byte buffer.
    /// @param[in] length Number of bytes available.
    /// @return List of parsed target measurement records.
    [[nodiscard]] virtual std::vector<LrfTargetMeasurement> parseIncomingBytes(
        const std::uint8_t* data, std::size_t length) = 0;

    /// @brief Clears any internal partial receive buffers.
    virtual void reset() = 0;

    /// @brief Generates wire command bytes to arm the laser transmitter.
    /// @return Byte vector ready for transport transmission.
    [[nodiscard]] virtual std::vector<std::uint8_t> buildArmCommand() = 0;

    /// @brief Generates wire command bytes to disarm the laser transmitter.
    /// @return Byte vector ready for transport transmission.
    [[nodiscard]] virtual std::vector<std::uint8_t> buildDisarmCommand() = 0;

    /// @brief Generates wire command bytes to fire a single ranging pulse.
    /// @return Byte vector ready for transport transmission.
    [[nodiscard]] virtual std::vector<std::uint8_t> buildFireCommand() = 0;

    /// @brief Generates wire command bytes to activate continuous pulse ranging.
    /// @param[in] mode Firing frequency mode (1 Hz, 5 Hz, 10 Hz).
    /// @return Byte vector ready for transport transmission.
    [[nodiscard]] virtual std::vector<std::uint8_t> buildContinuousCommand(LrfMode mode) = 0;

    /// @brief Generates wire command bytes to halt active pulse emissions.
    /// @return Byte vector ready for transport transmission.
    [[nodiscard]] virtual std::vector<std::uint8_t> buildStopCommand() = 0;
};

/// @class NmeaLrfParser
/// @brief NMEA-0183 protocol parser validating XOR checksums on $GPLRF and $PLRF sentences.
class NmeaLrfParser : public ILrfProtocolParser {
public:
    NmeaLrfParser() = default;
    ~NmeaLrfParser() override = default;

    [[nodiscard]] std::vector<LrfTargetMeasurement> parseIncomingBytes(
        const std::uint8_t* data, std::size_t length) override;

    void reset() override;

    [[nodiscard]] std::vector<std::uint8_t> buildArmCommand() override;
    [[nodiscard]] virtual std::vector<std::uint8_t> buildDisarmCommand() override;
    [[nodiscard]] virtual std::vector<std::uint8_t> buildFireCommand() override;
    [[nodiscard]] virtual std::vector<std::uint8_t> buildContinuousCommand(LrfMode mode) override;
    [[nodiscard]] virtual std::vector<std::uint8_t> buildStopCommand() override;

    /// @brief Calculates 8-bit XOR checksum over string excluding leading '$' and trailing '*'.
    [[nodiscard]] static std::uint8_t computeNmeaChecksum(const std::string& sentence);

    /// @brief Formats a complete NMEA sentence with leading '$', trailing '*', two hex checksum chars, and CRLF.
    [[nodiscard]] static std::string formatNmeaSentence(const std::string& body);

private:
    std::string m_rxBuffer {};
    std::uint32_t m_pulseCounter { 0U };
};

/// @class AsciiLrfParser
/// @brief Delimited ASCII text protocol parser for standard OEM modules.
class AsciiLrfParser : public ILrfProtocolParser {
public:
    explicit AsciiLrfParser(SerialLrfConfig config = {});
    ~AsciiLrfParser() override = default;

    [[nodiscard]] std::vector<LrfTargetMeasurement> parseIncomingBytes(
        const std::uint8_t* data, std::size_t length) override;

    void reset() override;

    [[nodiscard]] std::vector<std::uint8_t> buildArmCommand() override;
    [[nodiscard]] std::vector<std::uint8_t> buildDisarmCommand() override;
    [[nodiscard]] std::vector<std::uint8_t> buildFireCommand() override;
    [[nodiscard]] std::vector<std::uint8_t> buildContinuousCommand(LrfMode mode) override;
    [[nodiscard]] std::vector<std::uint8_t> buildStopCommand() override;

private:
    SerialLrfConfig m_config {};
    std::string m_rxBuffer {};
    std::uint32_t m_pulseCounter { 0U };
};

/// @class BinaryLrfParser
/// @brief Framed binary protocol parser with sync header (0xAA 0x55) and CRC-16 CCITT validation.
class BinaryLrfParser : public ILrfProtocolParser {
public:
    BinaryLrfParser() = default;
    ~BinaryLrfParser() override = default;

    [[nodiscard]] std::vector<LrfTargetMeasurement> parseIncomingBytes(
        const std::uint8_t* data, std::size_t length) override;

    void reset() override;

    [[nodiscard]] std::vector<std::uint8_t> buildArmCommand() override;
    [[nodiscard]] std::vector<std::uint8_t> buildDisarmCommand() override;
    [[nodiscard]] std::vector<std::uint8_t> buildFireCommand() override;
    [[nodiscard]] std::vector<std::uint8_t> buildContinuousCommand(LrfMode mode) override;
    [[nodiscard]] std::vector<std::uint8_t> buildStopCommand() override;

    /// @brief Calculates CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF).
    [[nodiscard]] static std::uint16_t computeCrc16(const std::uint8_t* data, std::size_t length) noexcept;

    // Binary packet command IDs
    static constexpr std::uint8_t kSyncByte1 { 0xAA };
    static constexpr std::uint8_t kSyncByte2 { 0x55 };
    static constexpr std::uint8_t kCmdFireSingle { 0x01 };
    static constexpr std::uint8_t kCmdContinuous { 0x02 };
    static constexpr std::uint8_t kCmdArm { 0x05 };
    static constexpr std::uint8_t kCmdDisarm { 0x06 };
    static constexpr std::uint8_t kCmdStop { 0x07 };
    static constexpr std::uint8_t kMsgEchoReport { 0x10 };

private:
    std::vector<std::uint8_t> m_rxBuffer {};
    std::uint32_t m_pulseCounter { 0U };
};

/// @brief Factory function creating an ILrfProtocolParser for the given configuration.
[[nodiscard]] std::unique_ptr<ILrfProtocolParser> createLrfParser(const SerialLrfConfig& config);

} // namespace PayloadHal
