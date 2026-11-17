#pragma once

/// @file LensController.h
/// @brief Master multi-channel motorized optical controller and subsystem orchestrator.

#include <cstddef>
#include <cstdint>

#include <array>
#include <memory>
#include <mutex>
#include <string_view>

#include "ActuatorPidController.h"
#include "AfHeuristicsTypes.h"
#include "AutoFocusEngine.h"
#include "FocusEvaluator.h"
#include "ILensDriver.h"
#include "LensDriverLoader.h"
#include "LensTypes.h"
#include "MotionControlTypes.h"
#include "MotionGatedAfManager.h"
#include "MotionProfiler.h"
#include "MultiZoneFocusEvaluator.h"
#include "OpticalGeometryCalculator.h"
#include "ParfocalFeedforwardController.h"
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

    /// @brief Serializes calibration track curves to file for designated channel.
    /// @param cam Channel index [0..3].
    /// @param filename File path to save calibration curve table.
    /// @return True on successful serialization and write.
    [[nodiscard]] bool saveTrackCurves(CameraIndex cam, std::string_view filename) const noexcept;

    /// @brief Queries whether real-time zoom tracking is currently active.
    /// @param cam Channel index [0..3].
    /// @return True if zoom tracking is enabled.
    [[nodiscard]] bool isZoomTrackEnabled(CameraIndex cam) const noexcept;

    /// @brief Provides access to the parfocal zoom-track engine for designated channel.
    /// @param cam Channel index [0..3].
    /// @return Pointer to ZoomTrackFocusEngine, or nullptr if channel invalid.
    [[nodiscard]] ZoomTrackFocusEngine* zoomTrackEngine(CameraIndex cam) noexcept;

    /// @brief Provides const access to the parfocal zoom-track engine for designated channel.
    /// @param cam Channel index [0..3].
    /// @return Const pointer to ZoomTrackFocusEngine, or nullptr if channel invalid.
    [[nodiscard]] const ZoomTrackFocusEngine* zoomTrackEngine(CameraIndex cam) const noexcept;

    // --- Actuator Dynamics & Motion Control ---

    /// @brief Enables or disables parfocal zoom-velocity derivative feedforward.
    /// @param cam Channel index [0..3].
    /// @param enable True to engage dynamic feedforward focus velocity calculation.
    void setFeedforwardEnabled(CameraIndex cam, bool enable) noexcept;

    /// @brief Queries whether dynamic parfocal feedforward is enabled.
    /// @param cam Channel index [0..3].
    /// @return True if feedforward is active.
    [[nodiscard]] bool isFeedforwardEnabled(CameraIndex cam) const noexcept;

    /// @brief Provides access to the parfocal feedforward controller.
    /// @param cam Channel index [0..3].
    /// @return Pointer to ParfocalFeedforwardController, or nullptr if invalid.
    [[nodiscard]] ParfocalFeedforwardController* feedforwardController(CameraIndex cam) noexcept;

    /// @brief Provides const access to the parfocal feedforward controller.
    /// @param cam Channel index [0..3].
    /// @return Const pointer to ParfocalFeedforwardController, or nullptr if invalid.
    [[nodiscard]] const ParfocalFeedforwardController* feedforwardController(CameraIndex cam) const noexcept;

    /// @brief Provides access to the focus axis 7-phase S-curve motion profiler.
    /// @param cam Channel index [0..3].
    /// @return Pointer to MotionProfiler, or nullptr if invalid.
    [[nodiscard]] MotionProfiler* focusProfiler(CameraIndex cam) noexcept;

    /// @brief Provides const access to the focus axis 7-phase S-curve motion profiler.
    /// @param cam Channel index [0..3].
    /// @return Const pointer to MotionProfiler, or nullptr if invalid.
    [[nodiscard]] const MotionProfiler* focusProfiler(CameraIndex cam) const noexcept;

    /// @brief Provides access to the focus axis closed-loop PID controller.
    /// @param cam Channel index [0..3].
    /// @return Pointer to ActuatorPidController, or nullptr if invalid.
    [[nodiscard]] ActuatorPidController* focusPid(CameraIndex cam) noexcept;

    /// @brief Provides const access to the focus axis closed-loop PID controller.
    /// @param cam Channel index [0..3].
    /// @return Const pointer to ActuatorPidController, or nullptr if invalid.
    [[nodiscard]] const ActuatorPidController* focusPid(CameraIndex cam) const noexcept;

    // --- Intelligent AF Heuristics & Sensor Integration ---

    /// @brief Ingests platform pan and tilt angular rates for CAF motion gating.
    /// @param cam Channel index [0..3].
    /// @param panRate Pan rate in deg/s.
    /// @param tiltRate Tilt rate in deg/s.
    /// @param dtSec Elapsed time in seconds.
    void setPlatformRates(
        CameraIndex cam, AngularVelocityDegPerSec panRate, AngularVelocityDegPerSec tiltRate, float dtSec) noexcept;

    /// @brief Returns the motion-gated AF manager for channel.
    /// @param cam Channel index [0..3].
    /// @return Pointer to MotionGatedAfManager, or nullptr if invalid.
    [[nodiscard]] MotionGatedAfManager* motionGatedAf(CameraIndex cam) noexcept;

    /// @brief Returns const access to motion-gated AF manager for channel.
    /// @param cam Channel index [0..3].
    /// @return Const pointer to MotionGatedAfManager, or nullptr if invalid.
    [[nodiscard]] const MotionGatedAfManager* motionGatedAf(CameraIndex cam) const noexcept;

    /// @brief Returns optical geometry calculator for channel.
    /// @param cam Channel index [0..3].
    /// @return Pointer to OpticalGeometryCalculator, or nullptr if invalid.
    [[nodiscard]] OpticalGeometryCalculator* opticalCalculator(CameraIndex cam) noexcept;

    /// @brief Returns const access to optical geometry calculator for channel.
    /// @param cam Channel index [0..3].
    /// @return Const pointer to OpticalGeometryCalculator, or nullptr if invalid.
    [[nodiscard]] const OpticalGeometryCalculator* opticalCalculator(CameraIndex cam) const noexcept;

    /// @brief Returns multi-zone focus evaluator for channel.
    /// @param cam Channel index [0..3].
    /// @return Pointer to MultiZoneFocusEvaluator, or nullptr if invalid.
    [[nodiscard]] MultiZoneFocusEvaluator* multiZoneEvaluator(CameraIndex cam) noexcept;

    /// @brief Returns const access to multi-zone focus evaluator for channel.
    /// @param cam Channel index [0..3].
    /// @return Const pointer to MultiZoneFocusEvaluator, or nullptr if invalid.
    [[nodiscard]] const MultiZoneFocusEvaluator* multiZoneEvaluator(CameraIndex cam) const noexcept;

    /// @brief Evaluates 3x3 spatial focus matrix across a video frame for channel.
    /// @param cam Channel index [0..3].
    /// @param grayPixels Pointer to 8-bit luma pixels.
    /// @param width Frame width in pixels.
    /// @param height Frame height in pixels.
    /// @param stride Row stride in bytes.
    /// @return MultiZoneResult containing 9 zone scores and arbitrated winner.
    [[nodiscard]] MultiZoneResult evaluateMultiZone(CameraIndex cam, const std::uint8_t* grayPixels, std::size_t width,
        std::size_t height, std::size_t stride = 0U) noexcept;

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

    /// @brief Sets the focus evaluation sharpness metric for designated channel.
    /// @param cam Channel index [0..3].
    /// @param metric Spatial or transform focus metric kernel.
    void setFocusMetric(CameraIndex cam, FocusMetricType metric) noexcept;

    /// @brief Sets the fractional region of interest for designated channel.
    /// @param cam Channel index [0..3].
    /// @param roi Normalized fractional coordinates in [0..255].
    void setFocusRoi(CameraIndex cam, NormalizedRoi roi) noexcept;

    /// @brief Enables or disables continuous autofocus defocus monitoring.
    /// @param cam Channel index [0..3].
    /// @param enable True to monitor scene sharpness continuously.
    /// @param thresh Fractional metric drop triggering automatic refocus.
    void setCafEnabled(CameraIndex cam, bool enable, float thresh = 0.25F) noexcept;

    /// @brief Sets the autofocus algorithm configuration for designated channel.
    /// @param cam Channel index [0..3].
    /// @param cfg Calibrated autofocus parameters.
    void setAutoFocusConfig(CameraIndex cam, const AutoFocusConfig& cfg) noexcept;

    /// @brief Retrieves the active autofocus state machine phase for channel.
    /// @param cam Channel index [0..3].
    /// @return Current AutoFocusState value.
    [[nodiscard]] AutoFocusState getAutoFocusState(CameraIndex cam) const noexcept;

    /// @brief Retrieves the latest peak fitting mathematical result for channel.
    /// @param cam Channel index [0..3].
    /// @return PeakFitResult structure.
    [[nodiscard]] PeakFitResult getPeakFitResult(CameraIndex cam) const noexcept;

    /// @brief Returns the count of recorded focus samples from latest run.
    /// @param cam Channel index [0..3].
    /// @return Count of valid samples in internal buffer.
    [[nodiscard]] std::size_t getAfSampleCount(CameraIndex cam) const noexcept;

    /// @brief Retrieves a recorded focus sweep sample point by index.
    /// @param cam Channel index [0..3].
    /// @param idx Sample index [0..count-1].
    /// @return SamplePoint structure.
    [[nodiscard]] SamplePoint getAfSample(CameraIndex cam, std::size_t idx) const noexcept;

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
        std::unique_ptr<ParfocalFeedforwardController> feedforwardCtrl { nullptr };
        std::unique_ptr<MotionProfiler> focusProfiler { nullptr };
        std::unique_ptr<ActuatorPidController> focusPid { nullptr };
        std::unique_ptr<MotionGatedAfManager> motionGatedAf { nullptr };
        std::unique_ptr<OpticalGeometryCalculator> opticalCalc { nullptr };
        std::unique_ptr<MultiZoneFocusEvaluator> multiZoneEval { nullptr };
        FocusEvaluator evaluator { };
        LensMode mode { LensMode::ManualJog };
        LensRanges ranges { 0U, 65535U, 0U, 65535U };
        MotorSpeed zoomSpeed { MotorSpeed::kDefaultSpeed };
        MotorSpeed focusSpeed { MotorSpeed::kDefaultSpeed };
        LensStatus status { };
        FocusMetricScore lastScore { };
        bool zoomTrackEnabled { false };
        bool feedforwardEnabled { false };
        MotorPosition lastZoomPos { 0U };
        mutable std::mutex mutex { };
    };

    [[nodiscard]] bool isValidChannel(CameraIndex cam) const noexcept;
    void refreshChannelEnginesLocked(Channel& ch) noexcept;

    std::array<Channel, kMaxChannels> m_channels { };
    LensDriverLoader m_driverLoader { };
};

} // namespace LensControl
