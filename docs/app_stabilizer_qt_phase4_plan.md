# Implementation Plan: `app-stabilizer-qt` Phase 4 — QML Tactical UI & Interactive Controls

## 1. Executive Summary & Objectives

### 1.1 Context & Scope
Phase 4 delivers the complete interactive graphical user interface and tactical control plane for **`app-stabilizer-qt`** (executable target: **`StabilizerAppQt`**).

Building directly upon the foundational infrastructure established in prior phases:
- **Phase 1**: Application bootstrap, CMake build system integration, and dark tactical window skeleton.
- **Phase 2**: Multi-source frame ingestion (`FFmpegDecoder`, `Live555Decoder`, `DeviceEnumerator`, `SyntheticJitterGenerator`), dedicated decoding worker thread, and zero-heap `AtomicTripleBuffer` frame exchange.
- **Phase 3**: Real-time presentation engine (`StabilizerQuickItem`), interactive split-screen wipe divider, synchronized side-by-side dual view, zero-heap difference heatmap LUT, and sub-pixel tactical overlays (feature landmarks, quiver vectors, crop boundaries, center reticle).

Phase 4 bridges operator inputs and algorithm tuning with the underlying [`libs/Stabilizer`](../libs/Stabilizer) engine. It provides computer vision engineers and flight test operators with an exhaustive, responsive, and visually intuitive control testbench. Operators can adjust every mathematical and algorithmic parameter in real time without stream interruption, select between multiple ingestion modalities, monitor microsecond stage latencies, observe rolling trajectory strip-charts, and switch between battle-tested operational presets with a single click.

Phase 4 specifically addresses five core tactical UI capabilities:
1. **Reusable Tactical Control Component Library**:
   - Standardized, high-contrast dark military/aerospace UI components ([`Components/TacticalSlider.qml`](../app-stabilizer-qt/qml/Components/TacticalSlider.qml), [`Components/TacticalSwitch.qml`](../app-stabilizer-qt/qml/Components/TacticalSwitch.qml), [`Components/TacticalComboBox.qml`](../app-stabilizer-qt/qml/Components/TacticalComboBox.qml), [`Components/StatusBadge.qml`](../app-stabilizer-qt/qml/Components/StatusBadge.qml)).
2. **Unified Source Management & Transport Dashboard**:
   - Intuitive source selector supporting local media files, low-latency live RTSP/UDP streams, auto-discovered hardware capture devices (DirectShow on Windows, V4L2 on Linux), and procedural vibration generator (`SyntheticJitterGenerator`).
   - Integrated media transport scrubber: Play, Pause, Single-Frame Step ($\pm 1$), Seek, Playback Rate ($0.25\times$ to $4.0\times$), and Loop toggle.
3. **Exhaustive Accordion Parameter Matrix**:
   - Organized accordion panels exposing **all parameters** across the 6 engine modules: Core Mode & Supervisory, Trajectory Smoothing (Kalman, Gaussian FIR, L1), Feature Detector (Shi-Tomasi & Grid Bucketing), Optical Flow Tracker (Pyramidal Lucas-Kanade), Motion Model Estimator (MSAC Consensus), and Warp Engine (SIMD Affine & CMOS Rolling Shutter).
   - Real-time parameter hot-reloading with zero video stuttering or pipeline stalls.
4. **Real-Time Telemetry & Trajectory Strip-Charts**:
   - Microsecond stage latency HUD (Pyramid, Detector, Flow, Estimator, Filter, Warp, Total Frame) with frame-budget indicators (16.6ms / 33.3ms).
   - Live 2x3 Affine transformation matrix readout comparing raw, smoothed, and correction matrices.
   - Dual-channel 150-frame rolling trajectory strip-chart rendered via hardware-accelerated QML Canvas (Raw vs. Smoothed X/Y, Jitter Variance, Crop Margin Utilization with soft damping alert).
5. **Operational Preset Management**:
   - Instantaneous application of tuned configurations: "Drone Gimbal", "Pole Vibration", "Handheld Action Cam", "Rolling Shutter", and "Bypass".
   - Export and import of custom parameter configurations via standard JSON.

---

### 1.2 Target Deliverables for Phase 4

| Deliverable | Target Files | Description |
| :--- | :--- | :--- |
| **Tactical QML Components** | [`app-stabilizer-qt/qml/Components/TacticalSlider.qml`](../app-stabilizer-qt/qml/Components/TacticalSlider.qml)<br>[`app-stabilizer-qt/qml/Components/TacticalSwitch.qml`](../app-stabilizer-qt/qml/Components/TacticalSwitch.qml)<br>[`app-stabilizer-qt/qml/Components/TacticalComboBox.qml`](../app-stabilizer-qt/qml/Components/TacticalComboBox.qml)<br>[`app-stabilizer-qt/qml/Components/StatusBadge.qml`](../app-stabilizer-qt/qml/Components/StatusBadge.qml) | High-contrast, dark-themed tactical UI building blocks with numeric readouts, glowing accents, and keyboard navigation. |
| **Source Management Panel** | [`app-stabilizer-qt/qml/SourcePanel.qml`](../app-stabilizer-qt/qml/SourcePanel.qml) | Source ingestion selector (File, RTSP/UDP, Hardware Device, Synthetic Shaker) and comprehensive playback transport controls. |
| **Accordion Parameter Panel** | [`app-stabilizer-qt/qml/StabilizerControlPanel.qml`](../app-stabilizer-qt/qml/StabilizerControlPanel.qml) | Collapsible 6-category accordion controls for real-time stabilization tuning with individual section defaults and hot-reload bindings. |
| **Real-Time Diagnostics Panel** | [`app-stabilizer-qt/qml/DiagnosticsPanel.qml`](../app-stabilizer-qt/qml/DiagnosticsPanel.qml) | Real-time tracking health telemetry, 2x3 affine matrix comparison, and microsecond pipeline latency breakdown bar chart. |
| **Trajectory Strip-Chart** | [`app-stabilizer-qt/qml/TrajectoryChart.qml`](../app-stabilizer-qt/qml/TrajectoryChart.qml) | High-performance QML Canvas strip-chart plotting 150-frame rolling trajectory history, jitter variance, and margin damping. |
| **C++ Configuration Model** | [`app-stabilizer-qt/StabilizerConfigModel.h`](../app-stabilizer-qt/StabilizerConfigModel.h)<br>[`app-stabilizer-qt/StabilizerConfigModel.cpp`](../app-stabilizer-qt/StabilizerConfigModel.cpp) | QObject ViewModel exposing strongly-typed domain configuration to QML with invariant bounds validation and change dispatch. |
| **Telemetry History Buffer** | [`app-stabilizer-qt/TelemetryRingBuffer.h`](../app-stabilizer-qt/TelemetryRingBuffer.h)<br>[`app-stabilizer-qt/TelemetryRingBuffer.cpp`](../app-stabilizer-qt/TelemetryRingBuffer.cpp) | Zero-heap circular history of 150 telemetry frames providing thread-safe, lock-free snapshots for QML strip-chart drawing. |
| **Preset Management Engine** | [`app-stabilizer-qt/PresetManager.h`](../app-stabilizer-qt/PresetManager.h)<br>[`app-stabilizer-qt/PresetManager.cpp`](../app-stabilizer-qt/PresetManager.cpp) | Static factory presets dictionary, invariant validation, and JSON configuration import/export. |
| **Controller Enhancements** | [`app-stabilizer-qt/StabilizerPlayerController.h`](../app-stabilizer-qt/StabilizerPlayerController.h)<br>[`app-stabilizer-qt/StabilizerPlayerController.cpp`](../app-stabilizer-qt/StabilizerPlayerController.cpp) | Integration of `StabilizerConfigModel`, `TelemetryRingBuffer`, `DeviceEnumerator` queries, and preset loading into master ViewModel. |
| **Master Layout & Integration** | [`app-stabilizer-qt/qml/Main.qml`](../app-stabilizer-qt/qml/Main.qml)<br>[`app-stabilizer-qt/qml.qrc`](../app-stabilizer-qt/qml.qrc) | Master window layout integrating collapsible side panels, top header preset bar, presentation canvas, and bottom status bar. |
| **Automated Verification Tests** | [`tests/TestStabilizerConfigModel.cpp`](../tests/TestStabilizerConfigModel.cpp)<br>[`tests/TestPresetManager.cpp`](../tests/TestPresetManager.cpp)<br>[`tests/TestTelemetryRingBuffer.cpp`](../tests/TestTelemetryRingBuffer.cpp) | Comprehensive GoogleTest suite validating QML property reflection, bounds clamping, JSON preset persistence, and zero-heap ring buffering. |

