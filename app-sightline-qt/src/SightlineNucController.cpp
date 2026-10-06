/// @file SightlineNucController.cpp
/// @brief Implementation of the QML-facing NUC / DPR controller.

#include "SightlineNucController.h"

#include <SightlineCore/modules/SightlineNucCaps.h>

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

namespace {

/// @brief Maximum number of User Warnings kept in the log.
constexpr qsizetype kMaxWarnings { 50 };
/// @brief Camera index meaning "all cameras" in 0x41 replies.
constexpr std::uint8_t kAllCameras { 255U };
/// @brief Stabilization mode bit 0: on.
constexpr std::uint8_t kStabOnBit { 0x01U };
/// @brief Stabilization mode bit 1: disable all.
constexpr std::uint8_t kStabDisableBit { 0x02U };
/// @brief Highest Sightline::NucRecipe value.
constexpr int kMaxRecipe { 3 };
/// @brief Highest Sightline::NucShow value.
constexpr int kMaxShow { 6 };
/// @brief Highest Sightline::DeadReplace value.
constexpr int kMaxReplace { 2 };
/// @brief Highest Sightline::NucFileOp value.
constexpr int kMaxFileOp { 7 };
/// @brief Highest Sightline::NucDefaultOp value.
constexpr int kMaxDefaultOp { 4 };
/// @brief Largest value of an 8-bit field.
constexpr int kU8Max { 255 };
/// @brief Largest value of a 16-bit unsigned field.
constexpr int kU16Max { 65535 };

/// @brief Table-name keys in the order NucWorkflow::queryState() sends the 0x36 queries.
const char* const kTableKeys[] { "nuc", "dead", "defaultNuc", "defaultDead" };

/// @brief Returns true if @p v lies in [lo, hi].
/// @param[in] v Value.
/// @param[in] lo Inclusive lower bound.
/// @param[in] hi Inclusive upper bound.
/// @return True if in range.
[[nodiscard]] constexpr bool inRange(qint64 v, qint64 lo, qint64 hi) noexcept
{
    return (v >= lo) && (v <= hi);
}

/// @brief Reads an integer limit from a map, falling back to a default.
/// @param[in] map Source map.
/// @param[in] key Key.
/// @param[in] def Default when the key is absent or not numeric.
/// @return Value as a 64-bit integer.
[[nodiscard]] qint64 limitOf(const QVariantMap& map, const char* key, qint64 def)
{
    bool ok { false };
    const qint64 v { map.value(QString::fromLatin1(key)).toLongLong(&ok) };
    return ok ? v : def;
}

/// @brief Returns the error text for a NucError.
/// @param[in] err Error.
/// @return Text.
[[nodiscard]] QString errText(Sightline::NucError err)
{
    return QString::fromLatin1(Sightline::SightlineNucCaps::errorText(err));
}

} // namespace

SightlineNucController::SightlineNucController(Sightline::NucPacketSink sink, QObject* parent)
    : QObject(parent)
    , m_sink { std::move(sink) }
    , m_flow { m_sink }
{
}

int SightlineNucController::camera() const noexcept
{
    return static_cast<int>(m_flow.camera());
}

void SightlineNucController::setCamera(int cam)
{
    const auto idx { static_cast<std::uint8_t>(std::clamp(cam, 0, kU8Max)) };
    if (idx == m_flow.camera()) {
        return;
    }
    if (busy()) {
        m_flow.abort();
    }
    m_flow.setCamera(idx);
    clearCamera();
    emit stateChanged();
    emit boardChanged();
    emit stabilizationChanged();
    emit deadStatsChanged();
    emit noiseStatsChanged();
    emit tablesChanged();
}

int SightlineNucController::stage() const noexcept
{
    return static_cast<int>(m_flow.stage());
}

bool SightlineNucController::busy() const noexcept
{
    return m_flow.stage() == Sightline::NucStage::Running;
}

int SightlineNucController::stepIndex() const noexcept
{
    return static_cast<int>(m_flow.stepIndex());
}

