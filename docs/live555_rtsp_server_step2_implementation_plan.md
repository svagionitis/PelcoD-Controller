# Implementation Plan: Step 2 - Custom Live555 FramedSource Subclasses (Live555 RTSP Server)

## Executive Summary
This document specifies the detailed technical design, frame extraction algorithms, packetization mechanisms, and step-by-step implementation tasks for **Step 2: Custom Live555 `FramedSource` Subclasses** of the [Live555 RTSP Streaming Server](live555_rtsp_server_implementation_plan.md).

Step 2 delivers two dedicated Live555 streaming source classes in `libs/Video`:
1. [Live555H264LiveSource](../libs/Video/Live555H264LiveSource.h): Subclasses `FramedSource` to ingest raw H.264 NAL units from [Live555FrameQueue](../libs/Video/Live555FrameQueue.h), strip Annex-B start codes, populate presentation timestamps, and feed downstream discrete framers (`H264VideoStreamDiscreteFramer`) and RTP packetizers (`H264VideoRTPSink`).
2. [Live555Mp2tLiveSource](../libs/Video/Live555Mp2tLiveSource.h): Subclasses `FramedSource` to ingest MPEG-2 Transport Stream (MP2T) packets (such as STANAG 4609 carrying multiplexed H.264 video and MISB ST 0601 / ST 1607 KLV metadata), enforcing strict 188-byte packet boundary slicing and bundling up to 7 TS packets (1316 bytes) per RTP transmission to optimize Ethernet MTU efficiency without IP fragmentation (RFC 2250).

---

## 1. Architectural Overview & Data Flow

### 1.1 Ingest, Slicing, and Asynchronous Delivery Pipeline

```
+-------------------------------------------------------------------------------------------------------------------+
| PRODUCER THREADS (Camera Pipeline / STANAG 4609 MpegTsKlvMuxer)                                                  |
|   pushH264Nalu(...)                                     pushTsPackets(...)                                        |
+--------------------------│--------------------------------------------------│-------------------------------------+
                           ▼                                                  ▼
+-------------------------------------------------------------------------------------------------------------------+
| Live555FrameQueue (Circular Ring Buffer from Step 1)                                                              |
|   - Slot 1 .. Slot N: Reference-counted std::shared_ptr<const std::vector<std::uint8_t>>                          |
|   - Sequence tracking, keyframe indexing, and lagging reader overrun recovery                                    |
+--------------------------│--------------------------------------------------│-------------------------------------+
                           │ reader->getNextFrame()                           │ reader->getNextFrame()
                           ▼                                                  ▼
+--------------------------------------------------+ +--------------------------------------------------------------+
| Live555H264LiveSource                            | | Live555Mp2tLiveSource                                        |
|                                                  | |                                                              |
| [Input]: QueuedFrame (Annex-B NAL unit)          | | [Input]: QueuedFrame (N * 188-byte TS packet block)          |
| [Processing]:                                    | | [Processing]:                                                |
|   1. Strip 0x000001 or 0x00000001 start code     | |   1. Maintain internal offset in multi-TS buffer             |
|   2. Check fMaxSize, set fNumTruncatedBytes      | |   2. Slice into chunks of up to 7 TS packets (1316 bytes)    |
|   3. Copy payload to Live555 fTo buffer          | |   3. Copy exact packet multiples to Live555 fTo buffer       |
|   4. Convert ptsSeconds to fPresentationTime     | |   4. Calculate/propagate PTS to fPresentationTime            |
|   5. FramedSource::afterGetting(this)            | |   5. FramedSource::afterGetting(this)                        |
+--------------------------│-----------------------+ +------------------------│-------------------------------------+
                           ▼                                                  ▼
+--------------------------------------------------+ +--------------------------------------------------------------+
| H264VideoStreamDiscreteFramer                    | | SimpleRTPSink (RFC 2250, Payload Type 33)                    |
| (Live555 Discrete NALU Framer)                   | | (Streams 1316-byte RTP TS payloads to network)               |
+--------------------------│-----------------------+ +------------------------│-------------------------------------+
                           ▼                                                  │
+--------------------------------------------------+                          │
| H264VideoRTPSink (RFC 6184, Payload Type 96)     |                          │
| (FU-A Fragmentation & Packetization)             |                          │
+--------------------------│-----------------------+                          │
                           ▼                                                  ▼
+-------------------------------------------------------------------------------------------------------------------+
| RTSP / RTP Sockets (UDP Unicast or TCP Interleaved Channels 0/1)                                                  |
+-------------------------------------------------------------------------------------------------------------------+
```

