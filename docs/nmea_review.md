# Senior Software Engineering & Architecture Review: NMEA Subsystem & Documentation

**Date:** 2026-10-29  
**Scope:**
- Documentation: `docs/NMEA_Features.md`
- Protocol Library: `libs/Nmea/` (39 headers, 25 implementation files, `CMakeLists.txt`)
- Core Test Suite: `libs/Nmea/tests/` (31 GoogleTest suites, 187 test cases)
- Downstream PTZ Autonomy Integration: `libs/PayloadHal/` (`TargetThreatEvaluator`, `AutoFramingController`, `VesselAttitudeCompensator`, `SlewToCueDirector`, `NmeaSlavingBridge`, 5 GoogleTest suites, 40 test cases)

**Reviewers:** Senior Software Architect / Senior C++ Systems & Marine Autonomy Engineer  
**Reference Standards:**
- MISRA C++:2023 & SEI CERT C++ Coding Standard (`.agents/rules/compliance.md`)
- Domain Modeling & Value Objects Standard (`.agents/rules/domain-modeling.md`)
- Include-What-You-Use (IWYU) & `<cstdint>` Policy (`.agents/rules/iwyu-cstdint.md`)
- 18-Point Verification Checklist (`.agents/rules/verification-checklist.md`)
- IEC 61162-1:2016 / NMEA 0183 v4.11 (Single Talker / Multiple Listeners)
- IEC 61162-3:2008+A1:2010+A2:2014 / NMEA 2000 & ISO 11783-3 (CAN Network & Fast Packet Protocol)
- IEC 61162-450:2018 (Lightweight Ethernet - LWE Multicast & Transmission Groups)
- IEC 61162-460:2020 (Ethernet Interconnections - Maritime Network Safety & Security Gateway)
- IEC 62923-1:2018 & IEC 62923-2:2018 (Bridge Alert Management - BAM Architecture & Sentences)
- ITU-R M.1371-5 (Technical Characteristics for Automatic Identification System - AIS)
- RFC 6455 (WebSocket Protocol) & RFC 7946 (GeoJSON)
- FLIR Systems Nexus / M-Series Marine NMEA 0183 Protocol ($PFEC)

---

## 1. Executive Summary

The NMEA subsystem in `PelcoD-Controller` represents a mission-critical, enterprise-grade maritime telemetry, navigation, and camera automation pipeline. It connects maritime navigation sensors (GNSS, Gyrocompass, Weather Transducers, ARPA Radar, AIS transponders, NMEA 2000 CAN buses, and IEC 61162-450 Lightweight Ethernet) with multi-axis electro-optical and thermal camera systems (`libs/PayloadHal`).

This review evaluates **both the technical specification / architecture documentation (`docs/NMEA_Features.md`)** and **the production C++17 implementation (`libs/Nmea/` and associated `libs/PayloadHal` modules)** against senior engineering, architectural, and safety compliance baselines.

### Summary Scorecard

| Evaluation Dimension | Status | Key Observations & Verdict |
| :--- | :---: | :--- |
| **Documentation Completeness (`docs/NMEA_Features.md`)** | 🟡 **Partial** | Highly detailed on mathematical models (CPA/TCPA, HFOV framing, attitude DCM transformations) and BAM/N2K/460 architectures. However, it completely omits major implemented subsystems: `NmeaRouteManager` (SAR patterns & waypoints), `ThermalTuningAdvisor` (environmental AGC/crossover tuning), `FlirPfecDevice` (FLIR M-Series PTZ driver), `NmeaTagBlockParser`/`Builder` (IEC 61162-1 Tag Blocks), and `NmeaJsonSerializer`/`NmeaWebSocketServer`. |
| **Architectural Layering & Boundaries** | 🟡 **Needs Clarification** | The documentation conflates protocol decoding (`libs/Nmea`) and kinematic camera automation (`libs/PayloadHal`). While the code maintains a clean physical decoupling (zero hardware HAL in `libs/Nmea`), `docs/NMEA_Features.md` does not explain this architectural boundary. |
| **Zero Qt Dependency Isolation** | 🟢 **100% Compliant** | Zero `#include <Q...>` occurrences across all 64 files in `libs/Nmea/`. Pure, deterministic C++17 portable across Linux, Windows, and embedded RTOS. |
| **Automated Compliance Baselines** | 🟢 **100% Compliant** | Automated verification scripts pass with zero violations: `verify_iwyu_cstdint.py` (98/98 files), `verify_function_lengths.py` (0 identifiers $\ge 30$ chars), `verify_type_conversions.py` (0 implicit sign-mismatch defects). |
| **Unit Test Coverage & Reliability** | 🟢 **Outstanding** | 31 test suites (187 test cases) in `libs/Nmea/tests` and 5 test suites (40 test cases) in `libs/PayloadHal/tests` (total **227 test cases**) pass at 100% with sub-second execution. |
| **`noexcept` Exception Safety (SEI CERT ERR55-CPP)** | 🟢 **Resolved (P0 Complete)** | Fixed token stack container `ParsedTokens` (`std::array<std::string_view, 48>`) implemented across `NmeaSentenceParser` (0 heap allocations). Invalid `noexcept` specifications removed from `AisBitReader::readString`, `AisDecoder::decodePayload`, and `N2kFastPacketAssembler::isFastPacketPgn`. |
| **Concurrency & Deadlock Safety (SEI CERT CON50-CPP)** | 🟢 **Resolved (P0 Complete)** | `Iec61162_460Firewall::recordViolationLocked()` refactored to release `m_mutex` prior to invoking external callbacks and `BridgeAlertManager::registerAlert()`. `MaritimeNetworkCoordinator` refactored to snapshot and release callbacks before invocation. |
| **Network Security & CIDR Integrity** | 🟢 **Resolved (P0 Complete)** | Robust 32-bit integer bitwise subnet mask evaluation implemented for IPv4 CIDR matching across all prefixes (`/0` through `/32`). Client peer IP forwarding wired across `NmeaTcpServer`, `NmeaUdpEndpoint`, and `MaritimeNetworkCoordinator`. |
| **Domain Modeling & Type Safety** | 🟡 **Primitive Obsession** | `NmeaTypes.h` uses primitive `double` and `uint32_t` for `latitudeDeg`, `longitudeDeg`, `mmsi`, `sogKnots`, and `headingDeg` instead of strongly-typed domain value objects. |

---

## 2. Architecture Assessment & Component Topology

### 2.1 Subsystem Decomposition (All 64 Files in `libs/Nmea`)

`libs/Nmea` is structured into eight distinct functional layers:

| Layer # | Subsystem | Key Files | Standard / RFC Reference | Functional Scope |
| :---: | :--- | :--- | :--- | :--- |
| **1** | **Core NMEA 0183 & Tag Blocks** | `NmeaTypes.h`, `NmeaChecksum.h`, `NmeaSentenceParser.h/.cpp`, `NmeaSentenceBuilder.h/.cpp`, `NmeaStreamAccumulator.h/.cpp`, `NmeaTagBlockParser.h/.cpp`, `NmeaTagBlockBuilder.h/.cpp`, `NmeaDevice.h/.cpp`, `NmeaStats.h` | IEC 61162-1, NMEA 0183 v4.11 | Framing, checksum verification (`*HH`), fast tokenization, stream hunting/resynchronization, Tag Block metadata (`s:`, `d:`, `c:`, `n:`, `t:`), and multi-sentence caching. |
| **2** | **AIS Protocol Engine** | `AisTypes.h`, `AisBitReader.h`, `AisDecoder.h/.cpp` | ITU-R M.1371-5, IEC 62287 | 6-bit armored ASCII unpacking, multi-sentence message reassembly (`!AIVDM`/`!AIVDO`), decoding for Types 1, 2, 3, 5, 14, 18, 19, 24, and emergency distress beacon detection (AIS-SART, MOB, EPIRB). |
| **3** | **Bridge Alert Management (BAM)** | `BridgeAlertTypes.h`, `BridgeAlertManager.h/.cpp` | IEC 62923-1, IEC 62923-2, IEC 61162-1 | Complete BAM alert state machine (`Normal`, `ActiveUnack`, `ActiveSilenced`, `ActiveAck`, `RectifiedUnack`), 30s silence timeout revert, escalation counters, sentence generation (`$xxALF`, `$xxALC`, `$xxHBT`), and operator request ingestion (`$xxARC`, `$xxACK`). |
| **4** | **NMEA 2000 (N2K) / CAN Engine** | `N2kTypes.h`, `N2kFastPacketAssembler.h/.cpp`, `N2kDecoder.h/.cpp`, `N2kEncoder.h/.cpp`, `N2kAddressClaimer.h/.cpp`, `N2kDevice.h/.cpp` | IEC 61162-3, ISO 11783-3 | 29-bit CAN identifier decoding, multi-frame Fast Packet reassembly/encoding, ISO address claim state machine (250 ms contention timing, 64-bit NAME arbitration), and PGN decoding (129025, 129026, 127250, 127257, 130306, 129038, 127245, 127258, 126992, 126464). |
| **5** | **Lightweight Ethernet (LWE)** | `LweTypes.h`, `LweMulticastTransport.h/.cpp`, `LweChannelManager.h/.cpp` | IEC 61162-450 | Standard UDP multicast transmission groups (`239.192.0.1`–`239.192.0.16` on ports `60001`–`60016`), network interface binding, IGMP membership management, and channel prioritization. |
| **6** | **Maritime Network Security & Gateway** | `Iec61162_460Types.h`, `Iec61162_460Firewall.h/.cpp`, `MaritimeNetworkCoordinator.h/.cpp`, `NmeaTcpServer.h/.cpp`, `NmeaUdpEndpoint.h/.cpp`, `NmeaWebSocketServer.h/.cpp`, `NmeaJsonSerializer.h/.cpp`, `Base64.h`, `Sha1.h` | IEC 61162-460, RFC 6455, RFC 7946 | Zone isolation (`BridgeNetwork`, `ExternalCamera`, `GeneralShipLan`, `InternetShore`), IP/MAC anti-spoofing, rate limiting, BAM security alerting, TCP/UDP broadcast servers, RFC 6455 WebSocket streaming, and zero-alloc JSON serialization. |
| **7** | **Route Management & SAR Engine** | `NmeaRouteManager.h/.cpp` | IEC 61162-1, IAMSAR Manual | Route reassembly (`$xxRTE`), waypoint dictionary (`$xxWPL`), cross-track error tracking (`$xxRMB`), look-ahead geodetic projection, oscillating SAR sweeping pattern calculation (`computeSarSweepPoint`), and pre-turn cueing. |
| **8** | **Environmental Advisor & Replay** | `ThermalTuningAdvisor.h/.cpp`, `NmeaReplayTransport.h/.cpp`, `ReplayTypes.h`, `FlirPfecDevice.h/.cpp`, `PfecTypes.h`, `NmeaGateway.h/.cpp` | Proprietary FLIR ($PFEC), WMO Meteorological Codes | Atmospheric physics analysis (`MTW`, `MDA`, `MMB`) for thermal crossover and fog penetration; high-fidelity voyage log replay transport; bidirectional N2K-to-0183 gateway with rate decimation; FLIR M-Series PTZ driver. |

---

### 2.2 Component & System Dependency Topology

#### ASCII Diagram: System Layering & Integration Architecture

```
+---------------------------------------------------------------------------------------------------------+
|                                           APPLICATION LAYER                                             |
|   +------------------------------------+   +--------------------------------------------------------+   |
|   |         Tactical Marine GUI        |   |                  Remote Bridge Displays                |   |
|   |   (QML / TacticalMapQuickItem)     |   |   OpenCPN / TimeZero / HTML5 Web Dashboards (Port 8088)|   |
|   +-----------------+------------------+   +---------------------------+----------------------------+   |
+---------------------|--------------------------------------------------|--------------------------------+
                      |                                                  |
                      v                                                  v
+---------------------------------------------------------------------------------------------------------+
|                                    AUTONOMOUS PTZ LAYER (libs/PayloadHal)                               |
|                                                                                                         |
|   +-----------------------------------+    +----------------------------------+                         |
|   |       SlewToCueDirector           |    |     TargetThreatEvaluator        |                         |
|   |  - Autonomous Workflow Coordinator|◄---|  - 2D/3D CPA & TCPA Threat Matrix|                         |
|   |  - Tour Preemption & Handover     |    |  - AIS/Radar Target Correlation  |                         |
|   |  - Video Tracker Centroid Lock    |    |  - Dark Vessel Heuristics        |                         |
|   +-----------------+-----------------+    +----------------------------------+                         |
|                     |                                                                                   |
|         +-----------+-----------+-----------------------+                                               |
|         v                       v                       v                                               |
|   +-------------------+   +--------------------+  +-------------------------+                           |
|   |AutoFramingControl.|   |VesselAttitudeCompen|  | NmeaSlavingBridge       |                           |
|   | - Adaptive HFOV   |   | - Dynamic DCM      |  | - Line-of-Sight Az/El   |                           |
|   | - Zoom Profiler   |   | - Horizon Leveling |  | - Predictive Coasting   |                           |
|   +-------------------+   +--------------------+  +-------------------------+                           |
+---------------------------------------------------------------------------------------------------------+
                                                  |
                                                  v
+---------------------------------------------------------------------------------------------------------+
|                                       PROTOCOL CORE (libs/Nmea)                                         |
|                                                                                                         |
|   +-------------------------------------------------------------------------------------------------+   |
|   |                                  MaritimeNetworkCoordinator                                     |   |
|   |   +------------------------+    +------------------------+    +-----------------------------+   |   |
|   |   |     NmeaTcpServer      |    |    NmeaUdpEndpoint     |    |     NmeaWebSocketServer     |   |   |
|   |   |  (Port 10110 Multi-Cli)|    |  (Port 10110 Bcast/Uni)|    |  (RFC 6455 / JSON Telemetry)|   |   |
|   |   +-----------+------------+    +-----------+------------+    +--------------+--------------+   |   |
|   +---------------|-----------------------------|--------------------------------|------------------+   |
|                   |                             |                                |                      |
|                   v                             v                                v                      |
|   +-------------------------------------------------------------------------------------------------+   |
|   |                      IEC 61162-460 Security Gateway (Iec61162_460Firewall)                       |   |
|   |  - Zone Isolation (BridgeNetwork, ExternalCamera, GeneralShipLan, InternetShore)                |   |
|   |  - IP/MAC Anti-Spoofing Tables & Ingress/Egress Whitelist                                        |   |
|   |  - Sliding-Window Rate Limiting (PPS) & Denial-of-Service Flood Protection                      |   |
|   +---------------------------------------------+---------------------------------------------------+   |
|                                                 |                                                       |
|                                                 v                                                       |
|   +-----------------------+    +-------------------------------+    +-------------------------------+   |
|   |     N2kDevice         |    |         NmeaDevice            |    |       FlirPfecDevice          |   |
|   | - Fast Packet Assembl.|    | - NMEA 0183 & Tag Blocks      |    | - FLIR M-Series PTZ Driver    |   |
|   | - Address Claimer     |    | - Stream Hunting Accumulator  |    | - Thermal IR / Daylight Switch|   |
|   | - PGN 126xxx - 130xxx |    | - Telemetry Caching & Dispatch|    | - Absolute & Velocity Slew    |   |
|   +-----------+-----------+    +---------------+---------------+    +---------------+---------------+   |
|               |                                |                                    |                   |
|               +----------------+---------------+------------------------------------+                   |
|                                |                                                                        |
|                                v                                                                        |
|   +-------------------------------------------------------------------------------------------------+   |
|   |                          Multi-Sensor Fusion & Analysis Engines                                 |   |
|   |  +--------------------+  +-------------------+  +--------------------+  +-------------------+   |   |
|   |  | NmeaSensorArbiter  |  | AisDecoder        |  | BridgeAlertManager |  | NmeaRouteManager  |   |   |
|   |  | - Dual GNSS/Gyro   |  | - M.1371 Types 1-24|  | - BAM IEC 62923   |  | - SAR Sweeps      |   |   |
|   |  | - Anti-Spoofing    |  | - SART/MOB/EPIRB  |  | - ALF/ALC/HBT/ARC  |  | - RTE/WPL/RMB     |   |   |
|   |  +--------------------+  +-------------------+  +--------------------+  +-------------------+   |   |
|   |  +--------------------+  +-------------------+  +--------------------+  +-------------------+   |   |
|   |  | ThermalTuningAdv.  |  | NmeaGateway       |  | LweChannelManager  |  | NmeaReplayTransp. |   |   |
|   |  | - Crossover / Fog  |  | - N2K <-> 0183    |  | - IEC 61162-450    |  | - Deterministic   |   |   |
|   |  | - AGC Profile Advice| | - Rate Decimation |  | - Multicast Groups |  |   Log Simulation  |   |   |
|   |  +--------------------+  +-------------------+  +--------------------+  +-------------------+   |   |
|   +-------------------------------------------------------------------------------------------------+   |
+---------------------------------------------------------------------------------------------------------+
                                                  |
                                                  v
+---------------------------------------------------------------------------------------------------------+
|                                       TRANSPORT LAYER (libs/Transport)                                  |
|   Serial RS-422/RS-232 | SocketCAN (Linux) / Win32 Comm | POSIX / Winsock2 UDP & TCP Sockets            |
+---------------------------------------------------------------------------------------------------------+
```