---

## 2. System Architecture & UI State Synchronization

### 2.1 End-to-End ViewModel Architecture

The application adopts a strict MVVM (Model-View-ViewModel) architectural pattern. The QML presentation tier remains completely decoupled from the C++ video stabilization pipeline and decoding threads. All interactions flow through `StabilizerPlayerController`, which coordinates child ViewModels (`StabilizerConfigModel`, `TelemetryRingBuffer`, `PresetManager`) and dispatches synchronized commands to `StabilizerWorker`.

#### ASCII Architecture Diagram

```
+-----------------------------------------------------------------------------------------------------------------------+
|                                              QML USER INTERFACE LAYER (Qt Quick)                                      |
|                                                                                                                       |
|  +-----------------------------------------------------------------------------------------------------------------+  |
|  |                                                 Main.qml                                                        |  |
|  |  +-------------------+  +------------------------------------+  +--------------------------------------------+  |  |
|  |  |  SourcePanel.qml  |  |       VideoDisplayView.qml         |  |        StabilizerControlPanel.qml          |  |  |
|  |  |  - File Browser   |  |  +------------------------------+  |  |  - Mode & Supervisory Accordion            |  |  |
|  |  |  - RTSP URL Field |  |  |     StabilizerQuickItem      |  |  |  - Trajectory Smoothing Accordion          |  |  |
|  |  |  - Device Dropdown|  |  |  - Split Wipe / Dual View    |  |  |  - Feature Detector Accordion              |  |  |
|  |  |  - Synthetic Shkr |  |  |  - Sub-pixel Landmarks       |  |  |  - Optical Flow Tracker Accordion          |  |  |
|  |  |  - Transport Bar  |  |  |  - Quiver Plot / Reticle     |  |  |  - Motion Estimator Accordion              |  |  |
|  |  +---------│---------+  |  +------------------------------+  |  |  - Warp & Rolling Shutter Accordion        |  |  |
|  |            │            |  +------------------------------+  |  +---------------------│----------------------+  |  |
|  |            │            |  |     TrajectoryChart.qml      |  |                        │                      |  |
|  |            │            |  |  - 150-frame Canvas Strip    |  |  +---------------------│----------------------+  |  |
|  |            │            |  +------------------------------+  |  |         DiagnosticsPanel.qml           |  |  |
|  |            │            +------------------│-----------------+  |  - Latency Profiling Bar Chart             |  |  |
|  |            │                               │                    |  - 2x3 Affine Transform Inspector          |  |  |
|  |            │                               │                    +---------------------│----------------------+  |  |
|  +------------│-------------------------------│------------------------------------------│-------------------------+  |
+---------------│-------------------------------│------------------------------------------│----------------------------+
                │                               │ Q_PROPERTY bindings / Q_INVOKABLE        │
                ▼                               ▼                                          ▼
+-----------------------------------------------------------------------------------------------------------------------+
|                                    C++ VIEWMODEL LAYER (Main GUI Thread)                                              |
|                                                                                                                       |
|  +-----------------------------------------------------------------------------------------------------------------+  |
|  |                                            StabilizerPlayerController                                           |  |
|  |  - Coordinates Playback Lifecycle (Idle, Opening, Playing, Paused, Error)                                       |  |
|  |  - Dispatches Asynchronous Worker Commands (play, pause, step, seek, openSource)                                |  |
|  |                                                                                                                 |  |
|  |  +-----------------------------+   +------------------------------+   +--------------------------------------+  |  |
|  |  |    StabilizerConfigModel    |   |      TelemetryRingBuffer     |   |            PresetManager             |  |  |
|  |  |  - Reflects all 38 params   |   |  - Rolling 150 frame history |   |  - Static preset library             |  |  |
|  |  |  - Validates invariants     |   |  - Lock-free atomic snapshot |   |  - JSON import / export              |  |  |
|  |  |  - Emits configChanged()    |   |  - Strip-chart coordinate gen|   |  - Applies configs to ConfigModel    |  |  |
|  |  +--------------│--------------+   +--------------▲---------------+   +--------------------------------------+  |  |
|  +-----------------│---------------------------------│-------------------------------------------------------------+  |
+--------------------│---------------------------------│----------------------------------------------------------------+
                     │ Asynchronous Signals / Slots    │ Atomic Telemetry Dispatch
                     ▼                                 │
+-----------------------------------------------------------------------------------------------------------------------+
|                                    C++ WORKER SUBSYSTEM (Dedicated QThread)                                           |
|                                                                                                                       |
|  +-----------------------------------------------------------------------------------------------------------------+  |
|  |                                                 StabilizerWorker                                                |  |
|  |  - Input Decoders: FFmpegDecoder / Live555Decoder / DeviceEnumerator / SyntheticJitterGenerator                  |  |
|  |  - Pending Config Latched on Next Frame Boundary: pendingConfig_ -> engine_.configure()                        |  |
|  |  - VideoStabilizerEngine Pipeline: Pyramid -> Shi-Tomasi -> LK Flow -> MSAC -> Kalman -> SIMD Warp             |  |
|  |  - Publishes StabilizerFramePacket into AtomicTripleBuffer                                                       |  |
|  |  - Emits telemetryUpdated(diagnostics, fps) to GUI Thread                                                        |  |
|  +-----------------------------------------------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------------------------------------------+
```

#### Mermaid State & Interaction Architecture

```mermaid
flowchart TD
    subgraph UI["QML Tactical Presentation Tier"]
        M1["Main.qml (Shell & Hotkeys)"]
        SP["SourcePanel.qml (Sources & Transport)"]
        CP["StabilizerControlPanel.qml (Accordion)"]
        DP["DiagnosticsPanel.qml (Telemetry HUD)"]
        TC["TrajectoryChart.qml (Canvas Strip-Chart)"]
        QI["StabilizerQuickItem.qml (Video View)"]
    end

    subgraph ViewModel["C++ ViewModel Architecture (GUI Thread)"]
        CTRL["StabilizerPlayerController"]
        CFG["StabilizerConfigModel (38 Params)"]
        RING["TelemetryRingBuffer (150 Frames)"]
        PST["PresetManager (Factory Presets)"]
    end

    subgraph Backend["High-Priority Worker Thread (QThread)"]
        WRK["StabilizerWorker"]
        ENG["VideoStabilizerEngine"]
        DEC["IVideoDecoder / SyntheticGen"]
        BUF["AtomicTripleBuffer<StabilizerFramePacket>"]
    end

    M1 --> SP & CP & DP & TC & QI
    SP -->|openSource / play / pause / seek| CTRL
    CP -->|Q_PROPERTY Two-Way Binding| CFG
    M1 -->|loadPreset("Drone Gimbal")| PST
    PST -->|Apply Config| CFG
    CFG -->|configChanged(VideoStabilizerConfig)| CTRL
    CTRL -->|QMetaObject::invokeMethod| WRK

    WRK -->|Latch Pending Config on Frame Boundary| ENG
    DEC -->|Decoded Frames| WRK
    WRK -->|processFrame()| ENG
    ENG -->|Stabilized Output + Inliers + Latencies| WRK
    WRK -->|Write Slot Swap| BUF
    WRK -->|telemetryUpdated(diagnostics)| CTRL
    CTRL -->|appendSample()| RING
    RING -->|historyUpdated()| TC
    CTRL -->|telemetryUpdated()| DP
    BUF -->|swapReadBuffer()| QI
```

