#pragma once

#include "SonyCameraModel.h"
#include "SonyViscaBuilder.h"
#include "SonyViscaParser.h"
#include "SonyViscaTypes.h"
#include <ViscaDevice.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>

namespace Visca::Sony {

/// @class SonyFCBDevice
/// @brief High-level device controller for Sony FCB-EV9520L and FCB-EW9500H cameras.
/// @details Automatically probes camera model upon initialization, enforces hardware capability
/// gating (e.g. preventing distortion compensation on EW9500H or 4K on EV9520L),
/// and coordinates Block Inquiry polling for complete status telemetry.
class SonyFCBDevice {
public:
    /// @brief Constructs a SonyFCBDevice wrapping a VISCA device.
    /// @param[in] transport Underlying transport instance.
    /// @param[in] cameraAddress Camera address on bus (1..7).
    explicit SonyFCBDevice(std::shared_ptr<::Transport::ITransport> transport, uint8_t cameraAddress = 1);

    /// @brief Virtual destructor.
    virtual ~SonyFCBDevice() = default;

    /// @brief Initializes device communication, clears interface, and identifies model capabilities.
    /// @return True if camera responded to version inquiry and was successfully identified.
    bool initialize();

    /// @brief Retrieves the identified camera model type.
    [[nodiscard]] SonyCameraModelType modelType() const noexcept;

    /// @brief Retrieves the camera capabilities descriptor.
    [[nodiscard]] const CameraCapabilities& capabilities() const noexcept;

    /// @brief Retrieves current cached camera status telemetry.
    [[nodiscard]] SonyFCBStatus status() const noexcept;

    /// @brief Accesses the underlying VISCA device state machine.
    [[nodiscard]] ViscaDevice& device() noexcept
    {
        return m_device;
    }

    /// @brief Captures transport-layer and kernel-level communication statistics.
    /// @return Aggregated snapshot containing generic and kernel-level metrics.
    [[nodiscard]] ::Transport::TransportStatsSnapshot getTransportStats() const
    {
        return m_device.getTransportStats();
    }

    /// @brief Polls all Block Inquiries (00 to 04) to update status telemetry.
    /// @return True if all block inquiries were answered and parsed.
    bool pollStatus();

    // --- Lens & Optics Controls ---
    bool setZoomDirect(uint16_t position);
    bool zoomTele(uint8_t speed = 4);
    bool zoomWide(uint8_t speed = 4);
    bool zoomStop();

    bool setFocusAuto(bool autoMode);
    bool setFocusDirect(uint16_t position);
    bool focusFar(uint8_t speed = 4);
    bool focusNear(uint8_t speed = 4);
    bool focusStop();
    bool focusOnePush();
    bool setFocusNearLimit(uint16_t limit);

    // --- Exposure Controls ---
    bool setExposureMode(SonyExposureMode mode);
    bool setShutter(uint8_t position);
    bool setIris(uint8_t position);
    bool setGain(uint8_t position);
    bool setExposureCompensation(bool on, uint8_t position = 0);

    // --- White Balance ---
    bool setWhiteBalance(SonyWhiteBalanceMode mode);
    bool setRgaiDirect(uint8_t position);
    bool setBgainDirect(uint8_t position);
    bool triggerOnePushWb();

    // --- Image Processing & Enhancements ---
    bool setStabilizer(SonyStabilizerMode mode);
    bool setDefog(SonyDefogMode mode);
    bool setIcr(bool on);
    bool setAutoIcr(bool on);

    // --- Picture & Sharpness Controls ---
    bool setAperture(uint8_t level);
    bool apertureReset();
    bool apertureUp();
    bool apertureDown();

    // --- Exposure & Sensitivity Enhancements ---
    bool setBacklight(bool on);
    bool setAutoSlowShutter(bool on);
    bool setHighSensitivity(bool on);

    // --- Noise Reduction ---
    bool setNoiseReduction2D(uint8_t level);
    bool setNoiseReduction3D(uint8_t level);

    // --- Dynamic Range & Effects ---
    bool setWideD(SonyWideDMode mode);
    bool setFreeze(bool on);
    bool setPictureFlip(bool on);
    bool setLrReverse(bool on);

    // --- Color & Gamma Adjustments ---
    bool setColorGain(uint8_t gain);
    bool setColorHue(uint8_t hue);
    bool setChromaSuppress(uint8_t level);
    bool setGamma(uint8_t mode);

    // --- Preset Memory Controls ---
    bool memorySet(uint8_t channel);
    bool memoryRecall(uint8_t channel);
    bool memoryReset(uint8_t channel);

