#pragma once

/// @file SightlineNucWorkflow.h
/// @brief Operator-driven NUC/DPR calibration sequencer (no Qt).
/// @details Implements the procedures of docs/protocols/Sightline/EAN-NUC-and-DPR.pdf on top of
///          SightlineNucBuilder. The EAN procedures are prompt-driven (for example "point at the
///          hot source, then Add"), so each step is sent only when the operator calls next().
///          Board feedback (User Warnings) is advisory. The only verbatim completion message in
///          the EAN, "Noise stats calculation complete", triggers the 0xAF query automatically.
///
///          Every 0x35 overwrites nucShow and, depending on its length, deadReplace, numReplace
///          and destripe*. None of these has a "keep current" value. The workflow therefore
///          caches the board's last 0x35 reply and re-sends those values unchanged. Without
///          a cached reply it sends only the 28-byte base form (EAN Appendix A4).
///
///          ```
///          start(TwoPoint)
///            [0] ResetAll  --next-->  0x35 run=6
///            [1] AddFrames --next-->  0x35 run=1 numFrames=N   (hot source)
///            [2] AddFrames --next-->  0x35 run=1 numFrames=N   (cold source)
///            [3] Calc2Pt   --next-->  0x35 run=8 numFrames=0   -> Done
///          ```
///
///          ```mermaid
///          stateDiagram-v2
///              [*] --> Idle
///              Idle --> Running: start
///              Running --> Running: next (more steps)
///              Running --> Done: next (last step)
///              Running --> Aborted: abort
///              Running --> Failed: validation / send error
///              Done --> Running: start
///              Aborted --> Running: start
///              Failed --> Running: start
///          ```
///
///          Not thread-safe: call from a single thread (for example, the Qt GUI thread).
///
///          Multi-NUC (UNVERIFIED ON HARDWARE). The IDD (3.11) defines 0x35 nucName only as
///          "Name of NUC to add the frames to when creating multiple NUCs at once"; EAN 4.3 / 4.4
///          do not give the command sequence. The recipe below is an interpretation: frames
///          are added per name, then a 2-point calculation is run per name with nucName set.
///          Confirm against a board before relying on it.
///
///          ```
///          start(MultiNuc, names = {A, B})
///            [0] AddFrames run=1 N name=A   (COLD, lens at A)
///            [1] AddFrames run=1 N name=B   (COLD, lens at B)
///            [2] AddFrames run=1 N name=A   (HOT,  lens at A)
///            [3] AddFrames run=1 N name=B   (HOT,  lens at B)
///            [4] Calc2Pt   run=8 0 name=A
///            [5] Calc2Pt   run=8 0 name=B   -> Done
///          ```
///
///          ```mermaid
///          flowchart LR
///              C["Cold frames: each name"] --> H["Hot frames: each name"]
///              H --> K["2-point calc: each name"]
///              K --> D([Done])
///          ```

#include "SightlineGeneral.h"
#include "SightlineNuc.h"
#include "SightlineNucCaps.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Sightline {

/// @enum NucRecipe
/// @brief Multi-step calibration procedures.
enum class NucRecipe : std::uint8_t {
    TwoPoint = 0U, ///< EAN 3.4: reset, hot frames, cold frames, 2-point calculation
    OnePoint = 1U, ///< EAN 4.6 / A4: clear frames, add frames, 1-point calculation
    ShutterFlatten = 2U, ///< EAN 4.7 / 4.7.1: shutter flatten, optional "_shutter_only" save
    NoiseStats = 3U, ///< EAN 5.5: calculate 3D noise statistics, then read 0xAF
    MultiNuc = 4U ///< IDD 0x35 nucName (FW 3.11): one table per name; unverified on hardware
};

/// @brief Maximum number of names in one multi-NUC run.
inline constexpr std::size_t kMaxMultiNuc { 16U };

/// @enum NucStage
/// @brief Lifecycle of the current recipe.
enum class NucStage : std::uint8_t {
    Idle = 0U, ///< No recipe started yet
    Running = 1U, ///< A step is waiting for next()
    Done = 2U, ///< All steps sent
    Aborted = 3U, ///< Stopped by the operator
    Failed = 4U ///< Stopped by a validation or transport error (see lastError())
};