---

### 2.2 Threading & Parameter Hot-Reloading Pipeline

A key design requirement is **zero video stuttering during parameter adjustment**. In typical computer vision testbenches, modifying parameters often restarts decoders or re-allocates memory arenas, causing dropped frames. `app-stabilizer-qt` guarantees non-stalling hot-reloads via an atomic two-stage latching protocol:

```
[QML Tactical Slider Drag]
          │
          ▼
[StabilizerConfigModel::setParam()]
          │ Invariant Validation & Clamping
          ▼
[StabilizerPlayerController::updateStabilizerConfig()]
          │ Qt::QueuedConnection (Zero GUI stall)
          ▼
[StabilizerWorker::updateConfig()]
          │ Writes pendingConfig_ & sets hasPendingConfig_ = true (Atomic)
          ▼
[StabilizerWorker::processTick()]
   On incoming frame boundary:
   if (hasPendingConfig_.exchange(false)) {
       // Check if frame dimensions or DOF mode changed:
       if (pendingConfig_.mode != currentConfig_.mode) {
           engine_.reset(); // Clean trajectory reset only when DOF topology changes
       }
       engine_.reconfigureSubsystems(pendingConfig_); // Zero-heap parameter update
   }
          │
          ▼
[VideoStabilizerEngine::processFrame() executes with new parameters]
```

### 2.3 Telemetry & Strip-Chart Ring Buffer Streaming

To satisfy **SEI CERT ERR55-CPP** (zero dynamic heap allocation on fast paths) while maintaining a fluid 60 FPS strip-chart:
1. `TelemetryRingBuffer` maintains a fixed stack/pre-allocated circular array of 150 `TelemetrySample` records.
2. `StabilizerWorker` emits `telemetryUpdated(StabilizerDiagnostics, double fps)` at the end of each frame processing cycle.
3. `StabilizerPlayerController` ingests the diagnostics structure, pushes it into `TelemetryRingBuffer`, and emits a throttled signal (`historyUpdated()`) capped at the display refresh rate (e.g. 60 Hz).
4. `TrajectoryChart.qml` reads typed array coordinates directly from `TelemetryRingBuffer` via Q_INVOKABLE accessors, avoiding JavaScript heap garbage collection spikes.

---

## 3. Tactical Component System (`Components/`)

All UI controls adhere to a unified tactical design language:
- **Palette**: Dark tactical obsidian background (`#0e1014`, `#141720`), subtle slate borders (`#242938`, `#374151`), high-contrast text (`#f0f4fc`, `#8894ab`), and semantic glowing accents:
  - Cyan (`#00e5ff`): Primary selection, active sliders, smooth trajectory curves.
  - Emerald Green (`#00e676`): Stable tracking, consensus inliers, operational status.
  - Amber (`#ff9100`): Damping engagement, warning thresholds, initializing status.
  - Crimson (`#ff1744`): Outliers, errors, boundary clipping, degenerate tracking.

---

### 3.1 `TacticalSlider.qml`

A high-precision slider designed for rapid tactile interaction and sub-pixel parameter tuning:
- **Visual Structure**: Horizontal track with contrasting filled active region, draggable glowing thumb handle, title label, and a direct editable numeric value field.
- **Properties**:
  - `label`: Display string describing the parameter (e.g. `"Quality Threshold"`).
  - `unit`: Display unit suffix (e.g. `"px"`, `"Hz"`, `"deg/s"`, `""`).
  - `from`, `to`: Allowable continuous floating-point or integer bounds.
  - `value`: Current two-way bound property.
  - `stepSize`: Discrete step increment (e.g. `0.001`, `1.0`).
  - `decimals`: Number of fractional decimal digits rendered in numeric badge.
  - `isLogarithmic`: Optional boolean allowing logarithmic mapping for variance scales ($10^{-6}$ to $10^{-1}$).
- **Ergonomics**: Direct mouse wheel incrementing over the slider, and direct text input for entering exact values.

---

### 3.2 `TacticalSwitch.qml`

A sleek, high-contrast toggle switch designed for binary algorithm features (e.g. bidirectional optical flow, least-squares refinement, auto-crop):
- **Visual Structure**: Rounded pill track with sliding circular thumb. When active, the track glows with neon green/cyan border; when inactive, it displays muted slate.
- **Properties**:
  - `text`: Label displayed adjacent to the toggle switch.
  - `checked`: Boolean state property.
  - `enabled`: Visual dimming when disabled by supervisory state.

---

### 3.3 `TacticalComboBox.qml`

A dark tactical drop-down menu replacing standard OS widgets:
- **Visual Structure**: Rectangular selection button with chevron indicator, dark obsidian popup menu (`#141720`), highlighted hover rows, and smooth open/close animations.
- **Properties**:
  - `label`: Associated parameter title.
  - `model`: List of enum names or human-readable option strings.
  - `currentIndex`: Currently selected index.
  - `onActivated`: Signal emitted when operator chooses an item.

---

### 3.4 `StatusBadge.qml`

A pulsing operational status indicator designed for glanceable military/aerospace health assessment:
- **Visual Structure**: Compact rounded rectangle containing an animated circular beacon and uppercase status text (e.g. `"TRACKING"`, `"DEGRADED"`, `"INITIALIZING"`, `"PAUSED"`, `"ERROR"`).
- **Properties**:
  - `state`: String or enum mapping to engine states (`EngineState`).
  - `showPulse`: Boolean enabling smooth opacity oscillation (600ms pulse) when active.

---

## 4. Source Management & Playback Controls (`SourcePanel.qml`)

[`SourcePanel.qml`](../app-stabilizer-qt/qml/SourcePanel.qml) is housed within a collapsible left drawer (width 320 px). It allows operators to configure inputs, switch between capture modalities, and control playback transport.

```
+--------------------------------------------------+
| [<<] SOURCE MANAGEMENT & TRANSPORT               |
+--------------------------------------------------+
| [ FILE ] [ RTSP / NET ] [ DEVICE ] [ SYNTHETIC ] |
+--------------------------------------------------+
|  MODE: LOCAL FILE                                |
|  Path: C:/videos/flight-test-01.mp4              |
|  [ Browse File... ] [ Recent Files v ]           |
|  Codec: H.264 / AVC  Resolution: 1920x1080       |
+--------------------------------------------------+
|  PLAYBACK TRANSPORT                              |
|  [ |< ] [ < ] [ > / || ] [ > ] [ >| ]  [ LOOP ]  |
|  ------------------O---------------------------  |
|  00:01:23.450 / 00:05:00.000                     |
|  Speed: [ 0.25x ] [ 0.5x ] [ 1.0x ] [ 2.0x ]     |
+--------------------------------------------------+
```

### 4.1 Ingestion Modalities Configuration

1. **Local Media File Mode**:
   - `FileDialog` picker filtering `*.mp4`, `*.mkv`, `*.ts`, `*.avi`, `*.mov`.
   - History combo box storing the last 5 recently opened file paths.
   - Stream metadata display: Width, Height, FPS, Container format.
2. **Network Stream Mode (RTSP / UDP / RTP / HTTP)**:
   - Editable URL text field with pre-populated samples (`rtsp://192.168.1.100:554/live`, `udp://@239.255.0.1:1234`).
   - RTSP Transport toggle: TCP (Interleaved RTP) vs. UDP.
   - Low-latency buffer tuning slider ($0 .. 1000\text{ ms}$).
   - Auto-reconnect switch with exponential backoff indicator.
3. **Hardware Capture Device Mode**:
   - Device selector combo box dynamically populated from `Video::DeviceEnumerator::enumerateDevices()`.
   - Displays device name (e.g. `"Integrated Webcam"`, `"/dev/video0"`) and hardware path.
   - Refresh button querying connected USB/UVC cameras on demand.
