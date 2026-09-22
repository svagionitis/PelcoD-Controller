#pragma once

/// @file DecoderFactory.h
/// @brief Factory class to instantiate video decoder backends.

#include "DecoderTypes.h"
#include "IVideoDecoder.h"

#include <memory>
#include <vector>

namespace Video {

/// @class DecoderFactory
/// @brief Instantiates concrete IVideoDecoder implementations (FFmpeg, GStreamer, Mock).
class DecoderFactory {
public:
    /// @brief Creates an IVideoDecoder instance for the requested backend.
    /// @param[in] backend Backend engine to instantiate.
    /// @return Unique pointer to decoder, or nullptr if backend unsupported.
    static std::unique_ptr<IVideoDecoder> create(BackendType backend = BackendType::FFmpeg);

    /// @brief Queries list of all currently compiled and available decoding backends.
    /// @return Vector of available BackendType enumerations.
    [[nodiscard]] static std::vector<BackendType> availableBackends();
};

} // namespace Video