---

### 1.2 Mermaid Sequence Diagram: Asynchronous Event Loop & Frame Delivery

```mermaid
sequenceDiagram
    autonumber
    participant Producer as Producer Thread (Video / Muxer)
    participant Queue as Live555FrameQueue
    participant Scheduler as Live555 TaskScheduler
    participant Source as Live555H264LiveSource / Live555Mp2tLiveSource
    participant Sink as Live555 RTPSink (Downstream)

    Note over Sink,Source: Client requests frame delivery
    Sink->>Source: getNextFrame(fTo, fMaxSize, afterGettingFunc, ...)
    Source->>Source: doGetNextFrame()
    alt Frame is immediately available in Queue
        Source->>Queue: reader->getNextFrame(outDropped)
        Queue-->>Source: Return QueuedFrame
        Source->>Scheduler: scheduleDelayedTask(0, deliverFrame0, this)
        Scheduler->>Source: deliverFrame()
        Source->>Sink: FramedSource::afterGetting(this)
    else Queue is empty (Starvation / Awaiting Camera)
        Source->>Source: Mark m_isAwaitingData = true
        Note over Source: Returns without calling afterGetting(); scheduler proceeds
        Producer->>Queue: push(payload, pts, isKey)
        Queue->>Scheduler: triggerEvent(eventTriggerId, clientData)
        Note over Scheduler: Live555 SingleStep() wakes up on event trigger
        Scheduler->>Source: deliverFrame0(clientData) -> deliverFrame()
        Source->>Queue: reader->getNextFrame(outDropped)
        Queue-->>Source: Return QueuedFrame
        Source->>Source: Copy to fTo, set fFrameSize, fPresentationTime
        Source->>Sink: FramedSource::afterGetting(this)
        Source->>Source: Reset m_isAwaitingData = false
    end
```

---

## 2. Technical Challenges & Design Decisions

### 2.1 Live555 Event Triggering vs. Scheduler Scalability
- **The Constraint:** Live555's default `BasicTaskScheduler0` allocates `EventTriggerId` as a 32-bit bitmask (`fTriggersAwaitingHandling`), restricting the total number of simultaneous event triggers across the entire application to 32.
- **The Architecture:**
  - Each `Live555H264LiveSource` and `Live555Mp2tLiveSource` instance creates an `EventTriggerId` upon construction:
    ```cpp
    m_eventTriggerId = envir().taskScheduler().createEventTrigger(deliverFrame0);
    ```
  - In unit tests with 1 to 4 sources, this provides complete hermetic independence without requiring an external server or dispatcher.
  - In the multi-client server (Step 4), a central event trigger or dispatcher can invoke `deliverFrame()` on all active subsessions, while individual source trigger IDs are safely reclaimed via `deleteEventTrigger()` upon client disconnect (`Medium::close()`).

### 2.2 Recursion Elimination via `scheduleDelayedTask(0)`
- Calling `FramedSource::afterGetting(this)` directly from within `doGetNextFrame()` can cause deep stack recursion:
  `doGetNextFrame()` $\rightarrow$ `afterGetting()` $\rightarrow$ `sink::afterGettingFrame()` $\rightarrow$ `source->getNextFrame()` $\rightarrow$ `doGetNextFrame()`.
- **The Solution:** When a frame is immediately available in the queue during `doGetNextFrame()`, the source delegates delivery to the next tick of the event loop:
  ```cpp
  nextTask() = envir().taskScheduler().scheduleDelayedTask(0, deliverFrame0, this);
  ```
  This caps call stack depth at 1 and eliminates risk of stack overflow during high-framerate playback.