/// @struct NucOptions
/// @brief Recipe parameters.
struct NucOptions {
    std::uint8_t numFrames { 30U }; ///< Frames per capture step (EAN 3.4 default 30)
    std::string saveName {}; ///< ShutterFlatten only: "_shutter_only" table name (FW 3.11); empty = no save
    std::vector<std::string> names {}; ///< MultiNuc only: unique table names, 1..kMaxMultiNuc
};

/// @struct DprLimits
/// @brief Dead pixel calculation limits for 0x35 run 9.
/// @details Gain and offset defaults are the worked example in EAN section 3.5, which derives
///          them from 4 standard deviations of the gain/offset images. They depend on the
///          application and should be tuned per camera. maxNumDead has no documented default.
struct DprLimits {
    std::uint16_t minGain { 76U }; ///< Minimum gain, percent (0..999)
    std::uint16_t maxGain { 124U }; ///< Maximum gain, percent (0..999)
    std::uint16_t minVal { 0U }; ///< Minimum input pixel value (0..65535)
    std::uint16_t maxVal { 65535U }; ///< Maximum input pixel value (0..65535)
    std::int32_t minOff { -520 }; ///< Minimum offset (-999999..999999)
    std::int32_t maxOff { 520 }; ///< Maximum offset (-999999..999999)
    std::uint32_t maxStdDev { 65535U }; ///< Maximum standard deviation (<= 65535)
    std::int32_t maxNumDead { 0 }; ///< Maximum number of dead pixels
};

/// @struct DprReplace
/// @brief Dead pixel replacement and dynamic filter settings (0x35 bytes 32..36, FW 3.3).
struct DprReplace {
    DeadReplace method { DeadReplace::Nearest }; ///< Replacement method
    std::uint8_t numReplace { 5U }; ///< Neighbours for average / median (1..8)
    DeadFilter filter { DeadFilter::Ignore }; ///< Dynamic dead filter
    std::int16_t threshold { 64 }; ///< Dynamic dead filter threshold (0..255)
};

/// @brief Transmits one framed packet; returns false if it could not be sent.
using NucPacketSink = std::function<bool(const std::vector<std::uint8_t>&)>;

/// @class NucWorkflow
/// @brief Sequences NUC/DPR recipes and validated one-shot commands for one camera.
class NucWorkflow {
public:
    /// @brief Constructs an idle workflow.
    /// @param[in] sink Packet transmitter; an empty function makes every send fail (NotSent).
    explicit NucWorkflow(NucPacketSink sink);

    /// @brief Selects the target camera.
    /// @details Changing camera discards the cached board state.
    /// @param[in] cameraIndex Camera index.
    void setCamera(std::uint8_t cameraIndex) noexcept;

    /// @brief Returns the target camera.
    /// @return Camera index.
    [[nodiscard]] std::uint8_t camera() const noexcept;

    /// @brief Records the firmware version from a 0x40 reply.
    /// @param[in] ver Parsed Version Number message.
    void onVersion(const MsgVersionNumber& ver) noexcept;

    /// @brief Returns the recorded firmware version (0.0 if unknown).
    /// @return Firmware version.
    [[nodiscard]] FwVersion firmware() const noexcept;

    /// @brief Caches a 0x35 reply as the board's current settings.
    /// @details Replies for other cameras are ignored.
    /// @param[in] reply Parsed NucParameters message.
    void onNucReply(const MsgNucParameters& reply);

    /// @brief Returns true once a 0x35 reply for the target camera has been cached.
    /// @return True if the board settings are known.
    [[nodiscard]] bool hasBoardState() const noexcept;

    /// @brief Feeds a User Warning text into the workflow.
    /// @details Advances a NoiseStats recipe waiting for "Noise stats calculation complete".
    /// @param[in] text Warning message text.
    /// @return True if the warning advanced the workflow.
    [[nodiscard]] bool onWarning(const std::string& text);

    /// @brief Starts a recipe; nothing is sent until next().
    /// @param[in] recipe Procedure to run.
    /// @param[in] options Recipe parameters.
    /// @return NucError::Ok, Busy, Unsupported, StateUnknown (MultiNuc without a cached 0x35),
    ///         NameCount, or a name validation error.
    [[nodiscard]] NucError start(NucRecipe recipe, const NucOptions& options);

    /// @brief Sends the current step and advances.
    /// @return NucError::Ok, NotRunning, or the error that moved the stage to Failed.
    [[nodiscard]] NucError next();

