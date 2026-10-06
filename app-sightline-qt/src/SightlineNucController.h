#pragma once

/// @file SightlineNucController.h
/// @brief QML-facing controller for the Sightline NUC / DPR workflow.
/// @details Thin Qt adapter over Sightline::NucWorkflow (libs/SightlineCore/modules/SightlineNucWorkflow.h).
///          It converts QML types, applies the EAN pre-condition that stabilization must be off
///          before a NUC, and exposes board replies (0x35, 0x36, 0xA1, 0xAF) as QVariantMaps.
///          Procedures follow docs/protocols/Sightline/EAN-NUC-and-DPR.pdf.
///
///          ```
///          QML (NucView) --Q_INVOKABLE--> SightlineNucController --NucWorkflow--> sink --> 0x35/0x36/0xA8/0x28
///          QSightlineDevice signals ----> on*() slots -------------> QVariantMap properties --> QML
///          ```
///
///          ```mermaid
///          flowchart LR
///              QML["NucView.qml"] -->|"Q_INVOKABLE"| CTL["SightlineNucController"]
///              CTL --> WF["Sightline::NucWorkflow"]
///              WF -->|"framed packets"| SINK["QSightlineDevice::sendFramed"]
///              DEV["QSightlineDevice signals"] -->|"on* slots"| CTL
///              CTL -->|"properties"| QML
///          ```
///
///          Not thread-safe: use from the GUI thread only.

#include <SightlineCore/modules/SightlineEnhancement.h>
#include <SightlineCore/modules/SightlineGeneral.h>
#include <SightlineCore/modules/SightlineNuc.h>
#include <SightlineCore/modules/SightlineNucWorkflow.h>
#include <SightlineCore/modules/SightlineStabilization.h>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <cstdint>
#include <deque>
#include <map>