int SightlineNucController::stepCount() const noexcept
{
    return static_cast<int>(m_flow.stepCount());
}

QString SightlineNucController::prompt() const
{
    return QString::fromLatin1(m_flow.prompt());
}

QString SightlineNucController::lastError() const
{
    return m_error;
}

QString SightlineNucController::firmware() const
{
    const Sightline::FwVersion fw { m_flow.firmware() };
    if (fw.major == 0U) {
        return QString {};
    }
    return QStringLiteral("%1.%2").arg(static_cast<int>(fw.major)).arg(static_cast<int>(fw.minor));
}

QVariantMap SightlineNucController::caps() const
{
    using Sightline::SightlineNucCaps;
    const Sightline::FwVersion fw { m_flow.firmware() };
    QVariantMap m {};
    m.insert(QStringLiteral("dpr"), SightlineNucCaps::atLeast(fw, 3U, 3U));
    m.insert(QStringLiteral("noise"), SightlineNucCaps::atLeast(fw, 3U, 4U));
    m.insert(QStringLiteral("destripe"), SightlineNucCaps::atLeast(fw, 3U, 9U));
    m.insert(QStringLiteral("named"), SightlineNucCaps::atLeast(fw, 3U, 10U));
    m.insert(QStringLiteral("shutterSave"), SightlineNucCaps::atLeast(fw, 3U, 11U));
    return m;
}

bool SightlineNucController::hasBoardState() const noexcept
{
    return m_flow.hasBoardState();
}

QVariantMap SightlineNucController::board() const
{
    return m_board;
}

bool SightlineNucController::stabilizationOn() const
{
    const auto it { m_stab.find(m_flow.camera()) };
    if (it != m_stab.cend()) {
        return it->second;
    }
    const auto all { m_stab.find(kAllCameras) };
    return (all != m_stab.cend()) && all->second;
}

QVariantMap SightlineNucController::deadStats() const
{
    return m_dead;
}

QVariantMap SightlineNucController::noiseStats() const
{
    return m_noise;
}

QVariantMap SightlineNucController::tables() const
{
    return m_tables;
}

QStringList SightlineNucController::warnings() const
{
    return m_warnings;
}

QString SightlineNucController::start(int recipe, int frames, const QString& saveName)
{
    if (!inRange(recipe, 0, kMaxRecipe) || !inRange(frames, 0, kU8Max)) {
        return fail(errText(Sightline::NucError::ReservedSet));
    }
    if (stabilizationOn()) {
        return fail(tr("Turn stabilization off before running a NUC (EAN-NUC-and-DPR)."));
    }
    Sightline::NucOptions opts {};
    opts.numFrames = static_cast<std::uint8_t>(frames);
    opts.saveName = saveName.toStdString();
    return report(m_flow.start(static_cast<Sightline::NucRecipe>(recipe), opts));
}

QString SightlineNucController::next()
{
    return report(m_flow.next());
}

void SightlineNucController::abort()
{
    m_flow.abort();
    emit stateChanged();
}

QString SightlineNucController::setShow(int show)
{
    if (!inRange(show, 0, kMaxShow)) {
        return fail(errText(Sightline::NucError::ReservedSet));
    }
    return report(m_flow.setShow(static_cast<Sightline::NucShow>(show)));
}

QVariantMap SightlineNucController::dprDefaults() const
{
    const Sightline::DprLimits d {};
    QVariantMap m {};
    m.insert(QStringLiteral("minGain"), static_cast<int>(d.minGain));
    m.insert(QStringLiteral("maxGain"), static_cast<int>(d.maxGain));
    m.insert(QStringLiteral("minVal"), static_cast<int>(d.minVal));
    m.insert(QStringLiteral("maxVal"), static_cast<int>(d.maxVal));
    m.insert(QStringLiteral("minOff"), static_cast<int>(d.minOff));
    m.insert(QStringLiteral("maxOff"), static_cast<int>(d.maxOff));
    m.insert(QStringLiteral("maxStdDev"), static_cast<qint64>(d.maxStdDev));
    m.insert(QStringLiteral("maxNumDead"), static_cast<int>(d.maxNumDead));
    return m;
}

