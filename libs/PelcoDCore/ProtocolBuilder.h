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

    [[nodiscard]] static std::vector<std::uint8_t> buildAutoFocus(std::uint8_t address, AutoMode mode);

    [[nodiscard]] static std::vector<std::uint8_t> buildAutoIris(std::uint8_t address, AutoMode mode);

    [[nodiscard]] static std::vector<std::uint8_t> buildAgc(std::uint8_t address, AutoMode mode);

    [[nodiscard]] static std::vector<std::uint8_t> buildBacklight(std::uint8_t address, SwitchState state);

    [[nodiscard]] static std::vector<std::uint8_t> buildWhiteBalance(std::uint8_t address, SwitchState state);

    [[nodiscard]] static std::vector<std::uint8_t> buildShutterSpeed(std::uint8_t address, std::uint16_t speed);

    [[nodiscard]] static std::vector<std::uint8_t> buildGain(std::uint8_t address, std::uint16_t gain);

    [[nodiscard]] static std::vector<std::uint8_t> buildAutoIrisLevel(std::uint8_t address, std::uint8_t level);

    [[nodiscard]] static std::vector<std::uint8_t> buildAutoIrisPeak(std::uint8_t address, std::uint8_t peak);

    // Absolute Positioning
    [[nodiscard]] static std::vector<std::uint8_t> buildSetPan(std::uint8_t address, std::uint16_t centidegrees);

    [[nodiscard]] static std::vector<std::uint8_t> buildSetTilt(std::uint8_t address, std::uint16_t centidegrees);

    [[nodiscard]] static std::vector<std::uint8_t> buildSetZoom(std::uint8_t address, std::uint16_t position);

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
};

} // namespace PelcoD
