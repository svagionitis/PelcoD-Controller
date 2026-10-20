#pragma once

/// @file Live555Mp2tLiveSource.h
/// @brief Live555 FramedSource subclass serving 188-byte MPEG-2 TS packets from Live555FrameQueue.

#if defined(PELCOD_HAS_LIVE555)

#include "Live555FrameTypes.h"

#include <cstddef>
#include <cstdint>

#include <functional>
#include <memory>
#include <optional>

#include <liveMedia.hh>

namespace Video {

class FrameQueueReader;
class Live555FrameQueue;

/// @class Live555Mp2tLiveSource
/// @brief Custom Live555 FramedSource delivering MPEG-2 TS packets formatted for RFC 2250 RTP.
/// @details Slices incoming multi-packet TS buffers into chunks of up to 7 TS packets (1316 bytes),
///          guaranteeing strict 188-byte alignment for SimpleRTPSink.
class Live555Mp2tLiveSource : public FramedSource {
public:
    /// @brief Callback signature invoked upon source destruction.
    using ClosedCallback = std::function<void(Live555Mp2tLiveSource*)>;

    /// @brief Factory creating a new Live555Mp2tLiveSource instance.
    /// @param[in] env Live555 usage environment.
    /// @param[in] queue Shared frame queue providing live TS packets.
    /// @param[in] syncToKeyframe True if reader should synchronize to newest PAT/PMT/IDR frame.
    /// @return Pointer to newly allocated Live555Mp2tLiveSource.
    [[nodiscard]] static Live555Mp2tLiveSource* createNew(
        UsageEnvironment& env, Live555FrameQueue& queue, bool syncToKeyframe = true);

    /// @brief Registers a callback invoked upon source destruction.
    /// @param[in] callback Function to call upon destruction.
    void setClosedCallback(ClosedCallback callback);

    /// @brief Retrieves the Live555 event trigger ID for asynchronous notification.
    /// @return EventTriggerId assigned to this source.
    [[nodiscard]] EventTriggerId eventTriggerId() const noexcept;

    /// @brief Delivers the next available TS chunk to the downstream RTP sink.
    /// @details Safe to invoke from the Live555 event loop thread.
    void deliverFrame();

    /// @brief Static bridge callback invoked by TaskScheduler::triggerEvent.
    /// @param[in] clientData Pointer to Live555Mp2tLiveSource instance.
    static void deliverFrame0(void* clientData);

    /// @brief Checks if downstream sink is actively awaiting frame delivery.
    /// @return True if awaiting data.
    [[nodiscard]] bool isAwaitingData() const noexcept;

    /// @brief Checks if the source has buffered TS packets remaining from a previous frame.
    /// @return True if partial buffer is pending delivery.
    [[nodiscard]] bool hasBufferedData() const noexcept;

    /// @brief Returns maximum frame size supported (1316 bytes for 7 TS packets).
    /// @return Maximum frame size in bytes.
    [[nodiscard]] unsigned maxFrameSize() const override;

protected:
    /// @brief Protected constructor per Live555 Medium conventions.
    /// @param[in] env Live555 usage environment.
    /// @param[in] queue Shared frame queue providing live TS packets.
    /// @param[in] syncToKeyframe True if reader should synchronize to newest PAT/PMT/IDR frame.
    Live555Mp2tLiveSource(UsageEnvironment& env, Live555FrameQueue& queue, bool syncToKeyframe);

    /// @brief Destructor releasing reader, event triggers, and pending tasks.
    ~Live555Mp2tLiveSource() override;

    /// @brief Initiates frame acquisition requested by downstream sink.
    void doGetNextFrame() override;

private:
    std::unique_ptr<FrameQueueReader> m_reader { nullptr };
    EventTriggerId m_eventTriggerId { 0U };
    bool m_isAwaitingData { false };
    std::optional<QueuedFrame> m_pendingFrame { std::nullopt };
    std::size_t m_currentOffset { 0U };
    ClosedCallback m_closedCallback {};
};

} // namespace Video

#endif // PELCOD_HAS_LIVE555
