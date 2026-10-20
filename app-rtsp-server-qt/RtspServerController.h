#pragma once

/// @file RtspServerController.h
/// @brief Primary QML view-model controller orchestrating Live555RtspServer and RtspStreamFeeder.

#include <cstddef>
#include <cstdint>

#include <memory>

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "RtspServerConfig.h"
#include "RtspStreamFeeder.h"
#include "ServerPresetManager.h"

namespace Video {
class Live555RtspServer;
} // namespace Video

namespace RtspServerApp {

/// @class RtspServerController
/// @brief Qt/QML bridge managing the embedded RTSP server lifecycle, configuration,
///        streaming worker thread, and telemetry aggregation.
class RtspServerController : public QObject {
    Q_OBJECT

    Q_PROPERTY(int serverState READ serverState NOTIFY serverStateChanged)
    Q_PROPERTY(QString serverStateString READ serverStateString NOTIFY serverStateChanged)
    Q_PROPERTY(QString primaryUrl READ primaryUrl NOTIFY serverStateChanged)
    Q_PROPERTY(QString localUrl READ localUrl NOTIFY serverStateChanged)

    Q_PROPERTY(int port READ port WRITE setPort NOTIFY configChanged)
    Q_PROPERTY(QString streamPath READ streamPath WRITE setStreamPath NOTIFY configChanged)
    Q_PROPERTY(QString streamName READ streamName WRITE setStreamName NOTIFY configChanged)
    Q_PROPERTY(QString streamInfo READ streamInfo WRITE setStreamInfo NOTIFY configChanged)
    Q_PROPERTY(int streamType READ streamType WRITE setStreamType NOTIFY configChanged)
    Q_PROPERTY(int transport READ transport WRITE setTransport NOTIFY configChanged)
    Q_PROPERTY(int maxClients READ maxClients WRITE setMaxClients NOTIFY configChanged)
    Q_PROPERTY(int queueCapacity READ queueCapacity WRITE setQueueCapacity NOTIFY configChanged)

    Q_PROPERTY(bool authEnabled READ authEnabled WRITE setAuthEnabled NOTIFY configChanged)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY configChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY configChanged)
    Q_PROPERTY(QString realm READ realm WRITE setRealm NOTIFY configChanged)

    Q_PROPERTY(bool enableMulticast READ enableMulticast WRITE setEnableMulticast NOTIFY configChanged)
    Q_PROPERTY(QString multicastAddress READ multicastAddress WRITE setMulticastAddress NOTIFY configChanged)
    Q_PROPERTY(int multicastPort READ multicastPort WRITE setMulticastPort NOTIFY configChanged)
    Q_PROPERTY(int multicastTtl READ multicastTtl WRITE setMulticastTtl NOTIFY configChanged)

    Q_PROPERTY(int feederMode READ feederMode WRITE setFeederMode NOTIFY configChanged)
    Q_PROPERTY(QString feederFilePath READ feederFilePath WRITE setFeederFilePath NOTIFY configChanged)
    Q_PROPERTY(bool feederLoop READ feederLoop WRITE setFeederLoop NOTIFY configChanged)
    Q_PROPERTY(int feederFps READ feederFps WRITE setFeederFps NOTIFY configChanged)

    Q_PROPERTY(int activeClients READ activeClients NOTIFY telemetryUpdated)
    Q_PROPERTY(int queueDepth READ queueDepth NOTIFY telemetryUpdated)
    Q_PROPERTY(qulonglong totalPushed READ totalPushed NOTIFY telemetryUpdated)
    Q_PROPERTY(qulonglong totalDropped READ totalDropped NOTIFY telemetryUpdated)
    Q_PROPERTY(double egressBitrateKbps READ egressBitrateKbps NOTIFY telemetryUpdated)
    Q_PROPERTY(double feederFpsActual READ feederFpsActual NOTIFY telemetryUpdated)
    Q_PROPERTY(QString serverUptime READ serverUptime NOTIFY telemetryUpdated)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QStringList presetList READ presetList NOTIFY presetListChanged)

