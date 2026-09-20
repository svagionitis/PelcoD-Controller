# Pelco-D Device Controller

A production-grade, cross-platform C++17 library and modern Qt 6 desktop client for controlling Pelco-D protocol devices (Pan/Tilt heads, gimbals, motorized domes, receiver/drivers, and integrated PTZ units) adhering strictly to **Pelco PTZ Protocols: D Protocol Version 5.0.1**.

---

## Architectural Highlights

- **Seven Modular Subsystems:**
  1. [`libs/PelcoDMath/`](libs/PelcoDMath/): Standalone, pure C++17 mathematics and DSP library (**zero external dependencies**). Encapsulates 1D/2D Fast Fourier Transforms (`Fft`), Discrete Cosine Transforms (`Dct`), Discrete Wavelet Transforms (`Dwt`), Short-Time Fourier Transforms (`Stft`), Goertzel single-tone filtering, digital biquad notch filters (`NotchFilter`), 2D FFT sub-pixel motion estimation (`PhaseCorrelation`), summed-area tables (`IntegralImage`), DCT autofocus sharpness evaluation, spectral colormaps (`SpectrogramColorMap`), and matrix linear algebra (`MatrixMath`).
  2. [`libs/PelcoDCore/`](libs/PelcoDCore/): Pure C++17 domain library with **zero Qt or socket dependencies**. Encapsulates framing, checksums, accumulator stream parser, abstract `ITransport` port, kinematics and latency simulation, `MockPelcoDDevice`, `BusScanner` with multi-baud discovery, `PatrolController`, `RttProfiler`, `PidController`, `PtzAutoTracker` (Kalman filter with constant acceleration, predictive lead, scale adaptation), `ScopedConnectionList`, async queries, and specialized camera profiles (`FujinonSX800Device`).
  3. [`libs/PelcoDTransport/`](libs/PelcoDTransport/): Dedicated hardware and network I/O transport layer. Implements thread-safe adapters (`SerialTransport` for RS-485 via Win32/termios, `TcpTransport`, `UdpTransport`, `BaseTransport`, and cross-platform `SocketUtils`) fulfilling the `ITransport` interface.
  4. [`libs/PelcoDVideo/`](libs/PelcoDVideo/): High-performance multi-backend video streaming pipeline supporting RTSP, local video files, and camera capture devices. Features atomic triple buffering (`AtomicTripleBuffer`), modular decoders (FFmpeg, GStreamer, Mock SMPTE), tactical OpenCV image enhancement filters (`IFrameProcessor`: atmospheric dehazing, CLAHE, heat shimmer reduction, thermal pseudo-coloring, motion detection, and Lucas-Kanade target tracking), and terminal Braille rendering (`BrailleRenderer`).
  5. [`libs/PelcoDOnvif/`](libs/PelcoDOnvif/): Standalone ONVIF Profile S and Profile T client library (zero Qt dependencies). Implements WS-Discovery multicast scanning, WS-Security password digest generation, Device Management, Media Streaming (RTSP & Snapshot URIs), PTZ controls (continuous, relative, absolute, home position, presets), Optical & Imaging Service (brightness, contrast, saturation, sharpness, IR cut filter, WDR, BLC, focus), and PullPoint Event Service (real-time motion/tamper notifications).
  6. [`libs/PelcoDQt/`](libs/PelcoDQt/): Qt 6 adapter layer exposing asynchronous signals and slots for UI integration (`QPelcoDDevice`, `QFujinonSX800Device`, `QRttProfiler`, `QPatrolController`, `QBusScanner`, `QVideoStreamWorker`, `QOnvifDevice`).
  7. User Applications:
     - [`app-qt/`](app-qt/): Modern dark-themed Qt 6 desktop dashboard with live RTSP video streaming, tactical HUD overlays, compass jog controls, preset sequence manager, Fujinon optics, ONVIF Profile S/T camera control tab, RTT profiler, bus scanner, and traffic inspector.
     - [`app-tui/`](app-tui/): Zero-dependency UTF-8 terminal interface featuring ASCII/Braille live video playback, interactive PTZ compass, traffic inspection, bus scan, and ONVIF discovery CLI tools.
