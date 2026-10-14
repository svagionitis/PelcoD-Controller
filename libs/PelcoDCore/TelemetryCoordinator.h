#pragma once

/// @file TelemetryCoordinator.h
/// @brief Thread-safe manager for device telemetry, status model, and background polling.

#include "DeviceStatus.h"
#include "PelcoDTypes.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace PelcoD {

/// @class TelemetryCoordinator
/// @brief Manages device state model, thread-safe snapshots, and status updates from incoming frames.
/// @details Encapsulates DeviceStatus and DeviceInfo behind dedicated synchronization locks, ensuring
///          atomic updates and preventing data races with the command execution strand.
class TelemetryCoordinator final {
public:
    TelemetryCoordinator();
    ~TelemetryCoordinator();

    TelemetryCoordinator(const TelemetryCoordinator&) = delete;
    TelemetryCoordinator& operator=(const TelemetryCoordinator&) = delete;
    TelemetryCoordinator(TelemetryCoordinator&&) = delete;
    TelemetryCoordinator& operator=(TelemetryCoordinator&&) = delete;

    /// @brief Retrieves a thread-safe snapshot copy of current device status.
    /// @return Current status snapshot.
    [[nodiscard]] DeviceStatus getStatus() const;

    /// @brief Retrieves a thread-safe snapshot copy of device identification.
    /// @return Current identification snapshot.
    [[nodiscard]] DeviceInfo getInfo() const;

    /// @brief Sets device serial number in identification model.
    /// @param[in] serial Serial number string.
    void setSerialNumber(const std::string& serial);

    /// @brief Synchronizes link connection state.
    /// @param[in] connected True if transport link is operational.
    void setConnected(bool connected) noexcept;

    /// @brief Updates device address in status model.
    /// @param[in] address Logical bus address (1-31).
    void setAddress(std::uint8_t address) noexcept;

    /// @brief Configures optical speeds.
    /// @param[in] zoomSpeed Zoom speed (0-3).
    /// @param[in] focusSpeed Focus speed (0-3).
    void setSpeeds(std::uint8_t zoomSpeed, std::uint8_t focusSpeed) noexcept;

    /// @brief Updates active preset tracking.
    /// @param[in] preset Active preset ID (0 = none/cleared).
    void setActivePreset(std::uint8_t preset) noexcept;

    /// @brief Updates absolute focus position coordinate.
    /// @param[in] pos Focus position.
    void setFocusPosition(std::uint16_t pos) noexcept;

    /// @brief Updates absolute iris position coordinate.
    /// @param[in] pos Iris position.
    void setIrisPosition(std::uint16_t pos) noexcept;

    /// @brief Updates limit angle coordinate.
    /// @param[in] isLeft True for left pan limit, false for right pan limit.
    /// @param[in] centidegrees Limit angle in hundredths of degrees.
    void setPanLimit(bool isLeft, std::uint16_t centidegrees) noexcept;

    /// @brief Updates scan limit angle coordinate.
    /// @param[in] isLeft True for scan left limit, false for scan right limit.
    /// @param[in] centidegrees Limit angle in hundredths of degrees.
    void setScanLimit(bool isLeft, std::uint16_t centidegrees) noexcept;

    /// @brief Configures auto focus mode.
    /// @param[in] mode Auto focus mode.
    void setAutoFocus(AutoMode mode) noexcept;

    /// @brief Configures auto iris mode.
    /// @param[in] mode Auto iris mode.
    void setAutoIris(AutoMode mode) noexcept;

    /// @brief Configures AGC mode.
    /// @param[in] mode AGC mode.
    void setAgc(AutoMode mode) noexcept;

    /// @brief Configures backlight compensation switch state.
    /// @param[in] state Backlight state.
    void setBacklightComp(SwitchState state) noexcept;

    /// @brief Configures auto white balance switch state.
    /// @param[in] state White balance state.
    void setAutoWhiteBalance(SwitchState state) noexcept;

    /// @brief Updates status and info from an incoming Pelco-D response frame.
    /// @param[in] frame Raw incoming frame.
    /// @param[in] limitId Optional limit identifier for Everest Limit responses.
    /// @return True if frame was recognized and applied to status.
    bool updateFromFrame(const std::vector<std::uint8_t>& frame, std::optional<EverestLimitId> limitId = std::nullopt);

    /// @brief Configures background telemetry polling state.
    /// @param[in] enable Whether to poll device telemetry periodically.
    /// @param[in] intervalMs Polling period in milliseconds.
    void setPolling(bool enable, std::uint32_t intervalMs = 1000U) noexcept;

    /// @brief Queries whether telemetry polling is active.
    /// @return True if polling is enabled.
    [[nodiscard]] bool isPolling() const noexcept;

    /// @brief Queries the configured polling interval.
    /// @return Polling interval in milliseconds.
    [[nodiscard]] std::uint32_t getPollIntervalMs() const noexcept;

private:
    mutable std::mutex m_mutex;
    DeviceStatus m_status {};
    DeviceInfo m_info {};

    std::atomic<bool> m_polling { false };
    std::atomic<std::uint32_t> m_pollIntervalMs { 1000U };
};

} // namespace PelcoD
