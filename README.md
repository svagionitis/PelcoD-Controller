# Pelco-D Device Controller

A production-grade, cross-platform C++17 library and modern Qt 6 desktop client for controlling Pelco-D protocol devices (Pan/Tilt heads, gimbals, motorized domes, receiver/drivers, and integrated PTZ units) adhering strictly to **Pelco PTZ Protocols: D Protocol Version 5.0.1**.

---

## Architectural Highlights

- **Three-Tier Separation of Concerns:**
  1. [`libs/PelcoDCore/`](libs/PelcoDCore/): Pure C++17 static library with **zero Qt dependencies**. Encapsulates framing, checksum calculations, direct stream accumulator parsing, transports, simulated device emulator (`MockPelcoDDevice`), protocol builders/parsers, high-level device controller (`PelcoDDevice`), and specialized camera profiles (`FujinonSX800Device`).
  2. [`libs/PelcoDQt/`](libs/PelcoDQt/): Qt 6 adapter layer (`QPelcoDDevice`, `QFujinonSX800Device`) exposing signals and slots for asynchronous UI integration.
  3. [`app-qt/`](app-qt/): Sleek, modern dark-themed Qt 6 desktop dashboard with live telemetry, interactive D-pad, preset manager, device settings, aux/zones/patterns, OSD labeling, and real-time hex traffic inspector.
- **Direct Real-Time RX Stream Framing:**
  - Inbound byte streams are framed and validated directly inside the transport callback (`onDataReceived`) using a bounded accumulator, eliminating redundant context switches and delivering sub-microsecond frame dispatch without thread sprawl.
- **Paced Command Queue:**
  - High-level commands are paced through an inter-command delay worker thread (~15–20 ms spacing) per Pelco-D RS-485 specifications with a bounded capacity (256 commands).
- **Cross-Platform Transports:**
  - **Serial:** Native POSIX termios on Linux, Win32 Comm API on Windows.
  - **TCP:** Native BSD sockets on Linux, Winsock2 on Windows.
  - **Mock Device:** Full in-memory hardware emulator for hardware-free development, testing, and continuous integration.
- **C4 Architecture Diagrams:**
  - Full system context, container, component, and sequence diagrams in ASCII and Mermaid are available in [docs/C4_Architecture.md](docs/C4_Architecture.md).

---

## Modular CMake Architecture & Build Options

The project uses a modular CMake architecture with dedicated `CMakeLists.txt` files per module and centralized compiler flags in [`cmake/CompilerFlags.cmake`](cmake/CompilerFlags.cmake).

### CMake Configuration Options

| Option | Default | Description |
|---|---|---|
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
- **Package Manager (Windows)**: [vcpkg](https://github.com/microsoft/vcpkg) using manifest mode (`vcpkg.json`) to manage `glog`.
- **Dependencies (Linux)**: `libgoogle-glog-dev`.
- **GUI & Qt Adapter**: Qt 6.2+ (`QtCore`, `QtGui`, `QtWidgets`, `QtTest`).

### Compiling & Running Tests

#### Linux (GCC / Clang)

```bash
# Configure standard release build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Compile all targets (Core, Qt adapter, GUI App, Tests)
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

### Launching the Application
```bash
./build/PelcoDApp
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
- **OSD:** Write character text at column coordinates 0–39, Clear screen.

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
- [Pelco-D Specification (v5.0.1 PDF)](docs/DProtocol_Version_5_Revision_1.pdf)
- [Fujinon SX800 / SX801 Protocol Specification v2.12.0 (PDF)](docs/pelco-d_protocol_specification_for_sx800_801_v2.12.0_en.pdf)
- [Fujinon SX800 / SX801 Protocol Specification v2.51 (PDF)](docs/Pelco-D_Protocol_Specification_for_SX800_801_V.2.51_ENG.pdf)

---

## License

This project is licensed under the terms of the [MIT License](LICENSE).
Copyright (c) 2026 Stavros Vagionitis.