- **Direct Real-Time RX Stream Framing:**
  - Inbound byte streams are framed and validated directly inside the transport callback (`onDataReceived`) using a bounded accumulator, delivering sub-microsecond frame dispatch without thread sprawl.
- **Paced Command Queue:**
  - High-level commands are paced through an inter-command delay worker thread (~15–20 ms spacing) per Pelco-D RS-485 specifications with a bounded capacity (256 commands).
- **Cross-Platform Transports:**
  - **Serial:** Native POSIX termios on Linux, Win32 Comm API on Windows.
  - **TCP:** Native BSD sockets on Linux, Winsock2 on Windows.
  - **UDP:** Datagram transport with bidirectional RX/TX socket pairing.
  - **Mock Device:** Full in-memory hardware emulator with configurable kinematics (velocity, acceleration) and link latency/jitter/packet loss.
- **C4 Architecture Diagrams:**
  - Full system context, container, component, and sequence diagrams in ASCII and Mermaid are available in [docs/C4_Architecture.md](docs/C4_Architecture.md).

---

## Modular CMake Architecture & Build Options

The project uses a modular CMake architecture with dedicated `CMakeLists.txt` files per module and centralized compiler flags in [`cmake/CompilerFlags.cmake`](cmake/CompilerFlags.cmake).

### CMake Configuration Options

| Option | Default | Description |
|---|---|---|
| `PELCOD_ENABLE_VIDEO` | `ON` | Compiles `PelcoDVideo` library and video decoding / tactical filter pipelines |
| `PELCOD_ENABLE_ONVIF` | `ON` | Compiles `PelcoDOnvif` library, Profile S/T client, and ONVIF tabs/CLI tools |
| `WARNINGS_AS_ERRORS` | `ON` | Treats compiler warnings as errors (`-Werror` / `/WX`) |
| `ENABLE_HARDENING` | `ON` | Enables security hardening flags (stack protection, control flow guard, PIE, ASLR, RELRO) |
| `ENABLE_ASAN` | `OFF` | Enables AddressSanitizer (ASan) for memory leak & boundary detection |
| `ENABLE_UBSAN` | `OFF` | Enables UndefinedBehaviorSanitizer (UBSan) |
| `ENABLE_TSAN` | `OFF` | Enables ThreadSanitizer (TSan) for race condition analysis |

---

## Build Instructions

