#pragma once

/// @file ProtocolBuilder.h
/// @brief Encoder constructing Pelco-D protocol frames according to D Protocol v5.0.1.

#include "PelcoDTypes.h"

#include <cstdint>
#include <vector>

namespace PelcoD {

/// @class ProtocolBuilder
/// @brief Factory methods for encoding standard and extended Pelco-D command packets.
class ProtocolBuilder {
public:
    ProtocolBuilder() = default;
    ~ProtocolBuilder() = default;

    // Standard Motion Commands
    [[nodiscard]] static std::vector<std::uint8_t> buildMotion(std::uint8_t address, PanDirection panDir,
        std::uint8_t panSpeed, TiltDirection tiltDir, std::uint8_t tiltSpeed, ZoomAction zoom = ZoomAction::Stop,
        FocusAction focus = FocusAction::Stop, IrisAction iris = IrisAction::Stop);

    [[nodiscard]] static std::vector<std::uint8_t> buildStop(std::uint8_t address);

    [[nodiscard]] static std::vector<std::uint8_t> buildPan(std::uint8_t address, PanDirection dir, std::uint8_t speed);

    [[nodiscard]] static std::vector<std::uint8_t> buildTilt(
        std::uint8_t address, TiltDirection dir, std::uint8_t speed);

    [[nodiscard]] static std::vector<std::uint8_t> buildZoom(std::uint8_t address, ZoomAction action);

    [[nodiscard]] static std::vector<std::uint8_t> buildFocus(std::uint8_t address, FocusAction action);

    [[nodiscard]] static std::vector<std::uint8_t> buildIris(std::uint8_t address, IrisAction action);

    [[nodiscard]] static std::vector<std::uint8_t> buildPower(std::uint8_t address, bool on);

    [[nodiscard]] static std::vector<std::uint8_t> buildScan(std::uint8_t address, bool autoScan);

    // Presets
    [[nodiscard]] static std::vector<std::uint8_t> buildSetPreset(std::uint8_t address, std::uint8_t presetId);

    [[nodiscard]] static std::vector<std::uint8_t> buildClearPreset(std::uint8_t address, std::uint8_t presetId);

    [[nodiscard]] static std::vector<std::uint8_t> buildGoToPreset(std::uint8_t address, std::uint8_t presetId);

    [[nodiscard]] static std::vector<std::uint8_t> buildFlip180(std::uint8_t address);

    [[nodiscard]] static std::vector<std::uint8_t> buildZeroPan(std::uint8_t address);

    /// @brief Initiates a preset scan touring defined presets with dwell time (opcode 0x47).
    /// @param[in] address Device bus address (1-255).
    /// @param[in] dwellSeconds Time in seconds to dwell at each visited preset.
    /// @return 7-byte Pelco-D formatted vector.
    [[nodiscard]] static std::vector<std::uint8_t> buildPresetScan(std::uint8_t address, std::uint8_t dwellSeconds);

    // Auxiliaries & Zones
    [[nodiscard]] static std::vector<std::uint8_t> buildSetAux(std::uint8_t address, std::uint8_t auxId);

    [[nodiscard]] static std::vector<std::uint8_t> buildClearAux(std::uint8_t address, std::uint8_t auxId);

    [[nodiscard]] static std::vector<std::uint8_t> buildSetZoneStart(std::uint8_t address, std::uint8_t zoneId);

    [[nodiscard]] static std::vector<std::uint8_t> buildSetZoneEnd(std::uint8_t address, std::uint8_t zoneId);

    [[nodiscard]] static std::vector<std::uint8_t> buildZoneScan(std::uint8_t address, bool enable);

    // Patterns
    [[nodiscard]] static std::vector<std::uint8_t> buildPatternStart(std::uint8_t address, std::uint8_t patternId);

    [[nodiscard]] static std::vector<std::uint8_t> buildPatternStop(std::uint8_t address);

    [[nodiscard]] static std::vector<std::uint8_t> buildRunPattern(std::uint8_t address, std::uint8_t patternId);

    // Speeds & Configuration
    [[nodiscard]] static std::vector<std::uint8_t> buildZoomSpeed(std::uint8_t address, std::uint8_t speed);

    [[nodiscard]] static std::vector<std::uint8_t> buildFocusSpeed(std::uint8_t address, std::uint8_t speed);

    [[nodiscard]] static std::vector<std::uint8_t> buildResetDefaults(std::uint8_t address);

