# Implementation Plan: Live555 + OpenH264 Low-Latency RTSP Decoder

## Executive Summary
This document outlines the architectural design, component breakdown, and step-by-step implementation plan for adding a dedicated **Live555 + Cisco OpenH264** video decoder backend (`Live555Decoder`) to `libs/Video/`. 

The primary objective is **ultra-low latency RTSP streaming (<50ms glass-to-glass decoding latency)** with a lightweight binary footprint, minimal buffering, and zero dependency on monolithic multimedia frameworks like FFmpeg or GStreamer.

---

## 1. Architectural Overview & Data Flow

### ASCII Pipeline Diagram
```
[ RTSP Server / Camera ]
         |
         | RTP Packets (UDP / TCP Interleaved)
         v
+--------------------------------------------------------------+
| Live555 RTSP Client Thread                                   |
|   - StreamRTSPClient (DESCRIBE, SETUP, PLAY)                 |
|   - H264VideoRTPSource (RTP packet depacketization)          |
|   - H264VideoSink (Reassembles NALUs with 00 00 00 01 AnnexB)|
+--------------------------------------------------------------+
         |
         | Raw Annex-B NALUs (SPS / PPS / IDR / non-IDR)
         v
+--------------------------------------------------------------+
| Thread-Safe SPSC / Lock-Free Ring Buffer                      |
+--------------------------------------------------------------+
         |
         | Raw NALU Byte Stream
         v
+--------------------------------------------------------------+
| OpenH264 Decoder Worker Thread (decodeNextFrame)             |
|   - ISVCDecoder::DecodeFrameNoDelay()                        |
|   - Output: YUV420P Planes                                   |
|   - YUV420P to RGB24/BGR24 SIMD/Lookup Converter             |
|   - publishToTripleBuffer()                                  |
+--------------------------------------------------------------+
         |
         v
[ AtomicTripleBuffer / Client Consumer UI ]
```

### Mermaid Pipeline Diagram
```mermaid
graph TD
    Camera["IP Camera / RTSP Server"] -->|RTP / RTSP| L555Client["Live555 StreamRTSPClient<br/>(Network Thread)"]
    
    subgraph Live555Pipeline["Live555 Subsystem"]
        L555Client --> Depacketizer["H264VideoRTPSource<br/>(FU-A / STAP-A Depacketization)"]
        Depacketizer --> Sink["Live555H264Sink<br/>(Annex-B 0x00000001 Framing)"]
    end

    Sink -->|Annex-B NALUs| Queue["Thread-Safe Ring Buffer / Packet Queue"]

    subgraph OpenH264Pipeline["OpenH264 Subsystem (decodeNextFrame)"]
        Queue --> WelsDecoder["Cisco OpenH264 (ISVCDecoder)<br/>DecodeFrameNoDelay()"]
        WelsDecoder -->|YUV420P Planes| ColorConverter["YUV420P to RGB24/BGR24<br/>SIMD Converter"]
    end

    ColorConverter -->|RGB24 Packed Buffer| TripleBuffer["BaseVideoDecoder<br/>AtomicTripleBuffer"]
    TripleBuffer --> Consumer["Consumer / Rendering Surface"]
```

---

## 2. Key Technical Decisions & Low-Latency Optimizations

1. **Zero-Delay Decoding Mode:**
   * Invoke `ISVCDecoder::DecodeFrameNoDelay()` instead of standard delayed decoding to bypass B-frame reordering buffers and output frames immediately upon I/P slice arrival.