QString SightlineNucController::calcDead(const QVariantMap& limits)
{
    using Sightline::NucError;
    const Sightline::DprLimits d {};
    const qint64 minGain { limitOf(limits, "minGain", d.minGain) };
    const qint64 maxGain { limitOf(limits, "maxGain", d.maxGain) };
    const qint64 minVal { limitOf(limits, "minVal", d.minVal) };
    const qint64 maxVal { limitOf(limits, "maxVal", d.maxVal) };
    const qint64 minOff { limitOf(limits, "minOff", d.minOff) };
    const qint64 maxOff { limitOf(limits, "maxOff", d.maxOff) };
    const qint64 maxStd { limitOf(limits, "maxStdDev", d.maxStdDev) };
    const qint64 maxNum { limitOf(limits, "maxNumDead", d.maxNumDead) };

    constexpr qint64 s32Min { std::numeric_limits<std::int32_t>::min() };
    constexpr qint64 s32Max { std::numeric_limits<std::int32_t>::max() };
    constexpr qint64 u32Max { std::numeric_limits<std::uint32_t>::max() };
    if (!inRange(minGain, 0, kU16Max) || !inRange(maxGain, 0, kU16Max)) {
        return fail(errText(NucError::GainRange));
    }
    if (!inRange(minVal, 0, kU16Max) || !inRange(maxVal, 0, kU16Max) || !inRange(maxNum, s32Min, s32Max)) {
        return fail(errText(NucError::ReservedSet));
    }
    if (!inRange(minOff, s32Min, s32Max) || !inRange(maxOff, s32Min, s32Max)) {
        return fail(errText(NucError::OffsetRange));
    }
    if (!inRange(maxStd, 0, u32Max)) {
        return fail(errText(NucError::StdDevRange));
    }

    Sightline::DprLimits lim {};
    lim.minGain = static_cast<std::uint16_t>(minGain);
    lim.maxGain = static_cast<std::uint16_t>(maxGain);
    lim.minVal = static_cast<std::uint16_t>(minVal);
    lim.maxVal = static_cast<std::uint16_t>(maxVal);
    lim.minOff = static_cast<std::int32_t>(minOff);
    lim.maxOff = static_cast<std::int32_t>(maxOff);
    lim.maxStdDev = static_cast<std::uint32_t>(maxStd);
    lim.maxNumDead = static_cast<std::int32_t>(maxNum);
    return report(m_flow.calcDead(lim));
}

QString SightlineNucController::calcReplace()
{
    return report(m_flow.calcReplace());
}

QString SightlineNucController::autoDead()
{
    return report(m_flow.autoDead());
}

QString SightlineNucController::setReplace(int method, int numReplace, int filter, int threshold)
{
    using Sightline::NucError;
    if (!inRange(method, 0, kMaxReplace)) {
        return fail(errText(NucError::ReservedSet));
    }
    const bool filterOk { inRange(filter, 0, 2) || (filter == static_cast<int>(Sightline::DeadFilter::Ignore)) };
    if (!filterOk) {
        return fail(errText(NucError::ReservedSet));
    }
    if (!inRange(numReplace, 0, kU8Max)) {
        return fail(errText(NucError::NumReplace));
    }
    if (!inRange(threshold, 0, kU8Max)) {
        return fail(errText(NucError::FilterThresh));
    }
    Sightline::DprReplace rep {};
    rep.method = static_cast<Sightline::DeadReplace>(method);
    rep.numReplace = static_cast<std::uint8_t>(numReplace);
    rep.filter = static_cast<Sightline::DeadFilter>(filter);
    rep.threshold = static_cast<std::int16_t>(threshold);
    return report(m_flow.setReplace(rep));
}