4. **Synthetic Jitter Generator Mode (Benchmark & Test Mode)**:
   - Procedural benchmark pattern for repeatable, non-camera regression tests.
   - Dedicated sliders for:
     - Mast Oscillation: Frequency X ($1 .. 30\text{ Hz}$), Frequency Y ($1 .. 30\text{ Hz}$), Amplitude X ($0 .. 50\text{ px}$), Amplitude Y ($0 .. 50\text{ px}$).
     - Handheld Random-Walk Drift: Brownian motion toggle, damping factor ($0.80 .. 0.99$), noise sigma ($0.1 .. 2.0$).
     - CMOS Rolling Shutter Wobble: Enable toggle, frequency ($1 .. 30\text{ Hz}$), shear amplitude ($0 .. 30\text{ px}$).
     - PRNG Seed button to replay identical jitter trajectories.

### 4.2 Playback Transport Controls
- **Play / Pause**: Large toggle button (`Space` hotkey).
- **Single-Frame Step ($\pm 1$)**: Advances or rewinds video by exactly one decoded frame (`Left` / `Right` arrow keys).
- **Timeline Scrubber**: Interactive slider linked to file presentation timestamp (PTS).
- **Playback Rate**: Multiplier buttons ($0.25\times, 0.5\times, 1.0\times, 2.0\times, 4.0\times$).
- **Loop Toggle**: Automatically rewinds to timestamp 0.0s upon reaching end-of-stream.

---

## 5. Parameter Matrix & Interactive Tuning (`StabilizerControlPanel.qml`)

[`StabilizerControlPanel.qml`](../app-stabilizer-qt/qml/StabilizerControlPanel.qml) is housed in the right dock (width 360 px) as a vertically scrollable accordion list. Each section maps directly to the parameter structs defined in [`libs/Stabilizer`](../libs/Stabilizer).

### 5.1 Accordion Categories & Parameter Mapping

```
+--------------------------------------------------+
| STABILIZATION PARAMETER TUNING             [ x ] |
+--------------------------------------------------+
| v 1. CORE MODE & SUPERVISORY                     |
|   Mode: [ Affine (6-DOF)                      v ]|
|   Fallback: [ Velocity Extrapolate            v ]|
|   Min Features:        |--------O-----|   20     |
|   Replenish Interval:  |---O----------|   15 frm |
|   Max Degraded Frames: |------O-------|   30 frm |
|   Min Inlier Ratio:    |----O---------|   25 %   |
|   [ Reset Section to Defaults ]                  |
+--------------------------------------------------+
| > 2. TRAJECTORY SMOOTHING & FILTERING            |
+--------------------------------------------------+
| > 3. FEATURE DETECTOR (SHI-TOMASI)               |
+--------------------------------------------------+
| > 4. OPTICAL FLOW TRACKER (PYRAMIDAL LK)         |
+--------------------------------------------------+
| > 5. MOTION ESTIMATOR (MSAC CONSENSUS)           |
+--------------------------------------------------+
| > 6. WARP ENGINE & ROLLING SHUTTER               |
+--------------------------------------------------+
```

#### Detailed Section Specifications:

1. **Core Mode & Supervisory (`VideoStabilizerConfig`)**:
   - `mode`: ComboBox (`Disabled`, `TranslationOnly`, `TranslationRotation`, `Similarity`, `Affine`, `RollingShutterComp`).
   - `fallback`: ComboBox (`IdentityHold`, `VelocityExtrapolate`, `BypassWarp`).
   - `minTrackedFeatures`: TacticalSlider [$5 .. 200$, step $1$].
   - `replenishIntervalFrames`: TacticalSlider [$1 .. 60$, step $1$].
   - `maxDegradedFrames`: TacticalSlider [$1 .. 120$, step $1$].
   - `minInlierRatio`: TacticalSlider [$0.05 .. 0.95$, step $0.01$, display $\%$].

2. **Trajectory Smoothing & Filter (`TrajectoryConfig`)**:
   - `method`: ComboBox (`KalmanFilter`, `GaussianFIR`, `L1Optimization`).
   - `windowSize`: TacticalSlider [$3 .. 63$, step $2$ (odd integer enforcement)].
   - `maxCropMargin`: TacticalSlider [$0.00 .. 0.30$, step $0.005$, display $\%$].
   - `softMarginRatio`: TacticalSlider [$0.10 .. 0.99$, step $0.01$].
   - `processNoiseBase`: TacticalSlider [$10^{-6} .. 10^{-1}$, logarithmic].
   - `measurementNoise`: TacticalSlider [$10^{-4} .. 1.0$, logarithmic].
   - `ptzFeedforwardGain`: TacticalSlider [$0.00 .. 1.50$, step $0.05$].
   - `ptzDeadband`: TacticalSlider [$0.00 .. 5.00\text{ deg/s}$, step $0.05$].
   - `focalLength`: TacticalSlider [$100.0 .. 20000.0\text{ px}$, step $50.0$].

3. **Feature Detector (`FeatureDetectorConfig`)**:
   - `qualityThreshold`: TacticalSlider [$0.0001 .. 0.5000$, step $0.001$].
   - `borderMargin`: TacticalSlider [$3 .. 64\text{ px}$, step $1$].
   - `maxTotalFeatures`: TacticalSlider [$10 .. 2048$, step $10$].
   - `gridCols`: TacticalSlider [$1 .. 32$, step $1$].
   - `gridRows`: TacticalSlider [$1 .. 32$, step $1$].
   - `maxFeaturesPerCell`: TacticalSlider [$1 .. 64$, step $1$].
   - `minDistance`: TacticalSlider [$0.0 .. 64.0\text{ px}$, step $0.5$].

4. **Optical Flow Tracker (`OpticalFlowConfig`)**:
   - `windowSize` (halfWidth): TacticalSlider [$2 .. 16\text{ px}$ (window extent $5\times 5$ to $33\times 33$), step $1$].
   - `numPyramidLevels`: TacticalSlider [$1 .. 3$, step $1$].
   - `maxIterations`: TacticalSlider [$1 .. 100$, step $1$].
   - `minEpsilon`: TacticalSlider [$0.001 .. 0.200\text{ px}$, step $0.001$].
   - `minEigenvalue`: TacticalSlider [$0.0001 .. 0.1000$, step $0.0005$].
   - `maxForwardBackwardError`: TacticalSlider [$0.05 .. 5.00\text{ px}$, step $0.05$].
   - `enableBidirectional`: TacticalSwitch.

5. **Parametric Motion Model Estimator (`MotionEstimatorConfig`)**:
   - `inlierThreshold`: TacticalSlider [$0.1 .. 15.0\text{ px}$, step $0.1$].
   - `maxIterations`: TacticalSlider [$10 .. 1000$, step $10$].
   - `minIterations`: TacticalSlider [$5 .. 500$, step $5$].
   - `confidence`: TacticalSlider [$0.80 .. 0.999$, step $0.001$].
   - `randomSeed`: Text display with "Reseed Deterministic" button.
   - `enableRefinement`: TacticalSwitch.

6. **Warp Engine & Rolling Shutter (`WarpConfig`)**:
   - `interpolation`: ComboBox (`NearestNeighbor`, `Bilinear`, `Bicubic`).
   - `borderMode`: ComboBox (`Constant`, `Replicate`, `Reflect`).
   - `borderValue`: TacticalSlider [$0 .. 255$, step $1$ (Luma fill)].
   - `cropRatio`: TacticalSlider [$0.00 .. 0.30$, step $0.005$, display $\%$].
   - `enableAutoCrop`: TacticalSwitch.
   - `enableRollingShutter`: TacticalSwitch.
   - `readoutRatio`: TacticalSlider [$0.00 .. 1.00$, step $0.02$].
   - `scanTopToBottom`: TacticalSwitch.

---

## 6. Real-Time Telemetry & Trajectory Strip-Charts (`DiagnosticsPanel.qml` & `TrajectoryChart.qml`)

### 6.1 Real-Time Diagnostics Panel (`DiagnosticsPanel.qml`)

Positioned as a secondary tab in the right dock, [`DiagnosticsPanel.qml`](../app-stabilizer-qt/qml/DiagnosticsPanel.qml) exposes critical telemetry metrics received from `StabilizerWorker`:

```
+--------------------------------------------------+
| REAL-TIME STABILIZER DIAGNOSTICS                 |
+--------------------------------------------------+
| STATUS: [ TRACKING ] (Nominal Inlier Consensus)  |
| Frame Index: 001482      Total Latency: 4.82 ms  |
| Feature Health:                                  |
|   Detected: 300   Tracked: 245   Inliers: 198    |
|   Inlier Consensus Ratio: 80.8% [========--]     |
+--------------------------------------------------+
| PIPELINE STAGE LATENCIES (Microseconds)          |
|   Scale Pyramid:      380 us [===              ] |
|   Shi-Tomasi Corners: 940 us [=======          ] |
|   Pyramidal LK Flow: 1420 us [===========      ] |
|   MSAC Motion Model:  610 us [=====            ] |
|   Kalman Trajectory:   80 us [=                ] |
|   SIMD Warp Synthesis:1390 us [===========      ] |
|   Total Frame Budget: 4820 us / 33333 us (14.5%) |
+--------------------------------------------------+
| 2x3 AFFINE MOTION TRANSFORMS                     |
| Raw Motion Matrix:                               |
|   [  0.9998  -0.0152  +12.45 ]                   |
|   [ +0.0152   0.9998   -4.12 ]                   |
| Smoothed Camera Motion:                          |
|   [  1.0000  -0.0004   +1.18 ]                   |
|   [ +0.0004   1.0000   -0.35 ]                   |
| Active Warp Correction Matrix:                   |
|   [  0.9998  +0.0148  -11.27 ]                   |
|   [ -0.0148   0.9998   +3.77 ]                   |
+--------------------------------------------------+
```

### 6.2 Trajectory Strip-Chart (`TrajectoryChart.qml`)

Positioned across the bottom of the presentation viewport (height 160 px, collapsible with hotkey `T`), [`TrajectoryChart.qml`](../app-stabilizer-qt/qml/TrajectoryChart.qml) renders a high-speed rolling 150-frame strip-chart using HTML5-style 2D context inside Qt Quick `Canvas`:

#### Visual Channels:
1. **Camera Trajectory (X & Y)**:
   - **Raw Inter-Frame Displacement (Cyan Line)**: Plots raw camera motion $t_x, t_y$.
   - **Filtered Smoothed Trajectory (Neon Green Line)**: Plots EKF/L1 smoothed trajectory $s_x, s_y$.
2. **Instantaneous Jitter Variance ($\text{px}^2$)**:
   - Shaded amber area curve representing high-frequency residual vibration energy.
3. **Crop Margin Consumption Bar & Damping Threshold**:
   - Visual horizontal boundary indicating maximum crop margin ($100\%$).
   - Dashed amber line indicating soft damping engagement boundary ($80\%$). When the smoothed trajectory approaches within the soft margin, a visual warning pill flashes amber.

---

## 7. Preset Management Subsystem

To eliminate manual tuning during flight tests or field demonstrations, the application provides 5 pre-configured factory profiles accessible from the master header bar.

### 7.1 Factory Preset Specifications

| Preset Profile | Target Use Case | Recommended Parameters |
| :--- | :--- | :--- |
| **Drone Gimbal / Aerial** | Drone aerial surveillance with high-frequency motor vibration and broad angular panning. | - Mode: `Affine`<br>- Filter: `KalmanFilter` ($q_a = 10^{-4}, R = 10^{-2}$)<br>- PTZ Gain: $0.95$, Deadband: $0.15\text{ deg/s}$<br>- Crop Margin: $8\%$, Soft Ratio: $0.80$<br>- Features: $300$, Bucketing: $8\times 6$ |
| **Pole / Mast Vibration** | Tower, mast, or perimeter surveillance cameras experiencing harmonic wind buffeting. | - Mode: `TranslationRotation`<br>- Filter: `GaussianFIR` ($window = 31$ frames)<br>- Features: $400$, Bucketing: $10\times 8$<br>- LK Window: $15\times 15$ ($halfWidth = 7$)<br>- Crop Margin: $10\%$ |
| **Handheld Action Cam** | Rapid multi-axis human walking, running, or vehicle dashcam motion. | - Mode: `Affine`<br>- Filter: `L1Optimization` ($window = 45$ frames)<br>- Crop Margin: $12\%$, Soft Ratio: $0.85$<br>- Flow: Bidirectional check active, max error $0.4\text{ px}$<br>- Replenish Interval: $10$ frames |
| **CMOS Rolling Shutter** | Drones and high-vibration platforms with un-stabilized CMOS rolling shutter sensors ("jello" wobble). | - Mode: `RollingShutterComp`<br>- Warp: `Bilinear`, `enableRollingShutter = true`<br>- Readout Ratio: $0.85$, Top-to-Bottom scan<br>- Auto-Crop: $6\%$ |
| **Bypass / Diagnostic** | Zero-stabilization baseline pass-through for algorithm latency benchmarking and raw overlay inspection. | - Mode: `Disabled`<br>- Fallback: `BypassWarp`<br>- Overlays: Landmarks, Quiver, and Center Reticle enabled |

### 7.2 Preset Serialization & Invariant Bounds Engine

[`PresetManager`](../app-stabilizer-qt/PresetManager.h) provides bidirectional serialization between `VideoStabilizerConfig` and compact JSON format. When loading a preset:
1. All JSON fields are validated against domain invariant bounds (`isValid()`).
2. Any out-of-range value is clamped or rejected with an informative error message.
3. The valid configuration is dispatched to `StabilizerConfigModel`, which updates UI sliders and sends the batch update to `StabilizerWorker`.

---

## 8. Detailed C++ Header & Class Specifications

### 8.1 `StabilizerConfigModel.h`