    // --- Spot Controls ---
    bool setSpotAe(bool on);
    bool setSpotAePosition(uint8_t x, uint8_t y);
    bool setSpotFocusPosition(uint8_t x, uint8_t y);
    bool setSpotAwbPosition(uint8_t x, uint8_t y);

    // --- Individual Fast-Path Parameter Queries ---

    /// @brief Queries current optical zoom position directly via CAM_ZoomPosInq.
    /// @return Current 16-bit zoom position, or std::nullopt on communication failure.
    [[nodiscard]] std::optional<uint16_t> queryZoomPosition();

    /// @brief Queries current focus position directly via CAM_FocusPosInq.
    /// @return Current 16-bit focus position, or std::nullopt on communication failure.
    [[nodiscard]] std::optional<uint16_t> queryFocusPosition();

    /// @brief Queries active exposure control mode directly via CAM_ExpModeInq.
    /// @return Active SonyExposureMode, or std::nullopt on communication failure.
    [[nodiscard]] std::optional<SonyExposureMode> queryExposureMode();

    /// @brief Queries current shutter speed position directly via CAM_ShutterPosInq.
    /// @return 8-bit shutter speed index, or std::nullopt on communication failure.
    [[nodiscard]] std::optional<uint8_t> queryShutterPosition();

    /// @brief Queries current iris step position directly via CAM_IrisPosInq.
    /// @return 8-bit iris step index, or std::nullopt on communication failure.
    [[nodiscard]] std::optional<uint8_t> queryIrisPosition();

    /// @brief Queries current analog gain position directly via CAM_GainPosInq.
    /// @return 8-bit gain step index, or std::nullopt on communication failure.
    [[nodiscard]] std::optional<uint8_t> queryGainPosition();

    /// @brief Queries current white balance mode directly via CAM_WBModeInq.
    /// @return Active SonyWhiteBalanceMode, or std::nullopt on communication failure.
    [[nodiscard]] std::optional<SonyWhiteBalanceMode> queryWhiteBalanceMode();

    /// @brief Queries current aperture sharpness level directly via CAM_ApertureInq.
    /// @return 8-bit aperture level, or std::nullopt on communication failure.
    [[nodiscard]] std::optional<uint8_t> queryAperture();

    /// @brief Queries last recalled preset memory channel directly via CAM_MemoryInq.
    /// @return Preset channel number (0..15), or std::nullopt on communication failure.
    [[nodiscard]] std::optional<uint8_t> queryLastMemoryChannel();

    // --- Hardware Gated Controls ---

    /// @brief Enables or disables lens distortion compensation (Register 0x57).
    /// @note Gated: Supported on FCB-EV9520L, NOT supported on FCB-EW9500H.
    /// @param[in] on True to enable, false to disable.
    /// @return True if command executed, false if unsupported by hardware.
    bool setDistortionCompensation(bool on);

    /// @brief Enables or disables optical axis gap compensation (Register 0x47).
    /// @note Gated: Supported on FCB-EV9520L, NOT supported on FCB-EW9500H.
    /// @param[in] on True to enable, false to disable.
    /// @return True if command executed, false if unsupported by hardware.
    bool setOpticalAxisGapCompensation(bool on);

    /// @brief Configures video operating mode / resolution (Register 0x72).
    /// @note Gated: Modes >= 0x25 are 4K UHD and only supported on FCB-EW9500H.
    /// @param[in] mode Register 0x72 operating mode code.
    /// @return True if command executed, false if unsupported mode requested.
    bool setOperatingMode(uint8_t mode);

    /// @brief Sets LVDS serial video output mode (Register 0x74).
    /// @note Gated: Supported on FCB-EV9520L, NOT supported on FCB-EW9500H.
    /// @param[in] mode Mode value.
    /// @return True if command executed, false if unsupported.
    bool setLvdsMode(uint8_t mode);

    /// @brief Sets TMDS / HDMI digital output mode (Register 0x60).
    /// @note Gated: Supported on FCB-EW9500H, NOT supported on FCB-EV9520L.
    /// @param[in] mode Mode value.
    /// @return True if command executed, false if unsupported.
    bool setDigitalOutputMode(uint8_t mode);

private:
    ViscaDevice m_device;
    SonyCameraModelType m_modelType { SonyCameraModelType::Unknown };
    CameraCapabilities m_capabilities {};

    mutable std::mutex m_statusMutex {};
    SonyFCBStatus m_status {};
};

} // namespace Visca::Sony
