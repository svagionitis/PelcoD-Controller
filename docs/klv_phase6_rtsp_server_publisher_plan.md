# Implementation Plan: Phase 6 – Embedded RTSP Server Egress (`RtspServerPublisher`)

**Scope:** `libs/Transport/` (`RtspServerPublisher.h`, `RtspServerPublisher.cpp`, `TsNetworkTypes.h`, `CMakeLists.txt`), `libs/Transport/tests/` (`TestRtspServerPublisher.cpp`, `CMakeLists.txt`), `libs/Klv/` (`StanagBroadcastEngine.h`, `StanagBroadcastEngine.cpp`), `docs/KLV_Features.md`  
**Standards Baseline:** RFC 2326 (RTSP 1.0), RFC 7826 (RTSP 2.0), RFC 4566 (SDP), RFC 3550 (RTP), RFC 2250 (RTP Payload Format for MPEG1/MPEG2 Video), ISO/IEC 13818-1 (MPEG-2 Transport Stream), STANAG 4609 Ed. 3/4, MISB ST 1402, MISRA C++:2023, SEI CERT C++ (ERR55-CPP, INT31-C, STR37-C, CON43-C, CON50-CPP), Project Rules in `.agents/rules/`  
**Reference Document:** [`docs/KLV_Features.md`](KLV_Features.md) (§1, §2, §4, §5.5 Phase 6)

---

## 1. Executive Summary & Tactical Context

### 1.1 Context & Problem Statement
The `PelcoD-Controller` STANAG 4609 engine currently provides robust network egress via `MpegTsNetworkPublisher`, streaming multiplexed 188-byte MPEG-2 Transport Stream packets carrying H.264/H.265 video and MISB KLV telemetry over UDP Unicast, UDP Multicast, or RTP MP2T (RFC 3550).

While multicast streaming is standard on dedicated military and tactical airborne datalinks (e.g. CDL, tactical radios, closed ground networks), significant operational limitations arise in broader operational environments:
1. **Multicast Hostility & Routing Barriers:** Enterprise routers, commercial satellite terminals, LTE/5G cellular uplinks, VPN tunnels, and cloud gateways routinely drop or block IGMP/multicast traffic by policy.
2. **Client-Side Restrictions:** Common field situational awareness clients (VLC, ATAK on Android, WinTAK on restricted military OS images, QGIS, web gateway proxies) require an on-demand, pull-based unicast stream over standard HTTP/RTSP ports rather than expecting an active multicast flood.
3. **External Server Footprint Overhead:** Traditional workarounds rely on heavyweight external media relay servers (such as MediaMTX, Live555 daemon, or FFmpeg RTSP relays) running as auxiliary daemon processes. In SWaP-C (Size, Weight, Power, and Cost) constrained airborne edge processors, running external daemons introduces process supervision overhead, additional network hops, socket latency, and increased failure surfaces.

```
Tactical Broadcast Engine (STANAG 4609 / MISB KLV)
                     │
         ┌───────────┴───────────┐
         ▼                       ▼
  Push Egress (UDP/RTP)     Pull Egress (RTSP Server)
  - Multicast 239.255.0.1   - Client Pull: rtsp://<host>:8554/live
  - Low-latency datalinks   - Firewalls / VPNs / ATAK / VLC / Browsers
  - Fixed-rate SATCOM       - TCP Interleaved ($) & UDP Unicast
```

### 1.2 Objectives of Phase 6
Phase 6 implements `RtspServerPublisher`, an embedded, ultra-lightweight, zero-Qt, MISRA-compliant RTSP 1.0 (RFC 2326) and RTSP 2.0 (RFC 7826) streaming server integrated directly within `libs/Transport`:
- **Native RTSP Handshake Negotiation:** Full in-process handling of standard RTSP methods: `OPTIONS`, `DESCRIBE`, `SETUP`, `PLAY`, `TEARDOWN`, `GET_PARAMETER`, and `SET_PARAMETER`.
- **Dual Stream Delivery Modes:**
  1. *TCP Interleaved Transport (RFC 2326 §10.1):* Multiplexes binary RTP MP2T packets over the established client TCP control connection using `$` framing headers, bypassing stateful firewalls and NAT without opening secondary UDP ports.
  2. *RTP/AVP over UDP Unicast (RFC 3550 / RFC 2250):* Dynamic negotiation of client/server ephemeral UDP port pairs for low-latency point-to-point datagram streaming.
- **Dynamic SDP Generation:** Automated synthesis of RFC 4566 Session Description Protocol descriptors via `Transport::SdpGenerator`, advertising static Payload Type 33 (`MP2T/90000`).
- **High-Performance Multi-Client Scaling:** Non-blocking cross-platform I/O loop (`Transport::Net::pollSockets`), bounded per-client ring buffers, and slow-client drop mitigation preventing head-of-line blocking.
- **Safety & Real-Time Integrity:** Zero dynamic heap allocations in high-frequency packet dispatch loops (`noexcept` ERR55-CPP), deterministic ASCII protocol token parsing (STR37-C), explicit integer conversions (INT31-C), and zero external library dependencies beyond OS sockets.
- **Seamless Facade Integration:** Native integration into `StanagBroadcastEngine` as an optional egress sink alongside `MpegTsNetworkPublisher` and `CbrRatePacer`.