/// @class SightlineNucController
/// @brief Exposes NUC recipes, DPR commands and calibration telemetry to QML.
class SightlineNucController : public QObject {
    Q_OBJECT
    Q_PROPERTY(int camera READ camera WRITE setCamera NOTIFY stateChanged)
    Q_PROPERTY(int stage READ stage NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(int stepIndex READ stepIndex NOTIFY stateChanged)
    Q_PROPERTY(int stepCount READ stepCount NOTIFY stateChanged)
    Q_PROPERTY(QString prompt READ prompt NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY stateChanged)
    Q_PROPERTY(QString firmware READ firmware NOTIFY capsChanged)
    Q_PROPERTY(QVariantMap caps READ caps NOTIFY capsChanged)
    Q_PROPERTY(bool hasBoardState READ hasBoardState NOTIFY boardChanged)
    Q_PROPERTY(QVariantMap board READ board NOTIFY boardChanged)
    Q_PROPERTY(bool stabilizationOn READ stabilizationOn NOTIFY stabilizationChanged)
    Q_PROPERTY(QVariantMap deadStats READ deadStats NOTIFY deadStatsChanged)
    Q_PROPERTY(QVariantMap noiseStats READ noiseStats NOTIFY noiseStatsChanged)
    Q_PROPERTY(QVariantMap tables READ tables NOTIFY tablesChanged)
    Q_PROPERTY(QStringList warnings READ warnings NOTIFY warningsChanged)

public:
    /// @brief Constructs an idle controller.
    /// @param[in] sink Packet transmitter (normally QSightlineDevice::sendFramed).
    /// @param[in] parent Optional QObject parent.
    explicit SightlineNucController(Sightline::NucPacketSink sink, QObject* parent = nullptr);

    /// @brief Returns the target camera index.
    /// @return Camera index.
    [[nodiscard]] int camera() const noexcept;

    /// @brief Selects the target camera; clears cached board state and statistics.
    /// @param[in] cam Camera index (clamped to 0..255).
    void setCamera(int cam);

    /// @brief Returns the recipe stage as an int (Sightline::NucStage).
    /// @return Stage value.
    [[nodiscard]] int stage() const noexcept;

    /// @brief Returns true while a recipe waits for next().
    /// @return True if running.
    [[nodiscard]] bool busy() const noexcept;

    /// @brief Returns the index of the step next() will send.
    /// @return Zero-based step index.
    [[nodiscard]] int stepIndex() const noexcept;

    /// @brief Returns the number of steps in the current recipe.
    /// @return Step count.
    [[nodiscard]] int stepCount() const noexcept;

    /// @brief Returns the operator instruction for the current step.
    /// @return Prompt text.
    [[nodiscard]] QString prompt() const;

    /// @brief Returns the text of the last error (empty if none).
    /// @return Error text.
    [[nodiscard]] QString lastError() const;

    /// @brief Returns the firmware version as "major.minor" (empty if unknown).
    /// @return Firmware string.
    [[nodiscard]] QString firmware() const;

    /// @brief Returns firmware-gated feature flags.
    /// @details Keys: dpr (3.3), noise (3.4), destripe (3.9), named (3.10), shutterSave (3.11).
    /// @return Capability map.
    [[nodiscard]] QVariantMap caps() const;

    /// @brief Returns true once a 0x35 reply for the camera has been received.
    /// @return True if the board settings are known.
    [[nodiscard]] bool hasBoardState() const noexcept;

    /// @brief Returns the last 0x35 settings for the camera.
    /// @return Map with nucShow, deadReplace, numReplace, deadFilter, deadFilterThresh,
    ///         destripeAmount, destripeSections (empty if unknown).
    [[nodiscard]] QVariantMap board() const;

    /// @brief Returns true if the board reports stabilization on for the camera.
    /// @return True if stabilization is on.
    [[nodiscard]] bool stabilizationOn() const;

    /// @brief Returns the last 0xA1 statistics for the camera (empty if none).
    /// @return Map keyed by the MsgDeadPixelStats field names.
    [[nodiscard]] QVariantMap deadStats() const;

    /// @brief Returns the last 0xAF statistics in grey levels (raw / 256).
    /// @return Map with sigT, sigV, sigH, sigVh, sigTv, sigTh, sigTvh, noiseTemporal.
    [[nodiscard]] QVariantMap noiseStats() const;

    /// @brief Returns the reported table names.
    /// @return Map with nuc, dead, defaultNuc, defaultDead.
    [[nodiscard]] QVariantMap tables() const;

    /// @brief Returns received User Warnings, newest first.
    /// @return Warning texts.
    [[nodiscard]] QStringList warnings() const;

    /// @brief Starts a recipe; nothing is sent until next().
    /// @param[in] recipe 0 TwoPoint, 1 OnePoint, 2 ShutterFlatten, 3 NoiseStats.
    /// @param[in] frames Frames per capture step (0..255).
    /// @param[in] saveName ShutterFlatten only: "_shutter_only" table name, empty = no save.
    /// @return Empty string on success, otherwise the error text.
    Q_INVOKABLE QString start(int recipe, int frames, const QString& saveName);

    /// @brief Sends the current step and advances.
    /// @return Empty string on success, otherwise the error text.
    Q_INVOKABLE QString next();

    /// @brief Stops the running recipe without sending anything.
    Q_INVOKABLE void abort();

    /// @brief Sets the video display mode (0x35 run 0).
    /// @param[in] show Sightline::NucShow value (0..6).
    /// @return Empty string on success, otherwise the error text.
    Q_INVOKABLE QString setShow(int show);

    /// @brief Returns the default dead pixel limits (EAN section 3.5 example).
    /// @return Map with minGain, maxGain, minVal, maxVal, minOff, maxOff, maxStdDev, maxNumDead.
    Q_INVOKABLE QVariantMap dprDefaults() const;

    /// @brief Calculates dead pixels (0x35 run 9) and queries 0xA1.
    /// @param[in] limits Map with the dprDefaults() keys; missing keys use the defaults.
    /// @return Empty string on success, otherwise the error text.
    Q_INVOKABLE QString calcDead(const QVariantMap& limits);

    /// @brief Recalculates only the replacement pixels (0x35 run 10).
    /// @return Empty string on success, otherwise the error text.
    Q_INVOKABLE QString calcReplace();

    /// @brief Runs automatic single-frame dead pixel detection (0x35 run 11).
    /// @return Empty string on success, otherwise the error text.
    Q_INVOKABLE QString autoDead();

    /// @brief Sets dead pixel replacement and dynamic filter (0x35 Dpr tail).
    /// @param[in] method Sightline::DeadReplace (0..2).
    /// @param[in] numReplace Neighbours (1..8).
    /// @param[in] filter Sightline::DeadFilter (0, 1, 2 or 255).
    /// @param[in] threshold Filter threshold (0..255).
    /// @return Empty string on success, otherwise the error text.
    Q_INVOKABLE QString setReplace(int method, int numReplace, int filter, int threshold);

    /// @brief Sets NUC destripe (0x35 Destripe tail).
    /// @param[in] amount 0 off .. 255 aggressive.
    /// @param[in] sections Vertical sections (1..255).
    /// @return Empty string on success, otherwise the error text.
    Q_INVOKABLE QString setDestripe(int amount, int sections);

    /// @brief Sends a 0x36 table command.
    /// @param[in] fileOp Sightline::NucFileOp (0..7).
    /// @param[in] defaultOp Sightline::NucDefaultOp (0..4).
    /// @param[in] name Primary table name.
    /// @param[in] secondary Secondary table name (interpolate / shutter flatten).
    /// @param[in] ratio Interpolation ratio (0..255).
    /// @return Empty string on success, otherwise the error text.
    Q_INVOKABLE QString tableOp(int fileOp, int defaultOp, const QString& name, const QString& secondary, int ratio);

    /// @brief Adds a pixel to the dead list (0xA8 mode 0).
    /// @param[in] col Column (x).
    /// @param[in] row Row (y).
    /// @return Empty string on success, otherwise the error text.
    Q_INVOKABLE QString addDeadPixel(int col, int row);

    /// @brief Removes a manually added pixel from the dead list (0xA8 mode 1).
    /// @param[in] col Column (x).
    /// @param[in] row Row (y).
    /// @return Empty string on success, otherwise the error text.
    Q_INVOKABLE QString removeDeadPixel(int col, int row);

    /// @brief Runs dynamic dead pixel detection (0xA8 mode 2).
    /// @param[in] kernel Kernel size, 0 = firmware default.
    /// @param[in] maxDiff Max pixel difference, 0 = automatic.
    /// @return Empty string on success, otherwise the error text.
    Q_INVOKABLE QString dynamicDead(int kernel, int maxDiff);

    /// @brief Queries 0x35, 0xA1, the four 0x36 table names and 0xAF.
    /// @return True if every query was sent.
    Q_INVOKABLE bool refresh();

    /// @brief Clears the warning log.
    Q_INVOKABLE void clearWarnings();

    // Board-reply handlers. Intentionally not slots: connect them with pointer-to-member syntax.
    // Declaring them as slots would make moc instantiate QMetaTypeId for the message types
    // before QSightlineDevice.h declares them (Q_DECLARE_METATYPE), breaking unity moc builds.
public:
    /// @brief Records the firmware version (0x40).
    /// @param[in] ver Version reply.
    void onVersion(const Sightline::MsgVersionNumber& ver);

    /// @brief Caches a 0x35 reply.
    /// @param[in] rep NUC parameters reply.
    void onNucParams(const Sightline::MsgNucParameters& rep);

    /// @brief Records a 0x36 table-name reply.
    /// @details Replies are matched to the queries sent by refresh() in order, because the
    ///          IDD does not state how a reply identifies the queried table.
    /// @param[in] rep Table reply.
    void onNucTable(const Sightline::MsgReadWriteNuc& rep);

    /// @brief Records 0xA1 statistics for the camera.
    /// @param[in] st Dead pixel statistics.
    void onDeadStats(const Sightline::MsgDeadPixelStats& st);

    /// @brief Records 0xAF statistics for the camera.
    /// @param[in] st 3D noise statistics.
    void onNoiseStats(const Sightline::MsgNoise3D& st);

    /// @brief Logs a User Warning and forwards it to the workflow.
    /// @param[in] warn Warning message.
    void onWarning(const Sightline::MsgUserWarningMessage& warn);

    /// @brief Tracks the stabilization on/off state per camera (0x41).
    /// @param[in] p Stabilization parameters.
    void onStabilization(const Sightline::MsgSetStabilizationParameters& p);

    /// @brief Forgets all board-derived state (call on disconnect); keeps the camera.
    void reset();

signals:
    /// @brief Recipe, step, prompt, camera or error changed.
    void stateChanged();
    /// @brief Firmware version changed.
    void capsChanged();
    /// @brief Cached 0x35 board state changed.
    void boardChanged();
    /// @brief Stabilization state changed.
    void stabilizationChanged();
    /// @brief 0xA1 statistics changed.
    void deadStatsChanged();
    /// @brief 0xAF statistics changed.
    void noiseStatsChanged();
    /// @brief Table names changed.
    void tablesChanged();
    /// @brief Warning log changed.
    void warningsChanged();

private:
    /// @brief Converts a workflow result into a QML result and records it.
    /// @param[in] err Workflow error.
    /// @return Empty string for Ok, otherwise the error text.
    QString report(Sightline::NucError err);

    /// @brief Records a controller-level error text.
    /// @param[in] text Error text.
    /// @return @p text.
    QString fail(const QString& text);

    /// @brief Clears per-camera statistics and table names.
    void clearCamera();

    Sightline::NucPacketSink m_sink {}; ///< Transport kept for reset()
    Sightline::NucWorkflow m_flow; ///< Core sequencer
    std::map<std::uint8_t, bool> m_stab {}; ///< Stabilization on per camera
    std::deque<QString> m_tableQueue {}; ///< Pending 0x36 table-name keys
    QVariantMap m_board {}; ///< Last 0x35 settings
    QVariantMap m_dead {}; ///< Last 0xA1 statistics
    QVariantMap m_noise {}; ///< Last 0xAF statistics
    QVariantMap m_tables {}; ///< Reported table names
    QStringList m_warnings {}; ///< Warning log, newest first
    QString m_error {}; ///< Last error text
};