### 2.3 Start Code Handling (RFC 6184 H.264)
- Producers often push H.264 NALUs formatted as Annex-B byte streams with start code prefixes:
  - 4-byte prefix: `0x00, 0x00, 0x00, 0x01`
  - 3-byte prefix: `0x00, 0x00, 0x01`
- Live555's `H264VideoStreamDiscreteFramer` and `H264VideoRTPSink` require raw NAL units **without** start code prefixes.
- `Live555H264LiveSource` inspects the leading bytes:
  ```cpp
  std::size_t offset { 0U };
  if ((size >= 4U) && (data[0] == 0x00U) && (data[1] == 0x00U) && (data[2] == 0x00U) && (data[3] == 0x01U)) {
      offset = 4U;
  } else if ((size >= 3U) && (data[0] == 0x00U) && (data[1] == 0x00U) && (data[2] == 0x01U)) {
      offset = 3U;
  }
  ```
  Only the actual NAL payload (`data + offset`, size `size - offset`) is copied to `fTo`.

### 2.4 MPEG-2 TS Packet Slicing & RFC 2250 MTU Alignment
- In RFC 2250, MPEG-2 Transport Stream packets must be transmitted over RTP (payload type 33) in multiples of 188 bytes.
- Standard Ethernet MTU is 1500 bytes. With IP (20 bytes), UDP (8 bytes), and RTP (12 bytes) headers, the maximum safe payload is $1500 - 40 = 1460$ bytes.
- $7 \times 188 = 1316$ bytes (fits safely within 1460 bytes). 8 packets would be 1504 bytes, forcing IP fragmentation.
- `Live555Mp2tLiveSource` accepts arbitrarily sized TS buffers from `Live555FrameQueue` (e.g., 188, 1316, 2632, or 37600 bytes) and maintains internal slice tracking (`m_currentFrame`, `m_currentOffset`).
- It extracts at most $\min(\lfloor fMaxSize / 188 \rfloor, 7) \times 188$ bytes per `doGetNextFrame()` delivery. When a multi-packet frame is only partially consumed, subsequent `doGetNextFrame()` calls continue slicing from `m_currentOffset` before pulling the next frame from the ring buffer.

---

## 3. Class Interfaces & Specifications

### 3.1 `Live555H264LiveSource.h`

```cpp
#pragma once

/// @file Live555H264LiveSource.h
/// @brief Live555 FramedSource subclass serving live H.264 NAL units from Live555FrameQueue.

#if defined(PELCOD_HAS_LIVE555)

#include <cstddef>
#include <cstdint>
#include <memory>

#include <liveMedia.hh>

namespace Video {

class Live555FrameQueue;
class FrameQueueReader;

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

protected:
    /// @brief Protected constructor per Live555 Medium conventions.
    Live555H264LiveSource(UsageEnvironment& env, Live555FrameQueue& queue, bool syncToKeyframe);

    /// @brief Destructor releasing reader, event triggers, and pending tasks.
    ~Live555H264LiveSource() override;

    /// @brief Initiates frame acquisition requested by downstream sink.
    void doGetNextFrame() override;

    /// @brief Returns maximum frame size supported (defaults to 300 KB).
    [[nodiscard]] unsigned maxFrameSize() const override;

private:
    std::unique_ptr<FrameQueueReader> m_reader { nullptr };
    EventTriggerId m_eventTriggerId { 0U };
    bool m_isAwaitingData { false };
};

} // namespace Video

#endif // PELCOD_HAS_LIVE555
```

---

### 3.2 `Live555Mp2tLiveSource.h`