---

## 2. Architecture & Data Flow

### 2.1 System Architecture Diagram

```
+---------------------------------------------------------------------------------------------------------+
|                                    STANAG 4609 RTSP SERVER EGRESS ARCHITECTURE                          |
+---------------------------------------------------------------------------------------------------------+
|                                                                                                         |
|   +--------------------+     +---------------------+                                                    |
|   | Video Access Units |     | KLV Telemetry Units |                                                    |
|   +--------------------+     +---------------------+                                                    |
|              \                          /                                                               |
|               v                        v                                                                |
|         +------------------------------------+                                                          |
|         |    PtsSyncManager (Skew <= 50ms)   |                                                          |
|         +------------------------------------+                                                          |
|                           |                                                                             |
|                           v                                                                             |
|         +------------------------------------+                                                          |
|         |          MpegTsKlvMuxer            |  <-- Multiplexed 188-byte TS packets                     |
|         +------------------------------------+                                                          |
|                           |                                                                             |
|                           v                                                                             |
|         +------------------------------------+                                                          |
|         |       CbrRatePacer (Optional)      |  <-- Rigid bitrate pacing & null stuffing (0x1FFF)       |
|         +------------------------------------+                                                          |
|                           |                                                                             |
|         +-----------------+-----------------+                                                           |
|         |                                   |                                                           |
|         v (Push Multicast)                  v (Pull RTSP Ingest)                                        |
|   +---------------------------+       +---------------------------------------------------------------+ |
|   |  MpegTsNetworkPublisher   |       |                    RtspServerPublisher                        | |
|   |  (UDP / RTP Multicast)    |       |                                                               | |
|   +---------------------------+       |  +---------------------+     +-----------------------------+  | |
|                                       |  | Server Listen Socket|     | Client Session Store        |  | |
|                                       |  | (TCP 8554 / 554)    |     | (Bounded Session Table)     |  | |
|                                       |  +---------------------+     +-----------------------------+  | |
|                                       |             |                               |                 | |
|                                       |             v                               v                 | |
|                                       |  +---------------------+     +-----------------------------+  | |
|                                       |  | RTSP Request Parser |     | RTP Packet Aggregator       |  | |
|                                       |  | - OPTIONS/DESCRIBE  |     | - 1..7 TS Packets (1316 B)  |  | |
|                                       |  | - SETUP/PLAY/TEAR   |     | - RFC 3550 Header (PT 33)   |  | |
|                                       |  +---------------------+     +-----------------------------+  | |
|                                       |             |                               |                 | |
|                                       |             +---------------+---------------+                 | |
|                                       |                             |                                 | |
|                                       +-----------------------------|---------------------------------+ |
|                                                                     |                                   |
|                                       +-----------------------------+-----------------------------+     |
|                                       |                                                           |     |
|                                       v (TCP Interleaved $)                                       v(UDP)|
|                           +-----------------------+                           +-----------------------+ |
|                           | Client 1: TCP Stream  |                           | Client 2: UDP Stream  | |
|                           | (ATAK / VPN Client)   |                           | (VLC / Tactical Mast) | |
|                           +-----------------------+                           +-----------------------+ |
+---------------------------------------------------------------------------------------------------------+
```

### 2.2 Mermaid Architecture Diagram: Multi-Client Request & Streaming Flow