QString SightlineNucController::setDestripe(int amount, int sections)
{
    if (!inRange(amount, 0, kU8Max) || !inRange(sections, 1, kU8Max)) {
        return fail(errText(Sightline::NucError::ReservedSet));
    }
    return report(m_flow.setDestripe(static_cast<std::uint8_t>(amount), static_cast<std::uint8_t>(sections)));
}

QString SightlineNucController::tableOp(
    int fileOp, int defaultOp, const QString& name, const QString& secondary, int ratio)
{
    if (!inRange(fileOp, 0, kMaxFileOp) || !inRange(defaultOp, 0, kMaxDefaultOp) || !inRange(ratio, 0, kU8Max)) {
        return fail(errText(Sightline::NucError::ReservedSet));
    }
    Sightline::MsgReadWriteNuc msg {};
    msg.fileOp = static_cast<Sightline::NucFileOp>(fileOp);
    msg.defaultOp = static_cast<Sightline::NucDefaultOp>(defaultOp);
    msg.fileName = name.toStdString();
    msg.secondaryFileName = secondary.toStdString();
    msg.interpolationRatio = static_cast<std::uint8_t>(ratio);
    return report(m_flow.tableOp(msg));
}

QString SightlineNucController::addDeadPixel(int col, int row)
{
    if (!inRange(col, 0, kU16Max) || !inRange(row, 0, kU16Max)) {
        return fail(errText(Sightline::NucError::ReservedSet));
    }
    return report(m_flow.addDeadPixel(static_cast<std::uint16_t>(col), static_cast<std::uint16_t>(row), false));
}

QString SightlineNucController::removeDeadPixel(int col, int row)
{
    if (!inRange(col, 0, kU16Max) || !inRange(row, 0, kU16Max)) {
        return fail(errText(Sightline::NucError::ReservedSet));
    }
    return report(m_flow.removeDeadPixel(static_cast<std::uint16_t>(col), static_cast<std::uint16_t>(row), false));
}

QString SightlineNucController::dynamicDead(int kernel, int maxDiff)
{
    if (!inRange(kernel, 0, kU8Max) || !inRange(maxDiff, 0, kU8Max)) {
        return fail(errText(Sightline::NucError::ReservedSet));
    }
    return report(m_flow.dynamicDead(static_cast<std::uint8_t>(kernel), static_cast<std::uint8_t>(maxDiff)));
}

bool SightlineNucController::refresh()
{
    m_tableQueue.clear();
    for (const char* key : kTableKeys) {
        m_tableQueue.push_back(QString::fromLatin1(key));
    }
    bool ok { m_flow.queryState() };
    ok = m_flow.queryNoise() && ok;
    return ok;
}

void SightlineNucController::clearWarnings()
{
    m_warnings.clear();
    emit warningsChanged();
}

void SightlineNucController::onVersion(const Sightline::MsgVersionNumber& ver)
{
    m_flow.onVersion(ver);
    emit capsChanged();
}

void SightlineNucController::onNucParams(const Sightline::MsgNucParameters& rep)
{
    if (rep.cameraIndex != m_flow.camera()) {
        return;
    }
    m_flow.onNucReply(rep);
    m_board.clear();
    m_board.insert(QStringLiteral("nucShow"), static_cast<int>(rep.nucShow));
    m_board.insert(QStringLiteral("deadReplace"), static_cast<int>(rep.deadReplace));
    m_board.insert(QStringLiteral("numReplace"), static_cast<int>(rep.numReplace));
    m_board.insert(QStringLiteral("deadFilter"), static_cast<int>(rep.deadFilter));
    m_board.insert(QStringLiteral("deadFilterThresh"), static_cast<int>(rep.deadFilterThresh));
    m_board.insert(QStringLiteral("destripeAmount"), static_cast<int>(rep.destripeAmount));
    m_board.insert(QStringLiteral("destripeSections"), static_cast<int>(rep.destripeSections));
    emit boardChanged();
}