    [[nodiscard]] static std::vector<std::uint8_t> buildRemoteReset(std::uint8_t address);

    /// @brief Sends a dummy keep-alive NOP packet to the device (opcode 0x0D).
    /// @param[in] address Device bus address (1-255).
    /// @return 7-byte Pelco-D formatted vector.
    [[nodiscard]] static std::vector<std::uint8_t> buildDummy(std::uint8_t address);

    [[nodiscard]] static std::vector<std::uint8_t> buildAutoFocus(std::uint8_t address, AutoMode mode);

    [[nodiscard]] static std::vector<std::uint8_t> buildAutoIris(std::uint8_t address, AutoMode mode);

    [[nodiscard]] static std::vector<std::uint8_t> buildAgc(std::uint8_t address, AutoMode mode);

    [[nodiscard]] static std::vector<std::uint8_t> buildBacklight(std::uint8_t address, SwitchState state);

    [[nodiscard]] static std::vector<std::uint8_t> buildWhiteBalance(std::uint8_t address, SwitchState state);

    [[nodiscard]] static std::vector<std::uint8_t> buildShutterSpeed(std::uint8_t address, std::uint16_t speed);

    [[nodiscard]] static std::vector<std::uint8_t> buildGain(std::uint8_t address, std::uint16_t gain);

    [[nodiscard]] static std::vector<std::uint8_t> buildAutoIrisLevel(std::uint8_t address, std::uint8_t level);

    [[nodiscard]] static std::vector<std::uint8_t> buildAutoIrisPeak(std::uint8_t address, std::uint8_t peak);

    [[nodiscard]] static std::vector<std::uint8_t> buildPhaseDelayMode(std::uint8_t address, SwitchState state);

    [[nodiscard]] static std::vector<std::uint8_t> buildLineLockDelay(std::uint8_t address, std::uint16_t centidegrees);

    [[nodiscard]] static std::vector<std::uint8_t> buildWhiteBalanceRB(std::uint8_t address, std::uint16_t value);

    [[nodiscard]] static std::vector<std::uint8_t> buildWhiteBalanceMG(std::uint8_t address, std::uint16_t value);

    // Absolute Positioning
    [[nodiscard]] static std::vector<std::uint8_t> buildSetPan(std::uint8_t address, std::uint16_t centidegrees);

    [[nodiscard]] static std::vector<std::uint8_t> buildSetTilt(std::uint8_t address, std::uint16_t centidegrees);

    [[nodiscard]] static std::vector<std::uint8_t> buildSetZoom(std::uint8_t address, std::uint16_t position);

    /// @brief Sets device hardware azimuth zero at current position (opcode 0x49).
    [[nodiscard]] static std::vector<std::uint8_t> buildSetZeroPosition(std::uint8_t address);

    /// @brief Sets optical magnification (opcode 0x5F).
    /// @param relative If true, value is a relative delta; if false, absolute.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetMagnification(
        std::uint8_t address, std::uint16_t value, bool relative = false);

    /// @brief Sets remote baud rate (opcode 0x67).
    [[nodiscard]] static std::vector<std::uint8_t> buildSetBaudRate(std::uint8_t address, std::uint32_t baud);

    // Queries
    [[nodiscard]] static std::vector<std::uint8_t> buildQueryPan(std::uint8_t address);

    [[nodiscard]] static std::vector<std::uint8_t> buildQueryTilt(std::uint8_t address);

    [[nodiscard]] static std::vector<std::uint8_t> buildQueryZoom(std::uint8_t address);

    [[nodiscard]] static std::vector<std::uint8_t> buildQueryMag(std::uint8_t address);

    [[nodiscard]] static std::vector<std::uint8_t> buildQueryDevType(std::uint8_t address);

    [[nodiscard]] static std::vector<std::uint8_t> buildQueryGeneral(std::uint8_t address);

    // OSD & Alarm
    [[nodiscard]] static std::vector<std::uint8_t> buildWriteChar(
        std::uint8_t address, std::uint8_t column, char asciiChar);

    [[nodiscard]] static std::vector<std::uint8_t> buildClearScreen(std::uint8_t address);

    [[nodiscard]] static std::vector<std::uint8_t> buildAlarmAck(std::uint8_t address, std::uint8_t alarmId);

    /// @brief Requests diagnostic information from device (opcode 0x6F).
    [[nodiscard]] static std::vector<std::uint8_t> buildQueryDiagnostics(std::uint8_t address);

