/// @file SightlineNucWorkflow.cpp
/// @brief Implementation of the NUC/DPR calibration sequencer.

#include "SightlineNucWorkflow.h"

#include "SightlineEnhancementBuilder.h"
#include "SightlineNucBuilder.h"

#include <utility>

namespace Sightline {

namespace {

    /// @brief Only verbatim completion message in the EAN (section 5.5 step 6).
    constexpr const char* kNoiseDone { "Noise stats calculation complete" };

    /// @brief Default numReplace when the field is not transmitted (IDD default).
    constexpr std::uint8_t kDefReplace { 5U };
    /// @brief Default deadFilterThresh when the field is not transmitted (IDD default).
    constexpr std::int16_t kDefThresh { 64 };
    /// @brief Default destripeSections when the field is not transmitted (IDD default).
    constexpr std::uint8_t kDefSections { 1U };

    constexpr const char* kPromptIdle { "Select a calibration procedure." };
    constexpr const char* kPromptDone { "Complete. Save the table or set it as the startup default if required." };
    constexpr const char* kPromptAbort { "Aborted. Frames already added stay on the board until cleared." };

    /// @brief Returns the ordinal of a tail for length comparisons.
    /// @param[in] tail Tail.
    /// @return 0 (Base) .. 3 (Named).
    [[nodiscard]] constexpr std::uint8_t rank(NucTail tail) noexcept
    {
        return static_cast<std::uint8_t>(tail);
    }