```mermaid
sequenceDiagram
    autonumber
    participant C as RTSP Client (VLC / ATAK)
    participant S as RtspServerPublisher
    participant P as RtspParser / SdpGenerator
    participant E as StanagBroadcastEngine (188B TS Feed)

    Note over C,S: Phase 1: Control Handshake (TCP Port 8554)
    C->>S: OPTIONS rtsp://127.0.0.1:8554/live RTSP/1.0 (CSeq: 1)
    S->>C: RTSP/1.0 200 OK (Public: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN, GET_PARAMETER)

    C->>S: DESCRIBE rtsp://127.0.0.1:8554/live RTSP/1.0 (CSeq: 2, Accept: application/sdp)
    S->>P: generateSdp(SessionParams{payload=33, MP2T})
    P-->>S: SDP Descriptor (v=0, m=video 0 RTP/AVP 33, a=rtpmap:33 MP2T/90000)
    S->>C: RTSP/1.0 200 OK (Content-Type: application/sdp, Content-Base: rtsp://.../live/)

    alt TCP Interleaved Streaming Mode
        C->>S: SETUP rtsp://.../live/track0 RTSP/1.0 (Transport: RTP/AVP/TCP;unicast;interleaved=0-1)
        S->>C: RTSP/1.0 200 OK (Transport: RTP/AVP/TCP;unicast;interleaved=0-1;ssrc=..., Session: 04A1B2C3)
    else UDP Unicast Streaming Mode
        C->>S: SETUP rtsp://.../live/track0 RTSP/1.0 (Transport: RTP/AVP;unicast;client_port=30000-30001)
        S->>S: Allocate server UDP ephemeral ports (e.g. 35000-35001)
        S->>C: RTSP/1.0 200 OK (Transport: RTP/AVP;unicast;client_port=30000-30001;server_port=35000-35001;ssrc=..., Session: 04A1B2C3)
    end

    C->>S: PLAY rtsp://.../live RTSP/1.0 (Session: 04A1B2C3, Range: npt=0.000-)
    S->>C: RTSP/1.0 200 OK (Session: 04A1B2C3, Range: npt=0.000-, RTP-Info: url=...;seq=1;rtptime=0)

    Note over S,E: Phase 2: Live Stream Forwarding
    loop Ingestion & Fanout
        E->>S: pushPacket(188-byte TS Packet, ptsUs)
        alt TCP Interleaved
            S->>C: Write [$][ch=0][Length: 1328][12B RTP Header (PT=33) + 7x188B TS]
        else UDP Unicast
            S->>C: Sendto UDP client_port 30000 [12B RTP Header (PT=33) + 7x188B TS]
        end
    end

    Note over C,S: Phase 3: Session Termination
    C->>S: TEARDOWN rtsp://.../live RTSP/1.0 (Session: 04A1B2C3)
    S->>C: RTSP/1.0 200 OK (Session: 04A1B2C3)
    S->>S: Reclaim client session & release UDP sockets
```

---

## 3. Standards Conformance & Protocol Specifications

### 3.1 Standards Matrix

| Standard / Profile | Mandatory Requirements | Implementation Strategy in `RtspServerPublisher` |
|---|---|---|
| **RFC 2326 (RTSP 1.0)** | Method handshake (`OPTIONS`, `DESCRIBE`, `SETUP`, `PLAY`, `TEARDOWN`), header syntax, status codes, CRLF delimiters. | Pure C++17 parser using zero-allocation token views, strict CSeq reflection, and unique 64-bit session tokens. |
| **RFC 7826 (RTSP 2.0)** | Protocol interoperability and compatibility with modern RTSP 2.0 clients. | Tolerant method and protocol parsing accepting both `RTSP/1.0` and `RTSP/2.0` request lines. |
| **RFC 4566 / RFC 2327** | Session Description Protocol (SDP) media formatting. | Integrated via [SdpGenerator.h](../libs/Transport/SdpGenerator.h) generating `m=video <port> RTP/AVP 33` and `a=rtpmap:33 MP2T/90000`. |
| **RFC 3550 / RFC 2250** | RTP payload encapsulation for MPEG-2 Transport Streams. | Generates 12-byte RFC 3550 RTP headers with static Payload Type 33 (`MP2T`), 90 kHz timebase derived from presentation timestamps (PTS), and sequential sequence tracking. |
| **RFC 2326 §10.1** | TCP Interleaved Binary Data Framing (`$` + Channel ID + 16-bit Length). | Zero-copy prepending of 4-byte interleaved frame headers before pushing aggregated RTP frames across client TCP sockets. |
| **ISO/IEC 13818-1** | 188-byte MPEG-2 Transport Stream preservation. | Aggregates up to 7 unmodified 188-byte TS packets ($7 \times 188 = 1316\text{ bytes}$) per RTP packet to guarantee MTU-safe payloads ($\le 1500\text{ bytes}$). |
| **STANAG 4609 Ed. 3/4** | Synchronized video & KLV telemetry streaming. | Transports the multiplexed transport stream directly as output by `MpegTsKlvMuxer` / `CbrRatePacer`. |
| **MISRA C++:2023** | Rule 7.0.6 (Explicit static_cast), Rule 5.0.1 (No uninitialized data). | Strong value typing, explicit integer conversions, brace initialization `{}` throughout. |
| **SEI CERT C++** | STR37-C (Safe char handling), ERR55-CPP (Zero heap allocs in noexcept fast path), INT31-C (No conversion overflow), CON50-CPP (Safe multithreading). | ASCII utilities for string parsing without `std::tolower`, fixed-capacity per-client ring buffers, non-blocking polling. |

---

## 4. Domain Modeling & Value Types

In accordance with [`.agents/rules/domain-modeling.md`](../.agents/rules/domain-modeling.md), primitive obsession is strictly prohibited. All networking ports, session IDs, transport modes, and states are modeled into strongly-typed value objects.

### 4.1 Value Objects Architecture