```cpp
#pragma once

/// @file StabilizerConfigModel.h
/// @brief QObject ViewModel exposing strongly-typed stabilization parameters to QML.

#include <cstdint>

#include <QObject>
#include <QString>

#include "FeatureDetector.h"
#include "MotionModelEstimator.h"
#include "OpticalFlowTracker.h"
#include "StabilizerTypes.h"
#include "TrajectoryFilter.h"
#include "VideoStabilizerEngine.h"
#include "WarpEngine.h"

namespace StabilizerApp {

/// @class StabilizerConfigModel
/// @brief Comprehensive property bridge exposing all 38 stabilization parameters with two-way QML bindings.
class StabilizerConfigModel : public QObject {
    Q_OBJECT

    // 1. Core Mode & Supervisory
    Q_PROPERTY(int stabilizationMode READ stabilizationMode WRITE setStabilizationMode NOTIFY configChanged)
    Q_PROPERTY(int fallbackPolicy READ fallbackPolicy WRITE setFallbackPolicy NOTIFY configChanged)
    Q_PROPERTY(int minTrackedFeatures READ minTrackedFeatures WRITE setMinTrackedFeatures NOTIFY configChanged)
    Q_PROPERTY(int replenishInterval READ replenishInterval WRITE setReplenishInterval NOTIFY configChanged)
    Q_PROPERTY(int maxDegradedFrames READ maxDegradedFrames WRITE setMaxDegradedFrames NOTIFY configChanged)
    Q_PROPERTY(double minInlierRatio READ minInlierRatio WRITE setMinInlierRatio NOTIFY configChanged)

    // 2. Trajectory Smoothing & Filter
    Q_PROPERTY(int smoothingMethod READ smoothingMethod WRITE setSmoothingMethod NOTIFY configChanged)
    Q_PROPERTY(double maxCropMargin READ maxCropMargin WRITE setMaxCropMargin NOTIFY configChanged)
    Q_PROPERTY(double softMarginRatio READ softMarginRatio WRITE setSoftMarginRatio NOTIFY configChanged)
    Q_PROPERTY(double processNoiseBase READ processNoiseBase WRITE setProcessNoiseBase NOTIFY configChanged)
    Q_PROPERTY(double measurementNoise READ measurementNoise WRITE setMeasurementNoise NOTIFY configChanged)
    Q_PROPERTY(double ptzGain READ ptzGain WRITE setPtzGain NOTIFY configChanged)
    Q_PROPERTY(double ptzDeadband READ ptzDeadband WRITE setPtzDeadband NOTIFY configChanged)
    Q_PROPERTY(double focalLength READ focalLength WRITE setFocalLength NOTIFY configChanged)
    Q_PROPERTY(int windowSize READ windowSize WRITE setWindowSize NOTIFY configChanged)

    // 3. Feature Detector
    Q_PROPERTY(double qualityThreshold READ qualityThreshold WRITE setQualityThreshold NOTIFY configChanged)
    Q_PROPERTY(int borderMargin READ borderMargin WRITE setBorderMargin NOTIFY configChanged)
    Q_PROPERTY(int maxTotalFeatures READ maxTotalFeatures WRITE setMaxTotalFeatures NOTIFY configChanged)
    Q_PROPERTY(int gridCols READ gridCols WRITE setGridCols NOTIFY configChanged)
    Q_PROPERTY(int gridRows READ gridRows WRITE setGridRows NOTIFY configChanged)
    Q_PROPERTY(int maxFeaturesPerCell READ maxFeaturesPerCell WRITE setMaxFeaturesPerCell NOTIFY configChanged)
    Q_PROPERTY(double minFeatureDistance READ minFeatureDistance WRITE setMinFeatureDistance NOTIFY configChanged)

    // 4. Optical Flow Tracker
    Q_PROPERTY(int flowWindowHalfWidth READ flowWindowHalfWidth WRITE setFlowWindowHalfWidth NOTIFY configChanged)
    Q_PROPERTY(int numPyramidLevels READ numPyramidLevels WRITE setNumPyramidLevels NOTIFY configChanged)
    Q_PROPERTY(int maxFlowIterations READ maxFlowIterations WRITE setMaxFlowIterations NOTIFY configChanged)
    Q_PROPERTY(double minEpsilon READ minEpsilon WRITE setMinEpsilon NOTIFY configChanged)
    Q_PROPERTY(double minEigenvalue READ minEigenvalue WRITE setMinEigenvalue NOTIFY configChanged)
    Q_PROPERTY(double maxFbError READ maxFbError WRITE setMaxFbError NOTIFY configChanged)
    Q_PROPERTY(bool enableBidirectional READ enableBidirectional WRITE setEnableBidirectional NOTIFY configChanged)

    // 5. Motion Estimator
    Q_PROPERTY(double inlierThreshold READ inlierThreshold WRITE setInlierThreshold NOTIFY configChanged)
    Q_PROPERTY(int maxMsacIterations READ maxMsacIterations WRITE setMaxMsacIterations NOTIFY configChanged)
    Q_PROPERTY(int minMsacIterations READ minMsacIterations WRITE setMinMsacIterations NOTIFY configChanged)
    Q_PROPERTY(double msacConfidence READ msacConfidence WRITE setMsacConfidence NOTIFY configChanged)
    Q_PROPERTY(bool enableRefinement READ enableRefinement WRITE setEnableRefinement NOTIFY configChanged)

    // 6. Warp Engine & Rolling Shutter
    Q_PROPERTY(int warpInterpolation READ warpInterpolation WRITE setWarpInterpolation NOTIFY configChanged)
    Q_PROPERTY(int borderMode READ borderMode WRITE setBorderMode NOTIFY configChanged)
    Q_PROPERTY(int borderValue READ borderValue WRITE setBorderValue NOTIFY configChanged)
    Q_PROPERTY(double warpCropRatio READ warpCropRatio WRITE setWarpCropRatio NOTIFY configChanged)
    Q_PROPERTY(bool enableAutoCrop READ enableAutoCrop WRITE setEnableAutoCrop NOTIFY configChanged)
    Q_PROPERTY(bool enableRollingShutter READ enableRollingShutter WRITE setEnableRollingShutter NOTIFY configChanged)
    Q_PROPERTY(double rsReadoutRatio READ rsReadoutRatio WRITE setRsReadoutRatio NOTIFY configChanged)
    Q_PROPERTY(bool rsScanTopToBottom READ rsScanTopToBottom WRITE setRsScanTopToBottom NOTIFY configChanged)

public:
    explicit StabilizerConfigModel(QObject* parent = nullptr);
    ~StabilizerConfigModel() override = default;

    /// @brief Retrieve underlying strongly-typed C++ configuration.
    [[nodiscard]] Stabilizer::VideoStabilizerConfig toConfig() const noexcept;

    /// @brief Overwrite state with an external configuration.
    void loadFromConfig(const Stabilizer::VideoStabilizerConfig& config);

    /// @brief Reset specific section to factory default settings.
    Q_INVOKABLE void resetSection(int sectionIndex);

    /// @brief Reset all parameters to factory default settings.
    Q_INVOKABLE void resetAll();

    // Getters
    [[nodiscard]] int stabilizationMode() const noexcept;
    [[nodiscard]] int fallbackPolicy() const noexcept;
    [[nodiscard]] int minTrackedFeatures() const noexcept;
    [[nodiscard]] int replenishInterval() const noexcept;
    [[nodiscard]] int maxDegradedFrames() const noexcept;
    [[nodiscard]] double minInlierRatio() const noexcept;

    [[nodiscard]] int smoothingMethod() const noexcept;
    [[nodiscard]] double maxCropMargin() const noexcept;
    [[nodiscard]] double softMarginRatio() const noexcept;
    [[nodiscard]] double processNoiseBase() const noexcept;
    [[nodiscard]] double measurementNoise() const noexcept;
    [[nodiscard]] double ptzGain() const noexcept;
    [[nodiscard]] double ptzDeadband() const noexcept;
    [[nodiscard]] double focalLength() const noexcept;
    [[nodiscard]] int windowSize() const noexcept;

    [[nodiscard]] double qualityThreshold() const noexcept;
    [[nodiscard]] int borderMargin() const noexcept;
    [[nodiscard]] int maxTotalFeatures() const noexcept;
    [[nodiscard]] int gridCols() const noexcept;
    [[nodiscard]] int gridRows() const noexcept;
    [[nodiscard]] int maxFeaturesPerCell() const noexcept;
    [[nodiscard]] double minFeatureDistance() const noexcept;

    [[nodiscard]] int flowWindowHalfWidth() const noexcept;
    [[nodiscard]] int numPyramidLevels() const noexcept;
    [[nodiscard]] int maxFlowIterations() const noexcept;
    [[nodiscard]] double minEpsilon() const noexcept;
    [[nodiscard]] double minEigenvalue() const noexcept;
    [[nodiscard]] double maxFbError() const noexcept;
    [[nodiscard]] bool enableBidirectional() const noexcept;

    [[nodiscard]] double inlierThreshold() const noexcept;
    [[nodiscard]] int maxMsacIterations() const noexcept;
    [[nodiscard]] int minMsacIterations() const noexcept;
    [[nodiscard]] double msacConfidence() const noexcept;
    [[nodiscard]] bool enableRefinement() const noexcept;

    [[nodiscard]] int warpInterpolation() const noexcept;
    [[nodiscard]] int borderMode() const noexcept;
    [[nodiscard]] int borderValue() const noexcept;
    [[nodiscard]] double warpCropRatio() const noexcept;
    [[nodiscard]] bool enableAutoCrop() const noexcept;
    [[nodiscard]] bool enableRollingShutter() const noexcept;
    [[nodiscard]] double rsReadoutRatio() const noexcept;
    [[nodiscard]] bool rsScanTopToBottom() const noexcept;

    // Setters with invariant bounds enforcement
    void setStabilizationMode(int mode);
    void setFallbackPolicy(int policy);
    void setMinTrackedFeatures(int count);
    void setReplenishInterval(int interval);
    void setMaxDegradedFrames(int count);
    void setMinInlierRatio(double ratio);

    void setSmoothingMethod(int method);
    void setMaxCropMargin(double margin);
    void setSoftMarginRatio(double ratio);
    void setProcessNoiseBase(double q);
    void setMeasurementNoise(double r);
    void setPtzGain(double gain);
    void setPtzDeadband(double deadband);
    void setFocalLength(double fl);
    void setWindowSize(int size);

    void setQualityThreshold(double val);
    void setBorderMargin(int margin);
    void setMaxTotalFeatures(int count);
    void setGridCols(int cols);
    void setGridRows(int rows);
    void setMaxFeaturesPerCell(int count);
    void setMinFeatureDistance(double dist);

    void setFlowWindowHalfWidth(int halfWidth);
    void setNumPyramidLevels(int levels);
    void setMaxFlowIterations(int iters);
    void setMinEpsilon(double eps);
    void setMinEigenvalue(double ev);
    void setMaxFbError(double err);
    void setEnableBidirectional(bool enable);

    void setInlierThreshold(double th);
    void setMaxMsacIterations(int iters);
    void setMinMsacIterations(int iters);
    void setMsacConfidence(double conf);
    void setEnableRefinement(bool enable);

    void setWarpInterpolation(int interp);
    void setBorderMode(int mode);
    void setBorderValue(int val);
    void setWarpCropRatio(double ratio);
    void setEnableAutoCrop(bool enable);
    void setEnableRollingShutter(bool enable);
    void setRsReadoutRatio(double ratio);
    void setRsScanTopToBottom(bool topToBottom);

signals:
    /// @brief Emitted whenever any parameter changes.
    void configChanged();

private:
    Stabilizer::VideoStabilizerConfig config_ { };
};

} // namespace StabilizerApp
```

