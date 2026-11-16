#pragma once

/// @file LensPresetManager.h
/// @brief Management and JSON persistence of optical lens presets across multiple camera channels.

#include <cstddef>
#include <cstdint>

#include <array>

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace LensApp {

/// @struct LensPreset
/// @brief Optical configuration profile for a specific preset slot.
struct LensPreset {
    int id { 1 };
    int channel { 0 };
    QString name { QStringLiteral("Preset") };
    std::uint16_t focusPosition { 0U };
    std::uint16_t zoomPosition { 0U };
    std::uint8_t focusSpeed { 127U };
    std::uint8_t zoomSpeed { 127U };
    int afAlgorithm { 1 };
    int focusMetric { 0 };
    QString description { };
    std::uint64_t timestampMs { 0U };
    bool isValid { false };
};

/// @class LensPresetManager
/// @brief Manages factory presets, user configurations, and JSON persistence.
class LensPresetManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(int activeChannel READ activeChannel WRITE setActiveChannel NOTIFY activeChannelChanged)
    Q_PROPERTY(QVariantList currentChannelPresets READ currentChannelPresets NOTIFY presetsChanged)

public:
    static constexpr std::size_t ChannelCount { 4U };
    static constexpr std::size_t PresetsPerChannel { 8U };

    explicit LensPresetManager(QObject* parent = nullptr);
    ~LensPresetManager() override = default;

    LensPresetManager(const LensPresetManager&) = delete;
    LensPresetManager& operator=(const LensPresetManager&) = delete;
    LensPresetManager(LensPresetManager&&) = delete;
    LensPresetManager& operator=(LensPresetManager&&) = delete;

    [[nodiscard]] int activeChannel() const noexcept;
    [[nodiscard]] QVariantList currentChannelPresets() const;

    /// @brief Retrieve preset by slot ID (1..8) for active channel.
    [[nodiscard]] bool getPreset(int id, LensPreset& outPreset) const;

    /// @brief Retrieve preset as QVariantMap for QML consumption.
    [[nodiscard]] Q_INVOKABLE QVariantMap getPresetMap(int id) const;

    /// @brief Save or update preset slot.
    Q_INVOKABLE bool savePreset(
        int id, const QString& name, int focusPos, int zoomPos, int focusSpd, int zoomSpd, int afAlgo, int metric);

    /// @brief Reset a preset slot to invalid/empty.
    Q_INVOKABLE bool deletePreset(int id);

    /// @brief Reset presets for active channel to factory tactical defaults.
    Q_INVOKABLE void resetFactoryDefaults();

    /// @brief Export presets for active channel to JSON file.
    [[nodiscard]] Q_INVOKABLE bool exportJson(const QString& filePath) const;

    /// @brief Import presets from JSON file for active channel.
    [[nodiscard]] Q_INVOKABLE bool importJson(const QString& filePath);

public slots:
    void setActiveChannel(int channel);

signals:
    void activeChannelChanged();
    void presetsChanged();
    void presetAnnounced(const QString& message, bool isError);

private:
    void initFactoryDefaults();

    int m_activeChannel { 0 };
    std::array<std::array<LensPreset, PresetsPerChannel>, ChannelCount> m_presets { };
};

} // namespace LensApp