### Prerequisites
- **Compiler**: C++17 compliant compiler (GCC 9+, Clang 10+, or MSVC 2019+).
- **Build System**: CMake 3.16+.
- **Package Manager (Windows)**: [vcpkg](https://github.com/microsoft/vcpkg) using manifest mode (`vcpkg.json`) to manage dependencies (`glog`, `curl`, `pugixml`, `opencv4`, `ffmpeg`, `gstreamer`).
- **Dependencies (Linux)**: `libgoogle-glog-dev`, `libcurl4-openssl-dev`, `libpugixml-dev`, and optionally `libopencv-dev`, `libavcodec-dev`, `libavformat-dev`, `libswscale-dev`, `libgstreamer1.0-dev`.
- **GUI & Qt Adapter**: Qt 6.2+ (`QtCore`, `QtGui`, `QtWidgets`, `QtTest`).

### Compiling & Running Tests

#### Linux (GCC / Clang)

```bash
# Configure standard release build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Compile all targets (Core, Video, ONVIF, Qt adapter, GUI App, TUI App, Tests)
cmake --build build -j$(nproc)

# Run complete automated test suite
ctest --test-dir build --output-on-failure
```

#### Windows (MSVC)

```cmd
# Configure build with vcpkg manifest mode and Qt 6
cmake -B build -S . ^
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.6.1\msvc2019_64

# Build all targets (Release)
cmake --build build --config Release

# Run complete automated test suite
set PATH=C:\Qt\6.6.1\msvc2019_64\bin;%PATH%
ctest --test-dir build -C Release --output-on-failure
```

### Compiling with Sanitizers
```bash
# Configure with ASan and UBSan
cmake -B build-asan -S . -DENABLE_ASAN=ON -DENABLE_UBSAN=ON

# Build and verify with sanitizers
cmake --build build-asan -j$(nproc)
ctest --test-dir build-asan --output-on-failure
```

### Code Formatting & Git Hook
```bash
# Apply clang-format (WebKit style) to all C++ source and header files
cmake --build build --target format

# Verify formatting compliance without modifying files
cmake --build build --target format-check
```
*Note: CMake automatically installs the `.git/hooks/pre-commit` hook upon configuration, ensuring staged C++ files are auto-formatted with `clang-format` prior to each commit.*

### Launching the Applications

#### Desktop GUI Client (`app-qt`)
```bash
./build/app-qt/PelcoDAppQt
# Windows:
# .\build\app-qt\Release\PelcoDAppQt.exe
```

#### Zero-Dependency Terminal Client & CLI Tools (`app-tui`)
```bash
# Launch interactive terminal UI (ASCII/Braille video, PTZ compass, traffic)
./build/app-tui/PelcoDAppTui

# Discover ONVIF cameras on the local network (WS-Discovery)
./build/app-tui/PelcoDAppTui --onvif-discover

# Inspect device information and services
./build/app-tui/PelcoDAppTui --onvif-info http://192.168.1.100/onvif/device_service -u admin -p secret

# Query and adjust imaging / optical parameters
./build/app-tui/PelcoDAppTui --onvif-imaging http://192.168.1.100/onvif/device_service -u admin -p secret

# Monitor real-time PullPoint security events (motion, tamper alarms)
./build/app-tui/PelcoDAppTui --onvif-events http://192.168.1.100/onvif/device_service -u admin -p secret
```

---

## Supported Features (Pelco-D v5.0.1)

- **Standard Motion:** Pan (Left/Right with speed 0–63 and Turbo), Tilt (Up/Down with speed 0–63), Zoom (Tele/Wide), Focus (Near/Far), Iris (Open/Close), Stop.
- **Absolute Positioning:** Set Pan angle (0.00°–359.99° in centidegrees), Set Tilt angle (0.00°–359.99° in centidegrees), Set Zoom counts.
- **Presets:** Set Preset (1–255), Clear Preset, Go To Preset, Flip 180°, Zero Pan, Preset Scan / Tour.
- **Auxiliaries & Zones:** Aux Relays 1–8 (Set/Clear), Sector Zones 1–8 (Start/End/Scan).
- **Patterns:** Record Start, Record Stop, Run Pattern (1–4).
- **Optics & Configuration:** Auto Focus, Auto Iris, AGC, Backlight Compensation, Auto White Balance, Shutter Speed, Gain, Auto-Iris Level and Peak.
- **Diagnostics & Queries:** Query Pan, Query Tilt, Query Zoom, Query Device Type, Query General (18-byte model/serial string).
- **Asynchronous Queries:** Non-blocking `std::future` queries (`queryPanAsync`, `queryTiltAsync`, `queryZoomAsync`).
- **Connection Management:** `ScopedConnection` and `ScopedConnectionList` for automatic RAII callback cleanup.
- **Bus Diagnostics:** Multi-baud auto-discovery `BusScanner` (2400–115200 baud) and real-time Round-Trip-Time (`RttProfiler`) latency monitoring.
- **OSD:** Write character text at column coordinates 0–39, Clear screen.

---

## Tactical Video Streaming & Computer Vision (`PelcoDVideo`)

The [`libs/PelcoDVideo/`](libs/PelcoDVideo/) subsystem provides an end-to-end multi-backend video streaming and real-time image processing pipeline:

- **Modular Video Decoders:**
  - **FFmpeg (`FfmpegDecoder`):** Hardware-accelerated RTSP, RTMP, and local video file decoding via `libavcodec` and `libswscale`.
  - **GStreamer (`GstreamerDecoder`):** Ultra-low-latency pipeline integration with hardware VAAPI / NVDEC acceleration.
  - **Mock SMPTE (`MockDecoder`):** Built-in synthetic test pattern generator with bouncing target for testing without physical cameras.
- **Zero-Copy Frame Synchronization:**
  - Lock-free, wait-free `AtomicTripleBuffer` decouples high-fps network decoder threads from GUI/TUI rendering loops without tearing or frame drops.
- **Tactical OpenCV Filter Chain (`IFrameProcessor`):**
  - **Atmospheric Dehazing (`DehazeFilter`):** Dark channel prior algorithm penetrating maritime fog, smoke, and smog.
  - **Adaptive Contrast (`ClaheFilter`):** Contrast Limited Adaptive Histogram Equalization for high-contrast and low-light environments.
  - **Heat Shimmer Reduction (`HeatShimmerMitigationFilter`):** Multi-frame temporal fusion mitigating atmospheric boiling and turbulence distortion over long focal lengths.
  - **Thermal False-Coloring (`ThermalColorFilter`):** Multi-palette thermal simulation (Ironbow, White-Hot, Black-Hot, Rainbow).
  - **Motion Detection (`MovingTargetIndicatorFilter`):** MOG2 background subtractor with shadow suppression and bounding box detection.
  - **Lucas-Kanade Tracking (`CentroidTargetTrackerFilter`):** Multi-point optical flow tracking with dynamic scale adaptation and breadcrumb trails.
  - **Tactical Overlays (`TacticalOverlayFilter`, `GridOverlayFilter`):** Reticles, azimuth/elevation compass ticks, pitch/roll indicators, and boresight crosshairs.
- **Terminal Braille Video Rendering (`BrailleRenderer`):**
  - High-speed spatial downsampler rendering live 30 FPS video streams into ANSI terminal Braille characters ($2 \times 4$ dot matrix per glyph) with TrueColor support.

---

## Closed-Loop Autonomous PTZ Tracking (`PtzAutoTracker`)

Autonomous closed-loop vision-guided tracking bridging `PelcoDVideo` and `PelcoDCore`:

- **3-Axis Tracking:** Autonomous control of Pan, Tilt, and dynamic Zoom Tele/Wide.
- **6-State Kinematic Kalman Filter:**
  - Constant Acceleration (CA) model tracking target state $[x, y, v_x, v_y, a_x, a_y]^T$.
  - Adaptive process noise $Q(k)$ driven by measurement innovation residuals to handle maneuvering, braking, or rapidly accelerating targets.
- **PID Control Loop:**
  - Independent Pan and Tilt `PidController` instances with integral anti-windup clamping and velocity smoothing.
- **Dynamic Scale & Zoom Adaptation:**
  - Measures pairwise feature dispersion across Lucas-Kanade points to estimate target scale changes, driving continuous zoom adjustment to keep target dimensions stable in the viewport.
- **Predictive Boresight Lead:**
  - Offsets the camera optical center along the target velocity vector to maintain leading framing on high-speed targets.

---

## Standalone ONVIF Profile S & Profile T Subsystem (`PelcoDOnvif`)

The [`libs/PelcoDOnvif/`](libs/PelcoDOnvif/) library is a pure C++17 client (zero Qt dependencies) conforming to ONVIF Profile S and Profile T specifications:

- **WS-Discovery:** Multicast probe (`239.255.255.250:3702`) with XML response parsing and camera network discovery.
- **WS-Security:** Password digest authentication generating cryptographic `Nonce`, ISO 8601 `Created` timestamps, and Base64-encoded SHA-1 digest tokens.
- **Device Management:** Device information (manufacturer, model, firmware version, serial number, hardware ID) and service endpoint resolution.
- **Media Service:** Stream profile enumeration, RTSP streaming URI generation (`GetStreamUri`), and high-resolution snapshot URI retrieval (`GetSnapshotUri`).
- **PTZ Service:** Continuous move (pan/tilt/zoom velocities), Relative move, Absolute move, Stop, Preset management (Set, Goto, Remove, GetPresets), and Home position (Set, Goto).
- **Optical & Imaging Service:** Query and adjust Brightness, Contrast, Color Saturation, Sharpness, IR Cut Filter mode (Auto/On/Off), Wide Dynamic Range (WDR), Backlight Compensation (BLC), and Focus modes.
- **PullPoint Event Service:** Real-time event notifications via `CreatePullPointSubscription` and `PullMessages` for motion detection, tamper alarms, and digital I/O inputs.

---

## Specialized Profiles: Fujinon SX800 / SX801 (Protocol v2.12.0)

Specialized camera profiles ([`FujinonSX800Device`](libs/PelcoDCore/FujinonSX800Device.h) and [`QFujinonSX800Device`](libs/PelcoDQt/QFujinonSX800Device.h)) provide full protocol coverage for Fujinon SX800 and SX801 long-range surveillance zoom cameras conforming to Protocol Specification Version 2.12.0 (November 2022):

- **Stabilization & Optical Filters:**
  - Optical & Electronic Image Stabilization (Auto, OIS On, EIS On, Off)
  - Visible-Light-Cut (VLC) optical filter toggle
  - Optical & Digital Defog (Off, Levels 1–3)
  - De-Heat Haze image reduction (Off, Levels 1–2)
  - Wide Dynamic Range (WDR Levels 1–3)
- **Fine Image Quality Parameter Adjustments (1–100):**
  - Brightness Fine (`0xEB`), Contrast Fine (`0xED`), Saturation Fine (`0xEF`), Sharpness Fine (`0xF1`)
  - White Balance Shift Red Fine (`0xF5`) & Blue Fine (`0xF7`)
  - Continuous telemetry querying (`0xFD` / `0xFF`)
- **Extended Day / Night Switching:**
  - Extended operating modes: Auto, Auto & Scheduled, Scheduled, Day, Night
  - Luminance thresholds: Day to Night (`0x03`) and Night to Day (`0x05`)
  - Auto-switching delay seconds (0–60s)
  - Scheduled Day start time (`0x09`) and Night start time (`0x0B`)
  - Day & Night optical filter overrides (IR cut vs. IR pass)
- **Advanced Optics & Zoom Telemetry:**
  - 40x optical zoom with extended zoom speeds (steps 1–8: 4s to 60s)
  - Extended focus speeds (steps 1–5) and One-Push Auto-Focus
  - Digital zoom modes: Off, Digital Zoom (1.25x–2.0x), and Sensor Crop Mode
  - Physical focal length conversion mapping 16-bit raw zoom coordinates to focal length (20.0 mm wide to 800.0 mm tele, up to 1600.0 mm with digital zoom)
- **System, Media & OSD Navigation:**
  - Anti-Aliasing filter toggle (`0xF0 0x55`)
  - Full OSD menu navigation: OK (`0x9B`), Direction (`0x9D`), and Back (`0xAB`)
  - SD card recording, movie playback, card formatting, and factory reset
  - OSD Language selection (English, French, Japanese)
  - Real-Time Clock (RTC) synchronization (`0x00 0x3B`: seconds, hours, minutes, date, year)

---

## Documentation

- [C4 Architecture Models (ASCII & Mermaid)](docs/C4_Architecture.md)
- [Visual Servoing & Tracking Architecture (Kalman + PID)](docs/PID_Kalman_Tracking.md)
- [PID Controller 101 Guide](docs/technical/PID_Controller_101.md)
- [Kalman Filter 101 Guide](docs/technical/Kalman_Filter_101.md)
- [Fast Fourier Transform (FFT) 101 Guide](docs/technical/FFT_101.md)
- [Discrete Cosine Transform (DCT) 101 Guide](docs/technical/DCT_101.md)
- [Discrete Wavelet Transform (DWT) 101 Guide](docs/technical/DWT_101.md)
- [Notch Filter 101 Guide](docs/technical/Notch_Filter_101.md)
- [Integral Images 101 Guide](docs/technical/Integral_Images_101.md)
- [Pelco-D Specification (v5.0.1 PDF)](docs/protocols/DProtocol_Version_5_Revision_1.pdf)
- [Fujinon SX800 / SX801 Protocol Specification v2.12.0 (PDF)](docs/protocols/pelco-d_protocol_specification_for_sx800_801_v2.12.0_en.pdf)
- [Fujinon SX800 / SX801 Protocol Specification v2.51 (PDF)](docs/protocols/Pelco-D_Protocol_Specification_for_SX800_801_V.2.51_ENG.pdf)

---

## License

This project is licensed under the terms of the [MIT License](LICENSE).
Copyright (c) 2026 Stavros Vagionitis.
