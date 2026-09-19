# New Features & Enhancement Roadmap

Here is a curated list of high-value features and enhancements that can be added to the project, organized by domain and impact:

---

### 1. Callback & Event System Extensions (Completed)

* **`ScopedConnectionList` / `ConnectionGroup` (Completed)**:
  * Container allowing batch registration (`connections += device.addStatusCallback(...)`) that automatically disconnects all registered callbacks when the group goes out of scope. Implemented in `libs/PelcoDCore/Connection.h` and tested in `TestConnection.cpp`.
* **`std::future` / Promise-Based Asynchronous Queries (Completed)**:
  * Exposes modern promise-based APIs (`queryPanAsync()`, `queryTiltAsync()`, `queryZoomAsync()`). Implemented in `libs/PelcoDCore/PelcoDDevice.h` and tested in `TestPelcoDDevice.cpp`.
* **Bus Address-Filtered Callbacks (Completed)**:
  * On shared RS-485 multi-drop busses with multiple cameras, `addTrafficCallback(uint8_t targetAddress, TrafficCallback cb)` filters out packets destined for other devices at the library level. Implemented in `PelcoDDevice.h`.

---

### 2. Protocol & Core Engine Capabilities (Completed)

* **Pelco-P Protocol Support (Completed)**:
  * Pelco-P (8-byte framing: `0xA0`, address, data bytes, checksum XOR/modulo-256, `0xAF`) builder and parser. Implemented in `PelcoPBuilder.h` and `PelcoPParser.h`, verified in `TestProtocolCompleteness.cpp`.
* **Command Retries with Configurable Backoff (Completed)**:
  * Configurable retry policies with backoff before triggering timeout callbacks. Implemented in `RetryPolicy.h` and tested in `TestRetryPolicy.cpp`.
* **Multi-Baud Auto-Discovery in `BusScanner` (Completed)**:
  * Auto-cycles through common baud rates (`2400`, `4800`, `9600`, `19200`, `38400`, `115200`) enabling true zero-config discovery. Implemented in `BusScanner.h` (`scanAllBauds()`) and tested in `TestBusScanner.cpp`.

---

### 3. Traffic Inspection & Diagnostics

* **Traffic Capture Export in Qt (`TrafficInspectorWidget`) (Completed)**:
  * Export captured traffic to **CSV**, **JSON Lines**, or **PCAP/Wireshark** format directly from `TrafficInspectorWidget` in the Qt GUI.
* **Packet Macro Playback / Hex Scripting**:
  * Allow operators to record a sequence of commands, save them as JSON/YAML, and play them back with millisecond timing control (useful for automated camera testing and repeatability benchmarks).
* **Round-Trip-Time (RTT) & Jitter Profiler (Completed)**:
  * Real-time response latency profiler between query dispatch and response frame arrival, rendering live min/max/average RTT telemetry. Implemented in `RttProfiler.h` and `RttProfilerDialog.h`, tested in `TestRttProfiler.cpp`.

---

### 4. Hardware Input & User Interface

* **USB Gamepad / Joystick Support**:
  * Map analog gamepad sticks (via Linux `/dev/input/js*` or `evdev`) to proportional Pan/Tilt speeds and Zoom tele/wide triggers.
* **Patrol Tour Timeline & Visualizer**:
  * Provide a graphical timeline editor for `PatrolController` showing dwell times, target presets, and smooth transitions.
* **RTSP / Video Stream Overlay (Completed)**:
  * Video preview widget side-by-side with PTZ compass and optics panel, with crosshair HUD, trajectory trails, and tactical overlay rendering. Implemented in `VideoStreamTab.cpp` and `VideoOverlayWidget.cpp`.

---

### 5. Video Analytics & Tracking Filters

#### A. Target Tracking Engine Enhancements
* **Dynamic Scale Adaptation (Zoom & Distance Compensation) (Completed)**:
  * Estimates scale expansion/contraction factor dynamically using pairwise Lucas-Kanade feature dispersion. Implemented in `CentroidTargetTrackerFilter::setScaleAdaptation()`.
* **Appearance Model Fusion (Anti-Drift / Re-Identification) (Completed)**:
  * Fuses optical flow with HSV/Luma histogram back-projection to periodically re-anchor the centroid and prevent track drift. Implemented in `CentroidTargetTrackerFilter::setAppearanceFusion()`.
* **Constant Acceleration (CA) Kinematic Kalman Model (Completed)**:
  * 6-state constant-acceleration model $[x, y, v_x, v_y, a_x, a_y]^T$ with adaptive process noise covariance $Q(k)$ driven by measurement innovation residuals. Implemented in `CentroidTargetTrackerFilter`.
* **Trajectory Breadcrumbs & Predictive Lead Vector (Completed)**:
  * Temporal trajectory trail with Catmull-Rom spline smoothing, thermal speed gradient coloring, CTRA turn prediction, Kalman covariance uncertainty ellipse, and PTZ boresight setpoint indicator. Implemented in `CentroidTargetTrackerFilter` and configurable via `TacticalOverlaysDialog`.