#### Mermaid Diagram: Component Interaction & Dataflow

```mermaid
flowchart TD
    subgraph ExternalSources ["Physical Network & Sensor Inputs"]
        SerialNMEA["Serial RS-422\n(GPS, Gyro, Radar TTM)"]
        CanBus["SocketCAN / N2K Bus\n(PGNs 129025, 127257)"]
        LweMcast["IEC 61162-450 Multicast\n(239.192.0.0/24:60001)"]
        NetClients["Navigation Software & Web UI\n(TCP/UDP 10110, WS 8088)"]
    end

    subgraph SecurityGate ["IEC 61162-460 Security Gateway"]
        Firewall["Iec61162_460Firewall\n- Zone Isolation\n- IP/MAC Anti-Spoofing\n- Sliding-Window Rate Limiter"]
        BAM["BridgeAlertManager\n(IEC 62923-1/2)\n- 46001/46002 Alerts\n- ALF / ALC / HBT Generation"]
    end

    subgraph CoreParsers ["Protocol Ingestion & Decoding (libs/Nmea)"]
        Accumulator["NmeaStreamAccumulator\n(Resync & Frame Extraction)"]
        SentenceParser["NmeaSentenceParser\n(0183 & Tag Block Decoding)"]
        N2kEngine["N2kFastPacketAssembler & N2kDecoder\n(CAN Reassembly & PGN Decode)"]
        AisEngine["AisDecoder & AisBitReader\n(ITU-R M.1371 & SART Detection)"]
    end

    subgraph AnalysisEngines ["Navigation, Route & Analysis Services"]
        Arbiter["NmeaSensorArbiter\n(Dual GNSS/Gyro & Anti-Spoofing)"]
        RouteMgr["NmeaRouteManager\n(ECDIS Routes & SAR Sweep)"]
        ThermalAdv["ThermalTuningAdvisor\n(Fog & Thermal Crossover Advice)"]
        Gateway["NmeaGateway\n(Bidirectional N2K <-> 0183)"]
    end

    subgraph PayloadAutonomy ["Downstream Autonomous Control (libs/PayloadHal)"]
        ThreatEval["TargetThreatEvaluator\n(CPA/TCPA & Threat Matrix)"]
        AttitudeComp["VesselAttitudeCompensator\n(Dynamic DCM & Horizon Leveling)"]
        AutoFraming["AutoFramingController\n(Range -> Optical Zoom HFOV)"]
        Director["SlewToCueDirector\n(Workflow State Machine & Tracker Handover)"]
        Slaving["NmeaSlavingBridge & GeoLockController\n(Gimbal Line-of-Sight Slew)"]
    end

    SerialNMEA --> Accumulator
    NetClients --> Firewall
    LweMcast --> Firewall
    Firewall --> Accumulator
    CanBus --> N2kEngine

    Accumulator --> SentenceParser
    SentenceParser --> Arbiter
    SentenceParser --> RouteMgr
    SentenceParser --> ThermalAdv
    SentenceParser --> AisEngine
    Firewall -. Violation .-> BAM

    Arbiter --> ThreatEval
    AisEngine --> ThreatEval
    SentenceParser -. TTM Target .-> ThreatEval

    N2kEngine --> Gateway
    Gateway --> SentenceParser
    N2kEngine --> AttitudeComp
    SentenceParser -. XDR/PASHR .-> AttitudeComp

    ThreatEval --> Director
    AttitudeComp --> Slaving
    Director --> AutoFraming
    Director --> Slaving
```

---

## 3. Documentation Assessment: `docs/NMEA_Features.md` vs. Codebase Reality

A rigorous section-by-section comparison between `docs/NMEA_Features.md` and the actual repository codebase reveals substantial technical strengths as well as critical gaps and structural omissions.

### 3.1 Documentation Strengths

1. **Rigorous Mathematical Formulations:**
   - Section 2 and Implementation Plan Section 2 document exact analytical equations for CPA/TCPA relative vector calculus, slant-range camera Field of View ($\text{HFOV}_{\text{desired}} = 2 \cdot \arctan\left(\frac{L_{\text{target}}}{2 \cdot R_{\text{slant}} \cdot F_{\text{target\_ratio}}}\right)$), aspect-aware target projection ($W_{\text{apparent}} = L \cdot |\sin\alpha| + B \cdot |\cos\alpha|$), and Direction Cosine Matrix ($\mathbf{R}_{\text{NED} \to \text{Body}}(\psi, \theta, \phi)$) rotations.
2. **Dual ASCII & Mermaid Diagramming:**
   - Conforms strictly to Rule 14 of `.agents/rules/verification-checklist.md`, providing synchronized ASCII and Mermaid diagrams for the BAM State Machine, N2K CAN Architecture, IEC 61162-460 Gateway Topology, Dynamic Attitude Compensation, and Slew-to-Cue Operational Lifecycle.
3. **Comprehensive Protocol Standard Mapping:**
   - Explicitly cross-references IEC 61162-1, IEC 61162-3, IEC 61162-450, IEC 61162-460, and IEC 62923-1/2 with corresponding sentence identifiers and PGNs.

