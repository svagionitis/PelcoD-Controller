#pragma once

/// @file SightlineRecording.h
/// @brief Sightline SLA Recording Module (SD card video recording and snapshots).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__record.html

#include "../SightlineTypes.h"

#include <cstdint>
#include <string>

namespace Sightline {

/// @struct MsgSetSDRecordingParameters
/// @brief Onboard SD card video recording control (Message ID 0x1E).
struct MsgSetSDRecordingParameters {
    std::uint8_t recordingState { 0U }; // 0: Stop, 1: Start, 2: Snapshot
    std::uint8_t cameraIndex { 0U };
    std::string filenamePrefix {};
};

} // namespace Sightline