---

### 8.2 `TelemetryRingBuffer.h`

```cpp
#pragma once

/// @file TelemetryRingBuffer.h
/// @brief Zero-heap circular history of stabilization telemetry for QML strip-chart drawing.

#include <cstddef>
#include <cstdint>

#include <array>
#include <mutex>

#include <QObject>
#include <QPointF>
#include <QVariantList>

#include "VideoStabilizerEngine.h"

namespace StabilizerApp {

/// @struct TelemetrySample
/// @brief Snapshot of trajectory and tracking health for a single frame.
struct TelemetrySample {
    std::uint32_t frameIndex { 0U };
    float rawTx { 0.0F };
    float rawTy { 0.0F };
    float smoothTx { 0.0F };
    float smoothTy { 0.0F };
    float jitterVariance { 0.0F };
    float marginUtilization { 0.0F };
    std::uint32_t inlierCount { 0U };
    float inlierRatio { 0.0F };
    std::uint32_t totalLatencyUs { 0U };
    bool isSoftMarginDamped { false };
};

/// @class TelemetryRingBuffer
/// @brief Pre-allocated circular ring buffer holding 150 historical telemetry samples.
class TelemetryRingBuffer : public QObject {
    Q_OBJECT

    Q_PROPERTY(int capacity READ capacity CONSTANT)
    Q_PROPERTY(int count READ count NOTIFY historyUpdated)
    Q_PROPERTY(double latestJitter READ latestJitter NOTIFY historyUpdated)
    Q_PROPERTY(double latestMarginUtilization READ latestMarginUtilization NOTIFY historyUpdated)
    Q_PROPERTY(bool isSoftMarginEngaged READ isSoftMarginEngaged NOTIFY historyUpdated)

public:
    static constexpr std::size_t BufferCapacity { 150U };

    explicit TelemetryRingBuffer(QObject* parent = nullptr);
    ~TelemetryRingBuffer() override = default;

    /// @brief Push new diagnostics sample into ring buffer (Zero dynamic heap allocation).
    /// @param diag Diagnostics record from VideoStabilizerEngine.
    void appendSample(const Stabilizer::StabilizerDiagnostics& diag) noexcept;

    /// @brief Clear buffer contents.
    Q_INVOKABLE void clear() noexcept;

    /// @brief Total capacity of ring buffer.
    [[nodiscard]] int capacity() const noexcept;

    /// @brief Number of active valid samples in buffer.
    [[nodiscard]] int count() const noexcept;

    /// @brief Most recent jitter variance metric.
    [[nodiscard]] double latestJitter() const noexcept;

    /// @brief Most recent crop margin utilization ratio [0.0 .. 1.0].
    [[nodiscard]] double latestMarginUtilization() const noexcept;

    /// @brief Whether soft damping is actively engaging.
    [[nodiscard]] bool isSoftMarginEngaged() const noexcept;

    /// @brief Copies normalized raw trajectory coordinates into QVariantList for QML Canvas.
    /// @param axis 0 for X, 1 for Y.
    /// @return Array of normalized Y points [0.0 .. 1.0].
    [[nodiscard]] Q_INVOKABLE QVariantList getRawTrajectory(int axis) const;

    /// @brief Copies normalized smoothed trajectory coordinates into QVariantList for QML Canvas.
    /// @param axis 0 for X, 1 for Y.
    /// @return Array of normalized Y points [0.0 .. 1.0].
    [[nodiscard]] Q_INVOKABLE QVariantList getSmoothTrajectory(int axis) const;

    /// @brief Copies jitter variance curve points for QML Canvas strip-chart.
    [[nodiscard]] Q_INVOKABLE QVariantList getJitterHistory() const;

signals:
    /// @brief Emitted when new sample is added.
    void historyUpdated();

private:
    mutable std::mutex mutex_ { };
    std::array<TelemetrySample, BufferCapacity> ring_ { };
    std::size_t head_ { 0U };
    std::size_t count_ { 0U };
    float minTx_ { -20.0F };
    float maxTx_ { +20.0F };
    float minTy_ { -20.0F };
    float maxTy_ { +20.0F };
};

} // namespace StabilizerApp
```

---

### 8.3 `PresetManager.h`

```cpp
#pragma once

/// @file PresetManager.h
/// @brief Factory stabilization presets and JSON persistence manager.

#include <QObject>
#include <QString>
#include <QStringList>

#include "VideoStabilizerEngine.h"

namespace StabilizerApp {

/// @class PresetManager
/// @brief Manages factory configuration presets and JSON file import/export.
class PresetManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(QStringList presetNames READ presetNames CONSTANT)

public:
    explicit PresetManager(QObject* parent = nullptr);
    ~PresetManager() override = default;

    /// @brief List of available preset profile names.
    [[nodiscard]] QStringList presetNames() const;

    /// @brief Retrieves a named preset configuration.
    /// @param name Preset name ("Drone Gimbal", "Pole Vibration", etc.)
    /// @param outConfig Receives configuration parameters.
    /// @return true if found; false if invalid preset name.
    [[nodiscard]] bool getPreset(const QString& name, Stabilizer::VideoStabilizerConfig& outConfig) const;

    /// @brief Exports a configuration to JSON file.
    /// @param filePath Destination file path.
    /// @param config Configuration to serialize.
    /// @return true on success; false on filesystem or JSON error.
    [[nodiscard]] static bool exportJson(const QString& filePath, const Stabilizer::VideoStabilizerConfig& config);

    /// @brief Imports a configuration from JSON file.
    /// @param filePath Source file path.
    /// @param outConfig Receives validated configuration.
    /// @return true on success; false on parse error or invariant violation.
    [[nodiscard]] static bool importJson(const QString& filePath, Stabilizer::VideoStabilizerConfig& outConfig);
};

} // namespace StabilizerApp
```

---

## 9. Step-by-Step Implementation Roadmap

```
Phase 4.1: Tactical Component Library (QML Primitives)
   │
   ▼
Phase 4.2: C++ ViewModel Layer (ConfigModel, RingBuffer, PresetManager)
   │
   ▼
Phase 4.3: Source Management Panel & Hardware Device Enumeration
   │
   ▼
Phase 4.4: Accordion Parameter Control Panel (6 Subsystem Sections)
   │
   ▼
Phase 4.5: Real-Time Diagnostics Panel & Canvas Trajectory Strip-Chart
   │
   ▼
Phase 4.6: Preset Management & Hot-Reload Protocol Integration
   │
   ▼
Phase 4.7: Master QML Layout Integration (Main.qml) & Hotkey Mapping
```

