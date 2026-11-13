#pragma once

/// @file PresentationTypes.h
/// @brief Presentation enums and display definitions for QML layer.

#include <cstdint>

#include <QObject>

namespace LensApp {

/// @class PresentationTypes
/// @brief Scoped container for QML-exported domain enums.
class PresentationTypes {
    Q_GADGET

public:
    enum class PlaybackState : std::uint8_t { Idle = 0U, Opening, Playing, Paused, Error };
    Q_ENUM(PlaybackState)

    enum class LensModeQml : std::uint8_t {
        ManualJog = 0U,
        AutoFocusOnce = 1U,
        AutoFocusContinuous = 2U,
        ZoomTrackOnly = 3U,
        Calibrating = 4U,
        Idle = 5U
    };
    Q_ENUM(LensModeQml)

    enum class LimitFlagBits : std::uint8_t {
        None = 0x00U,
        NearLimit = 0x01U,
        FarLimit = 0x02U,
        WideLimit = 0x04U,
        TeleLimit = 0x08U
    };
    Q_ENUM(LimitFlagBits)
};

} // namespace LensApp