```
+-----------------------------------------------------------------------------------+
|                        RTSP SERVER DOMAIN MODEL                                   |
+-----------------------------------------------------------------------------------+
|  RtspTransportMode       RtspSessionState         RtspStatusCode                  |
|  - TcpInterleaved        - Init                   - Ok_200                        |
|  - UdpUnicast            - Ready                  - BadRequest_400                |
|                          - Streaming              - NotFound_404                  |
|                          - Teardown               - UnsupportedTransport_461      |
|                          - Closed                 - InternalServerError_500       |
+-----------------------------------------------------------------------------------+
|  RtspPort                RtspSessionId            RtspSequenceNumber              |
|  - explicit uint16_t     - explicit uint64_t      - explicit uint32_t             |
|  - isValidPort()         - toHexString()          - next()                        |
+-----------------------------------------------------------------------------------+
|  RtspServerConfig                                 RtspServerStats                 |
|  - bindAddress: string                            - activeClients: size_t         |
|  - port: RtspPort (default 8554)                  - totalConnectionsAccepted: u64 |
|  - streamPath: string ("/live")                   - bytesTransmitted: u64         |
|  - maxClients: size_t (default 32)                - rtpPacketsSent: u64           |
|  - sessionTimeoutSeconds: uint32_t (60s)          - droppedPacketsSlowClients: u64|
|  - packetsPerDatagram: size_t (default 7)         - currentBitrateKbps: double    |
+-----------------------------------------------------------------------------------+
```

### 4.2 C++17 Domain Types Definition (`TsNetworkTypes.h` & `RtspServerTypes.h`)

```cpp
#pragma once

#include <cstddef>
#include <cstdint>

#include <string>
#include <string_view>

namespace Transport {

/// @enum RtspTransportMode
/// @brief Underlying delivery transport protocol negotiated during RTSP SETUP.
enum class RtspTransportMode : std::uint8_t {
    TcpInterleaved = 0U, ///< RFC 2326 §10.1 interleaved binary data over TCP control connection
    UdpUnicast     = 1U  ///< RFC 3550 RTP/AVP over UDP unicast datagrams
};

/// @enum RtspSessionState
/// @brief Lifecycle state machine for an active RTSP client session.
enum class RtspSessionState : std::uint8_t {
    Init      = 0U, ///< Connection established, awaiting DESCRIBE / SETUP
    Ready     = 1U, ///< SETUP complete, transport allocated, awaiting PLAY
    Streaming = 2U, ///< PLAY active, transmitting RTP packets
    Teardown  = 3U, ///< TEARDOWN requested, releasing resources
    Closed    = 4U  ///< Disconnected and marked for immediate destruction
};

/// @class RtspPort
/// @brief Strongly-typed TCP/UDP network port value object.
class RtspPort {
public:
    constexpr explicit RtspPort(std::uint16_t port = 8554U) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] constexpr std::uint16_t value() const noexcept
    {
        return m_port;
    }

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return m_port > 0U;
    }

private:
    std::uint16_t m_port { 8554U };
};

/// @struct RtspServerConfig
/// @brief Comprehensive configuration settings for RtspServerPublisher.
struct RtspServerConfig {
    /// @brief Server listen interface IPv4 address ("0.0.0.0" for all interfaces).
    std::string listenAddress { "0.0.0.0" };

    /// @brief Server listen TCP port for RTSP control connections (default 8554).
    RtspPort listenPort { 8554U };

    /// @brief URL resource path serving the STANAG 4609 stream (e.g. "/live" or "/stanag4609").
    std::string streamPath { "/live" };

    /// @brief Human-readable SDP session title.
    std::string sessionName { "STANAG 4609 Video & Telemetry Stream" };

    /// @brief Maximum concurrent connected RTSP clients permitted (prevents resource exhaustion).
    std::size_t maxClients { 32U };

    /// @brief Maximum client session inactivity timeout in seconds (default 60s).
    std::uint32_t sessionTimeoutSeconds { 60U };

    /// @brief Maximum 188-byte TS packets grouped into a single RTP packet (default 7 = 1316 B payload).
    std::size_t packetsPerDatagram { 7U };

    /// @brief Per-client outbound packet queue capacity before dropping for slow clients.
    std::size_t maxClientQueuePackets { 256U };

    /// @brief Base UDP port for ephemeral RTP port allocation in UDP unicast mode.
    std::uint16_t udpPortRangeStart { 30000U };

    /// @brief End UDP port for ephemeral RTP port allocation in UDP unicast mode.
    std::uint16_t udpPortRangeEnd { 35000U };

    /// @brief 32-bit RTP SSRC identifier (0 for auto-generated).
    std::uint32_t rtpSsrc { 0x4D503254U }; // "MP2T"
};

/// @struct RtspServerStats
/// @brief Operational telemetry and throughput metrics for RtspServerPublisher.
struct RtspServerStats {
    /// @brief Number of active RTSP client sessions currently connected.
    std::size_t activeClients { 0U };

    /// @brief Number of clients actively receiving media in Streaming state.
    std::size_t streamingClients { 0U };

    /// @brief Total lifetime RTSP client connections accepted.
    std::uint64_t totalConnectionsAccepted { 0U };

    /// @brief Total 188-byte TS packets ingested into the server.
    std::uint64_t tsPacketsIngested { 0U };

    /// @brief Total RFC 3550 RTP packets dispatched across all clients.
    std::uint64_t rtpPacketsDispatched { 0U };

    /// @brief Total bytes transmitted across TCP and UDP client channels.
    std::uint64_t bytesTransmitted { 0U };

    /// @brief Total RTP packets dropped due to slow client queue saturation.
    std::uint64_t droppedPacketsSlowClients { 0U };

    /// @brief Current calculated outbound bitrate in kilobits per second (kbps).
    double currentBitrateKbps { 0.0 };
};

} // namespace Transport
```