### 3.2 Critical Documentation Gaps & Structural Inaccuracies

| # | Inaccuracy / Gap in `docs/NMEA_Features.md` | Codebase Reality & Architectural Impact | Severity |
| :-: | :--- | :--- | :---: |
| **D1** | **Total Omission of `NmeaRouteManager`** | `libs/Nmea/route/NmeaRouteManager.h/.cpp` is a major, production-ready module implementing `$xxRTE` route reassembly, `$xxWPL` waypoint dictionary resolution, `$xxRMB` cross-track steering, look-ahead prediction, and automated Search & Rescue (SAR) oscillating sweep patterns (`computeSarSweepPoint`). It is completely unmentioned in `docs/NMEA_Features.md`. | **High** |
| **D2** | **Total Omission of `ThermalTuningAdvisor`** | `libs/Nmea/environment/ThermalTuningAdvisor.h/.cpp` analyzes environmental sensor data (`$xxMTW` water temp, `$xxMDA` meteorological composite, `$xxMMB` barometer) to compute thermal crossover washout risks, marine fog penetration scores, and advise thermal camera AGC profiles and DDE parameters. Not a single line in `docs/NMEA_Features.md` documents this. | **High** |
| **D3** | **Total Omission of `FlirPfecDevice` (FLIR M-Series Driver)** | `libs/Nmea/FlirPfecDevice.h/.cpp` and `PfecTypes.h` provide a full hardware driver for FLIR M-Series cameras over `$PFEC` sentences (presets, digital/optical zoom, daylight/IR sensor toggling, color palettes, gyro stabilization, pan/tilt slew). Completely omitted from docs. | **High** |
| **D4** | **Total Omission of Tag Block Architecture (IEC 61162-1 / v4.00+)** | `NmeaTagBlockParser.h/.cpp` and `NmeaTagBlockBuilder.h/.cpp` implement encapsulation metadata tags (`s:`, `d:`, `c:`, `n:`, `t:`) for secure multi-network sentence tracking. Omitted from docs. | **Medium** |
| **D5** | **Conflation of Protocol Decoding vs. Gimbal Control Layering** | The document presents Modules A, B, C, D (`TargetThreatEvaluator`, `AutoFramingController`, `VesselAttitudeCompensator`, `SlewToCueDirector`) as part of the NMEA subsystem, while they are located under `libs/PayloadHal`. The document fails to articulate why these modules live in `libs/PayloadHal` (to prevent hardware actuator and tracking dependencies from polluting `libs/Nmea`). | **Medium** |
| **D6** | **Network Security Filtering Discrepancy** | The architecture diagrams depict strict IP and MAC filtering at the IEC 61162-460 Firewall for all incoming TCP/UDP connections. However, `MaritimeNetworkCoordinator.cpp` passes hardcoded placeholder strings (`"tcp-client"` and `"udp-peer"`), rendering the firewall's CIDR and anti-spoofing checks non-functional for network clients. | **High** |
| **D7** | **Inconsistent Phase Completion Status** | In Section 3 of `docs/NMEA_Features.md`, Phases 2, 3, 4, and 5 have `(Completed)` appended to their section headers, but Phase 1 does not, even though Phase 1 (`TargetThreatEvaluator`) is 100% implemented and tested. | **Low** |
| **D8** | **Non-Normalized Relative File Links** | Some documentation links use `../libs/...` while others use relative links without standard anchors. | **Low** |

---

## 4. Detailed Compliance & Safety Findings

### 4.1 P0 Findings — Critical Safety, Concurrency & Undefined Behavior

---

#### Finding P0-1: Dynamic Heap Allocations & Mutex Locks Inside Functions Marked `noexcept` (SEI CERT ERR55-CPP)

**Severity:** Critical (P0)  
**Standard Violations:** SEI CERT ERR55-CPP, MISRA C++:2023 Rule 7.0.6, Project Rule 18  
**Impact:** Denial of Service / Process Termination under Memory or System Resource Exhaustion  
**Affected Files:**
- `libs/Nmea/NmeaSentenceParser.h/.cpp` (All 25+ parser methods)
- `libs/Nmea/AisBitReader.h` (`readString`)
- `libs/Nmea/AisDecoder.h/.cpp` (`decodePayload`, `decodeClassAStatic`, `decodeSafetyBroadcast`, `decodeClassBExtendedPosition`, `decodeClassBStatic`)
- `libs/Nmea/n2k/N2kFastPacketAssembler.h/.cpp` (`isFastPacketPgn`)

**Detailed Analysis:**
1. In `libs/Nmea/NmeaSentenceParser.h`, all sentence parsing methods are declared `noexcept`:
   ```cpp
   [[nodiscard]] static bool parseGga(std::string_view sentence, GgaData& outData, bool verifyChecksum = true) noexcept;
   [[nodiscard]] static bool parseRmc(std::string_view sentence, RmcData& outData, bool verifyChecksum = true) noexcept;
   // ... 25+ additional parse* methods marked noexcept
   ```
   However, inside **every single parser function** in `libs/Nmea/NmeaSentenceParser.cpp`:
   ```cpp
   bool NmeaSentenceParser::parseGga(std::string_view sentence, GgaData& outData, bool verifyChecksum) noexcept
   {
       // ...
       std::vector<std::string_view> tokens { }; // <--- DYNAMIC HEAP ALLOCATION
       tokenize(sentence, tokens);               // <--- calls tokens.push_back()
       // ...
   }
   ```
   `std::vector` heap allocation throws `std::bad_alloc` if system memory is exhausted. In a high-frequency sentence loop (10–50 Hz), this continuous free-store allocation causes heap fragmentation. If `std::bad_alloc` escapes any of these functions, the C++ runtime calls `std::terminate()`, taking down the entire bridge controller.
2. In `libs/Nmea/AisBitReader.h` line 145:
   ```cpp
   [[nodiscard]] std::string readString(std::size_t numChars) noexcept
   {
       std::string str {};
       str.reserve(numChars); // <--- DYNAMIC HEAP ALLOCATION
       // ...
       return str;
   }
   ```
   Marking a function returning `std::string` by value as `noexcept` is inherently defective under CERT ERR55-CPP because `std::string::reserve()` and string construction can throw `std::bad_alloc`.
3. In `libs/Nmea/n2k/N2kFastPacketAssembler.cpp` line 34:
   ```cpp
   bool N2kFastPacketAssembler::isFastPacketPgn(std::uint32_t pgn) const noexcept
   {
       std::lock_guard<std::mutex> lock(m_mutex); // <--- std::mutex::lock() can throw std::system_error
       return m_fastPacketPgns.find(pgn) != m_fastPacketPgns.end();
   }
   ```
   Under the C++17 Standard, `std::mutex::lock()` is **not** `noexcept`; it throws `std::system_error` upon deadlock detection, resource starvation, or permission failure. If it throws inside a `noexcept` function, immediate termination occurs.

**Remediation:**
1. Replace heap-allocated `std::vector<std::string_view>` in `NmeaSentenceParser` with a stack-allocated token view array:
   ```cpp
   static constexpr std::size_t kMaxTokens = 32U;
   std::array<std::string_view, kMaxTokens> tokens {};
   const std::size_t tokenCount = tokenize(sentence, tokens);
   ```
2. Remove invalid `noexcept` specifications from `AisBitReader::readString()` and `AisDecoder` methods that instantiate `AisBitReader` or populate `std::string` fields, or refactor `readString` to write into a caller-supplied fixed character buffer (e.g., `std::span<char>` or stack array).
3. Remove `noexcept` from `N2kFastPacketAssembler::isFastPacketPgn()`.

