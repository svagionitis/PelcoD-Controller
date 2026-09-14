Searched for "TODO"
Listed directory docs
Viewed C4_Architecture.md:1-100
Searched for "PatrolController"
Searched for "export"
Viewed TrafficInspectorWidget.cpp:1-60
Viewed BusScanner.h:1-60
Viewed KinematicsSimulator.h:1-60

Here is a curated list of high-value features and enhancements that can be added to the project, organized by domain and impact:

---

### 1. Callback & Event System Extensions (Direct Follow-ups)

* **`ScopedConnectionList` / `ConnectionGroup`**:
  * Currently, listeners wanting multiple callbacks (e.g. status, traffic, timeout, and Fujinon) must hold multiple individual `ScopedConnection` variables.
  * A `ScopedConnectionList` container allows batch registration (`connections += device.addStatusCallback(...)`) and automatically disconnects all registered callbacks when the group goes out of scope.
* **`std::future` / Promise-Based Asynchronous Queries**:
  * Currently, queries rely on worker loop polling and updating state before notifying `addStatusCallback`.
  * Expose modern promise-based APIs:
    ```cpp
    std::future<std::uint16_t> panAngle = device.queryPanAsync(timeoutMs);
    ```
* **Bus Address-Filtered Callbacks**:
  * On shared RS-485 multi-drop busses with multiple cameras, traffic callbacks receive all frames. An `addTrafficCallback(uint8_t targetAddress, TrafficCallback cb)` overload filters out packets destined for other devices at the library level.

---

### 2. Protocol & Core Engine Capabilities

* **Pelco-P Protocol Support**:
  * Pelco-P (8-byte framing: `0xA0`, address, data bytes, checksum XOR/modulo-256, `0xAF`) is the most common companion standard to Pelco-D in industrial CCTV systems.
  * Adding `PelcoPBuilder`, `PelcoPParser`, and a protocol selection toggle in `PelcoDDevice` / transports.
* **Command Retries with Configurable Backoff**:
  * RS-485 lines in industrial environments are prone to electrical noise and dropped bytes.
  * Add configurable retry policies (e.g., attempt query up to 3 times before triggering `TimeoutCallback`).
* **Multi-Baud Auto-Discovery in `BusScanner`**:
  * Currently, `BusScanner` scans addresses 1–255 at the active baud rate.
  * Enhance it to cycle through common baud rates (`2400`, `4800`, `9600`, `19200`, `38400`, `115200`), enabling true zero-config discovery for unknown devices.

---

### 3. Traffic Inspection & Diagnostics

* **Traffic Capture Export in Qt (`TrafficInspectorWidget`)**:
  * The TUI (`TrafficView`) already has a text log export, but the Qt GUI currently only displays the table without export options.
  * Add export to **CSV**, **JSON Lines**, or **PCAP/Wireshark** format so captured traffic can be analyzed externally.
* **Packet Macro Playback / Hex Scripting**:
  * Allow operators to record a sequence of commands, save them as JSON/YAML, and play them back with millisecond timing control (useful for automated camera testing and repeatability benchmarks).
* **Round-Trip-Time (RTT) & Jitter Profiler**:
  * Measure real-time response latencies between query dispatch and response frame arrival, rendering live min/max/average RTT telemetry.

---

### 4. Hardware Input & User Interface

* **USB Gamepad / Joystick Support**:
  * Map analog gamepad sticks (via Linux `/dev/input/js*` or `evdev`) to proportional Pan/Tilt speeds and Zoom tele/wide triggers.
* **Patrol Tour Timeline & Visualizer**:
  * Provide a graphical timeline editor for `PatrolController` showing dwell times, target presets, and smooth transitions.
* **RTSP / Video Stream Overlay**:
  * Add a video preview widget (via Qt Multimedia or GStreamer/libvlc) side-by-side with the PTZ compass and Fujinon optics panel.

---

### Recommendation

If you want to stay in the **core architecture & communication layer**, the two best next steps are:
1. **`ScopedConnectionList`** (to streamline handling multiple connections cleanly).
2. **`queryAsync()` via `std::future`** (to bring modern C++17 async queries alongside the callback system).

If you want to expand **tools & diagnostics**, adding **PCAP / CSV Traffic Export to the Qt Traffic Inspector** is a high-utility improvement.