#include "ServerPresetManager.h"

#include <cstddef>
#include <cstdint>

#include <optional>
#include <string>

#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariant>

#include "RtspServerConfig.h"
#include "RtspStreamFeeder.h"

namespace RtspServerApp {

namespace {

    constexpr const char* kOrganization { "PelcoD" };
    constexpr const char* kApplication { "RtspServerApp" };
    constexpr const char* kPresetsGroup { "Presets" };

} // namespace

ServerPresetManager::ServerPresetManager()
{
    initDefaultPresets();
}

bool ServerPresetManager::savePreset(
    const QString& name, const Video::RtspServerConfig& serverCfg, const FeederConfig& feederCfg)
{
    if (name.trimmed().isEmpty()) {
        return false;
    }

    QSettings settings(QString::fromLatin1(kOrganization), QString::fromLatin1(kApplication));
    settings.beginGroup(QString::fromLatin1(kPresetsGroup));
    settings.beginGroup(name);

    settings.setValue(QStringLiteral("port"), serverCfg.port);
    settings.setValue(QStringLiteral("streamPath"), QString::fromStdString(serverCfg.streamPath));
    settings.setValue(QStringLiteral("streamName"), QString::fromStdString(serverCfg.streamName));
    settings.setValue(QStringLiteral("streamInfo"), QString::fromStdString(serverCfg.streamInfo));
    settings.setValue(QStringLiteral("streamType"), static_cast<int>(serverCfg.streamType));
    settings.setValue(QStringLiteral("transport"), static_cast<int>(serverCfg.transport));
    settings.setValue(QStringLiteral("maxClients"), static_cast<qulonglong>(serverCfg.maxClients));
    settings.setValue(QStringLiteral("queueCapacity"), static_cast<qulonglong>(serverCfg.queueCapacity));

    if (serverCfg.username.has_value()) {
        settings.setValue(QStringLiteral("username"), QString::fromStdString(*serverCfg.username));
    } else {
        settings.remove(QStringLiteral("username"));
    }

    if (serverCfg.password.has_value()) {
        settings.setValue(QStringLiteral("password"), QString::fromStdString(*serverCfg.password));
    } else {
        settings.remove(QStringLiteral("password"));
    }

    settings.setValue(QStringLiteral("realm"), QString::fromStdString(serverCfg.realm));
    settings.setValue(QStringLiteral("enableMulticast"), serverCfg.enableMulticast);
    settings.setValue(QStringLiteral("multicastAddress"), QString::fromStdString(serverCfg.multicastAddress));
    settings.setValue(QStringLiteral("multicastPort"), serverCfg.multicastPort);
    settings.setValue(QStringLiteral("multicastTtl"), serverCfg.multicastTtl);

    // Feeder settings
    settings.setValue(QStringLiteral("feederMode"), static_cast<int>(feederCfg.mode));
    settings.setValue(QStringLiteral("feederFilePath"), QString::fromStdString(feederCfg.filePath));
    settings.setValue(QStringLiteral("feederLoop"), feederCfg.loopPlayback);
    settings.setValue(QStringLiteral("feederFps"), feederCfg.targetFps);

    settings.endGroup();
    settings.endGroup();
    settings.sync();
    return true;
}

bool ServerPresetManager::loadPreset(
    const QString& name, Video::RtspServerConfig& serverCfg, FeederConfig& feederCfg) const
{
    QSettings settings(QString::fromLatin1(kOrganization), QString::fromLatin1(kApplication));
    settings.beginGroup(QString::fromLatin1(kPresetsGroup));

    if (!settings.childGroups().contains(name)) {
        settings.endGroup();
        return false;
    }

    settings.beginGroup(name);

    serverCfg.port = static_cast<std::uint16_t>(settings.value(QStringLiteral("port"), 8554).toUInt());
    serverCfg.streamPath
        = settings.value(QStringLiteral("streamPath"), QStringLiteral("live")).toString().toStdString();
    serverCfg.streamName
        = settings.value(QStringLiteral("streamName"), QStringLiteral("Live Stream")).toString().toStdString();
    serverCfg.streamInfo
        = settings.value(QStringLiteral("streamInfo"), QStringLiteral("PelcoD RTSP Server")).toString().toStdString();
    serverCfg.streamType = static_cast<Video::RtspStreamType>(settings.value(QStringLiteral("streamType"), 0).toUInt());
    serverCfg.transport
        = static_cast<Video::RtspTransportProtocol>(settings.value(QStringLiteral("transport"), 0).toUInt());
    serverCfg.maxClients = static_cast<std::size_t>(settings.value(QStringLiteral("maxClients"), 16).toULongLong());
    serverCfg.queueCapacity
        = static_cast<std::size_t>(settings.value(QStringLiteral("queueCapacity"), 30).toULongLong());

    if (settings.contains(QStringLiteral("username"))
        && !settings.value(QStringLiteral("username")).toString().isEmpty()) {
        serverCfg.username = settings.value(QStringLiteral("username")).toString().toStdString();
    } else {
        serverCfg.username = std::nullopt;
    }

    if (settings.contains(QStringLiteral("password"))
        && !settings.value(QStringLiteral("password")).toString().isEmpty()) {
        serverCfg.password = settings.value(QStringLiteral("password")).toString().toStdString();
    } else {
        serverCfg.password = std::nullopt;
    }

    serverCfg.realm
        = settings.value(QStringLiteral("realm"), QStringLiteral("PelcoD_RTSP_Server")).toString().toStdString();
    serverCfg.enableMulticast = settings.value(QStringLiteral("enableMulticast"), false).toBool();
    serverCfg.multicastAddress
        = settings.value(QStringLiteral("multicastAddress"), QStringLiteral("239.255.0.1")).toString().toStdString();
    serverCfg.multicastPort
        = static_cast<std::uint16_t>(settings.value(QStringLiteral("multicastPort"), 5004).toUInt());
    serverCfg.multicastTtl = static_cast<std::uint8_t>(settings.value(QStringLiteral("multicastTtl"), 16).toUInt());

    // Feeder settings
    feederCfg.mode = static_cast<FeederSourceMode>(settings.value(QStringLiteral("feederMode"), 0).toUInt());
    feederCfg.filePath = settings.value(QStringLiteral("feederFilePath"), QStringLiteral("")).toString().toStdString();
    feederCfg.loopPlayback = settings.value(QStringLiteral("feederLoop"), true).toBool();
    feederCfg.targetFps = settings.value(QStringLiteral("feederFps"), 30).toUInt();

    settings.endGroup();
    settings.endGroup();
    return true;
}

bool ServerPresetManager::deletePreset(const QString& name)
{
    QSettings settings(QString::fromLatin1(kOrganization), QString::fromLatin1(kApplication));
    settings.beginGroup(QString::fromLatin1(kPresetsGroup));
    if (!settings.childGroups().contains(name)) {
        settings.endGroup();
        return false;
    }
    settings.remove(name);
    settings.endGroup();
    settings.sync();
    return true;
}

QStringList ServerPresetManager::presetNames() const
{
    QSettings settings(QString::fromLatin1(kOrganization), QString::fromLatin1(kApplication));
    settings.beginGroup(QString::fromLatin1(kPresetsGroup));
    const QStringList groups = settings.childGroups();
    settings.endGroup();
    return groups;
}

void ServerPresetManager::initDefaultPresets()
{
    QSettings settings(QString::fromLatin1(kOrganization), QString::fromLatin1(kApplication));
    settings.beginGroup(QString::fromLatin1(kPresetsGroup));

    if (settings.childGroups().isEmpty()) {
        settings.endGroup();

        // Preset 1: Synthetic Colorbars
        Video::RtspServerConfig synSrv {};
        synSrv.port = 8554U;
        synSrv.streamPath = "live";
        synSrv.streamName = "Synthetic Colorbars";
        synSrv.streamInfo = "Built-in H.264 Test Pattern";
        synSrv.streamType = Video::RtspStreamType::H264ElementaryStream;
        synSrv.transport = Video::RtspTransportProtocol::All;

        FeederConfig synFeed {};
        synFeed.mode = FeederSourceMode::SyntheticPattern;
        synFeed.targetFps = 30U;
        synFeed.loopPlayback = true;

        static_cast<void>(savePreset(QStringLiteral("Synthetic Colorbars (H.264)"), synSrv, synFeed));

        // Preset 2: STANAG 4609 MPEG-TS Loop
        Video::RtspServerConfig tsSrv {};
        tsSrv.port = 8554U;
        tsSrv.streamPath = "stanag4609";
        tsSrv.streamName = "STANAG 4609 Aerial Flight";
        tsSrv.streamInfo = "MPEG-2 TS with MISB ST 0601 Telemetry";
        tsSrv.streamType = Video::RtspStreamType::Mpeg2TransportStream;
        tsSrv.transport = Video::RtspTransportProtocol::All;

        FeederConfig tsFeed {};
        tsFeed.mode = FeederSourceMode::MpegTsFile;
        tsFeed.filePath = "sample-videos/mpegts-klv-day-flight.ts";
        tsFeed.targetFps = 30U;
        tsFeed.loopPlayback = true;

        static_cast<void>(savePreset(QStringLiteral("STANAG 4609 Aerial Flight (TS)"), tsSrv, tsFeed));

        // Preset 3: Authenticated TCP Broadcast
        Video::RtspServerConfig authSrv {};
        authSrv.port = 8554U;
        authSrv.streamPath = "secure";
        authSrv.streamName = "Secure Tactical Feed";
        authSrv.streamInfo = "Digest Authenticated Stream";
        authSrv.streamType = Video::RtspStreamType::H264ElementaryStream;
        authSrv.transport = Video::RtspTransportProtocol::TcpOnly;
        authSrv.username = "operator";
        authSrv.password = "tactical123";

        FeederConfig authFeed {};
        authFeed.mode = FeederSourceMode::SyntheticPattern;
        authFeed.targetFps = 30U;
        authFeed.loopPlayback = true;

        static_cast<void>(savePreset(QStringLiteral("Secure Stream (TCP / Auth)"), authSrv, authFeed));
    } else {
        settings.endGroup();
    }
}

} // namespace RtspServerApp