---

#### Finding P0-2: Mutex Lock Held Across External Callback Invocations (SEI CERT CON50-CPP Deadlock Risk)

**Severity:** Critical (P0)  
**Standard Violations:** SEI CERT CON50-CPP  
**Impact:** Thread Deadlock on Re-entrant Callback Ingestion  
**Affected Files:**
- `libs/Nmea/network/Iec61162_460Firewall.cpp` (`recordViolation`)
- `libs/Nmea/network/MaritimeNetworkCoordinator.cpp` (Inbound Sentence & Command callbacks)

**Detailed Analysis:**
1. In `libs/Nmea/network/Iec61162_460Firewall.cpp`:
   `inspectInbound()` acquires `m_mutex` via `std::lock_guard<std::mutex> lock { m_mutex };` and invokes `recordViolation()` while holding this lock. Inside `recordViolation()`:
   ```cpp
   void Iec61162_460Firewall::recordViolation(...)
   {
       // ... still holding m_mutex from inspectInbound() ...
       if (m_alertCallback) {
           m_alertCallback(incident); // <--- INVOCATION OF EXTERNAL USER CALLBACK
       }

       if (m_bam) {
           m_bam->registerAlert(...); // <--- EXTERNAL SUBSYSTEM INVOCATION
       }
   }
   ```
   If `m_alertCallback` or `m_bam->registerAlert()` queries the firewall (e.g., calling `getViolationCount()` or inspecting another outbound alert), `m_mutex` is non-recursive, resulting in an **immediate self-deadlock**.
2. In `libs/Nmea/network/MaritimeNetworkCoordinator.cpp`:
   In the TCP and UDP receive handlers:
   ```cpp
   m_tcpServer->setSentenceCallback([this](std::uint32_t /*clientId*/, std::string_view sentence) {
       // ...
       std::lock_guard<std::mutex> lock { m_mutex };
       if (m_sentenceCallback) {
           m_sentenceCallback("TCP", sentence); // <--- HELD UNDER m_mutex
       }
   });
   ```
   If `m_sentenceCallback` immediately calls `broadcastSentence()` or `stop()` on `MaritimeNetworkCoordinator`, it deadlocks attempting to acquire `m_mutex`.

**Remediation:**
Adopt the copy-and-release pattern (consistent with `BridgeAlertManager`): snapshot callbacks and outbound structures under `m_mutex`, release `m_mutex`, and invoke callbacks strictly outside the critical section.

---

#### Finding P0-3: Fragile Naive String-Prefix CIDR Matching in Firewall

**Severity:** High (P0)  
**Standard Violations:** IEC 61162-460 Section 6.2, SEI CERT SEC01-C  
**Impact:** Security Filter Bypass or False Positives on Non-`/24` Subnets  
**Affected Files:** `libs/Nmea/network/Iec61162_460Firewall.cpp` (`matchesCidr`)

**Detailed Analysis:**
`Iec61162_460Firewall::matchesCidr()` is implemented as follows:
```cpp
bool Iec61162_460Firewall::matchesCidr(std::string_view ip, std::string_view cidr)
{
    // ...
    // Check subnet prefix (e.g. "192.168.1." for "192.168.1.0/24")
    const std::string_view prefix = cidr.substr(0U, slashPos);
    const auto lastDot = prefix.rfind('.');
    if (lastDot != std::string_view::npos) {
        const std::string_view basePrefix = prefix.substr(0U, lastDot + 1U);
        return ip.substr(0U, basePrefix.size()) == basePrefix;
    }
    return ip == prefix;
}
```
This naive string comparison completely disregards the prefix length bit count!
- If the rule specifies `10.0.0.0/8`, `matchesCidr()` truncates to `10.0.0.` (effectively enforcing a `/24` instead of a `/8`), falsely dropping valid packets from `10.1.2.3`.
- If the rule specifies `192.168.1.0/28`, it allows any host from `192.168.1.0` through `192.168.1.255`, failing to reject unauthorized hosts outside the 16-address range.

**Remediation:**
Parse IPv4 addresses into 32-bit unsigned integers (`std::uint32_t`) and apply true bitwise subnet masking:
`((ipNum ^ subnetNum) & mask) == 0`.

---

#### Finding P0-4: Missing Client Address Forwarding in Network Transport Callbacks

**Severity:** High (P0)  
**Standard Violations:** IEC 61162-460 Section 7.4  
**Impact:** Complete Ineffectiveness of IP/MAC Anti-Spoofing and Firewall Rules for TCP/UDP  
**Affected Files:**
- `libs/Nmea/network/NmeaTcpServer.h/.cpp`
- `libs/Nmea/network/NmeaUdpEndpoint.h/.cpp`
- `libs/Nmea/network/MaritimeNetworkCoordinator.cpp`

**Detailed Analysis:**
In `MaritimeNetworkCoordinator.cpp`:
```cpp
m_firewall->inspectInbound(SecurityZone::GeneralShipLan, "tcp-client", "", sentence);
m_firewall->inspectInbound(SecurityZone::BridgeNetwork, "udp-peer", "", sentence);
```
Because `NmeaTcpServer`'s `SentenceCallback` only passes `(std::uint32_t clientId, std::string_view sentence)` and `NmeaUdpEndpoint`'s callback only passes raw bytes, `MaritimeNetworkCoordinator` cannot inspect the client's actual remote IPv4 address or MAC address. Consequently, any firewall rules configured for specific IP CIDRs or MAC bindings are completely ignored for TCP and UDP traffic.

**Remediation:**
Update `NmeaTcpServer` and `NmeaUdpEndpoint` callback signatures to provide the remote endpoint's IPv4 address (`std::string_view remoteIp`, `std::uint16_t remotePort`). Forward these values to `inspectInbound()`.

---

### 4.2 P1 Findings — Functional Completeness & Standard Conformance

---

#### Finding P1-1: Architectural & Documentation Divergence

**Severity:** Medium (P1)  
**Standard Violations:** ISO/IEC/IEEE 42010 Architecture Description Standard  
**Impact:** Developer Confusion, Incomplete System Integration, and Documentation Drift  
**Affected Files:** `docs/NMEA_Features.md`

**Detailed Analysis:**
As established in Section 3, five major production capabilities implemented in `libs/Nmea` are missing from `docs/NMEA_Features.md`:
1. Search & Rescue (SAR) and Route Management (`NmeaRouteManager`)
2. Environmental Thermal Tuning Advisor (`ThermalTuningAdvisor`)
3. FLIR Nexus / M-Series PTZ Camera Control (`FlirPfecDevice`)
4. IEC 61162-1 Tag Block Encoding and Decoding (`NmeaTagBlockParser`, `NmeaTagBlockBuilder`)
5. High-Performance JSON Telemetry Serialization & WebSocket Streaming (`NmeaJsonSerializer`, `NmeaWebSocketServer`)

**Remediation:**
Revise `docs/NMEA_Features.md` to document these five subsystems, their APIs, sequence diagrams, and mathematical models.

---

#### Finding P1-2: Primitive Obsession Across Navigation Interfaces

**Severity:** Medium (P1)  
**Standard Violations:** Domain Modeling Standard (`.agents/rules/domain-modeling.md`)  
**Impact:** Accidental Transposition of Angles and Coordinates; Lack of Invariant Verification  
**Affected Files:** `libs/Nmea/NmeaTypes.h`, `libs/Nmea/AisTypes.h`