void SightlineNucController::onNucTable(const Sightline::MsgReadWriteNuc& rep)
{
    if ((rep.cameraIndex != m_flow.camera()) || m_tableQueue.empty()) {
        return;
    }
    const QString key { m_tableQueue.front() };
    m_tableQueue.pop_front();
    m_tables.insert(key, QString::fromStdString(rep.fileName));
    emit tablesChanged();
}

void SightlineNucController::onDeadStats(const Sightline::MsgDeadPixelStats& st)
{
    if (st.cameraIndex != m_flow.camera()) {
        return;
    }
    m_dead.clear();
    m_dead.insert(QStringLiteral("nDead"), static_cast<qint64>(st.nDead));
    m_dead.insert(QStringLiteral("nGainLo"), static_cast<qint64>(st.nGainLo));
    m_dead.insert(QStringLiteral("nGainHi"), static_cast<qint64>(st.nGainHi));
    m_dead.insert(QStringLiteral("nAvgLo"), static_cast<qint64>(st.nAvgLo));
    m_dead.insert(QStringLiteral("nAvgHi"), static_cast<qint64>(st.nAvgHi));
    m_dead.insert(QStringLiteral("nOffLo"), static_cast<qint64>(st.nOffLo));
    m_dead.insert(QStringLiteral("nOffHi"), static_cast<qint64>(st.nOffHi));
    m_dead.insert(QStringLiteral("nDevHi"), static_cast<qint64>(st.nDevHi));
    emit deadStatsChanged();
}

void SightlineNucController::onNoiseStats(const Sightline::MsgNoise3D& st)
{
    if (st.cameraIndex != m_flow.camera()) {
        return;
    }
    constexpr double scale { static_cast<double>(Sightline::kNoise3DScale) };
    const std::pair<const char*, std::uint16_t> fields[] {
        { "sigT", st.sigT8 },
        { "sigV", st.sigV8 },
        { "sigH", st.sigH8 },
        { "sigVh", st.sigVh8 },
        { "sigTv", st.sigTv8 },
        { "sigTh", st.sigTh8 },
        { "sigTvh", st.sigTvh8 },
        { "noiseTemporal", st.noiseTemporal8 },
    };
    m_noise.clear();
    for (const auto& f : fields) {
        m_noise.insert(QString::fromLatin1(f.first), static_cast<double>(f.second) / scale);
    }
    emit noiseStatsChanged();
}

void SightlineNucController::onWarning(const Sightline::MsgUserWarningMessage& warn)
{
    m_warnings.prepend(QString::fromStdString(warn.message));
    while (m_warnings.size() > kMaxWarnings) {
        m_warnings.removeLast();
    }
    emit warningsChanged();
    if (m_flow.onWarning(warn.message)) {
        emit stateChanged();
    }
}

void SightlineNucController::onStabilization(const Sightline::MsgSetStabilizationParameters& p)
{
    const bool on { ((p.mode & kStabOnBit) != 0U) && ((p.mode & kStabDisableBit) == 0U) };
    if (p.cameraIndex == kAllCameras) {
        m_stab.clear();
    }
    m_stab[p.cameraIndex] = on;
    emit stabilizationChanged();
}

void SightlineNucController::reset()
{
    const std::uint8_t cam { m_flow.camera() };
    m_flow = Sightline::NucWorkflow { m_sink };
    m_flow.setCamera(cam);
    m_stab.clear();
    clearCamera();
    m_error.clear();
    emit stateChanged();
    emit capsChanged();
    emit boardChanged();
    emit stabilizationChanged();
    emit deadStatsChanged();
    emit noiseStatsChanged();
    emit tablesChanged();
}

QString SightlineNucController::report(Sightline::NucError err)
{
    m_error = (err == Sightline::NucError::Ok) ? QString {} : errText(err);
    emit stateChanged();
    return m_error;
}

QString SightlineNucController::fail(const QString& text)
{
    m_error = text;
    emit stateChanged();
    return m_error;
}

void SightlineNucController::clearCamera()
{
    m_board.clear();
    m_dead.clear();
    m_noise.clear();
    m_tables.clear();
    m_tableQueue.clear();
}