#### B. Closed-Loop PTZ Auto-Tracking Extensions
* **Closed-Loop 3-Axis Auto-Zoom (Target Framing) (Completed)**:
  * Issues dynamic continuous/stepped zoom commands via `QPelcoDDevice::zoom()` to maintain constant relative target size on screen. Implemented in `PtzAutoTracker::setAutoZoomEnabled()`.
* **Predictive Lead Angle Boresight Deflection (Completed)**:
  * Offsets camera boresight along target velocity vector so maneuvering objects remain centered in their direction of travel. Implemented in `PtzAutoTracker::setPredictiveLeadEnabled()`.

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

* **ONVIF Profile M & Profile T Video Analytics Rule Engine & Classification (Completed)**:
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

### 7. Signal Processing, DSP & Advanced Control Tools

Techniques from digital signal processing (DSP), system identification, and control theory that complement the FFT, Kalman Filter, and PID controller:

#### A. Frequency-Domain & Spectral Tools
* **Goertzel Algorithm (Targeted Single-Frequency Monitoring) (Completed)**:
  * Computes discrete Fourier transform power at a specific target frequency using a 2nd-order IIR filter with $O(N)$ efficiency. Implemented in `GoertzelFilter.h`, tested in `TestGoertzelFilter.cpp`.
* **Short-Time Fourier Transform (STFT) & Real-Time Spectrogram (Completed)**:
  * Pure C++17 sliding-window frequency-domain engine producing a 2D time-frequency energy distribution (spectrogram / waterfall matrix) without external dependencies. Implemented in `Stft.h` and `SpectrogramWidget`.
* **Discrete Wavelet Transform (DWT / Haar / Daubechies) (Completed)**:
  * Multi-resolution decomposition with flexible time-frequency localization for transient shocks and wind blast impulses. Implemented in `Dwt.h`, tested in `TestDwt.cpp`.

#### B. Time-Domain Filtering & State Estimation
* **Cross-Correlation Latency Estimator (Completed)**:
  * Measures lagged similarity between commanded PTZ motor velocities and observed optical flow velocities to discover physical end-to-end latency. Implemented in `LatencyEstimator.h` and `LatencyCalibrator.h`.
* **LMS / RLS Adaptive Filter (Active Vibration Cancellation - AVC)**:
  * Dynamically adapts FIR filter weights to cancel an interfering noise source in real time using IMU/accelerometer reference signals.
* **Savitzky-Golay Polynomial Smoothing Filter**:
  * Fits local low-degree polynomials via moving convolution to smooth noisy optical flow centroids without phase lag.
* **Extended / Unscented Kalman Filter (EKF / UKF) (Completed)**:
  * Non-linear Bayesian state estimator upgrading linear 2D tracker to true 3D spherical kinematics and pinhole camera projective geometry. Implemented in `ExtendedKalmanFilter.h` and `UnscentedKalmanFilter.h`.

#### C. Video Domain Enhancements
* **Discrete Cosine Transform (DCT) Auto-Focus Sharpness Metric (Completed)**:
  * Computes spatial frequency energy using real-valued DCT basis functions across 8x8 blocks for contrast-invariant lens focus sweeps. Implemented in `Dct.h`, tested in `TestDctSharpness.cpp`.
* **Integral Images (Summed-Area Tables) (Completed)**:
  * Computes arbitrary rectangular pixel sums in $O(1)$ constant time for adaptive local thresholding and contrast normalization. Implemented in `IntegralImage.h`, tested in `TestIntegralImage.cpp`.
* **Phase Correlation (2D FFT Global Motion Estimation) (Completed)**:
  * Estimates sub-pixel translational shifts between frames using normalized cross-power spectrum for featureless environment stabilization. Implemented in `PhaseCorrelation.h`, tested in `TestPhaseCorrelation.cpp`.

#### D. Control System Identification
* **Automated Chirp / Swept-Sine Plant Identification (Empirical Bode Plot) (Completed)**:
  * Pure C++17 system identification engine (`PlantIdentifier`) and physical sweep orchestrator (`ChirpCalibrator`), estimating empirical Frequency Response Functions and computing PID auto-tuning. Implemented in `PlantIdentifier.h` and `BodePlotWidget`.

---

### Recommendation

If you want to stay in the **core architecture & communication layer**, the two best next steps are:
1. **`ScopedConnectionList`** (to streamline handling multiple connections cleanly).
2. **`queryAsync()` via `std::future`** (to bring modern C++17 async queries alongside the callback system).

If you want to advance **video analytics & autonomous PTZ tracking**, the highest-impact steps are:
1. **Target Tracking Scale Adaptation & Appearance Fusion** (eliminating drift and allowing full zoom independence).
2. **Cross-Correlation Latency Estimator** (empirically tuning lookahead compensation using real motor vs. video feedback).
3. **Closed-Loop Auto-Zoom** (completing the full 3-axis autonomous tracking suite).
4. **Goertzel Filter / Swept-Sine Auto-Tuning** (providing robust anti-hunting and zero-config PID auto-tuning for any camera).