#pragma once

/// @file DeviceEnumerator.h
/// @brief Cross-platform hardware video capture device enumeration.

#include "DecoderTypes.h"
#include <vector>

namespace Video {

/// @class DeviceEnumerator
/// @brief Discovers local hardware video input devices (DirectShow on Windows, V4L2 on Linux).
class DeviceEnumerator {
public:
    /// @brief Enumerates connected video capture devices.
    /// @return Vector of discovered VideoDeviceInfo descriptors.
    [[nodiscard]] static std::vector<VideoDeviceInfo> enumerateDevices();
};

} // namespace Video