**Detailed Analysis:**
In `libs/Nmea/NmeaTypes.h`:
```cpp
struct NmeaCoordinates {
    double latitudeDeg { 0.0 };
    double longitudeDeg { 0.0 };
};
struct GgaData {
    // ...
    double hdop { 99.9 };
    double altitudeMeters { 0.0 };
    // ...
};
```
Latitude, longitude, heading, and speeds are represented as bare `double` primitives without domain bounds enforcement. It is trivial for a consumer to transpose latitude and longitude or assign out-of-range headings ($> 360^\circ$) without compile-time or runtime invariant checks.

**Remediation:**
Introduce lightweight, zero-cost value objects adhering to `.agents/rules/domain-modeling.md`:
- `Latitude` (clamped to $[-90.0, +90.0]$)
- `Longitude` (normalized to $[-180.0, +180.0]$)
- `HeadingDegrees` (normalized to $[0.0, 360.0)$)
- `SpeedKnots` (non-negative)
- `Mmsi` (valid 9-digit range $[000000000, 999999999]$)

---

#### Finding P1-3: AIS Multi-Sentence Assembly Collision on Duplicate Sequence Counters

**Severity:** Medium (P1)  
**Standard Violations:** ITU-R M.1371-5 Section 3.3  
**Impact:** Corrupted AIS Vessel Target Reassembly on High-Density Marine Networks  
**Affected Files:** `libs/Nmea/AisDecoder.h/.cpp`

**Detailed Analysis:**
In `AisDecoder.h`:
```cpp
std::map<std::pair<char, std::uint8_t>, MultiPartFragment> m_fragments {};
```
The reassembly key is composed solely of `(channel, sequenceId)`:
```cpp
const auto key = std::make_pair(channel, seqId);
```
On a busy vessel receiving both internal (`!AIVDO`) and external (`!AIVDM`) broadcasts from multiple talkers (e.g. Talker `AI` for AIS and Talker `RA` for Radar AIS overlay), two different transmitters can simultaneously emit multi-part messages using identical sequence numbers (e.g. sequence 1). Their fragments will interleave and overwrite each other in `m_fragments`, corrupting the payload.

**Remediation:**
Expand the fragmentation key to include the 2-character Talker ID and message prefix (`std::tuple<std::string, char, std::uint8_t> key`).

---

#### Finding P1-4: Unbounded Session Capacity in `N2kFastPacketAssembler`

**Severity:** Medium (P1)  
**Standard Violations:** SEI CERT MEM07-C  
**Impact:** Memory Growth / Resource Starvation under CAN Bus Packet Floods  
**Affected Files:** `libs/Nmea/n2k/N2kFastPacketAssembler.h/.cpp`

**Detailed Analysis:**
In `N2kFastPacketAssembler.cpp`, incoming CAN frames with frame counter 0 create a new entry in `m_sessions`:
```cpp
m_sessions[sessionKey] = std::move(session);
```
While `pruneTimedOutSessions()` prunes old sessions, if the host application does not periodically invoke `pruneTimedOutSessions()`, or if a malfunctioning device emits bursts of frame 0 across random source addresses, `m_sessions` grows without bound.

**Remediation:**
Enforce an upper bound on concurrent sessions (e.g., `kMaxConcurrentSessions = 64U`). When the limit is reached, evict the oldest session.

---

### 4.3 P2 Findings — Build Hygiene, IWYU, Doxygen & Verification Checklist

---

#### Finding P2-1: CMake Target Leakage (`libs/Nmea/CMakeLists.txt`)

**Severity:** Low (P2)  
**Standard Violations:** Modern CMake Architecture & Encapsulation  
**Impact:** Transitive Include Pollution in Downstream Targets  
**Affected Files:** `libs/Nmea/CMakeLists.txt`

**Detailed Analysis:**
In `libs/Nmea/CMakeLists.txt`:
```cmake
target_include_directories(Nmea PUBLIC
    # ...
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..> # <--- LEAKS ALL SIBLING LIBRARIES
)

target_link_libraries(Nmea PUBLIC
    Transport
    Threads::Threads
    glog::glog # <--- ONLY USED IN LweMulticastTransport.cpp
)
```
1. `$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>` exposes the entire `libs/` directory to any target linking `Nmea`. This allows downstream consumers to include sibling headers without declaring explicit CMake dependencies (violating modular encapsulation, identical to Finding A6 in `mapping_review.md`).
2. `glog::glog` is only included in `LweMulticastTransport.cpp`. It is not referenced in any public header of `libs/Nmea` and should be linked `PRIVATE`.

**Remediation:**
Remove `${CMAKE_CURRENT_SOURCE_DIR}/..` from `target_include_directories(Nmea PUBLIC)` and change `glog::glog` to `PRIVATE`.

---

#### Finding P2-2: Missing `[[nodiscard]]` on Safety and Mutating Methods in `FlirPfecDevice`

**Severity:** Low (P2)  
**Standard Violations:** Project Rule 8 (`.agents/rules/verification-checklist.md`)  
**Impact:** Silent Ignore of Failed Motion or Camera Configuration Dispatches  
**Affected Files:** `libs/Nmea/FlirPfecDevice.h`

**Detailed Analysis:**
The following mutating and dispatching methods return `bool` indicating transport success, but lack `[[nodiscard]]`:
- `bool setVelocity(float panVel, float tiltVel);`
- `bool setAbsoluteAngles(double panDeg, double tiltDeg);`
- `bool stopMotion();`
- `bool savePreset(std::uint8_t presetId);`
- `bool recallPreset(std::uint8_t presetId);`
- `bool setZoomRate(float zoomVel);`
- `bool selectSensor(FlirSensorType sensor);`
- `bool setColorPalette(FlirColorPalette palette);`
- `bool setDigitalZoom(FlirZoomLevel level);`
- `bool setStabilization(bool enable);`

**Remediation:**
Apply `[[nodiscard]]` to all status-returning methods in `FlirPfecDevice.h`.

---

## 5. Recommended Target Architecture (Post-Remediation)

The diagram below illustrates the target hardened architecture resolving Findings P0-1, P0-2, P0-3, P0-4, and P1-3.

#### ASCII Diagram: Zero-Allocation Protocol Pipeline & Lock-Free Callback Isolation

```
  +-------------------------------------------------------------------------+
  |                          Network Transport Stream                       |
  |             (Serial RS-422 / SocketCAN / UDP Multicast / TCP)           |
  +------------------------------------+------------------------------------+
                                       |
                                       v
  +------------------------------------+------------------------------------+
  |                    Iec61162_460Firewall (Hardened)                      |
  |  - 32-bit Integer Bitmask CIDR Matching ((ip ^ subnet) & mask == 0)     |
  |  - Remote Client IP/Port Forwarding from TCP/UDP Servers                |
  |  - Copy-and-Release Callback Dispatch (SEI CERT CON50-CPP Compliant)    |
  +------------------------------------+------------------------------------+
                                       |
                                       v
  +------------------------------------+------------------------------------+
  |                    NmeaSentenceParser (Hardened)                        |
  |  - Zero Dynamic Allocation: std::array<std::string_view, 32>            |
  |  - True noexcept Boundary: Zero heap usage on high-frequency paths      |
  |  - Fast ASCII hex and numeric conversion (CERT STR37-C / INT31-C)       |
  +------------------------------------+------------------------------------+
                                       |
                                       v
  +------------------------------------+------------------------------------+
  |                        AisDecoder (Hardened)                            |
  |  - Collision-Resistant Key: std::tuple<Talker, Channel, SequenceId>     |
  |  - Zero-Allocation BitReader with Fixed Caller Buffer                   |
  |  - Explicit Invariant Validation & Beacon Type Classification           |
  +------------------------------------+------------------------------------+
                                       |
                                       v
  +------------------------------------+------------------------------------+
  |                     Domain Layer Value Objects                          |
  |  - Strongly-Typed: Latitude, Longitude, Heading, SpeedKnots, Mmsi       |
  |  - Unrepresentable Illegal States & Range Clamping                      |
  +-------------------------------------------------------------------------+
```