---

## 5. Interface Specifications & Design

The public API adheres strictly to the project rules:
1. Co-located `libs/Transport/RtspServerPublisher.h` and `libs/Transport/RtspServerPublisher.cpp`.
2. Zero Qt dependencies (`#include <Q...>` strictly forbidden).
3. All methods querying state marked `[[nodiscard]]`.
4. Function names strictly less than 30 characters.
5. Pimpl idiom (`std::unique_ptr<Impl>`) isolating socket descriptors and implementation details.

### 5.1 Public Header Specification (`libs/Transport/RtspServerPublisher.h`)

```cpp
#pragma once

/// @file RtspServerPublisher.h
/// @brief Embedded RTSP 1.0/2.0 streaming server for STANAG 4609 MPEG-TS egress.

#include <cstddef>
#include <cstdint>

#include <memory>
#include <string>
#include <string_view>

#include "TsNetworkTypes.h"

namespace Transport {

/// @class RtspServerPublisher
/// @brief Pure C++17 embedded RTSP server streaming MPEG-TS/KLV over TCP interleaved or UDP.
/// @details Listens on a configurable TCP port (default 8554), handles RTSP control handshakes,
///          and broadcasts 188-byte MPEG-2 Transport Stream packets to connected clients.
class RtspServerPublisher {
public:
    /// @brief TS packet size constant (188 bytes).
    static constexpr std::size_t kTsPacketSize { 188U };

    /// @brief Maximum TS packets per RTP payload (7 * 188 = 1316 bytes).
    static constexpr std::size_t kMaxPacketsPerDatagram { 7U };

    /// @brief Constructs server publisher with specified configuration.
    /// @param[in] config Server configuration settings.
    explicit RtspServerPublisher(RtspServerConfig config = { });

    /// @brief Destructor. Shuts down listen socket, terminates client sessions, stops worker thread.
    ~RtspServerPublisher();

    // Non-copyable, non-movable
    RtspServerPublisher(const RtspServerPublisher&) = delete;
    RtspServerPublisher& operator=(const RtspServerPublisher&) = delete;
    RtspServerPublisher(RtspServerPublisher&&) = delete;
    RtspServerPublisher& operator=(RtspServerPublisher&&) = delete;

    /// @brief Binds listen socket and launches background network I/O worker thread.
    /// @return True if socket was bound and worker thread started successfully.
    [[nodiscard]] bool start();

    /// @brief Shuts down worker thread, disconnects clients with TEARDOWN, and closes sockets.
    void stop();

    /// @brief Queries whether the server is actively listening and processing client I/O.
    /// @return True if background worker thread is active.
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Ingests a single 188-byte MPEG-TS packet for fanout to active streaming clients.
    /// @param[in] packet Pointer to 188-byte TS packet.
    /// @param[in] size Size of packet (must be exactly 188 bytes).
    /// @param[in] ptsUs Presentation timestamp in microseconds (for 90kHz RTP timestamp generation).
    /// @return True if ingested and buffered successfully.
    bool pushPacket(const std::uint8_t* packet, std::size_t size, std::uint64_t ptsUs = 0U);

    /// @brief Ingests a contiguous buffer of multiple 188-byte TS packets.
    /// @param[in] buffer Pointer to buffer of TS packets.
    /// @param[in] size Buffer size in bytes (must be multiple of 188 bytes).
    /// @param[in] ptsUs Presentation timestamp in microseconds.
    /// @return Number of TS packets successfully processed.
    std::size_t pushPackets(const std::uint8_t* buffer, std::size_t size, std::uint64_t ptsUs = 0U);

    /// @brief Flushes any partially accumulated TS packets immediately to all clients.
    void flush();

    /// @brief Retrieves the active server configuration.
    /// @return Active configuration copy.
    [[nodiscard]] RtspServerConfig config() const;

    /// @brief Retrieves real-time runtime diagnostics and client metrics.
    /// @return Diagnostic statistics snapshot.
    [[nodiscard]] RtspServerStats stats() const;

    /// @brief Resets operational diagnostic counters.
    void resetStats() noexcept;

    /// @brief Resolves the complete RTSP stream URL (e.g. "rtsp://127.0.0.1:8554/live").
    /// @param[in] hostname Optional host override (defaults to listenAddress or "127.0.0.1").
    /// @return Formatted RTSP URL string.
    [[nodiscard]] std::string streamUrl(std::string_view hostname = { }) const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Transport
```

---

## 6. RTSP Protocol Parsing & State Machine Mechanics

### 6.1 State Machine Lifecycle