2. **Socket & Network Buffering:**
   * Configure Live555 socket receive buffers (`increaseReceiveBufferTo`) to match network bandwidth while disabling transport-level queuing delays.
   * Support both TCP interleaved mode (`RTP-over-RTSP`) to avoid firewall packet drops and UDP for lowest transmission latency via [IVideoDecoder::setRtspTransport](../libs/Video/IVideoDecoder.h#L87).
3. **Annex-B NALU Depacketization:**
   * RTP payload RFC 6184 encapsulates H.264 into Single NALU, Aggregation (STAP-A), and Fragmentation Units (FU-A). Live555's `H264VideoRTPSource` strips RTP headers; our custom `Live555H264Sink` prefixes `0x00, 0x00, 0x00, 0x01` start codes before handing chunks to OpenH264.
4. **Fast Color Space Conversion (YUV420P $\rightarrow$ RGB24/BGR24):**
   * OpenH264 outputs planar `videoFormatI420` (Y, U, V with independent strides).
   * Implement a dedicated, MISRA-compliant SIMD/scalar fixed-point converter without introducing `swscale` or FFmpeg dependencies.
5. **Thread Model & Asynchronous Interruption:**
   * Live555 requires an event loop (`doEventLoop()`). We execute Live555 inside a background I/O worker thread.
   * [IVideoDecoder::interrupt()](../libs/Video/IVideoDecoder.h#L125) flags an `eventLoopWatchVariable = 1` to unblock the Live555 event loop cleanly during shutdown or stream resets.

---

## 3. Class Structure & File Layout

All new components will reside in `libs/Video/` alongside the existing decoders:

| File Path | Description |
| :--- | :--- |
| `libs/Video/Live555Decoder.h` | Derived from [BaseVideoDecoder](../libs/Video/BaseVideoDecoder.h), public header declaring `Live555Decoder`. |
| `libs/Video/Live555Decoder.cpp` | Implementation of initialization, Live555 event loop, OpenH264 decoding, color conversion, and cleanup. |
| `libs/Video/Live555RTSPClient.h` | Subclass of Live555 `RTSPClient` handling asynchronous DESCRIBE/SETUP/PLAY state machine and out-of-band SPS/PPS parameter set extraction. |
| `libs/Video/Live555RTSPClient.cpp` | Session negotiation, Authenticator credentials, transport configuration, and subsession dispatch. |
| `libs/Video/Live555H264Sink.h` | Internal `MediaSink` subclass to capture NALUs and format Annex-B byte stream. |
| `libs/Video/Live555H264Sink.cpp` | Buffer management, truncation expansion, and NALU boundary handling. |
| `libs/Video/ColorConverter.h` | Standalone header-only / inline optimized YUV420P to RGB24/BGR24 converter. |
| `libs/Video/tests/TestLive555Decoder.cpp` | Unit and mock integration tests. |

---

## 4. Step-by-Step Implementation Phases

### Phase 1: Build System & Dependency Discovery (CMake) [Completed]
- Add CMake options:
  - `PELCOD_ENABLE_LIVE555` (default: `ON`)
  - `PELCOD_ENABLE_OPENH264` (default: `ON`)
- Add [cmake/FindOrFetchLive555.cmake](../cmake/FindOrFetchLive555.cmake):
  - Checks system for pre-installed `live555` via pkg-config and standard system paths.
  - **Official Tarball Fallback:** If not found on system, automatically downloads official archive from `https://download.live555.com/live555-latest.tar.gz`, unpacks, and configures in-tree static targets (`liveMedia_in_tree`, `groupsock_in_tree`, `BasicUsageEnvironment_in_tree`, `UsageEnvironment_in_tree`).
  - Detects `openh264` via pkg-config or system paths.
- Update `libs/Video/CMakeLists.txt`:
  - Conditionally compile `Live555Decoder.cpp`, `Live555H264Sink.cpp`.
  - Link against `liveMedia`, `groupsock`, `BasicUsageEnvironment`, `UsageEnvironment`, and `openh264`.
  - Add compile definition `PELCOD_HAS_LIVE555`.

### Phase 2: Core Data Types & Factory Registration [Completed]
- Update [DecoderTypes.h](../libs/Video/DecoderTypes.h):
  - Add `BackendType::Live555` to the `BackendType` enum:
    ```cpp
    enum class BackendType {
        FFmpeg,
        GStreamer,
        Mock,
        Live555 ///< Low-latency Live555 RTSP + OpenH264 pipeline
    };
    ```
- Update [DecoderFactory.cpp](../libs/Video/DecoderFactory.cpp):
  - Register `BackendType::Live555` returning `std::make_unique<Live555Decoder>()`.
  - Query `PELCOD_HAS_LIVE555` in `availableBackends()`.

### Phase 3: Live555 RTSP Client & MediaSink Integration [Completed]
- Implement `Live555H264Sink`:
  - Inherits from `MediaSink`.
  - Overrides `continuePlaying()`.
  - Receives RTP frame packets, checks frame boundary markers (`bMarkerBit`), and constructs Annex-B NAL units with `0x00 0x00 0x00 0x01` prefixes.
  - Dynamically expands receive buffer upon frame truncation (`numTruncatedBytes > 0`).
  - Enqueues framed NALUs into a thread-safe bounded circular queue.
- Implement RTSP session negotiation in `Live555RTSPClient` and `Live555Decoder`:
  - Construct `TaskScheduler` and `BasicUsageEnvironment`.
  - Subclass `RTSPClient` to handle async response handlers:
    - `continueAfterDESCRIBE()`
    - `continueAfterSETUP()`
    - `continueAfterPLAY()`
    - RTCP BYE and stream closure handlers
  - Handle out-of-band H.264 SPS/PPS extraction from SDP `sprop-parameter-sets`.
  - Handle Digest authentication via `Authenticator` if credentials are set in `BaseVideoDecoder`.
  - Support both TCP interleaved mode (`RTP-over-RTSP`) and UDP transport configured via `IVideoDecoder::setRtspTransport`.
  - Comprehensive unit test suite in `libs/Video/tests/TestLive555Decoder.cpp`.

### Phase 4: Cisco OpenH264 Integration & Frame Decoding
- Initialize OpenH264 in `Live555Decoder::initialize()`:
  - Call `WelsCreateDecoder(&m_pDecoder)`.
  - Setup `SDecodingParam`:
    - `sDecParam.eOutputColorFormat = videoFormatI420;`
    - `sDecParam.uiTargetVideoWidth = ...;`
    - `sDecParam.bParseOnly = false;`
  - Call `m_pDecoder->Initialize(&sDecParam)`.
- Frame decoding in `decodeNextFrame()`:
  - Pop next Annex-B NALU from queue.
  - Call `m_pDecoder->DecodeFrameNoDelay(...)`.
  - When `sDstBufInfo.iBufferStatus == 1` (frame decoded):
    - Read planar Y, U, V pointers (`pDst[0]`, `pDst[1]`, `pDst[2]`) and line strides.
    - Execute color conversion into `m_frameBuffer` (RGB24 / BGR24).
    - Update timing metrics (`m_lastDecodeTimeMs`, `m_decodedFramesCount`).
    - Invoke `dispatchFrameProcessors()` and `publishToTripleBuffer()`.

### Phase 5: Reconnection, Error Handling & Lifecycle
- Implement `close()`:
  - Signal event loop termination (`m_eventLoopWatchVariable = 1`).
  - Send RTSP `TEARDOWN` command if connected.
  - Join Live555 background thread.
  - Uninitialize and destroy OpenH264 decoder (`WelsDestroyDecoder`).
- Implement `interrupt()`:
  - Atomically set `m_eventLoopWatchVariable = 1` to immediately break blocking socket selects in `BasicTaskScheduler0::SingleStep`.
- Support `seek()`:
  - Return `false` (Live RTSP streams do not support arbitrary file seeking).

### Phase 6: Testing & Quality Assurance
- Implement unit tests in `libs/Video/tests/Test_Live555Decoder.cpp`:
  - Test instantiation via [DecoderFactory](../libs/Video/DecoderFactory.h).
  - Verify initialization state machine and credential handling.
  - Mock NALU feeding into OpenH264 to test synthetic frame decoding without physical camera hardware.
- Execute project verification checks:
  - Run `scripts/verify_iwyu_cstdint.py`.
  - Verify zero Qt dependencies in `libs/Video/`.
  - Ensure all function names are strictly under 30 characters.
  - Verify compliance with MISRA C++:2023 and SEI CERT C++.

---

## 5. Potential Risks & Mitigations

| Risk | Impact | Mitigation Strategy |
| :--- | :--- | :--- |
| **OpenH264 High Profile Compatibility** | Older OpenH264 versions did not support all High Profile tools (e.g., 8x8 transform). | Require OpenH264 $\ge$ v2.3.0, which has broad High Profile 4:2:0 decode support. |
| **Live555 Global / Static State** | Live555 usage environment can suffer from concurrency issues if multiple decoders share state. | Instantiate isolated `TaskScheduler` and `UsageEnvironment` instances per `Live555Decoder` instance. |
| **Color Space Conversion Overhead** | Converting 1080p/4K YUV420P to RGB24 in software can consume significant CPU cycles. | Implement integer fixed-point arithmetic with cache-aligned line processing (or SSE2/NEON optimizations). |
| **Non-H.264 Streams** | Camera streams configured for H.265 or MJPEG will fail to decode. | Detect codec in `DESCRIBE` SDP response. If not `H264`, fail fast with descriptive glog warning guiding the user to select FFmpeg or GStreamer. |
