#pragma once

/// @file ServerPresetManager.h
/// @brief Persistent preset management for RTSP server and media feeder configurations.

#include <cstddef>
#include <cstdint>

#include <string>

#include <QObject>
#include <QString>
#include <QStringList>

#include "RtspServerConfig.h"
#include "RtspStreamFeeder.h"

namespace RtspServerApp {

/// @class ServerPresetManager
/// @brief Manages storage, retrieval, and presets for RTSP streaming settings via QSettings.
class ServerPresetManager final {
public:
    /// @brief Constructs preset manager and initializes default built-in presets if empty.
    ServerPresetManager();

    ~ServerPresetManager() = default;

    ServerPresetManager(const ServerPresetManager&) = delete;
    ServerPresetManager& operator=(const ServerPresetManager&) = delete;
    ServerPresetManager(ServerPresetManager&&) = delete;
    ServerPresetManager& operator=(ServerPresetManager&&) = delete;

    /// @brief Saves server and feeder configuration under named preset.
    /// @param[in] name Preset identifier.
    /// @param[in] serverCfg RTSP server configuration.
    /// @param[in] feederCfg Media feeder configuration.
    /// @return True if saved successfully.
    [[nodiscard]] bool savePreset(
        const QString& name, const Video::RtspServerConfig& serverCfg, const FeederConfig& feederCfg);

    /// @brief Loads named configuration preset.
    /// @param[in] name Preset identifier.
    /// @param[out] serverCfg Destination server configuration.
    /// @param[out] feederCfg Destination feeder configuration.
    /// @return True if loaded successfully.
    [[nodiscard]] bool loadPreset(
        const QString& name, Video::RtspServerConfig& serverCfg, FeederConfig& feederCfg) const;

    /// @brief Deletes named preset.
    /// @param[in] name Preset identifier.
    /// @return True if deleted.
    [[nodiscard]] bool deletePreset(const QString& name);

    /// @brief Lists all available preset identifiers.
    /// @return List of preset names.
    [[nodiscard]] QStringList presetNames() const;

private:
    void initDefaultPresets();
};

} // namespace RtspServerApp