    /// @brief Resets fields that @p tail does not transmit to their IDD defaults.
    /// @details Unsent fields cannot affect the board, so they must not fail validation.
    /// @param[in,out] msg Message to normalise.
    /// @param[in] tail Tail that will be encoded.
    void maskUnsent(MsgNucParameters& msg, NucTail tail) noexcept
    {
        if (rank(tail) < rank(NucTail::Dpr)) {
            msg.deadReplace = DeadReplace::Nearest;
            msg.numReplace = kDefReplace;
            msg.deadFilter = DeadFilter::Ignore;
            msg.deadFilterThresh = kDefThresh;
        }
        if (rank(tail) < rank(NucTail::Destripe)) {
            msg.destripeAmount = 0U;
            msg.destripeSections = kDefSections;
        }
    }

} // namespace

NucWorkflow::NucWorkflow(NucPacketSink sink)
    : m_sink { std::move(sink) }
{
}

void NucWorkflow::setCamera(std::uint8_t cameraIndex) noexcept
{
    if (cameraIndex != m_camera) {
        m_camera = cameraIndex;
        m_state = MsgNucParameters {};
        m_hasState = false;
    }
}

std::uint8_t NucWorkflow::camera() const noexcept
{
    return m_camera;
}

void NucWorkflow::onVersion(const MsgVersionNumber& ver) noexcept
{
    m_fw = SightlineNucCaps::fwFromVersion(ver);
}

FwVersion NucWorkflow::firmware() const noexcept
{
    return m_fw;
}

void NucWorkflow::onNucReply(const MsgNucParameters& reply)
{
    if (reply.cameraIndex == m_camera) {
        m_state = reply;
        m_hasState = true;
    }
}

bool NucWorkflow::hasBoardState() const noexcept
{
    return m_hasState;
}

bool NucWorkflow::onWarning(const std::string& text)
{
    const bool waiting { (m_stage == NucStage::Running) && (m_step < m_steps.size())
        && (m_steps[m_step].action == StepAction::NoiseRead) };
    if (!waiting || (text.find(kNoiseDone) == std::string::npos)) {
        return false;
    }
    return next() == NucError::Ok;
}

NucError NucWorkflow::start(NucRecipe recipe, const NucOptions& options)
{
    if (m_stage == NucStage::Running) {
        return NucError::Busy;
    }
    const NucError plan { planSteps(recipe, options) };
    if (plan != NucError::Ok) {
        return plan;
    }
    m_recipe = recipe;
    m_options = options;
    m_step = 0U;
    m_lastError = NucError::Ok;
    m_stage = NucStage::Running;
    return NucError::Ok;
}

NucError NucWorkflow::next()
{
    if ((m_stage != NucStage::Running) || (m_step >= m_steps.size())) {
        return NucError::NotRunning;
    }
    const NucError err { runStep(m_steps[m_step].action) };
    if (err != NucError::Ok) {
        m_lastError = err;
        m_stage = NucStage::Failed;
        return err;
    }
    ++m_step;
    if (m_step >= m_steps.size()) {
        m_stage = NucStage::Done;
    }
    return NucError::Ok;
}

void NucWorkflow::abort() noexcept
{
    if (m_stage == NucStage::Running) {
        m_stage = NucStage::Aborted;
    }
}

NucStage NucWorkflow::stage() const noexcept
{
    return m_stage;
}

NucRecipe NucWorkflow::recipe() const noexcept
{
    return m_recipe;
}

std::size_t NucWorkflow::stepIndex() const noexcept
{
    return m_step;
}

std::size_t NucWorkflow::stepCount() const noexcept
{
    return m_steps.size();
}

const char* NucWorkflow::prompt() const noexcept
{
    const char* text { kPromptIdle };
    switch (m_stage) {
    case NucStage::Running:
        text = (m_step < m_steps.size()) ? m_steps[m_step].prompt : kPromptDone;
        break;
    case NucStage::Done:
        text = kPromptDone;
        break;
    case NucStage::Aborted:
        text = kPromptAbort;
        break;
    case NucStage::Failed:
        text = SightlineNucCaps::errorText(m_lastError);
        break;
    case NucStage::Idle:
    default:
        text = kPromptIdle;
        break;
    }
    return text;
}

NucError NucWorkflow::lastError() const noexcept
{
    return m_lastError;
}

NucError NucWorkflow::setShow(NucShow show)
{
    MsgNucParameters msg { baseMsg() };
    msg.nucShow = show;
    return sendNuc(msg, runTail());
}

NucError NucWorkflow::calcDead(const DprLimits& limits)
{
    MsgNucParameters msg { baseMsg() };
    msg.nucRunMode = NucRunMode::CalcDead;
    msg.minDeadGain = limits.minGain;
    msg.maxDeadGain = limits.maxGain;
    msg.minDeadVal = limits.minVal;
    msg.maxDeadVal = limits.maxVal;
    msg.minDeadOff = limits.minOff;
    msg.maxDeadOff = limits.maxOff;
    msg.maxStdDevDead = limits.maxStdDev;
    msg.maxNumDead = limits.maxNumDead;
    const NucError err { sendNuc(msg, runTail()) };
    if (err != NucError::Ok) {
        return err;
    }
    return send(SightlineNucBuilder::buildGetDeadPixelStats(m_camera)) ? NucError::Ok : NucError::NotSent;
}

NucError NucWorkflow::calcReplace()
{
    return sendRun(NucRunMode::CalcReplace, 0U);
}

NucError NucWorkflow::autoDead()
{
    return sendRun(NucRunMode::AutoDead, 0U);
}

NucError NucWorkflow::setReplace(const DprReplace& rep)
{
    const NucTail maxTail { SightlineNucCaps::maxNucTail(m_fw) };
    if (rank(maxTail) < rank(NucTail::Dpr)) {
        return NucError::Unsupported;
    }
    MsgNucParameters msg { baseMsg() };
    msg.deadReplace = rep.method;
    msg.numReplace = rep.numReplace;
    msg.deadFilter = rep.filter;
    msg.deadFilterThresh = rep.threshold;
    // Every Dpr-tail field is supplied here, so the Dpr form is safe without a cached state.
    return sendNuc(msg, m_hasState ? maxTail : NucTail::Dpr);
}

NucError NucWorkflow::setDestripe(std::uint8_t amount, std::uint8_t sections)
{
    const NucTail maxTail { SightlineNucCaps::maxNucTail(m_fw) };
    if (rank(maxTail) < rank(NucTail::Destripe)) {
        return NucError::Unsupported;
    }
    if (!m_hasState) {
        return NucError::StateUnknown;
    }
    MsgNucParameters msg { baseMsg() };
    msg.destripeAmount = amount;
    msg.destripeSections = sections;
    return sendNuc(msg, maxTail);
}

NucError NucWorkflow::tableOp(MsgReadWriteNuc msg)
{
    msg.cameraIndex = m_camera;
    const NucError err { SightlineNucBuilder::checkReadWriteNuc(msg, m_fw) };
    if (err != NucError::Ok) {
        return err;
    }
    const std::vector<std::uint8_t> pkt { SightlineNucBuilder::buildReadWriteNuc(msg) };
    if (pkt.empty()) {
        return NucError::NameTooLong;
    }
    return send(pkt) ? NucError::Ok : NucError::NotSent;
}

NucError NucWorkflow::addDeadPixel(std::uint16_t column, std::uint16_t row, bool deferUpdate)
{
    MsgDeadPixel msg {};
    msg.mode = DeadPixelMode::Add;
    msg.a = column;
    msg.b = row;
    msg.c = deferUpdate ? std::uint8_t { 1U } : std::uint8_t { 0U };
    return sendDead(msg);
}

NucError NucWorkflow::removeDeadPixel(std::uint16_t column, std::uint16_t row, bool deferUpdate)
{
    MsgDeadPixel msg {};
    msg.mode = DeadPixelMode::Remove;
    msg.a = column;
    msg.b = row;
    msg.c = deferUpdate ? std::uint8_t { 1U } : std::uint8_t { 0U };
    return sendDead(msg);
}

NucError NucWorkflow::dynamicDead(std::uint8_t kernelSize, std::uint8_t maxPixelDiff)
{
    MsgDeadPixel msg {};
    msg.mode = DeadPixelMode::DynamicDetect;
    msg.a = kernelSize;
    msg.b = maxPixelDiff;
    return sendDead(msg);
}

bool NucWorkflow::queryState()
{
    bool ok { send(SightlineNucBuilder::buildGetNucParameters(m_camera)) };
    ok = send(SightlineNucBuilder::buildGetDeadPixelStats(m_camera)) && ok;
    ok = send(SightlineNucBuilder::buildGetReadWriteNuc(NucTableQuery::NucTable, m_camera)) && ok;
    ok = send(SightlineNucBuilder::buildGetReadWriteNuc(NucTableQuery::DeadTable, m_camera)) && ok;
    ok = send(SightlineNucBuilder::buildGetReadWriteNuc(NucTableQuery::DefaultNuc, m_camera)) && ok;
    ok = send(SightlineNucBuilder::buildGetReadWriteNuc(NucTableQuery::DefaultDead, m_camera)) && ok;
    return ok;
}

bool NucWorkflow::queryNoise()
{
    return send(SightlineEnhancementBuilder::buildGetNoise3D(m_camera));
}

NucError NucWorkflow::planSteps(NucRecipe recipe, const NucOptions& options)
{
    std::vector<Step> steps {};
    switch (recipe) {
    case NucRecipe::TwoPoint:
        steps = {
            { StepAction::ResetAll, "Set stabilization OFF, then reset the NUC and dead pixel calculations." },
            { StepAction::AddFrames, "Point the camera at a uniform HOT source filling the view, then add frames." },
            { StepAction::AddFrames, "Point the camera at a uniform COLD source filling the view, then add frames." },
            { StepAction::Calc2Point, "Calculate the 2-point NUC (enables NUC and DPR on the video)." },
        };
        break;
    case NucRecipe::OnePoint:
        steps = {
            { StepAction::ClearFrames, "Clear frames added earlier in this power cycle." },
            { StepAction::AddFrames, "Close the shutter or point at a flat field, then add frames." },
            { StepAction::Calc1Point, "Calculate the 1-point NUC (updates offsets of the loaded table)." },
        };
        break;
    case NucRecipe::ShutterFlatten:
        if (!SightlineNucCaps::supportsRun(m_fw, NucRunMode::ShutterFlatten)) {
            return NucError::Unsupported;
        }
        steps = { { StepAction::Flatten, "Defocus the lens until no features are visible, then run shutter flatten." } };
        if (!options.saveName.empty()) {
            MsgReadWriteNuc save {};
            save.fileOp = NucFileOp::SaveShutterFlatten;
            save.fileName = options.saveName;
            const NucError err { SightlineNucBuilder::checkReadWriteNuc(save, m_fw) };
            if (err != NucError::Ok) {
                return err;
            }
            steps.push_back({ StepAction::SaveFlatten,
                "Save the shutter flatten table now (do not save the NUC table)." });
        }
        break;
    case NucRecipe::NoiseStats:
        if (!SightlineNucCaps::supportsRun(m_fw, NucRunMode::Noise3DStats)) {
            return NucError::Unsupported;
        }
        steps = {
            { StepAction::NoiseCalc, "Choose raw or NUC/DPR video, then calculate the noise statistics." },
            { StepAction::NoiseRead, "Waiting for 'Noise stats calculation complete'; retrieve results manually if needed." },
        };
        break;
    default:
        return NucError::Unsupported;
    }
    m_steps = std::move(steps);
    return NucError::Ok;
}

NucError NucWorkflow::runStep(StepAction action)
{
    NucError err { NucError::Ok };
    switch (action) {
    case StepAction::ResetAll:
        err = sendRun(NucRunMode::ResetAll, 0U);
        break;
    case StepAction::AddFrames:
        err = sendRun(NucRunMode::AddFrames, m_options.numFrames);
        break;
    case StepAction::Calc2Point:
        err = sendRun(NucRunMode::Calc2Point, 0U);
        if (err == NucError::Ok) {
            m_state.nucShow = NucShow::NucAndDpr; // EAN 3.4 step 7: 2-pt enables NUC and DPR
        }
        break;
    case StepAction::ClearFrames:
        err = sendRun(NucRunMode::ClearFrames, 0U);
        break;
    case StepAction::Calc1Point:
        err = sendRun(NucRunMode::Calc1Point, 0U);
        break;
    case StepAction::Flatten:
        err = sendRun(NucRunMode::ShutterFlatten, m_options.numFrames);
        break;
    case StepAction::SaveFlatten: {
        MsgReadWriteNuc save {};
        save.fileOp = NucFileOp::SaveShutterFlatten;
        save.fileName = m_options.saveName;
        err = tableOp(save);
        break;
    }
    case StepAction::NoiseCalc:
        err = sendRun(NucRunMode::Noise3DStats, m_options.numFrames);
        break;
    case StepAction::NoiseRead:
        err = queryNoise() ? NucError::Ok : NucError::NotSent;
        break;
    default:
        err = NucError::Unsupported;
        break;
    }
    return err;
}

NucError NucWorkflow::sendRun(NucRunMode run, std::uint8_t frames)
{
    MsgNucParameters msg { baseMsg() };
    msg.nucRunMode = run;
    msg.numFrames = frames;
    return sendNuc(msg, runTail());
}

NucError NucWorkflow::sendNuc(MsgNucParameters msg, NucTail tail)
{
    maskUnsent(msg, tail);
    const NucError err { SightlineNucBuilder::checkNucParams(msg, m_fw) };
    if (err != NucError::Ok) {
        return err;
    }
    const std::vector<std::uint8_t> pkt { SightlineNucBuilder::buildNucParameters(msg, tail) };
    if (pkt.empty()) {
        return NucError::NameTooLong;
    }
    if (!send(pkt)) {
        return NucError::NotSent;
    }

    // The board now holds what was sent: keep the cache in step.
    m_state.nucShow = msg.nucShow;
    if (rank(tail) >= rank(NucTail::Dpr)) {
        m_state.deadReplace = msg.deadReplace;
        m_state.numReplace = msg.numReplace;
        if (msg.deadFilter != DeadFilter::Ignore) {
            m_state.deadFilter = msg.deadFilter;
            m_state.deadFilterThresh = msg.deadFilterThresh;
        }
    }
    if (rank(tail) >= rank(NucTail::Destripe)) {
        m_state.destripeAmount = msg.destripeAmount;
        m_state.destripeSections = msg.destripeSections;
    }
    return NucError::Ok;
}

NucError NucWorkflow::sendDead(MsgDeadPixel msg)
{
    msg.cameraIndex = m_camera;
    const NucError err { SightlineNucBuilder::checkDeadPixel(msg, m_fw) };
    if (err != NucError::Ok) {
        return err;
    }
    return send(SightlineNucBuilder::buildDeadPixel(msg)) ? NucError::Ok : NucError::NotSent;
}

MsgNucParameters NucWorkflow::baseMsg() const
{
    MsgNucParameters msg { m_state };
    msg.cameraIndex = m_camera;
    msg.nucRunMode = NucRunMode::None;
    msg.numFrames = 0U;
    msg.minDeadGain = 0U;
    msg.maxDeadGain = 0U;
    msg.minDeadVal = 0U;
    msg.maxDeadVal = 0U;
    msg.minDeadOff = 0;
    msg.maxDeadOff = 0;
    msg.maxStdDevDead = 0U;
    msg.maxNumDead = 0;
    msg.nucName.clear();
    return msg;
}

NucTail NucWorkflow::runTail() const noexcept
{
    return m_hasState ? SightlineNucCaps::maxNucTail(m_fw) : NucTail::Base;
}

bool NucWorkflow::send(const std::vector<std::uint8_t>& packet)
{
    if (!m_sink || packet.empty()) {
        return false;
    }
    return m_sink(packet);
}

} // namespace Sightline
