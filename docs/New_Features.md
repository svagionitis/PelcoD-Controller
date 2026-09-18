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

### 7. Signal Processing, DSP & Advanced Control Tools

Techniques from digital signal processing (DSP), system identification, and control theory that complement the FFT, Kalman Filter, and PID controller:

#### A. Frequency-Domain & Spectral Tools
* **Goertzel Algorithm (Targeted Single-Frequency Monitoring)**:
  * Computes discrete Fourier transform power at a specific target frequency using a 2nd-order IIR filter with $O(N)$ efficiency, zero complex arithmetic in the inner loop, and no power-of-2 buffer sizing constraints.
  * Continuously monitors known pole resonance modes (e.g. 10 Hz mast buffeting) or hunting bands (e.g. 2 Hz limit cycles) on every incoming sample with near-zero CPU footprint.
* **Short-Time Fourier Transform (STFT) & Real-Time Spectrogram**:
  * Applies a sliding-window FFT over time to produce a 2D time-frequency energy distribution (spectrogram / waterfall).
  * Render live vibration and hunting history in the TUI (`TrafficView`) or Qt GUI to visualize structural vibrations, motor gear degradation, or stability changes over minutes and hours.
* **Discrete Wavelet Transform (DWT / Haar / Daubechies)**:
  * Multi-resolution decomposition with flexible time-frequency localization.
  * Unlike Fourier transforms, wavelets excel at detecting **transient shocks, wind blast impulses, and vehicle jolts** without windowing smearing or edge artifacts.

#### B. Time-Domain Filtering & State Estimation
* **Cross-Correlation Latency Estimator (Completed)**:
  * Measures the lagged similarity between commanded PTZ motor velocities $u(t)$ and visual velocities $v(t)$ observed by optical flow:
    $$R_{uv}(\tau) = \sum_{t} u(t) \cdot v(t + \tau)$$
  * Discovers the exact empirical physical end-to-end latency $\Delta t_{\text{delay}}$ (combining RS-485 transmission, motor acceleration ramp, camera image sensor exposure, RTSP networking, and H.264 decoding) to dynamically tune Kalman lookahead prediction.
  * Native negative polarity support for inverse camera-to-scene optical flow, 3-point parabolic peak interpolation for sub-millisecond precision, and asynchronous timestamped resampling.
  * Includes `LatencyCalibrator` active doublet pulse sequence and Qt GUI integration with dynamic Kalman lookahead adaptation in `VideoStreamTab`.
* **LMS / RLS Adaptive Filter (Active Vibration Cancellation - AVC)**:
  * Dynamically adapts FIR filter weights to cancel an interfering noise source in real time.
  * Uses an external accelerometer / IMU reference mounted on the camera mast or vehicle to subtract structural vibration directly from tracking error signals or optical flow measurements.
* **Savitzky-Golay Polynomial Smoothing Filter**:
  * Fits local low-degree polynomials via moving convolution to smooth noisy optical flow centroids.
  * Preserves peak heights, widths, and sharp maneuver inflection points without introducing the phase distortion and group delay caused by standard moving-average filters.
* **Extended / Unscented Kalman Filter (EKF / UKF) (Completed)**:
  * Non-linear Bayesian state estimator upgrading the linear 2D Cartesian tracker to true 3D spherical kinematics and pinhole camera projective geometry.
  * Zero external dependencies: includes header-only fixed-size `Matrix<Rows, Cols>` and `Vector<Dim>` templates with zero heap allocation, supporting Gauss-Jordan inversion with partial pivoting and lower-triangular Cholesky decomposition.
  * Camera projective geometry model (`PtzCameraModel`) supporting dynamic optical zoom focal length scaling $f(z) = f_0 \cdot z$, 2nd-order radial distortion ($k_1, k_2$), gimbal rotation projections, iterative unprojection, and central difference Jacobian computation.
  * Dual non-linear estimator algorithms:
    * **Extended Kalman Filter (`ExtendedKalmanFilter`)**: Analytical Jacobian linearization with Joseph form stabilized covariance updates and Mahalanobis innovation distance outlier gating.
    * **Unscented Kalman Filter (`UnscentedKalmanFilter`)**: 13 deterministic sigma points via scaled unscented transform ($\alpha, \beta, \kappa$) propagating directly through the exact non-linear camera projection without Jacobian approximations.
  * Domain estimator `PtzSphericalEstimator` integrating telemetry, lookahead latency prediction, lock acquisition, and angular tracking error outputs $(\Delta\theta, \Delta\phi)$ and velocities $(\omega_\theta, \omega_\phi)$ in deg/s.
  * Fully integrated into `PtzAutoTracker::updateAngular()` and Qt GUI `VideoStreamTab` with real-time filter algorithm selection (Linear 2D, EKF, UKF).

#### C. Video Domain Enhancements
* **Discrete Cosine Transform (DCT) Auto-Focus Sharpness Metric**:
  * Computes spatial frequency energy using real-valued DCT basis functions across 8x8 image blocks.
  * Summing high-frequency AC coefficients yields a contrast-invariant, illumination-robust image sharpness score for driving high-speed motorized lens focus sweeps.
* **Integral Images (Summed-Area Tables)**:
  * Computes arbitrary rectangular pixel sums in $O(1)$ constant time regardless of bounding box dimensions.
  * Powers real-time adaptive local thresholding, instant contrast normalization, and rapid bounding-box feature extraction for target trackers.
* **Phase Correlation (2D FFT Global Motion Estimation)**:
  * Estimates sub-pixel translational shifts between consecutive frames using the normalized cross-power spectrum:
    $$R = \frac{F_1 \cdot F_2^*}{\|F_1 \cdot F_2^*\|}$$
  * Complements sparse optical flow in `ImageStabilizationFilter` for robust global motion stabilization in featureless or low-texture environments (e.g. open ocean, haze, fog, overcast sky).

#### D. Control System Identification
* **Automated Chirp / Swept-Sine Plant Identification (Empirical Bode Plot)**:
  * Drives the PTZ motors with a sweeping frequency chirp signal ($0.1 \to 20\text{ Hz}$) and computes the frequency response function $H(f) = Y(f) / X(f)$.
  * Automatically measures physical motor inertia, gear backlash, and resonance modes to auto-tune optimal PID gains ($K_p, K_i, K_d$) for any connected third-party camera without trial-and-error manual tuning.

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