    /// @brief Stops a running recipe without sending anything.
    /// @details Frames already added stay on the board until cleared (EAN A4).
    void abort() noexcept;

    /// @brief Returns the current stage.
    /// @return Stage.
    [[nodiscard]] NucStage stage() const noexcept;

    /// @brief Returns the most recently started recipe.
    /// @return Recipe.
    [[nodiscard]] NucRecipe recipe() const noexcept;

    /// @brief Returns the index of the step next() will send.
    /// @return Zero-based step index (equals stepCount() when done).
    [[nodiscard]] std::size_t stepIndex() const noexcept;

    /// @brief Returns the number of steps in the current recipe.
    /// @return Step count.
    [[nodiscard]] std::size_t stepCount() const noexcept;

    /// @brief Returns the operator instruction for the current state.
    /// @details The pointer stays valid until the next successful start().
    /// @return Null-terminated string; never null.
    [[nodiscard]] const char* prompt() const noexcept;

    /// @brief Returns the error that ended the last recipe (Ok if none).
    /// @return Last error.
    [[nodiscard]] NucError lastError() const noexcept;

    /// @brief Sets the video display mode (0x35 run 0).
    /// @param[in] show Display mode.
    /// @return NucError::Ok or the failure.
    [[nodiscard]] NucError setShow(NucShow show);

    /// @brief Calculates dead pixels (0x35 run 9), then queries the 0xA1 statistics.
    /// @param[in] limits Dead pixel limits.
    /// @return NucError::Ok or the failure.
    [[nodiscard]] NucError calcDead(const DprLimits& limits);

    /// @brief Recalculates only the replacement pixels (0x35 run 10).
    /// @return NucError::Ok or the failure.
    [[nodiscard]] NucError calcReplace();

    /// @brief Runs automatic single-frame dead pixel detection (0x35 run 11, FW 3.3).
    /// @return NucError::Ok or the failure.
    [[nodiscard]] NucError autoDead();

    /// @brief Sets the replacement method and dynamic filter (0x35 Dpr tail, FW 3.3).
    /// @param[in] rep Replacement settings.
    /// @return NucError::Ok or the failure.
    [[nodiscard]] NucError setReplace(const DprReplace& rep);

    /// @brief Sets NUC destripe (0x35 Destripe tail, FW 3.9); needs the cached board state.
    /// @param[in] amount 0 = off .. 255 = aggressive.
    /// @param[in] sections Vertical sections (default 1).
    /// @return NucError::Ok, StateUnknown, or another failure.
    [[nodiscard]] NucError setDestripe(std::uint8_t amount, std::uint8_t sections);

    /// @brief Sends a table save / load / interpolate / default command (0x36).
    /// @param[in] msg Command; cameraIndex is replaced by the target camera.
    /// @return NucError::Ok or the failure.
    [[nodiscard]] NucError tableOp(MsgReadWriteNuc msg);

    /// @brief Adds a pixel to the dead list (0xA8 mode 0, FW 3.3).
    /// @param[in] column Pixel column (x).
    /// @param[in] row Pixel row (y).
    /// @param[in] deferUpdate True if more pixels follow.
    /// @return NucError::Ok or the failure.
    [[nodiscard]] NucError addDeadPixel(std::uint16_t column, std::uint16_t row, bool deferUpdate);

    /// @brief Removes a manually added pixel from the dead list (0xA8 mode 1, FW 3.3).
    /// @param[in] column Pixel column (x).
    /// @param[in] row Pixel row (y).
    /// @param[in] deferUpdate True if more pixels follow.
    /// @return NucError::Ok or the failure.
    [[nodiscard]] NucError removeDeadPixel(std::uint16_t column, std::uint16_t row, bool deferUpdate);

    /// @brief Runs dynamic dead pixel detection (0xA8 mode 2, FW 3.3).
    /// @param[in] kernelSize Neighbourhood size; 0 = firmware default.
    /// @param[in] maxPixelDiff Threshold; 0 = automatic.
    /// @return NucError::Ok or the failure.
    [[nodiscard]] NucError dynamicDead(std::uint8_t kernelSize, std::uint8_t maxPixelDiff);

