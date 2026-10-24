#pragma once

/// @file StubThermalAdapter.h
/// @brief Adapter managing ONVIF Thermal and Radiometry service features.

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "OnvifServerTypes.h"
#include <PelcoDCore/PelcoDDevice.h>

namespace Onvif {

/// @class StubThermalAdapter
/// @brief Provider for ONVIF Thermal Service radiometry, false-color palettes, and NUC trigger.
/// @details Implements IThermalHandler per Interface Segregation Principle.
class StubThermalAdapter : public IThermalHandler {
public:
    /// @brief Constructs adapter with optional PelcoDDevice instance for hardware NUC execution.
    /// @param[in] device Shared pointer to PelcoDDevice instance, or nullptr for simulation.
    explicit StubThermalAdapter(std::shared_ptr<PelcoD::PelcoDDevice> device = nullptr);

    ~StubThermalAdapter() override = default;

    StubThermalAdapter(const StubThermalAdapter&) = delete;
    StubThermalAdapter& operator=(const StubThermalAdapter&) = delete;
    StubThermalAdapter(StubThermalAdapter&&) = delete;
    StubThermalAdapter& operator=(StubThermalAdapter&&) = delete;

    /// @brief Registers an event publisher callback for temperature alarms.
    /// @param[in] publisher Callback function.
    void setEventPublisher(EventCallback publisher);

    // =========================================================================
    // IThermalHandler Implementation
    // =========================================================================

    /// @brief Retrieves radiometric parameters for a video source.
    /// @param[in] videoSourceToken Video source token.
    /// @return RadiometryConfig structure.
    [[nodiscard]] RadiometryConfig handleGetRadiometryConfig(const std::string& videoSourceToken) override;

    /// @brief Updates radiometric compensation parameters.
    /// @param[in] videoSourceToken Video source token.
    /// @param[in] config Updated radiometry configuration.
    /// @return True on success.
    [[nodiscard]] bool handleSetRadiometryConfig(
        const std::string& videoSourceToken, const RadiometryConfig& config) override;

    /// @brief Retrieves radiometric spot meters.
    /// @param[in] videoSourceToken Video source token.
    /// @return Vector of RadiometrySpot.
    [[nodiscard]] std::vector<RadiometrySpot> handleGetRadiometrySpots(const std::string& videoSourceToken) override;

    /// @brief Updates or replaces radiometric spot meters.
    /// @param[in] videoSourceToken Video source token.
    /// @param[in] spots Vector of spots.
    /// @return True on success.
    [[nodiscard]] bool handleSetRadiometrySpots(
        const std::string& videoSourceToken, const std::vector<RadiometrySpot>& spots) override;

    /// @brief Retrieves radiometric measurement boxes.
    /// @param[in] videoSourceToken Video source token.
    /// @return Vector of RadiometryBox.
    [[nodiscard]] std::vector<RadiometryBox> handleGetRadiometryBoxes(const std::string& videoSourceToken) override;

    /// @brief Updates or replaces radiometric measurement boxes.
    /// @param[in] videoSourceToken Video source token.
    /// @param[in] boxes Vector of measurement boxes.
    /// @return True on success.
    [[nodiscard]] bool handleSetRadiometryBoxes(
        const std::string& videoSourceToken, const std::vector<RadiometryBox>& boxes) override;

    /// @brief Retrieves available thermal false-color palettes.
    /// @param[in] videoSourceToken Video source token.
    /// @return Vector of ColorPalette.
    [[nodiscard]] std::vector<ColorPalette> handleGetColorPalettes(const std::string& videoSourceToken) override;

    /// @brief Sets active thermal false-color palette.
    /// @param[in] videoSourceToken Video source token.
    /// @param[in] paletteToken Desired palette token.
    /// @return True on success.
    [[nodiscard]] bool handleSetColorPalette(
        const std::string& videoSourceToken, const std::string& paletteToken) override;

    /// @brief Triggers Non-Uniformity Correction (NUC / flat-field shutter calibration).
    /// @param[in] videoSourceToken Video source token.
    /// @return True on success.
    [[nodiscard]] bool handleTriggerNuc(const std::string& videoSourceToken) override;

private:
    std::shared_ptr<PelcoD::PelcoDDevice> m_device;
    mutable std::mutex m_mutex { };
    EventCallback m_eventPublisher { };
    RadiometryConfig m_radiometryConfig { };
    std::vector<RadiometrySpot> m_radiometrySpots { { "Spot_1", { 0.5f, 0.5f }, "Center Spot", 24.5f } };
    std::vector<RadiometryBox> m_radiometryBoxes { { "Box_1", { 0.2f, 0.2f }, { 0.8f, 0.8f }, "Central Target Zone",
        21.0f, 36.8f, 28.4f } };
    std::vector<RadiometryAlarmConfig> m_radiometryAlarms { { "Box_1", 50.0f, 2.0f,
        RadiometryAlarmType::HighTemperature, true } };
    std::vector<ColorPalette> m_colorPalettes { { "WhiteHot", "White Hot", true }, { "BlackHot", "Black Hot", false },
        { "Ironbow", "Ironbow", false }, { "Rainbow", "Rainbow", false }, { "Sepia", "Sepia", false },
        { "Fire", "Fire", false } };
    std::string m_activeColorPalette { "WhiteHot" };
};

} // namespace Onvif
