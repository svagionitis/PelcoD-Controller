#pragma once

/// @file MockLensDriver.h
/// @brief In-memory mock lens driver implementing ILensDriver for deterministic simulation and unit testing.

#include <cstddef>
#include <cstdint>

#include <mutex>
#include <vector>

#include "ILensDriver.h"
#include "LensTypes.h"

namespace LensControl {

/// @class MockLensDriver
/// @brief Deterministic, zero-hardware mock implementation of ILensDriver.
class MockLensDriver final : public ILensDriver {
public:
    /// @brief Constructs a mock lens driver with default travel bounds.
    /// @param ranges Configurable mechanical limit boundaries.
    explicit MockLensDriver(LensRanges ranges = { }) noexcept;
    ~MockLensDriver() override = default;

    MockLensDriver(const MockLensDriver&) = delete;
    MockLensDriver& operator=(const MockLensDriver&) = delete;
    MockLensDriver(MockLensDriver&&) = delete;
    MockLensDriver& operator=(MockLensDriver&&) = delete;

    // --- ILensDriver Interface Methods ---

    [[nodiscard]] bool initialize(std::uint8_t port, std::uint32_t baud) noexcept override;
    void moveFocus(std::int16_t steps, MotorSpeed speed) noexcept override;
    void moveZoom(std::int16_t steps, MotorSpeed speed) noexcept override;
    void setFocusPosition(MotorPosition pos, MotorSpeed speed) noexcept override;
    void setZoomPosition(MotorPosition pos, MotorSpeed speed) noexcept override;
    void stopMotion() noexcept override;
    [[nodiscard]] MotorPosition currentFocus() const noexcept override;
    [[nodiscard]] MotorPosition currentZoom() const noexcept override;
    [[nodiscard]] std::uint8_t limitFlags() const noexcept override;
    void sendRawCommand(const std::uint8_t* data, std::size_t len) noexcept override;
    [[nodiscard]] bool hasBuiltinAutofocus() const noexcept override;
    bool triggerBuiltinAutofocus() noexcept override;
    [[nodiscard]] bool hasBuiltinZoomTracking() const noexcept override;
    bool setBuiltinZoomTracking(bool enable) noexcept override;
    [[nodiscard]] LensRanges lensRanges() const noexcept override;

    // --- Test Inspection & Mock Configuration Methods ---

    /// @brief Overrides travel boundary limits.
    void setRanges(const LensRanges& ranges) noexcept;

    /// @brief Configures whether native hardware autofocus is simulated.
    void setBuiltinAfSupported(bool supported) noexcept;

    /// @brief Configures whether native parfocal zoom-tracking is simulated.
    void setBuiltinZoomSupported(bool supported) noexcept;

    /// @brief Checks whether stopMotion was commanded.
    [[nodiscard]] bool isMotionStopped() const noexcept;

    /// @brief Returns bound COM port index.
    [[nodiscard]] std::uint8_t boundPort() const noexcept;

    /// @brief Returns configured serial baud rate.
    [[nodiscard]] std::uint32_t baudRate() const noexcept;

    /// @brief Returns last commanded focus velocity.
    [[nodiscard]] MotorSpeed lastFocusSpeed() const noexcept;

    /// @brief Returns last commanded zoom velocity.
    [[nodiscard]] MotorSpeed lastZoomSpeed() const noexcept;

    /// @brief Returns a copy of received raw serial bytes for assertions.
    [[nodiscard]] std::vector<std::uint8_t> rawCommands() const;

    /// @brief Clears logged raw serial command buffer.
    void clearRawCommands() noexcept;

private:
    void updateFocusLimitsLocked() noexcept;
    void updateZoomLimitsLocked() noexcept;

    mutable std::mutex m_mutex { };
    LensRanges m_ranges { };
    MotorPosition m_focusPos { 0U };
    MotorPosition m_zoomPos { 0U };
    MotorSpeed m_lastFocusSpeed { 0U };
    MotorSpeed m_lastZoomSpeed { 0U };
    std::uint8_t m_limitFlags { 0U };
    std::uint8_t m_boundPort { 0xFFU };
    std::uint32_t m_baudRate { 0U };
    bool m_motionStopped { true };
    bool m_builtinAfSupported { false };
    bool m_builtinZoomTrackSupported { false };
    bool m_builtinAfTriggered { false };
    std::vector<std::uint8_t> m_rawBuffer { };
};

} // namespace LensControl