```cpp
#pragma once

/// @file Live555Mp2tLiveSource.h
/// @brief Live555 FramedSource subclass serving 188-byte MPEG-2 TS packets from Live555FrameQueue.

#if defined(PELCOD_HAS_LIVE555)

#include "Live555FrameTypes.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include <liveMedia.hh>

namespace Video {

class Live555FrameQueue;
class FrameQueueReader;

/// @class Live555Mp2tLiveSource
/// @brief Custom Live555 FramedSource delivering MPEG-2 TS packets formatted for RFC 2250 RTP.
/// @details Slices incoming multi-packet TS buffers into chunks of up to 7 TS packets (1316 bytes),
///          guaranteeing strict 188-byte alignment for SimpleRTPSink.
class Live555Mp2tLiveSource : public FramedSource {
public:
    /// @brief Factory creating a new Live555Mp2tLiveSource instance.
    /// @param[in] env Live555 usage environment.
    /// @param[in] queue Shared frame queue providing live TS packets.
    /// @param[in] syncToKeyframe True if reader should synchronize to newest PAT/PMT/IDR frame.
    /// @return Pointer to newly allocated Live555Mp2tLiveSource.
    [[nodiscard]] static Live555Mp2tLiveSource* createNew(
        UsageEnvironment& env, Live555FrameQueue& queue, bool syncToKeyframe = true);

    /// @brief Retrieves the Live555 event trigger ID for asynchronous notification.
    /// @return EventTriggerId assigned to this source.
    [[nodiscard]] EventTriggerId eventTriggerId() const noexcept;

    /// @brief Delivers the next available TS chunk to the downstream RTP sink.
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

protected:
    /// @brief Protected constructor per Live555 Medium conventions.
    Live555Mp2tLiveSource(UsageEnvironment& env, Live555FrameQueue& queue, bool syncToKeyframe);

    /// @brief Destructor releasing reader, event triggers, and pending tasks.
    ~Live555Mp2tLiveSource() override;

    /// @brief Initiates frame acquisition requested by downstream sink.
    void doGetNextFrame() override;

    /// @brief Returns maximum frame size supported (1316 bytes for 7 TS packets).
    [[nodiscard]] unsigned maxFrameSize() const override;

private:
    std::unique_ptr<FrameQueueReader> m_reader { nullptr };
    EventTriggerId m_eventTriggerId { 0U };
    bool m_isAwaitingData { false };
    std::optional<QueuedFrame> m_pendingFrame { std::nullopt };
    std::size_t m_currentOffset { 0U };
};

} // namespace Video

#endif // PELCOD_HAS_LIVE555
```

---

## 4. Detailed Step-by-Step Implementation Tasks

### Task 2.1: Implement `Live555H264LiveSource.h` & `Live555H264LiveSource.cpp` [Completed]
- **Location**: `libs/Video/Live555H264LiveSource.h`, `libs/Video/Live555H264LiveSource.cpp`
- **Actions**:
  - [x] Implement `createNew()` factory allocating via `new` per Live555 pattern.
  - [x] Implement constructor creating `FrameQueueReader` from `Live555FrameQueue` and allocating `EventTriggerId`.
  - [x] Implement destructor unscheduling `nextTask()` via `envir().taskScheduler().unscheduleDelayedTask(nextTask())` and deleting `m_eventTriggerId`.
  - [x] Implement `doGetNextFrame()`:
    - If `m_reader` has a frame immediately available, schedule `deliverFrame0` via `scheduleDelayedTask(0, ...)`.
    - Otherwise, set `m_isAwaitingData = true`.
  - [x] Implement `deliverFrame()`:
    - Verify `isCurrentlyAwaitingData()`; if false, return immediately.
    - Fetch frame from `m_reader->getNextFrame(dropped)`. If none, return.
    - Inspect and strip Annex-B start codes (`00 00 00 01` or `00 00 01`).
    - Handle truncation if `naluSize > fMaxSize` (`fFrameSize = fMaxSize; fNumTruncatedBytes = naluSize - fMaxSize;`).
    - Copy data into `fTo` using `std::memmove`.
    - Compute `fPresentationTime` from `frame.ptsSeconds` or `gettimeofday()`.
    - Call `FramedSource::afterGetting(this)`.