### Phase 4.1: Tactical Component Library (`qml/Components/`)
- Implement `TacticalSlider.qml`: Draggable cyan thumb, editable numeric badge, unit label, mouse-wheel support, min/max text.
- Implement `TacticalSwitch.qml`: Glowing toggle switch with high-contrast active state.
- Implement `TacticalComboBox.qml`: Dark popup list with item highlight, key navigation, and chevron arrow.
- Implement `StatusBadge.qml`: Pulsing multi-state badge with color mapping (`Tracking`, `Degraded`, `Initializing`, `Paused`, `Error`).
- Register component path in `qml.qrc`.

### Phase 4.2: C++ ViewModel Layer
- Implement `StabilizerConfigModel.h` and `.cpp`: Expose all 38 parameters via `Q_PROPERTY`, validate bounds in setters, and provide `resetSection()` / `resetAll()`.
- Implement `TelemetryRingBuffer.h` and `.cpp`: 150-sample circular array, zero-heap `appendSample()`, normalized coordinate getters for QML Canvas.
- Implement `PresetManager.h` and `.cpp`: Static dictionary of 5 profiles, JSON `QJsonDocument` serialization/deserialization.
- Update `StabilizerPlayerController.h` / `.cpp`:
  - Expose child instances `configModel`, `telemetryRingBuffer`, `presetManager` as properties.
  - Implement `Q_INVOKABLE QStringList availableHardwareDevices() const` calling `Video::DeviceEnumerator::enumerateDevices()`.
  - Connect `StabilizerWorker::telemetryUpdated` to `TelemetryRingBuffer::appendSample()`.
  - Connect `StabilizerConfigModel::configChanged` to `StabilizerPlayerController::updateStabilizerConfig()`.

### Phase 4.3: Source Management Panel (`qml/SourcePanel.qml`)
- Implement 4 source tabs: File, RTSP/Network, Hardware Device, Synthetic Shaker.
- Integrate `FileDialog` for local file ingestion.
- Connect Hardware Device tab to `controller.availableHardwareDevices()`.
- Expose synthetic jitter controls (Mast, Random-Walk, Rolling Shutter) bound to `controller.syntheticConfig`.
- Implement unified media transport bar (Play, Pause, Step +/- 1, Seekbar slider, Speed selector, Loop).

### Phase 4.4: Accordion Parameter Control Panel (`qml/StabilizerControlPanel.qml`)
- Create 6 collapsible accordion sections:
  1. Core Mode & Supervisory
  2. Trajectory Smoothing & Filter
  3. Feature Detector (Shi-Tomasi)
  4. Optical Flow Tracker (Pyramidal LK)
  5. Motion Model Estimator (MSAC)
  6. Warp Engine & Rolling Shutter
- Bind all controls to `controller.configModel`.
- Add section reset buttons and hot-reload verification.

### Phase 4.5: Diagnostics Panel & Trajectory Strip-Chart
- Implement `DiagnosticsPanel.qml`:
  - Feature health stats (Detected, Tracked, Inliers, Ratio).
  - Pipeline stage latencies bar chart with microsecond numeric readouts and budget line.
  - 2x3 Affine transformation matrix viewer.
- Implement `TrajectoryChart.qml`:
  - QML `Canvas` plotting 150 frames of raw vs. smoothed trajectories.
  - Jitter variance shaded area curve.
  - Crop margin boundary and soft damping warning line.

### Phase 4.6: Preset Management & Hot-Reload Integration
- Add header preset selector buttons ("Drone", "Pole", "Action Cam", "Rolling Shutter", "Bypass").
- Implement `controller.loadPreset(name)`.
- Implement JSON configuration export and import dialogs (`FileDialog`).

### Phase 4.7: Master QML Layout Integration (`qml/Main.qml`)
- Integrate collapsible left drawer (`SourcePanel.qml`) and right drawer (`StabilizerControlPanel.qml` / `DiagnosticsPanel.qml`).
- Place bottom collapsible strip-chart (`TrajectoryChart.qml`).
- Map global tactical hotkeys:
  - `Space`: Play/Pause toggle (or raw/stabilized toggle in SingleToggle mode).
  - `Left` / `Right`: Step frame $\pm 1$.
  - `T`: Toggle Trajectory Strip-Chart.
  - `S`: Toggle Source Management Panel.
  - `C`: Toggle Parameter Control Panel.
  - `D`: Toggle Diagnostics Panel.
  - `1` through `5`: Quick-load Presets 1 through 5.

---

## 10. Safety Compliance, Verification & Test Strategy

### 10.1 Safety Standards Compliance
- **MISRA C++:2023 Rule 7.0.6 & SEI CERT INT31-C**: All numeric conversions between Qt UI types (`int`, `double`) and domain integer types (`std::uint32_t`, `std::uint16_t`, `std::uint8_t`) must use explicit `static_cast` with range validation.
- **SEI CERT ERR55-CPP (Zero-Heap Fast Path)**: `TelemetryRingBuffer::appendSample()` must execute without heap allocation (`new`, `malloc`, `std::vector::push_back`).
- **IWYU & `<cstdint>`**: All new C++ headers must include `<cstdint>` and `<cstddef>` explicitly, scoping all fixed-width integer types under `std::`.
- **Relative Path Rule**: All file links in documentation and source comments must use relative paths.

### 10.2 Automated Unit Testing
Create three dedicated unit test translation units under [`tests/`](../tests):

1. **`tests/TestStabilizerConfigModel.cpp`**:
   - Verify initial configuration matches engine defaults.
   - Verify invariant clamping for all 38 parameter setters.
   - Verify `resetSection()` restores default parameters for that section while leaving other sections untouched.
   - Verify `toConfig()` produces valid `VideoStabilizerConfig::isValid() == true`.
2. **`tests/TestPresetManager.cpp`**:
   - Verify all 5 built-in presets load cleanly and pass `isValid()`.
   - Verify JSON round-trip serialization (Config $\rightarrow$ JSON $\rightarrow$ Config) preserves numerical equality within float epsilon.
   - Verify corrupt or out-of-range JSON files are rejected cleanly without throwing exceptions.
3. **`tests/TestTelemetryRingBuffer.cpp`**:
   - Verify circular overwriting behavior when more than 150 samples are appended.
   - Verify thread safety under simulated concurrent append and read.
   - Verify normalized trajectory output remains strictly within $[0.0 .. 1.0]$.

### 10.3 Manual UI & Performance Verification Checklist
- [ ] Application boots cleanly in under 1 second with dark tactical theme.
- [ ] Dragging parameter sliders updates stabilization output on the very next video frame without stutter or dropped frames.
- [ ] Accordion sections collapse and expand smoothly without layout distortion.
- [ ] Trajectory strip-chart maintains smooth 60 FPS rendering on QML Canvas.
- [ ] File dialogs correctly filter supported video formats (`mp4`, `mkv`, `ts`, `avi`).
- [ ] Hardware capture device dropdown lists all physical connected cameras.
- [ ] Switching presets immediately updates all accordion sliders to the preset values.
- [ ] All keyboard shortcuts (`Space`, `Left`/`Right`, `T`, `S`, `C`, `D`, `1`-`5`) function deterministically.

---

## 11. Conclusion & Readiness Criteria

Phase 4 completes the transformation of `app-stabilizer-qt` into a professional, military-grade desktop video stabilization testbench. By combining decoupled MVVM C++ architecture, zero-heap telemetry streaming, exhaustive parameter tuning, and battle-tested operational presets, engineers can evaluate and optimize the [`libs/Stabilizer`](../libs/Stabilizer) engine across any real-world operational condition.

Following Phase 4 completion, the application will proceed to **Phase 5: Verification, Compliance & Quality Assurance**, ensuring comprehensive test coverage, multi-compiler build hygiene, and end-to-end flight test validation.

