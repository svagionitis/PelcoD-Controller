#pragma once

/// @file LensController.h
/// @brief Master multi-channel motorized optical controller and subsystem orchestrator.

#include <cstddef>
#include <cstdint>

#include <array>
#include <memory>
#include <mutex>
#include <string_view>

#include "AutoFocusEngine.h"
#include "FocusEvaluator.h"
#include "ILensDriver.h"
#include "LensDriverLoader.h"
#include "LensTypes.h"
#include "ZoomTrackFocusEngine.h"

namespace LensControl {

/// @class LensController
/// @brief Central coordinator managing multi-channel optics, limit guards, AF, and parfocal tracking.
class LensController final {
public:
    static constexpr std::size_t kMaxChannels = 4U;

    LensController() noexcept;
    ~LensController() noexcept;

    LensController(const LensController&) = delete;
    LensController& operator=(const LensController&) = delete;
    LensController(LensController&&) = delete;
    LensController& operator=(LensController&&) = delete;

    // --- Mode & Boundaries ---

    /// @brief Configures operational mode for designated camera channel.
    /// @param cam Channel index [0..3].
    /// @param mode Desired optical operating mode.
    /// @param data Optional mode-specific parameter.
    void setLensMode(CameraIndex cam, LensMode mode, std::uint16_t data = 0U) noexcept;

    /// @brief Establishes mechanical travel boundaries for designated channel.
    /// @param cam Channel index [0..3].
    /// @param ranges Range limits for focus and zoom axes.
    void setLensRanges(CameraIndex cam, const LensRanges& ranges) noexcept;

    /// @brief Configures default zoom actuator travel speed.
    /// @param cam Channel index [0..3].
    /// @param speed Velocity count [0..255].
    void setZoomSpeed(CameraIndex cam, MotorSpeed speed) noexcept;

    /// @brief Configures default focus actuator travel speed.
    /// @param cam Channel index [0..3].
    /// @param speed Velocity count [0..255].
    void setFocusSpeed(CameraIndex cam, MotorSpeed speed) noexcept;

    // --- Motor Movement & Limits ---

    /// @brief Commands relative focus step displacement.
    /// @param cam Channel index [0..3].
    /// @param steps Relative encoder offset (negative = Near, positive = Far).
    void moveFocusStep(CameraIndex cam, std::int16_t steps) noexcept;

    /// @brief Commands relative zoom step displacement.
    /// @param cam Channel index [0..3].
    /// @param steps Relative encoder offset (negative = Wide, positive = Tele).
    void moveZoomStep(CameraIndex cam, std::int16_t steps) noexcept;

    /// @brief Commands absolute focus positioning with range clamping.
    /// @param cam Channel index [0..3].
    /// @param pos Target focus coordinate.
    void setFocusTarget(CameraIndex cam, MotorPosition pos) noexcept;

    /// @brief Commands absolute zoom positioning with range clamping.
    /// @param cam Channel index [0..3].
    /// @param pos Target zoom coordinate.
    void setZoomTarget(CameraIndex cam, MotorPosition pos) noexcept;

    /// @brief Immediately halts all optical motion across designated channel.
    /// @param cam Channel index [0..3].
    void stopMotion(CameraIndex cam) noexcept;

    // --- Autofocus & Parfocal Tracking ---

    /// @brief Initiates closed-loop autofocus on designated channel.
    /// @param cam Channel index [0..3].
    /// @param method Algorithmic search variant.
    void triggerAutoFocus(CameraIndex cam, AutoFocusMethod method) noexcept;

    /// @brief Aborts active autofocus operation on designated channel.
    /// @param cam Channel index [0..3].
    void stopAutoFocus(CameraIndex cam) noexcept;

    /// @brief Enables or disables real-time parfocal zoom-to-focus tracking.
    /// @param cam Channel index [0..3].
    /// @param enable True to engage parfocal focus slew during zoom.
    void setZoomTrackFocus(CameraIndex cam, bool enable) noexcept;

    /// @brief Loads calibration track curve file for designated channel.
    /// @param cam Channel index [0..3].
    /// @param filename File path to calibration curve table.
    /// @return True on successful parsing and ingestion.
    [[nodiscard]] bool loadTrackCurves(CameraIndex cam, std::string_view filename) noexcept;