    /// @brief Prepares device to receive a firmware download (opcode 0x57).
    /// @param[in] address Device bus address (1-255).
    /// @return 7-byte Pelco-D formatted vector.
    [[nodiscard]] static std::vector<std::uint8_t> buildPrepareForDownload(std::uint8_t address);

    /// @brief Instructs device to start firmware download data reception (opcode 0x69).
    /// @param[in] address Device bus address (1-255).
    /// @return 7-byte Pelco-D formatted vector.
    [[nodiscard]] static std::vector<std::uint8_t> buildStartDownload(std::uint8_t address);

    /// @brief Activates RS-485 loopback echo mode for diagnostic testing (opcode 0x65).
    /// @param[in] address Device bus address (1-255).
    /// @return 7-byte Pelco-D formatted vector.
    [[nodiscard]] static std::vector<std::uint8_t> buildEchoMode(std::uint8_t address);

    /// @brief Commands screen coordinate repositioning (opcode 0x79).
    /// @details Byte 3 holds 0x00 for absolute or 0x01 for relative screen moves.
    ///          Bytes 5 and 6 contain signed 8-bit percentages (-100 to +100) from screen center.
    ///          Positive values are right for pan and up for tilt per spec §5.61.
    /// @param[in] address Device bus address (1-255).
    /// @param[in] panPercent Signed percentage from center (-100 to 100, positive = right).
    /// @param[in] tiltPercent Signed percentage from center (-100 to 100, positive = up).
    /// @param[in] relative If true, move is relative to current screen position; if false, absolute.
    /// @return 7-byte Pelco-D formatted vector.
    [[nodiscard]] static std::vector<std::uint8_t> buildScreenMove(
        std::uint8_t address, std::int8_t panPercent, std::int8_t tiltPercent, bool relative = false);

    // Version Information Macro (opcode 0x73)
    /// @brief Requests software application version from device (opcode 0x73, sub 0x00).
    /// @param[in] address Device bus address (1-255).
    /// @return 7-byte Pelco-D formatted vector.
    [[nodiscard]] static std::vector<std::uint8_t> buildQuerySoftwareVersion(std::uint8_t address);

    /// @brief Requests build number from device (opcode 0x73, sub 0x02).
    /// @param[in] address Device bus address (1-255).
    /// @return 7-byte Pelco-D formatted vector.
    [[nodiscard]] static std::vector<std::uint8_t> buildQueryBuildNumber(std::uint8_t address);

    // Time Commands Macro (opcode 0x77)
    /// @brief Sets device clock seconds and synchronizes (opcode 0x77, sub 0x00).
    /// @param[in] address Device bus address (1-255).
    /// @param[in] seconds Seconds (0-59).
    /// @return 7-byte Pelco-D formatted vector.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetSeconds(std::uint8_t address, std::uint8_t seconds);

    /// @brief Sets device clock hour and minute (opcode 0x77, sub 0x02).
    /// @param[in] address Device bus address (1-255).
    /// @param[in] hour Hour in 24-hour format (0-23).
    /// @param[in] minute Minute (0-59).
    /// @return 7-byte Pelco-D formatted vector.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetHourMinute(
        std::uint8_t address, std::uint8_t hour, std::uint8_t minute);

    /// @brief Sets device calendar month and day (opcode 0x77, sub 0x04).
    /// @param[in] address Device bus address (1-255).
    /// @param[in] month Month (1-12).
    /// @param[in] day Day of month (1-31).
    /// @return 7-byte Pelco-D formatted vector.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetMonthDay(
        std::uint8_t address, std::uint8_t month, std::uint8_t day);

    /// @brief Sets device calendar year (opcode 0x77, sub 0x06).
    /// @param[in] address Device bus address (1-255).
    /// @param[in] year Full year (e.g. 2026).
    /// @return 7-byte Pelco-D formatted vector.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetYear(std::uint8_t address, std::uint16_t year);

    /// @brief Requests time or date component from device (opcode 0x77, odd sub-opcodes).
    /// @param[in] address Device bus address (1-255).
    /// @param[in] queryType ReportSeconds (0x01), ReportHourMinute (0x03), ReportMonthDay (0x05), or ReportYear (0x07).
    /// @return 7-byte Pelco-D formatted vector.
    [[nodiscard]] static std::vector<std::uint8_t> buildQueryTime(std::uint8_t address, TimeSubOpcode queryType);
};

} // namespace PelcoD
