#pragma once

/// @file PresetManager.h
/// @brief Factory stabilization presets and JSON configuration manager.

#include <QObject>
#include <QString>
#include <QStringList>

#include "VideoStabilizerEngine.h"

namespace StabilizerApp {

/// @class PresetManager
/// @brief Manages factory configuration presets and JSON configuration import/export.
class PresetManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(QStringList presetNames READ presetNames CONSTANT)

public:
    explicit PresetManager(QObject* parent = nullptr);
    ~PresetManager() override = default;

    PresetManager(const PresetManager&) = delete;
    PresetManager& operator=(const PresetManager&) = delete;
    PresetManager(PresetManager&&) = delete;
    PresetManager& operator=(PresetManager&&) = delete;

    /// @brief List of available preset profile names.
    [[nodiscard]] QStringList presetNames() const;

    /// @brief Retrieves a named preset configuration.
    /// @param name Preset name ("Drone Gimbal", "Pole Vibration", etc.)
    /// @param outConfig Receives configuration parameters.
    /// @return true if found; false if invalid preset name.
    [[nodiscard]] bool getPreset(const QString& name, Stabilizer::VideoStabilizerConfig& outConfig) const;

    /// @brief Exports a configuration to JSON file.
    /// @param filePath Destination file path.
    /// @param config Configuration to serialize.
    /// @return true on success; false on filesystem or JSON error.
    [[nodiscard]] static bool exportJson(const QString& filePath, const Stabilizer::VideoStabilizerConfig& config);

    /// @brief Imports a configuration from JSON file.
    /// @param filePath Source file path.
    /// @param outConfig Receives validated configuration.
    /// @return true on success; false on parse error or invariant violation.
    [[nodiscard]] static bool importJson(const QString& filePath, Stabilizer::VideoStabilizerConfig& outConfig);
};

} // namespace StabilizerApp
