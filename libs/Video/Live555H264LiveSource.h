#pragma once

/// @file Live555H264LiveSource.h
/// @brief Live555 FramedSource subclass serving live H.264 NAL units from Live555FrameQueue.

#if defined(PELCOD_HAS_LIVE555)

#include <cstddef>
#include <cstdint>

#include <memory>

#include <liveMedia.hh>

namespace Video {

class FrameQueueReader;
class Live555FrameQueue;

/// @class Live555H264LiveSource
/// @brief Custom Live555 FramedSource reading H.264 frames from Live555FrameQueue.
/// @details Strips Annex-B start codes, sets presentation timestamps, handles truncation,
///          and coordinates with Live555 TaskScheduler via event triggers.
class Live555H264LiveSource : public FramedSource {
public:
    /// @brief Factory creating a new Live555H264LiveSource instance.
    /// @param[in] env Live555 usage environment.
    /// @param[in] queue Shared frame queue providing live frames.
    /// @param[in] syncToKeyframe True if reader should synchronize to newest keyframe first.
    /// @return Pointer to newly allocated Live555H264LiveSource.
    [[nodiscard]] static Live555H264LiveSource* createNew(
        UsageEnvironment& env, Live555FrameQueue& queue, bool syncToKeyframe = true);

    /// @brief Retrieves the Live555 event trigger ID for asynchronous notification.
    /// @return EventTriggerId assigned to this source.
    [[nodiscard]] EventTriggerId eventTriggerId() const noexcept;

    /// @brief Delivers the next available frame to the downstream Live555 sink/framer.
    /// @details Safe to invoke from the Live555 event loop thread.
    void deliverFrame();

    /// @brief Static bridge callback invoked by TaskScheduler::triggerEvent.
    /// @param[in] clientData Pointer to Live555H264LiveSource instance.
    static void deliverFrame0(void* clientData);

    /// @brief Checks if downstream sink is actively awaiting frame delivery.
    /// @return True if awaiting data.
    [[nodiscard]] bool isAwaitingData() const noexcept;

    /// @brief Returns maximum frame size supported (defaults to 300 KB).
    /// @return Maximum frame size in bytes.
    [[nodiscard]] unsigned maxFrameSize() const override;

protected:
    /// @brief Protected constructor per Live555 Medium conventions.
    /// @param[in] env Live555 usage environment.
    /// @param[in] queue Shared frame queue providing live frames.
    /// @param[in] syncToKeyframe True if reader should synchronize to newest keyframe first.
    Live555H264LiveSource(UsageEnvironment& env, Live555FrameQueue& queue, bool syncToKeyframe);

    /// @brief Destructor releasing reader, event triggers, and pending tasks.
    ~Live555H264LiveSource() override;

    /// @brief Initiates frame acquisition requested by downstream sink.
    void doGetNextFrame() override;

private:
    std::unique_ptr<FrameQueueReader> m_reader { nullptr };
    EventTriggerId m_eventTriggerId { 0U };
    bool m_isAwaitingData { false };
};

} // namespace Video

#endif // PELCOD_HAS_LIVE555