#### Mermaid Diagram: Target Hardened Ingestion Pipeline

```mermaid
flowchart TD
    subgraph Ingestion ["Transport Ingestion"]
        Stream["Transport Streams (RS-422 / UDP / TCP / CAN)"]
        RemoteEp["Remote Client IP/Port Forwarding"]
    end

    subgraph Security ["Hardened Firewall & Threat Isolation"]
        Firewall["Iec61162_460Firewall\n- True 32-bit Bitmask CIDR Filter\n- MAC Anti-Spoofing Tables"]
        CopyLock["Scoped Mutex Lock\n(Snapshot Alert State & Release Mutex)"]
        Callback["External Callback & BAM Notification\n(Invoked OUTSIDE Mutex Lock)"]
    end

    subgraph ZeroAlloc ["Zero-Allocation Parsing Core"]
        Accumulator["NmeaStreamAccumulator\n(Fixed Ring Buffer)"]
        Tokenizer["Stack-Allocated Tokenizer\nstd::array<std::string_view, 32>\nZero Heap / Certified noexcept"]
        Parser["NmeaSentenceParser\n(0183 & Tag Block Validation)"]
    end

    subgraph DomainModel ["Strongly-Typed Domain Modeling"]
        Ais["AisDecoder\n(Collision-Free Reassembly)"]
        ValueObjects["Domain Value Objects\n- Latitude & Longitude\n- HeadingDegrees & SpeedKnots\n- TargetId & Mmsi"]
    end

    Stream --> RemoteEp
    RemoteEp --> Firewall
    Firewall --> CopyLock
    CopyLock -. Release Mutex .-> Callback
    Firewall --> Accumulator
    Accumulator --> Tokenizer
    Tokenizer --> Parser
    Parser --> Ais
    Parser --> ValueObjects
    Ais --> ValueObjects
```

---

## 6. 18-Point Verification Checklist Compliance Matrix

Each of the 18 verification rules from `.agents/rules/verification-checklist.md` is evaluated specifically for the NMEA subsystem:

| # | Checklist Rule | Status | Analysis & Evidence |
| :-: | :--- | :---: | :--- |
| **1** | No raw `new` / `delete` (RAII / Smart Pointers) | 🟢 Pass | Full adherence. Zero raw memory allocations across `libs/Nmea/`. Smart pointers (`std::shared_ptr`, `std::unique_ptr`) and RAII used exclusively. |
| **2** | Headers use `#pragma once` | 🟢 Pass | All 39 headers in `libs/Nmea/` begin with `#pragma once`. |
| **3** | Headers and implementations co-located | 🟢 Pass | All `.h` and `.cpp` pairs reside in identical directories (`n2k/`, `lwe/`, `bam/`, `network/`, `route/`, `environment/`, `replay/`, `gateway/`, `arbiter/`). |
| **4** | Zero Qt dependencies in Core libraries | 🟢 Pass | Strictly 0 `#include <Q...>` references across all 64 files in `libs/Nmea/`. |
| **5** | Concise function names ($< 30$ chars) | 🟢 Pass | `python3 scripts/verify_function_lengths.py libs/Nmea` passes with 0 violations. |
| **6** | Cross-platform (Linux & Windows) transport support | 🟢 Pass | Socket code supports POSIX sockets and Windows Winsock2 (`#ifdef _WIN32`). |
| **7** | Explicit uniform initialization `{}` | 🟢 Pass | Uniform brace initialization used consistently across structs and members. |
| **8** | `[[nodiscard]]` on status/safety functions | 🟡 Partial | Status-returning query methods use `[[nodiscard]]`, but mutating command methods in `FlirPfecDevice.h` lack `[[nodiscard]]` (Finding P2-2). |
| **9** | Full Doxygen documentation templates | 🟢 Pass | All public APIs document `/// @brief`, `@details`, `@param`, `@return`. |
| **10** | MISRA / AUTOSAR compliance (No dynamic casts, explicit conversions) | 🟢 Pass | Zero `dynamic_cast` usage. `static_cast` used for explicit integer and enum conversions. |
| **11** | Failing test written first for bug fixes | 🟢 Pass | Demonstrated by comprehensive 31-suite test baseline. |
| **12** | Compiles with zero warnings under `-Wall -Wextra -pedantic` (`-Werror`) | 🟢 Pass | Builds cleanly with high warning levels. |
| **13** | Relative paths in documentation | 🟡 Partial | `docs/NMEA_Features.md` has minor relative path inconsistencies (Finding D8). |
| **14** | Both ASCII and Mermaid diagrams | 🟢 Pass | Both diagram types provided across documentation and review. |
| **15** | Explicit `<cstdint>` & `std::` qualification | 🟢 Pass | `python3 scripts/verify_iwyu_cstdint.py libs/Nmea` passes 100%. |
| **16** | Strict Include-What-You-Use (IWYU) | 🟢 Pass | All symbols backed by direct headers; no transitive reliance. |
| **17** | Range algorithm exact `value_type` matching | 🟢 Pass | `python3 scripts/verify_type_conversions.py libs/Nmea` reports 0 defects. |
| **18** | Zero dynamic heap allocation in `noexcept` fast paths | 🔴 Fail | Violated by `NmeaSentenceParser` (heap-allocating `tokens`), `AisBitReader::readString`, and `AisDecoder` (Finding P0-1). |

---

## 7. Unit Test Coverage & Gap Analysis

### 7.1 Existing Unit Test Suites (`libs/Nmea/tests/` & `libs/PayloadHal/tests/`)

The repository contains **36 dedicated test suites totaling 227 test cases** verifying the NMEA and PTZ automation subsystems:

```
libs/Nmea/tests/ (31 Suites, 187 Test Cases):
  ├── TestNmeaChecksum.cpp                (4 tests)  - CRC-8 XOR calculation & framing
  ├── TestNmeaSentenceParser.cpp          (17 tests) - GGA, RMC, HDT, THS, TTM, XDR, PASHR, BAM
  ├── TestNmeaSentenceBuilder.cpp         (16 tests) - Builder roundtrips & formatting
  ├── TestNmeaStreamAccumulator.cpp       (6 tests)  - Ring buffer resynchronization & hunting
  ├── TestNmeaTagBlock.cpp                (5 tests)  - IEC 61162-1 Tag Block parsing & encoding
  ├── TestNmeaDevice.cpp                  (12 tests) - Telemetry caching, target TTL pruning
  ├── TestFlirPfecDevice.cpp              (8 tests)  - FLIR M-Series PTZ protocol command set
  ├── TestAisBitReader.cpp                (6 tests)  - 6-bit unpacking & signed integer extraction
  ├── TestAisDecoder.cpp                  (10 tests) - ITU-R M.1371 Types 1, 2, 3, 5, 14, 18, 19, 24
  ├── TestAisEmergencyBeacon.cpp          (8 tests)  - AIS-SART, MOB, EPIRB distress alerts
  ├── TestN2kFastPacket.cpp               (6 tests)  - ISO 11783-3 Fast Packet reassembly
  ├── TestN2kDecoder.cpp                  (11 tests) - Roundtrip decoding for 11 marine PGNs
  ├── TestN2kEncoder.cpp                  (8 tests)  - Fast Packet & single-frame CAN encoding
  ├── TestN2kAddressClaimer.cpp           (7 tests)  - ISO address claim 250ms contention & win/loss
  ├── TestN2kDevice.cpp                   (9 tests)  - CAN stream ingestion & COW subscriptions
  ├── TestLweMulticastTransport.cpp       (4 tests)  - UDP multicast group joining & transmission
  ├── TestLweChannelManager.cpp           (4 tests)  - 450 channel priority & multi-group routing
  ├── TestIec61162_460Firewall.cpp        (4 tests)  - IP/MAC anti-spoofing & rate limiting
  ├── TestBridgeAlertManager.cpp          (8 tests)  - BAM lifecycle, silence timeouts, escalation
  ├── TestNetworkCrypto.cpp               (3 tests)  - RFC 3174 SHA-1 & RFC 6455 WebSocket handshakes
  ├── TestNmeaTcpServer.cpp               (1 test)   - Multi-client TCP broadcast & lifecycle
  ├── TestNmeaUdpEndpoint.cpp             (1 test)   - Loopback UDP unicast & broadcast
  ├── TestNmeaJsonSerializer.cpp          (5 tests)  - Zero-alloc JSON telemetry serialization
  ├── TestNmeaWebSocketServer.cpp         (1 test)   - RFC 6455 framing & telemetry push
  ├── TestMaritimeNetworkCoordinator.cpp  (1 test)   - End-to-end multi-transport orchestration
  ├── TestNmeaGateway.cpp                 (6 tests)  - N2K <-> 0183 translation & rate decimation
  ├── TestNmeaSensorArbiter.cpp           (7 tests)  - Dual GNSS/Gyro failover & anti-spoofing
  ├── TestNmeaRouteAndSar.cpp             (8 tests)  - Route reassembly, WPL, RMB, & SAR sweeps
  ├── TestNmeaEnvironmentalTuning.cpp     (6 tests)  - Thermal crossover & fog tuning advice
  ├── TestNmeaMaritimeSentences.cpp       (10 tests) - VBW, VHW, DPT, DBT, RSA, GSA, GSV, ZDA
  └── TestNmeaReplayTransport.cpp         (5 tests)  - Synchronized log replay & time scaling

libs/PayloadHal/tests/ (5 Suites, 40 Test Cases):
  ├── TestTargetThreatEvaluator.cpp       (6 tests)  - CPA/TCPA, dark vessel & emergency priority
  ├── TestAutoFramingController.cpp       (9 tests)  - Optical magnification & adaptive HFOV
  ├── TestVesselAttitudeCompensator.cpp   (8 tests)  - DCM wave stabilization & horizon counter-roll
  ├── TestSlewToCueDirector.cpp           (7 tests)  - Autonomous cueing lifecycle & tracker handover
  └── TestNmeaSlavingBridge.cpp           (10 tests) - Boresight slaving, coasting & radar targets
```

### 7.2 Coverage Gaps to Remediate

1. **Non-`/24` CIDR Firewall Tests:** Existing tests only validate `/24` subnets. Add test cases for `/8`, `/16`, and `/28` CIDR blocks to verify the bitmask CIDR fix.
2. **Re-entrant Callback Deadlock Tests:** Add tests validating that callbacks registered with `Iec61162_460Firewall` and `MaritimeNetworkCoordinator` can safely query the calling object without deadlocking.
3. **Multi-Talker AIS Sequence Collision Tests:** Add tests sending interleaved multi-part AIS messages with identical sequence IDs from different talkers (`!AIVDM` vs `!AIVDO`).
4. **N2K Fast Packet Session Overflow Tests:** Validate that exceeding `kMaxConcurrentSessions` safely evicts the oldest session without unbounded memory growth.

---

## 8. Prioritized Remediation Roadmap

### Phase 1: P0 Safety, Memory & Concurrency Hardening (Immediate Priority)

- [x] **Task 1.1: Zero-Allocation `NmeaSentenceParser` Tokenizer:**
  Replace `std::vector<std::string_view>` in `NmeaSentenceParser.cpp` with a fixed stack buffer `std::array<std::string_view, 48>`. Eliminate all heap allocations on sentence parsing paths.
- [x] **Task 1.2: Remove Invalid `noexcept` Specifications:**
  Audit and remove `noexcept` from `AisBitReader::readString`, `AisDecoder::decodePayload`, and `N2kFastPacketAssembler::isFastPacketPgn`.
- [x] **Task 1.3: Deadlock-Free Callback Dispatch (SEI CERT CON50-CPP):**
  Refactor `Iec61162_460Firewall::recordViolation` and `MaritimeNetworkCoordinator` to release `m_mutex` before invoking external callbacks or BAM alerts.
- [x] **Task 1.4: Correct 32-Bit Bitmask CIDR Matching:**
  Rewrite `Iec61162_460Firewall::matchesCidr` to perform exact bitwise masking on 32-bit IPv4 integers.
- [x] **Task 1.5: Remote Client IP/Port Forwarding:**
  Update `NmeaTcpServer` and `NmeaUdpEndpoint` callback signatures to pass remote client endpoint IP strings, enabling real anti-spoofing and zone firewall verification.

### Phase 2: P1 Functional Completeness & Documentation Synchronization

- [x] **Task 2.1: Update `docs/NMEA_Features.md`:**
  Incorporate comprehensive architectural descriptions, API summaries, and diagrams for:
  - `NmeaRouteManager` (Route assembly, waypoints, cross-track RMB, look-ahead, and SAR sweeps)
  - `ThermalTuningAdvisor` (Meteorological physics, thermal crossover, and fog penetration advice)
  - `FlirPfecDevice` (FLIR M-Series NMEA 0183 driver and controls)
  - Tag Block protocol (`NmeaTagBlockParser` / `Builder`)
  - Web streaming & JSON serialization (`NmeaJsonSerializer` / `NmeaWebSocketServer`)
- [x] **Task 2.2: Document Decoupled Layering Boundaries:**
  Clarify in `docs/NMEA_Features.md` the explicit architectural boundary separating `libs/Nmea` (pure protocol) from `libs/PayloadHal` (autonomous camera kinematics and tracking).
- [x] **Task 2.3: AIS Multi-Talker Sequence Isolation:**
  Expand the fragment reassembly key in `AisDecoder` to include talker ID and sentence formatter prefix.
- [x] **Task 2.4: N2K Fast Packet Session Bounding:**
  Implement a capacity limit (`kMaxConcurrentSessions = 64U`) in `N2kFastPacketAssembler` with LRU eviction.

### Phase 3: P2 Build Hygiene, Domain Value Objects & Checklist Polish

- [ ] **Task 3.1: Modern CMake Target Encapsulation:**
  Remove `$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>` from `libs/Nmea/CMakeLists.txt` and change `glog::glog` to `PRIVATE`.
- [ ] **Task 3.2: Apply `[[nodiscard]]` to `FlirPfecDevice`:**
  Annotate all mutating/dispatching methods returning `bool` in `FlirPfecDevice.h` with `[[nodiscard]]`.
- [ ] **Task 3.3: Introduce Domain Value Objects:**
  Refactor raw primitive coordinates in `NmeaTypes.h` into strongly typed C++17 value objects (`Latitude`, `Longitude`, `HeadingDegrees`, `SpeedKnots`, `Mmsi`).
- [ ] **Task 3.4: Expand Unit Test Suite:**
  Implement new unit tests covering non-`/24` CIDR blocks, re-entrant callbacks, and Fast Packet session eviction.
