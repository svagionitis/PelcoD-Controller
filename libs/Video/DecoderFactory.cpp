#include "DecoderFactory.h"
#include "MockVideoDecoder.h"

#if defined(PELCOD_HAS_FFMPEG)
#include "FFmpegDecoder.h"
#endif

#if defined(PELCOD_HAS_GSTREAMER)
#include "GStreamerDecoder.h"
#endif

#include <glog/logging.h>

namespace Video {

std::unique_ptr<IVideoDecoder> DecoderFactory::create(BackendType backend)
{
    switch (backend) {
    case BackendType::Mock:
        return std::make_unique<MockVideoDecoder>();

    case BackendType::FFmpeg:
#if defined(PELCOD_HAS_FFMPEG)
        return std::make_unique<FFmpegDecoder>();
#else
        LOG(WARNING) << "DecoderFactory: FFmpeg backend not compiled. Falling back to Mock decoder.";
        return std::make_unique<MockVideoDecoder>();
#endif

    case BackendType::GStreamer:
#if defined(PELCOD_HAS_GSTREAMER)
        return std::make_unique<GStreamerDecoder>();
#else
        LOG(WARNING) << "DecoderFactory: GStreamer backend not compiled. Falling back to Mock decoder.";
        return std::make_unique<MockVideoDecoder>();
#endif

    default:
        LOG(ERROR) << "DecoderFactory: Unknown backend requested. Returning nullptr.";
        return nullptr;
    }
}

std::vector<BackendType> DecoderFactory::availableBackends()
{
    std::vector<BackendType> backends;
#if defined(PELCOD_HAS_FFMPEG)
    backends.push_back(BackendType::FFmpeg);
#endif
#if defined(PELCOD_HAS_GSTREAMER)
    backends.push_back(BackendType::GStreamer);
#endif
    backends.push_back(BackendType::Mock);
    return backends;
}

} // namespace Video