    /// @brief Queries 0x35, 0xA1 and the four 0x36 table names for the target camera.
    /// @return True if all six queries were sent.
    [[nodiscard]] bool queryState();

    /// @brief Queries the 3D noise statistics (0xAF).
    /// @return True if sent.
    [[nodiscard]] bool queryNoise();

private:
    /// @brief Action performed by one recipe step.
    enum class StepAction : std::uint8_t {
        ResetAll, ///< 0x35 run 6
        AddFrames, ///< 0x35 run 1, numFrames = options
        Calc2Point, ///< 0x35 run 8
        ClearFrames, ///< 0x35 run 2
        Calc1Point, ///< 0x35 run 7
        Flatten, ///< 0x35 run 12, numFrames = options
        SaveFlatten, ///< 0x36 op 7, fileName = options.saveName
        NoiseCalc, ///< 0x35 run 13, numFrames = options
        NoiseRead ///< 0x28 [0xAF, cam]
    };

    /// @brief One recipe step.
    struct Step {
        StepAction action { StepAction::ResetAll }; ///< What next() sends
        std::string prompt {}; ///< Operator instruction shown before sending
        std::string name {}; ///< 0x35 nucName for this step (MultiNuc only)
    };

    /// @brief Validates recipe preconditions and fills m_steps.
    /// @param[in] recipe Procedure.
    /// @param[in] options Parameters.
    /// @return NucError::Ok or the reason the recipe cannot run.
    [[nodiscard]] NucError planSteps(NucRecipe recipe, const NucOptions& options);

    /// @brief Validates multi-NUC preconditions and builds its steps.
    /// @param[in] names Table names.
    /// @param[out] steps Planned steps; filled only on success.
    /// @return NucError::Ok or the reason the recipe cannot run.
    [[nodiscard]] NucError planMulti(const std::vector<std::string>& names, std::vector<Step>& steps) const;

    /// @brief Executes one step.
    /// @param[in] step Step to send.
    /// @return NucError::Ok or the failure.
    [[nodiscard]] NucError runStep(const Step& step);

    /// @brief Sends a 0x35 run command built from the cached state.
    /// @param[in] run Run mode.
    /// @param[in] frames numFrames.
    /// @param[in] name nucName (empty for single-table recipes).
    /// @return NucError::Ok or the failure.
    [[nodiscard]] NucError sendRun(NucRunMode run, std::uint8_t frames, const std::string& name);

    /// @brief Validates, encodes with @p tail, sends, and updates the cached state.
    /// @param[in] msg Message to send.
    /// @param[in] tail Tail length.
    /// @return NucError::Ok or the failure.
    [[nodiscard]] NucError sendNuc(MsgNucParameters msg, NucTail tail);

    /// @brief Sends a 0xA8 command after validation.
    /// @param[in] msg Dead pixel command (cameraIndex is set here).
    /// @return NucError::Ok or the failure.
    [[nodiscard]] NucError sendDead(MsgDeadPixel msg);

    /// @brief Returns the cached state prepared for a new command.
    /// @details Camera set, run None, numFrames 0, DPR limits zeroed, nucName cleared.
    /// @return Base message.
    [[nodiscard]] MsgNucParameters baseMsg() const;

    /// @brief Tail for run commands: maxNucTail(fw) with a known state, otherwise Base.
    /// @return Tail.
    [[nodiscard]] NucTail runTail() const noexcept;

    /// @brief Forwards a packet to the sink.
    /// @param[in] packet Framed packet.
    /// @return True if sent.
    [[nodiscard]] bool send(const std::vector<std::uint8_t>& packet);

    NucPacketSink m_sink {}; ///< Transport
    FwVersion m_fw {}; ///< Reported firmware (0.0 = unknown)
    MsgNucParameters m_state {}; ///< Last known board 0x35 settings
    bool m_hasState { false }; ///< True once m_state came from the board
    std::uint8_t m_camera { 0U }; ///< Target camera
    NucRecipe m_recipe { NucRecipe::TwoPoint }; ///< Current / last recipe
    NucStage m_stage { NucStage::Idle }; ///< Current stage
    std::vector<Step> m_steps {}; ///< Planned steps
    std::size_t m_step { 0U }; ///< Next step index
    NucOptions m_options {}; ///< Current recipe options
    NucError m_lastError { NucError::Ok }; ///< Error that ended the last recipe
};

} // namespace Sightline