    /// @brief Ingests active tracking target bounding box for dynamic AF ROI centering.
    /// @param cam Channel index [0..3].
    /// @param normX Target center X [0.0, 1.0].
    /// @param normY Target center Y [0.0, 1.0].
    /// @param normW Target width [0.0, 1.0].
    /// @param normH Target height [0.0, 1.0].
    /// @param margin Padding factor around bounding box.
    void setTrackerRoi(
        CameraIndex cam, float normX, float normY, float normW, float normH, float margin = 0.2F) noexcept;

    /// @brief Clears dynamic tracker ROI and restores center evaluation window.
    /// @param cam Channel index [0..3].
    void clearTrackerRoi(CameraIndex cam) noexcept;

    // --- Video Frame Processing (0-Heap) ---

    /// @brief Ingests 8-bit luma video frame for autofocus sharpness evaluation.
    /// @param cam Channel index [0..3].
    /// @param luma Pointer to 8-bit grayscale pixel buffer.
    /// @param width Frame width in pixels.
    /// @param height Frame height in pixels.
    /// @param timestampNs Acquisition timestamp in nanoseconds.
    /// @param frameId Monotonically increasing sequence ID.
    void processFrame(CameraIndex cam, const std::uint8_t* luma, std::size_t width, std::size_t height,
        std::uint64_t timestampNs, std::uint32_t frameId) noexcept;

    // --- Control Loop Tick ---

    /// @brief Advances channel state machine, steps AF, and executes parfocal slew.
    /// @param cam Channel index [0..3].
    void update(CameraIndex cam) noexcept;

    // --- Telemetry & Status ---

    /// @brief Retrieves latest operational status record for channel.
    /// @param cam Channel index [0..3].
    /// @return Cached LensStatus snapshot.
    [[nodiscard]] LensStatus getLensStatus(CameraIndex cam) const noexcept;

    /// @brief Retrieves configured travel ranges for channel.
    /// @param cam Channel index [0..3].
    /// @return LensRanges struct.
    [[nodiscard]] LensRanges getLensRanges(CameraIndex cam) const noexcept;

    /// @brief Queries whether autofocus is actively running on channel.
    /// @param cam Channel index [0..3].
    /// @return True if AF search state machine is in progress.
    [[nodiscard]] bool isAutoFocusRunning(CameraIndex cam) const noexcept;

    /// @brief Retrieves the latest focus evaluation score for channel.
    /// @param cam Channel index [0..3].
    /// @return FocusMetricScore record.
    [[nodiscard]] FocusMetricScore getLastMetricScore(CameraIndex cam) const noexcept;

    // --- Hardware Driver Binding & Dynamic Loading ---

    /// @brief Explicitly assigns a pre-instantiated driver to channel.
    /// @param cam Channel index [0..3].
    /// @param driver Unique pointer to hardware driver instance.
    void setDriver(CameraIndex cam, std::unique_ptr<ILensDriver> driver) noexcept;

    /// @brief Loads an external dynamic lens driver DLL/SO and binds to channel.
    /// @param cam Channel index [0..3].
    /// @param dllName Shared library name or path.
    /// @param port UART / COM port index.
    /// @param baud Communication baud rate in bps.
    /// @return Status code indicating success or error reason.
    [[nodiscard]] LensLoadResult loadLensDll(
        CameraIndex cam, std::string_view dllName, std::uint8_t port, std::uint32_t baud) noexcept;

    /// @brief Unloads dynamic driver and releases bound communication port.
    /// @param cam Channel index [0..3].
    void unloadLensDll(CameraIndex cam) noexcept;

private:
    struct Channel {
        std::unique_ptr<ILensDriver> driver { nullptr };
        std::unique_ptr<AutoFocusEngine> afEngine { nullptr };
        std::unique_ptr<ZoomTrackFocusEngine> zoomTrackEngine { nullptr };
        FocusEvaluator evaluator { };
        LensMode mode { LensMode::ManualJog };
        LensRanges ranges { 0U, 65535U, 0U, 65535U };
        MotorSpeed zoomSpeed { MotorSpeed::kDefaultSpeed };
        MotorSpeed focusSpeed { MotorSpeed::kDefaultSpeed };
        LensStatus status { };
        FocusMetricScore lastScore { };
        bool zoomTrackEnabled { false };
        MotorPosition lastZoomPos { 0U };
        mutable std::mutex mutex { };
    };

    [[nodiscard]] bool isValidChannel(CameraIndex cam) const noexcept;
    void refreshChannelEnginesLocked(Channel& ch) noexcept;

    std::array<Channel, kMaxChannels> m_channels { };
    LensDriverLoader m_driverLoader { };
};

} // namespace LensControl