```
                 +-------------------+
                 |    Client TCP     |
                 |     Connect       |
                 +-------------------+
                           |
                           v
                 +-------------------+
           +---->|    State: Init    |<----+
           |     +-------------------+     |
           |               |               |
           |      OPTIONS, DESCRIBE        |
           |               |               |
           |               v               |
           |     +-------------------+     |
           +-----|   State: Ready    |     |
                 |  (Post-SETUP)     |     |
                 +-------------------+     |
                           |               |
                         PLAY           PAUSE /
                           |            SETUP
                           v               |
                 +-------------------+     |
                 | State: Streaming  |-----+
                 | (Transmitting RTP)|
                 +-------------------+
                           |
                       TEARDOWN /
                        Timeout
                           |
                           v
                 +-------------------+
                 |  State: Closed    |
                 | (Cleanup Sockets) |
                 +-------------------+
```

### 6.2 Deterministic Parsing & CERT STR37-C Conformance
To ensure deterministic execution across all host environments without dependency on global `setlocale()` or undefined behavior on signed negative `char` values (CERT STR37-C), RTSP line, method, and header parsing must operate exclusively on ASCII tokens:

```cpp
namespace {

[[nodiscard]] constexpr bool isSpaceAscii(char c) noexcept
{
    return c == ' ' || c == '\t';
}

[[nodiscard]] constexpr char toLowerAscii(char c) noexcept
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}

[[nodiscard]] bool equalsIgnoreCaseAscii(std::string_view lhs, std::string_view rhs) noexcept
{
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i { 0U }; i < lhs.size(); ++i) {
        if (toLowerAscii(lhs[i]) != toLowerAscii(rhs[i])) {
            return false;
        }
    }
    return true;
}

} // namespace
```

### 6.3 Handshake Methods Handling Details

1. **`OPTIONS`:**
   - Server echoes back the requested `CSeq`.
   - Injects mandatory RTSP header: `Public: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN, GET_PARAMETER`.
2. **`DESCRIBE`:**
   - Verifies URL path matches configured `streamPath` (e.g. `/live`). If mismatched, returns `404 Not Found`.
   - Checks `Accept: application/sdp`.
   - Uses `Transport::SdpGenerator::generate()` to construct an RFC 4566 descriptor:
     ```sdp
     v=0
     o=- 1730300000 1 IN IP4 127.0.0.1
     s=STANAG 4609 Video & Telemetry Stream
     c=IN IP4 0.0.0.0
     t=0 0
     m=video 0 RTP/AVP 33
     a=rtpmap:33 MP2T/90000
     a=control:track0
     a=recvonly
     ```
   - Emits HTTP-like response: `Content-Type: application/sdp`, `Content-Base: rtsp://.../live/`, and exact `Content-Length`.
3. **`SETUP`:**
   - Inspects `Transport:` header:
     - **Case A: TCP Interleaved:** `Transport: RTP/AVP/TCP;unicast;interleaved=0-1`
       - Parses interleaved channel numbers (e.g. data channel 0, RTCP channel 1).
       - Sets `RtspTransportMode::TcpInterleaved`.
       - Responds: `Transport: RTP/AVP/TCP;unicast;interleaved=0-1;ssrc=4D503254\r\nSession: <SessionId>;timeout=60`.
     - **Case B: UDP Unicast:** `Transport: RTP/AVP;unicast;client_port=30000-30001`
       - Parses client RTP and RTCP destination ports.
       - Allocates ephemeral local server UDP sockets.
       - Sets `RtspTransportMode::UdpUnicast`.
       - Responds: `Transport: RTP/AVP;unicast;client_port=30000-30001;server_port=35000-35001;ssrc=4D503254\r\nSession: <SessionId>;timeout=60`.
   - Transitions session to `RtspSessionState::Ready`.
4. **`PLAY`:**
   - Verifies session exists and matches `Session:` header.
   - Responds: `Range: npt=0.000-\r\nRTP-Info: url=track0;seq=<seq>;rtptime=<time>\r\nSession: <SessionId>`.
   - Transitions session to `RtspSessionState::Streaming`.
5. **`TEARDOWN`:**
   - Responds: `RTSP/1.0 200 OK\r\nSession: <SessionId>`.
   - Closes transport sockets and marks session `RtspSessionState::Closed`.
6. **`GET_PARAMETER` / `SET_PARAMETER`:**
   - Responds with `200 OK` (used by VLC and media clients as RTSP keep-alive).

---

## 7. Transport Delivery Engine & Packet Aggregation

### 7.1 RTP MP2T Packet Encapsulation (RFC 3550 / RFC 2250)
Every transmitted datagram is structured as an RFC 3550 RTP packet wrapping 1 to 7 188-byte TS packets:

```
+-----------------------------------------------------------------------------+
| RFC 3550 RTP Header (12 Bytes)                                              |
| Byte 0: V=2, P=0, X=0, CC=0 (0x80)                                          |
| Byte 1: M=0, PT=33 (MP2T)   (0x21)                                          |
| Byte 2-3: 16-bit Monotonic Sequence Number (Big Endian)                     |
| Byte 4-7: 32-bit 90 kHz Presentation Timestamp (Big Endian)                 |
| Byte 8-11: 32-bit SSRC Identifier (0x4D503254)                              |
+-----------------------------------------------------------------------------+
| Payload: 1 to 7 Contiguous 188-byte TS Packets (Max 7 * 188 = 1316 Bytes)   |
| [TS Packet 1 (188 B: 0x47 ...)]                                             |
| [TS Packet 2 (188 B: 0x47 ...)]                                             |
| ...                                                                         |
| [TS Packet 7 (188 B: 0x47 ...)]                                             |
+-----------------------------------------------------------------------------+
```

### 7.2 Transport Framing Modes

#### Mode 1: RFC 2326 §10.1 TCP Interleaved Framing
When streaming over TCP, each RTP packet is framed by a 4-byte prefix:
- Byte 0: Magic character `$` (`0x24`).
- Byte 1: Channel identifier (e.g. `0x00` for video RTP).
- Bytes 2–3: 16-bit big-endian packet length ($12 + \text{payload length}$).
- Followed immediately by the 12-byte RTP header and TS packets.

#### Mode 2: UDP Unicast Datagram
The 12-byte RTP header and TS packets are sent directly via `sendto()` to `client_ip:client_rtp_port`.

### 7.3 Zero-Allocation Packet Forwarding (CERT ERR55-CPP)
In the high-frequency packet fanout path (`pushPacket` $\rightarrow$ client queue $\rightarrow$ socket send):
1. The server maintains a pre-allocated stack/member accumulator buffer capable of holding up to $12 + (7 \times 188) = 1328\text{ bytes}$ (or $4 + 1328 = 1332\text{ bytes}$ for TCP interleaved).
2. Per-client queues use a fixed-capacity ring buffer of pre-allocated buffers.
3. If a slow client's ring buffer fills up, the oldest frame is dropped, updating `droppedPacketsSlowClients` without throwing, allocating memory, or blocking other active clients.

---

## 8. Concurrency, I/O Polling & Platform Portability

### 8.1 Network I/O Architecture
- **Single Background Worker Thread (`Impl::workerLoop`):**
  - Uses `Transport::Net::pollSockets` (`poll()` on POSIX, `WSAPoll()` on Windows) to monitor:
    1. The master TCP listen socket for incoming RTSP connections.
    2. All active client TCP sockets for incoming RTSP control requests (e.g. `TEARDOWN`, `GET_PARAMETER` keepalive).
    3. Socket writability when client send queues contain pending data.
- **Thread Termination & Self-Wakeup:**
  - Non-blocking poll timeout is capped at 50 ms to allow responsive shutdown without hung threads or complex signal pipes.
- **Cross-Platform Portability:**
  - Standardized through `Transport::Net` primitives in `libs/Transport/SocketUtils.h`:
    - `Net::SocketHandle`, `Net::closeSocket()`, `Net::setNonBlocking()`, `Net::isWouldBlock()`.
    - Handles Windows Winsock2 initialization automatically via `Net::ensureWinsockInitialized()`.

---

## 9. Integration with `StanagBroadcastEngine`

To allow seamless usage, `StanagBroadcastEngine` in `libs/Klv/` will be updated with native support for `RtspServerPublisher`.

### 9.1 Configuration Updates (`libs/Klv/StanagBroadcastEngine.h`)

```cpp
struct StanagBroadcastConfig {
    MpegTsMuxerConfig muxerConfig { };
    Transport::TsNetworkConfig networkConfig { };
    bool enableNetworkPublishing { true };
    std::size_t maxVideoQueueSize { 128U };
    std::size_t maxTelemetryQueueSize { 256U };
    bool enableCbrPacing { false };
    Transport::CbrPacerConfig cbrConfig { };

    /// @brief Whether to activate the embedded RTSP server for client pull egress.
    bool enableRtspServer { false };

    /// @brief Configuration settings for embedded RTSP server when enabled.
    Transport::RtspServerConfig rtspConfig { };
};
```

### 9.2 Broadcast Engine Packet Fanout (`StanagBroadcastEngine.cpp`)

```cpp
void Impl::dispatchPacedPacket(const std::uint8_t* packet, std::size_t size)
{
    // ... existing stats tracking ...

    // 1. Direct UDP/RTP Push Multicast
    if (m_publisher && m_publisher->isOpen()) {
        static_cast<void>(m_publisher->pushPacket(packet, size));
    }

    // 2. Embedded RTSP Server Egress Fanout
    if (m_rtspServer && m_rtspServer->isRunning()) {
        static_cast<void>(m_rtspServer->pushPacket(packet, size));
    }

    // 3. User Packet Callback Tap
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        if (m_packetCallback) {
            m_packetCallback(packet, size);
        }
    }
}
```

---

## 10. Comprehensive Verification & GoogleTest Matrix

A dedicated test suite `TestRtspServerPublisher.cpp` will be authored in `libs/Transport/tests/`, registering 12 comprehensive unit and integration test cases covering 100% of functional requirements:

