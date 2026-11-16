/// @file LensPresetManager.cpp
/// @brief Implementation of optical preset manager and JSON serialization.

#include "LensPresetManager.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <chrono>

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace LensApp {

namespace {

    QString getAlgorithmName(int algo)
    {
        switch (algo) {
        case 0:
            return QStringLiteral("SeekAdaptive");
        case 1:
            return QStringLiteral("ScanThreePhase");
        case 2:
            return QStringLiteral("GoldenSection");
        case 3:
            return QStringLiteral("AdaptiveMomentum");
        case 4:
            return QStringLiteral("DctFrequency");
        case 5:
            return QStringLiteral("BuiltInDriver");
        default:
            return QStringLiteral("ScanThreePhase");
        }
    }

    QString getMetricName(int metric)
    {
        switch (metric) {
        case 0:
            return QStringLiteral("SobelGradientSquared");
        case 1:
            return QStringLiteral("DiscreteLaplacian");
        case 2:
            return QStringLiteral("TenengradThreshold");
        case 3:
            return QStringLiteral("VollathCorrelationF4");
        case 4:
            return QStringLiteral("VollathCorrelationF5");
        case 5:
            return QStringLiteral("DctHighFrequency");
        default:
            return QStringLiteral("SobelGradientSquared");
        }
    }

} // namespace

LensPresetManager::LensPresetManager(QObject* parent)
    : QObject(parent)
{
    initFactoryDefaults();
}

void LensPresetManager::initFactoryDefaults()
{
    for (std::size_t ch = 0U; ch < ChannelCount; ++ch) {
        // Preset 1: Wide Surveillance
        m_presets[ch][0] = LensPreset { 1, static_cast<int>(ch), QStringLiteral("Wide Surveillance"), 4200U, 1000U,
            200U, 200U, 1, 0, QStringLiteral("Perimeter wide-angle overview"),
            static_cast<std::uint64_t>(QDateTime::currentMSecsSinceEpoch()), true };

        // Preset 2: Mid Recon
        m_presets[ch][1] = LensPreset { 2, static_cast<int>(ch), QStringLiteral("Mid Recon"), 5400U, 20000U, 180U, 180U,
            1, 1, QStringLiteral("Medium-range tactical reconnaissance"),
            static_cast<std::uint64_t>(QDateTime::currentMSecsSinceEpoch()), true };

        // Preset 3: Tele Target
        m_presets[ch][2] = LensPreset { 3, static_cast<int>(ch), QStringLiteral("Tele Target"), 7100U, 45000U, 140U,
            140U, 0, 2, QStringLiteral("Long-range target identification"),
            static_cast<std::uint64_t>(QDateTime::currentMSecsSinceEpoch()), true };

        // Preset 4: Extreme Spotter
        m_presets[ch][3] = LensPreset { 4, static_cast<int>(ch), QStringLiteral("Extreme Spotter"), 8600U, 62000U, 100U,
            100U, 2, 3, QStringLiteral("Maximum optical spotter magnification"),
            static_cast<std::uint64_t>(QDateTime::currentMSecsSinceEpoch()), true };

        // Presets 5..8: Unassigned user slots
        for (std::size_t slot = 4U; slot < PresetsPerChannel; ++slot) {
            m_presets[ch][slot] = LensPreset { static_cast<int>(slot + 1U), static_cast<int>(ch),
                QStringLiteral("Preset %1").arg(slot + 1U), 0U, 0U, 127U, 127U, 1, 0,
                QStringLiteral("Unassigned user slot"), 0U, false };
        }
    }
}

int LensPresetManager::activeChannel() const noexcept
{
    return m_activeChannel;
}

void LensPresetManager::setActiveChannel(int channel)
{
    if (channel < 0 || channel >= static_cast<int>(ChannelCount)) {
        return;
    }
    if (m_activeChannel != channel) {
        m_activeChannel = channel;
        emit activeChannelChanged();
        emit presetsChanged();
    }
}

QVariantList LensPresetManager::currentChannelPresets() const
{
    QVariantList list;
    list.reserve(static_cast<int>(PresetsPerChannel));

    const auto& channelSlots = m_presets[static_cast<std::size_t>(m_activeChannel)];
    for (std::size_t i = 0U; i < PresetsPerChannel; ++i) {
        const auto& p = channelSlots[i];
        QVariantMap map;
        map[QStringLiteral("id")] = p.id;
        map[QStringLiteral("channel")] = p.channel;
        map[QStringLiteral("name")] = p.name;
        map[QStringLiteral("focusPosition")] = static_cast<int>(p.focusPosition);
        map[QStringLiteral("zoomPosition")] = static_cast<int>(p.zoomPosition);
        map[QStringLiteral("focusSpeed")] = static_cast<int>(p.focusSpeed);
        map[QStringLiteral("zoomSpeed")] = static_cast<int>(p.zoomSpeed);
        map[QStringLiteral("afAlgorithm")] = p.afAlgorithm;
        map[QStringLiteral("afAlgorithmName")] = getAlgorithmName(p.afAlgorithm);
        map[QStringLiteral("focusMetric")] = p.focusMetric;
        map[QStringLiteral("focusMetricName")] = getMetricName(p.focusMetric);
        map[QStringLiteral("description")] = p.description;
        map[QStringLiteral("timestampMs")] = static_cast<double>(p.timestampMs);
        map[QStringLiteral("isValid")] = p.isValid;
        list.append(map);
    }
    return list;
}

bool LensPresetManager::getPreset(int id, LensPreset& outPreset) const
{
    if (id < 1 || id > static_cast<int>(PresetsPerChannel)) {
        return false;
    }
    const std::size_t idx = static_cast<std::size_t>(id - 1);
    const auto& p = m_presets[static_cast<std::size_t>(m_activeChannel)][idx];
    if (!p.isValid) {
        return false;
    }
    outPreset = p;
    return true;
}

QVariantMap LensPresetManager::getPresetMap(int id) const
{
    QVariantMap map;
    if (id < 1 || id > static_cast<int>(PresetsPerChannel)) {
        return map;
    }
    const std::size_t idx = static_cast<std::size_t>(id - 1);
    const auto& p = m_presets[static_cast<std::size_t>(m_activeChannel)][idx];

    map[QStringLiteral("id")] = p.id;
    map[QStringLiteral("channel")] = p.channel;
    map[QStringLiteral("name")] = p.name;
    map[QStringLiteral("focusPosition")] = static_cast<int>(p.focusPosition);
    map[QStringLiteral("zoomPosition")] = static_cast<int>(p.zoomPosition);
    map[QStringLiteral("focusSpeed")] = static_cast<int>(p.focusSpeed);
    map[QStringLiteral("zoomSpeed")] = static_cast<int>(p.zoomSpeed);
    map[QStringLiteral("afAlgorithm")] = p.afAlgorithm;
    map[QStringLiteral("afAlgorithmName")] = getAlgorithmName(p.afAlgorithm);
    map[QStringLiteral("focusMetric")] = p.focusMetric;
    map[QStringLiteral("focusMetricName")] = getMetricName(p.focusMetric);
    map[QStringLiteral("description")] = p.description;
    map[QStringLiteral("isValid")] = p.isValid;
    return map;
}

bool LensPresetManager::savePreset(
    int id, const QString& name, int focusPos, int zoomPos, int focusSpd, int zoomSpd, int afAlgo, int metric)
{
    if (id < 1 || id > static_cast<int>(PresetsPerChannel)) {
        emit presetAnnounced(QStringLiteral("Invalid preset slot ID: %1").arg(id), true);
        return false;
    }

    const std::size_t idx = static_cast<std::size_t>(id - 1);
    auto& p = m_presets[static_cast<std::size_t>(m_activeChannel)][idx];

    p.id = id;
    p.channel = m_activeChannel;
    p.name = name.trimmed().isEmpty() ? QStringLiteral("Preset %1").arg(id) : name.trimmed();
    p.focusPosition = static_cast<std::uint16_t>(std::clamp(focusPos, 0, 65535));
    p.zoomPosition = static_cast<std::uint16_t>(std::clamp(zoomPos, 0, 65535));
    p.focusSpeed = static_cast<std::uint8_t>(std::clamp(focusSpd, 1, 255));
    p.zoomSpeed = static_cast<std::uint8_t>(std::clamp(zoomSpd, 1, 255));
    p.afAlgorithm = std::clamp(afAlgo, 0, 5);
    p.focusMetric = std::clamp(metric, 0, 5);
    p.description = QStringLiteral("Saved on %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    p.timestampMs = static_cast<std::uint64_t>(QDateTime::currentMSecsSinceEpoch());
    p.isValid = true;

    emit presetsChanged();
    emit presetAnnounced(QStringLiteral("Preset %1 ('%2') saved successfully").arg(id).arg(p.name), false);
    return true;
}

bool LensPresetManager::deletePreset(int id)
{
    if (id < 1 || id > static_cast<int>(PresetsPerChannel)) {
        return false;
    }

    const std::size_t idx = static_cast<std::size_t>(id - 1);
    auto& p = m_presets[static_cast<std::size_t>(m_activeChannel)][idx];
    p.name = QStringLiteral("Preset %1").arg(id);
    p.focusPosition = 0U;
    p.zoomPosition = 0U;
    p.focusSpeed = 127U;
    p.zoomSpeed = 127U;
    p.description.clear();
    p.timestampMs = 0U;
    p.isValid = false;

    emit presetsChanged();
    emit presetAnnounced(QStringLiteral("Preset %1 cleared").arg(id), false);
    return true;
}

void LensPresetManager::resetFactoryDefaults()
{
    initFactoryDefaults();
    emit presetsChanged();
    emit presetAnnounced(QStringLiteral("Factory default presets restored"), false);
}

bool LensPresetManager::exportJson(const QString& filePath) const
{
    if (filePath.trimmed().isEmpty()) {
        return false;
    }

    QJsonObject root;
    root[QStringLiteral("schemaVersion")] = 1;
    root[QStringLiteral("exportedAt")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    root[QStringLiteral("channel")] = m_activeChannel;

    QJsonArray presetsArr;
    const auto& channelSlots = m_presets[static_cast<std::size_t>(m_activeChannel)];
    for (std::size_t i = 0U; i < PresetsPerChannel; ++i) {
        const auto& p = channelSlots[i];
        QJsonObject obj;
        obj[QStringLiteral("id")] = p.id;
        obj[QStringLiteral("name")] = p.name;
        obj[QStringLiteral("focusPosition")] = static_cast<int>(p.focusPosition);
        obj[QStringLiteral("zoomPosition")] = static_cast<int>(p.zoomPosition);
        obj[QStringLiteral("focusSpeed")] = static_cast<int>(p.focusSpeed);
        obj[QStringLiteral("zoomSpeed")] = static_cast<int>(p.zoomSpeed);
        obj[QStringLiteral("afAlgorithm")] = p.afAlgorithm;
        obj[QStringLiteral("focusMetric")] = p.focusMetric;
        obj[QStringLiteral("description")] = p.description;
        obj[QStringLiteral("timestampMs")] = static_cast<double>(p.timestampMs);
        obj[QStringLiteral("isValid")] = p.isValid;
        presetsArr.append(obj);
    }
    root[QStringLiteral("presets")] = presetsArr;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    const QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool LensPresetManager::importJson(const QString& filePath)
{
    if (filePath.trimmed().isEmpty()) {
        emit presetAnnounced(QStringLiteral("File path cannot be empty"), true);
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit presetAnnounced(QStringLiteral("Failed to open file: %1").arg(filePath), true);
        return false;
    }

    QJsonParseError parseError { };
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        emit presetAnnounced(QStringLiteral("JSON parse error: %1").arg(parseError.errorString()), true);
        return false;
    }

    const QJsonObject root = doc.object();
    if (!root.contains(QStringLiteral("presets")) || !root[QStringLiteral("presets")].isArray()) {
        emit presetAnnounced(QStringLiteral("Invalid JSON: missing 'presets' array"), true);
        return false;
    }

    const QJsonArray presetsArr = root[QStringLiteral("presets")].toArray();
    auto& channelSlots = m_presets[static_cast<std::size_t>(m_activeChannel)];

    for (const auto& val : presetsArr) {
        if (!val.isObject()) {
            continue;
        }
        const QJsonObject obj = val.toObject();
        const int id = obj[QStringLiteral("id")].toInt(0);
        if (id < 1 || id > static_cast<int>(PresetsPerChannel)) {
            continue;
        }

        const std::size_t idx = static_cast<std::size_t>(id - 1);
        auto& p = channelSlots[idx];
        p.id = id;
        p.channel = m_activeChannel;
        p.name = obj[QStringLiteral("name")].toString(QStringLiteral("Preset %1").arg(id));
        p.focusPosition
            = static_cast<std::uint16_t>(std::clamp(obj[QStringLiteral("focusPosition")].toInt(0), 0, 65535));
        p.zoomPosition = static_cast<std::uint16_t>(std::clamp(obj[QStringLiteral("zoomPosition")].toInt(0), 0, 65535));
        p.focusSpeed = static_cast<std::uint8_t>(std::clamp(obj[QStringLiteral("focusSpeed")].toInt(127), 1, 255));
        p.zoomSpeed = static_cast<std::uint8_t>(std::clamp(obj[QStringLiteral("zoomSpeed")].toInt(127), 1, 255));
        p.afAlgorithm = std::clamp(obj[QStringLiteral("afAlgorithm")].toInt(1), 0, 5);
        p.focusMetric = std::clamp(obj[QStringLiteral("focusMetric")].toInt(0), 0, 5);
        p.description = obj[QStringLiteral("description")].toString();
        p.timestampMs = static_cast<std::uint64_t>(obj[QStringLiteral("timestampMs")].toDouble(0.0));
        p.isValid = obj[QStringLiteral("isValid")].toBool(true);
    }

    emit presetsChanged();
    emit presetAnnounced(QStringLiteral("Presets successfully imported from %1").arg(filePath), false);
    return true;
}

} // namespace LensApp
