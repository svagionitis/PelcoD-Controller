# New Features & Enhancement Roadmap

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

### 5. Video Analytics & Tracking Filters

#### A. Target Tracking Engine Enhancements
* **Dynamic Scale Adaptation (Zoom & Distance Compensation)**:
  * Currently, target bounding box dimensions (`width`, `height`) in `CentroidTargetTrackerFilter` remain fixed at acquisition. If the target approaches/recedes or the camera zooms, the box becomes ill-fitted.
  * Estimate scale expansion/contraction factor $s$ dynamically using pairwise Lucas-Kanade feature dispersion:
    $$s = \operatorname{median}\left(\frac{\|\mathbf{p}_i^{(t)} - \mathbf{p}_j^{(t)}\|}{\|\mathbf{p}_i^{(t-1)} - \mathbf{p}_j^{(t-1)}\|}\right)$$
  * Expand the Kalman filter to a 6-state vector $[x, y, w, h, v_x, v_y]^T$ for scale-invariant target framing.
* **Appearance Model Fusion (Anti-Drift / Re-Identification)**:
  * Pure Lucas-Kanade optical flow can accumulate sub-pixel drift over long tracking sessions and latch onto background clutter.
  * Fuse optical flow with an HSV Color / Luma Histogram Back-Projection or an NCC / MOSSE correlation template centered on the target to periodically re-anchor the centroid and prevent track drift.
* **Constant Acceleration (CA) Kinematic Kalman Model**:
  * Upgrade the linear 4-state constant-velocity filter to a 6-state constant-acceleration model $[x, y, v_x, v_y, a_x, a_y]^T$ with adaptive process noise covariance $Q(k)$ driven by measurement innovation residuals, dramatically improving response on maneuvering, braking, or accelerating targets.
* **Trajectory Breadcrumbs & Predictive Lead Vector**:
  * Render a decaying temporal trajectory path showing past target coordinates and a forward-pointing velocity vector arrow projecting where the target will be in 1–2 seconds.

#### B. Closed-Loop PTZ Auto-Tracking Extensions
* **Closed-Loop 3-Axis Auto-Zoom (Target Framing)**:
  * Expand `PtzAutoTracker` beyond Pan and Tilt to issue dynamic continuous/stepped `Zoom Tele` and `Zoom Wide` commands via `QPelcoDDevice::zoom()`, maintaining a constant relative target size on screen (e.g. 20% of viewport height).
* **Predictive Lead Angle Boresight Deflection**:
  * Offset the camera boresight slightly ahead along the target's estimated velocity vector so fast-moving objects stay centered in their direction of travel rather than lagging at the screen periphery.

#### C. New Tactical & Surveillance Filters
* **`LoiteringDetectorFilter` (Stationary Dwell-Time Alarm)**:
  * Define an arbitrary polygon Region of Interest (ROI); any target detected inside starts an accumulation dwell timer. If it remains longer than a configurable threshold (e.g. > 15s), it triggers an alert callback and flashing visual alarm brackets.
* **`MultiTargetTrackerFilter` (SORT / Hungarian Data Association)**:
  * Fuses `MovingTargetIndicatorFilter` (MOG2) detections with a bank of Kalman filters and bipartite IoU matching, assigning persistent track IDs (`#01`, `#02`, `#03`) across multiple simultaneous targets with speed readings and trajectory trails. Clicking any ID binds the PTZ auto-tracker to that target.
* **`TurbulenceMitigationFilter` (Heat Shimmer / Atmospheric Distortion Suppression)**:
  * Counteracts extreme telephoto atmospheric boiling over hot desert sand, runways, or maritime surfaces using temporal multi-frame "Lucky Imaging" patch selection and variance-weighted reconstruction.
* **`GeometricRangeCalculatorFilter` (Mast Elevation Stadiametric Rangefinder)**:
  * Computes real-time slant and ground distance to target based on camera elevation angle $\theta_{\text{el}}$ and known mast installation height: $D_{\text{ground}} = H_{\text{mast}} / \tan(-\theta_{\text{el}})$, overlaying live range readings on the OSD HUD.
* **`RetinexFilter` (Multi-Scale Retinex Night / Shadow Enhancement)**:
  * Decomposes scenes into reflectance and illumination components using MSRCR, recovering details hidden in dark shadows without washing out high-intensity headlights or perimeter floodlights.

---

### 6. ONVIF Standards & Profiles (Profile M & Profile T Analytics - Completed)

* **ONVIF Profile M & Profile T Video Analytics Rule Engine & Classification**:
  * Full ONVIF Analytics Service (`tan:`) client and server implementation.
  * Geometric rule evaluators:
    * `tt:LineDetector` (Tripwire segment crossing with directionality: `LeftToRight`, `RightToLeft`, `Any`).
    * `tt:FieldDetector` (Polygon intrusion detection via ray casting).
    * `tt:LoiteringDetector` (Polygon dwell-time tracking with threshold timers).
    * `tt:CellMotionDetector` (Sensitivity grid filtering).
  * Object classification and likelihood filters (`Human`, `Vehicle`, `TwoWheeler`) with minimum confidence thresholds.
  * Real-time notification dispatch via ONVIF PullPoint (`tns1:RuleEngine/*`) and live metadata stream integration.
  * Complete Qt layer (`QOnvifDevice`) and UI controls (`OnvifCameraTab`) for configuring rules and inspecting triggered events.

---

### Recommendation

If you want to stay in the **core architecture & communication layer**, the two best next steps are:
1. **`ScopedConnectionList`** (to streamline handling multiple connections cleanly).
2. **`queryAsync()` via `std::future`** (to bring modern C++17 async queries alongside the callback system).

If you want to advance **video analytics & autonomous PTZ tracking**, the highest-impact steps are:
1. **Target Tracking Scale Adaptation & Appearance Fusion** (eliminating drift and allowing full zoom independence).
2. **`LoiteringDetectorFilter`** (critical commercial/security surveillance analytics).
3. **Closed-Loop Auto-Zoom** (completing the full 3-axis autonomous tracking suite).