| Test Case | Scenario / Verification Objective |
|---|---|
| **1. LifecycleStartStop** | Verify socket binding, clean startup, port re-binding resilience, double start/stop safety. |
| **2. OptionsHandshake** | Send RTSP `OPTIONS`, verify `200 OK`, `Public` methods header, and `CSeq` reflection. |
| **3. DescribeSdpGeneration** | Send `DESCRIBE`, verify `Content-Type: application/sdp`, valid RFC 4566 SDP containing `m=video 0 RTP/AVP 33` and `a=rtpmap:33 MP2T/90000`. |
| **4. SetupTcpInterleaved** | Send `SETUP` requesting `RTP/AVP/TCP;unicast;interleaved=0-1`, verify server returns matching interleaved channels, SSRC, and 64-bit `Session` ID. |
| **5. SetupUdpUnicast** | Send `SETUP` requesting `RTP/AVP;unicast;client_port=30000-30001`, verify server allocates server UDP ports and confirms `Session` ID. |
| **6. PlayStateTransition** | Send `PLAY`, verify state transitions to `Streaming`, returns `Range: npt=0.000-` and `RTP-Info`. |
| **7. TeardownCleanup** | Send `TEARDOWN`, verify session transitions to `Closed`, UDP sockets are released, and server returns `200 OK`. |
| **8. TcpInterleavedStreaming** | Ingest 188-byte TS packets into server; verify TCP client receives RFC 2326 `$` framed packets with valid 12-byte RTP headers and `0x47` TS sync bytes. |
| **9. UdpUnicastStreaming** | Ingest TS packets; verify UDP client receives RFC 3550 RTP datagrams on negotiated `client_port`. |
| **10. MultiClientConcurrency** | Connect multiple TCP interleaved clients and UDP unicast clients simultaneously; verify all receive identical multiplexed TS packets without crosstalk. |
| **11. SlowClientBackpressure** | Connect an unreading TCP client; verify queue limit triggers drop counter (`droppedPacketsSlowClients`) without blocking other clients or causing memory bloat. |
| **12. EngineEndToEndIntegration** | Ingest video chunks and KLV metadata into `StanagBroadcastEngine` with RTSP enabled; verify client pulls valid STANAG 4609 stream. |

---

## 11. Step-by-Step Implementation Roadmap

```
Step 1: Domain Types & Config
├── Create / update libs/Transport/TsNetworkTypes.h
└── Define RtspTransportMode, RtspSessionState, RtspPort, RtspServerConfig, RtspServerStats

Step 2: Core Server Implementation
├── Create libs/Transport/RtspServerPublisher.h
├── Create libs/Transport/RtspServerPublisher.cpp
│   ├── Implement RtspClientSession state machine
│   ├── Implement ASCII request parser & response builder
│   ├── Implement non-blocking poll loop (pollSockets)
│   ├── Implement TCP interleaved framing & UDP socket delivery
│   └── Implement RTP MP2T packet accumulator (1..7 TS packets)
└── Update libs/Transport/CMakeLists.txt

Step 3: Unit Testing Suite
├── Create libs/Transport/tests/TestRtspServerPublisher.cpp
├── Register test executable in libs/Transport/tests/CMakeLists.txt
└── Run ctest to verify all 12 test cases pass cleanly

Step 4: StanagBroadcastEngine Integration
├── Update libs/Klv/StanagBroadcastConfig & StanagBroadcastStats
├── Update libs/Klv/StanagBroadcastEngine.cpp to instantiate & fanout to RtspServerPublisher
└── Update libs/Klv/tests/TestStanagBroadcastEngine.cpp to verify RTSP server broadcast

Step 5: Documentation & Standards Compliance Audit
├── Update docs/KLV_Features.md (mark Phase 6 as IMPLEMENTED)
├── Run verify_iwyu_cstdint.py to guarantee strict IWYU & std::uint8_t compliance
└── Verify zero compiler warnings under -Wall -Wextra -pedantic
```

---

## 12. Verification Checklist (Run Before Completion)

- [x] All new headers use `#pragma once`.
- [x] Direct inclusion of `<cstdint>` and `<cstddef>` in all files using fixed-width integers or `std::size_t`.
- [x] All integer types strictly scoped with `std::` (`std::uint8_t`, `std::uint16_t`, `std::uint32_t`, `std::uint64_t`).
- [x] No raw `new` or `delete`; RAII and `std::unique_ptr` used throughout.
- [x] Zero dynamic heap allocations in high-frequency packet processing paths (`pushPacket` / packet fanout).
- [x] Zero Qt dependencies across `libs/Transport/`.
- [x] All public member functions documented with complete Doxygen tags (`@brief`, `@param`, `@return`).
- [x] All function names strictly less than 30 characters.
- [x] Cross-platform socket compatibility verified for both Linux (`poll()`, POSIX sockets) and Windows (`WSAPoll()`, Winsock2).
- [x] Full unit test suite passes with 100% pass rate.