public:
    /// @enum State
    /// @brief Operating state of the RTSP server.
    enum State : std::uint8_t { Stopped = 0U, Starting = 1U, Running = 2U, Error = 3U };
    Q_ENUM(State)

    /// @brief Constructs controller, initializes telemetry timers and preset manager.
    /// @param[in] parent Optional QObject parent.
    explicit RtspServerController(QObject* parent = nullptr);

    /// @brief Destructor. Gracefully shuts down server and feeder.
    ~RtspServerController() override;

    RtspServerController(const RtspServerController&) = delete;
    RtspServerController& operator=(const RtspServerController&) = delete;
    RtspServerController(RtspServerController&&) = delete;
    RtspServerController& operator=(RtspServerController&&) = delete;

    // Getters
    [[nodiscard]] int serverState() const noexcept;
    [[nodiscard]] QString serverStateString() const;
    [[nodiscard]] QString primaryUrl() const;
    [[nodiscard]] QString localUrl() const;

    [[nodiscard]] int port() const noexcept;
    [[nodiscard]] QString streamPath() const;
    [[nodiscard]] QString streamName() const;
    [[nodiscard]] QString streamInfo() const;
    [[nodiscard]] int streamType() const noexcept;
    [[nodiscard]] int transport() const noexcept;
    [[nodiscard]] int maxClients() const noexcept;
    [[nodiscard]] int queueCapacity() const noexcept;

    [[nodiscard]] bool authEnabled() const noexcept;
    [[nodiscard]] QString username() const;
    [[nodiscard]] QString password() const;
    [[nodiscard]] QString realm() const;

    [[nodiscard]] bool enableMulticast() const noexcept;
    [[nodiscard]] QString multicastAddress() const;
    [[nodiscard]] int multicastPort() const noexcept;
    [[nodiscard]] int multicastTtl() const noexcept;

    [[nodiscard]] int feederMode() const noexcept;
    [[nodiscard]] QString feederFilePath() const;
    [[nodiscard]] bool feederLoop() const noexcept;
    [[nodiscard]] int feederFps() const noexcept;

    [[nodiscard]] int activeClients() const noexcept;
    [[nodiscard]] int queueDepth() const noexcept;
    [[nodiscard]] qulonglong totalPushed() const noexcept;
    [[nodiscard]] qulonglong totalDropped() const noexcept;
    [[nodiscard]] double egressBitrateKbps() const noexcept;
    [[nodiscard]] double feederFpsActual() const noexcept;
    [[nodiscard]] QString serverUptime() const;
    [[nodiscard]] QString statusMessage() const;
    [[nodiscard]] QStringList presetList() const;

    // Setters
    void setPort(int val);
    void setStreamPath(const QString& val);
    void setStreamName(const QString& val);
    void setStreamInfo(const QString& val);
    void setStreamType(int val);
    void setTransport(int val);
    void setMaxClients(int val);
    void setQueueCapacity(int val);

    void setAuthEnabled(bool val);
    void setUsername(const QString& val);
    void setPassword(const QString& val);
    void setRealm(const QString& val);

    void setEnableMulticast(bool val);
    void setMulticastAddress(const QString& val);
    void setMulticastPort(int val);
    void setMulticastTtl(int val);

    void setFeederMode(int val);
    void setFeederFilePath(const QString& val);
    void setFeederLoop(bool val);
    void setFeederFps(int val);

    // Q_INVOKABLE Actions
    Q_INVOKABLE bool startServer();
    Q_INVOKABLE void stopServer();
    Q_INVOKABLE bool toggleServer();
    Q_INVOKABLE void copyUrlToClipboard();
    Q_INVOKABLE bool savePreset(const QString& name);
    Q_INVOKABLE bool loadPreset(const QString& name);
    Q_INVOKABLE bool deletePreset(const QString& name);
    Q_INVOKABLE QStringList availableNetworkInterfaces() const;

signals:
    void serverStateChanged();
    void telemetryUpdated();
    void configChanged();
    void statusMessageChanged();
    void presetListChanged();
    void logMessageEmitted(const QString& timestamp, const QString& level, const QString& message);

private slots:
    void onTelemetryTimer();

private:
    void logEvent(const QString& level, const QString& message);
    void updatePrimaryUrl();
    [[nodiscard]] QString detectHostIp() const;

    State m_state { State::Stopped };
    QString m_statusMessage { QStringLiteral("Ready") };
    QString m_primaryUrl {};
    QString m_localUrl {};

    Video::RtspServerConfig m_serverConfig {};
    FeederConfig m_feederConfig {};
    bool m_authEnabled { false };

    std::unique_ptr<Video::Live555RtspServer> m_server {};
    std::unique_ptr<RtspStreamFeeder> m_feeder {};
    ServerPresetManager m_presetManager {};

    QTimer m_telemetryTimer {};
    QElapsedTimer m_uptimeTimer {};

    std::size_t m_activeClients { 0U };
    std::size_t m_queueDepth { 0U };
    std::uint64_t m_totalPushed { 0U };
    std::uint64_t m_totalDropped { 0U };
    double m_egressBitrateKbps { 0.0 };
    double m_feederFpsActual { 0.0 };
    std::uint64_t m_lastBytesStreamed { 0U };
    qint64 m_lastBitrateCalcTime { 0 };
};

} // namespace RtspServerApp