### Task 2.2: Implement `Live555Mp2tLiveSource.h` & `Live555Mp2tLiveSource.cpp` [Completed]
- **Location**: `libs/Video/Live555Mp2tLiveSource.h`, `libs/Video/Live555Mp2tLiveSource.cpp`
- **Actions**:
  - [x] Implement `createNew()` factory and protected constructor creating `FrameQueueReader`.
  - [x] Implement destructor cleaning up tasks and event trigger.
  - [x] Implement `doGetNextFrame()`:
    - If `m_pendingFrame` has remaining bytes OR `m_reader` has a frame ready, schedule `deliverFrame0` with delay 0.
    - Otherwise, set `m_isAwaitingData = true`.
  - [x] Implement `deliverFrame()`:
    - Verify `isCurrentlyAwaitingData()`; if false, return.
    - If `!m_pendingFrame.has_value() || m_currentOffset >= m_pendingFrame->data->size()`, read next frame from `m_reader`.
    - Calculate available 188-byte TS packets.
    - Bundle up to $\min(\lfloor fMaxSize / 188 \rfloor, 7)$ packets (1316 bytes maximum).
    - Copy packets to `fTo`, update `m_currentOffset`.
    - If `m_currentOffset >= m_pendingFrame->data->size()`, reset `m_pendingFrame = std::nullopt` and `m_currentOffset = 0U`.
    - Compute `fPresentationTime` and invoke `FramedSource::afterGetting(this)`.

### Task 2.3: Register Sources in `libs/Video/CMakeLists.txt` [Completed]
- **Location**: `libs/Video/CMakeLists.txt`
- **Actions**:
  - [x] In `if(PELCOD_ENABLE_LIVE555 AND LIVE555_FOUND AND OPENH264_FOUND)` block, append:
    - `Live555H264LiveSource.h`
    - `Live555H264LiveSource.cpp`
    - `Live555Mp2tLiveSource.h`
    - `Live555Mp2tLiveSource.cpp`

### Task 2.4: Comprehensive Unit Test Suite `TestLive555Sources.cpp` [Completed]
- **Location**: `libs/Video/tests/TestLive555Sources.cpp`
- **Actions**:
  - [x] Register `TestLive555Sources` in `libs/Video/tests/CMakeLists.txt` under `PELCOD_ENABLE_LIVE555`.
  - [x] Implement tests:
    1. `H264SourceImmediateDelivery`: Push frame, call `getNextFrame()`, run single scheduler step, verify delivered NALU.
    2. `H264SourceStartCodeStripping`: Test 4-byte prefix, 3-byte prefix, and raw NALU; verify start codes are stripped.
    3. `H264SourceAsyncEventTrigger`: Sink calls `getNextFrame()` while queue empty; producer pushes frame in another thread; verify trigger wakes scheduler and delivers frame.
    4. `H264SourceTruncation`: Provide small `fMaxSize`; verify `fNumTruncatedBytes` is set accurately.
    5. `Mp2tSourceSingleBundleDelivery`: Push 7 TS packets (1316 bytes); verify single delivery of 1316 bytes.
    6. `Mp2tSourceMultiChunkSlicing`: Push 21 TS packets (3948 bytes); verify 3 consecutive deliveries of 1316 bytes each with 188-byte alignment.
    7. `Mp2tSourceSmallPacketDelivery`: Push 2 TS packets (376 bytes); verify correct 376-byte delivery without padding corruption.
    8. `SourceLifecycleCleanTeardown`: Construct and close source instances using `Medium::close()`; verify zero memory leaks or dangling event triggers.

---

## 5. Verification Checklist & Compliance

| Requirement | Implementation Verification |
| :--- | :--- |
| **MISRA C++:2023 / SEI CERT** | Object creation via `createNew()`; teardown via `Medium::close()`. Zero raw `delete`. Explicit casts. |
| **IWYU & `<cstdint>`** | `#include <cstdint>` and `<cstddef>` in all headers. All integer types strictly scoped (`std::uint8_t`, `std::size_t`). |
| **Zero Qt Dependencies** | Zero `#include <Q...>` in `libs/Video/`. |
| **Function Name Length** | All member and free function names strictly under 30 characters. |
| **Header Guards & Co-location** | `#pragma once` used exclusively; `.h` and `.cpp` co-located in `libs/Video/`. |
| **Doxygen Documentation** | Full Doxygen blocks (`/// @brief`, `@details`, `@param`, `@return`) for all public and protected methods. |
| **Thread Safety** | Multi-threaded handoff strictly mediated through `Live555FrameQueue` and `TaskScheduler::triggerEvent`. Live555 objects remain strictly single-threaded. |
| **Zero Compiler Warnings** | Compiles cleanly with zero warnings under `-Wall -Wextra -pedantic` (`-Werror`) and `/W4` on MSVC. |
