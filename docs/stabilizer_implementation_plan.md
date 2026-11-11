# Video Stabilizer Library Implementation Plan (`libs/Stabilizer`)

## 1. Overview & Objectives

This document establishes the technical master design, architectural decomposition, mathematical algorithms, and phased implementation schedule for **`libs/Stabilizer`**.

`libs/Stabilizer` is a clean-room, high-performance, zero-heap, strongly-typed C++17 digital video stabilization library designed for defense, aerospace, robotics, and high-speed electro-optical/infrared (EO/IR) gimbal camera systems.

> [!IMPORTANT]
> **Clean-Room Policy & IP Compliance Notice**:
> In accordance with software engineering ethics and intellectual property integrity, this library is engineered strictly clean-room from public peer-reviewed computer vision literature, IEEE/CVF publications, and open robotics standards. No proprietary or decompiled binaries/source trees are accessed, inspected, or incorporated.

### Key Objectives
1. **Real-Time Global Motion Estimation (GME)**:
   - Sub-pixel inter-frame motion estimation operating on 8-bit grayscale (luma/Y) frames at $\ge 60$ FPS (1080p) and $\ge 30$ FPS (4K).
   - Multi-scale Pyramidal Lucas-Kanade (LK) feature tracking with Shi-Tomasi corner detection and spatial bucketing.
   - Frequency-domain 2D Phase Correlation fallback utilizing `libs/Math/PhaseCorrelation.h` for texture-poor or low-contrast thermal (LWIR/MWIR) imagery.
2. **Robust Parametric Motion Modeling**:
   - Random Sample Consensus (RANSAC) and M-Estimator Sample Consensus (MSAC) outlier rejection.
   - Support for Translation (2-DOF), Euclidean/Rigid (3-DOF: translation + rotation), Similarity (4-DOF: rigid + uniform scale), Affine (6-DOF: shear + non-uniform scale), and Projective/Homography (8-DOF).
   - Degeneracy detection (collinear points, insufficient inliers, conditioning thresholds).
3. **Intentional Motion vs. High-Frequency Jitter Separation**:
   - Discrete Extended Kalman Filter (EKF) and Constant Velocity/Acceleration state-space filters.
   - Adaptive Gaussian Moving Average (GMA) and Savitzky-Golay trajectory smoothing.
   - Constrained $L_1$ Optimal Path Smoothing (Total Variation / Linear Programming) yielding piecewise-constant, linear, and parabolic cinematic camera trajectories.
   - Operator Slew / PTZ Feedforward Integration: Fuses PTZ pan/tilt telemetry (`libs/PelcoDCore`, `libs/ViscaCore`) and IMU/gyro data (`libs/DjiTelemetry`) to distinguish operator-commanded panning from vehicle engine vibration and wind buffeting.
4. **Motion Compensation & High-Quality Image Warping**:
   - Zero-heap forward and backward geometric mapping with sub-pixel bilinear and bicubic SIMD-optimized interpolation.
   - Dynamic Margin Auto-Cropping with adaptive zoom scaling preventing black edge artifacts during large angular excursions.
   - Rolling Shutter Wobble / Jello Correction: Row-wise motion interpolation for CMOS rolling shutter sensors.
5. **Multi-Sensor Fusion & Tactical Mission Profiles**:
   - High-rate IMU/gyroscope Extended Kalman Filter fusion (500 Hz–1 kHz) providing sub-frame predictive compensation.
   - Tactical Horizon Lock / Roll Leveling anchored to STANAG 4609 KLV attitude telemetry or gravitational reference.
   - Observation Staring Lock (Virtual Tripod) and Target-Centric Stabilization coupled directly with `libs/Tracking`.
   - Low-latency look-ahead trajectory buffering for jerk-free recording streams.
6. **Advanced Optical Quality & Border Reconstruction**:
   - Single-pass unified lens distortion rectification (radial/tangential dewarping) integrated into SIMD warp interpolation.
   - Temporal background mosaic inpainting eliminating crop zoom and black border artifacts.
   - Native SIMD video surface warping for planar YUV420p, semi-planar NV12, and packed RGB24.
7. **Architectural & Safety Standards**:
   - Zero Qt dependencies (`#include <Q...>` strictly prohibited in core modules).
   - Strictly conforming to **MISRA C++:2023** and **SEI CERT C++** rules.
   - Zero dynamic heap allocation on the per-frame processing path (`noexcept`, CERT ERR55-CPP compliant).
   - All function names strictly less than 30 characters.
   - Full Include-What-You-Use (IWYU) adherence with explicit `<cstdint>` and `std::` namespace qualification.

---

## 2. Mathematical Foundations & Production Algorithms

### 2.1 Global Motion Estimation (GME)

```
+-----------------------------------------------------------------------------------------------+
|                             GLOBAL MOTION ESTIMATION PIPELINE                                 |
|                                                                                               |
|   +-------------------+      +-----------------------+      +-----------------------------+   |
|   |   Current Frame   | ---> | 3-Level Image Pyramid | ---> | Shi-Tomasi Feature Detector |   |
|   |    $I_t(x, y)$    |      | $L_0, L_1, L_2$       |      | (Spatial Grid Bucketing)    |   |
|   +-------------------+      +-----------------------+      +--------------+--------------+   |
|                                                                            |                  |
|   +-------------------+      +-----------------------+                     ▼                  |
|   |  Previous Frame   | ---> | Pyramidal Optical     | <-------------------+                  |
|   |   $I_{t-1}(x, y)$ |      | Flow (Lucas-Kanade)   |                                        |
|   +-------------------+      +-----------+-----------+                                        |
|                                          |                                                    |
|                                          ▼ Raw Correspondences                                |
|                              +-----------------------+                                        |
|                              |   RANSAC / MSAC Fit   | ---> Transformation Matrix             |
|                              | (Affine / Euclidean)  |      $H_t \in \mathbb{R}^{3 \times 3}$ |
|                              +-----------------------+                                        |
+-----------------------------------------------------------------------------------------------+
```

```mermaid
flowchart TD
    subgraph GME["Global Motion Estimation Pipeline"]
        F1["Current Frame I(t)"] --> Pyr1["Construct 3-Level Pyramid"]
        F0["Previous Frame I(t-1)"] --> Pyr0["Construct 3-Level Pyramid"]
        Pyr1 --> Det["Shi-Tomasi Corner Detection<br/>(Spatial Cell Bucketing)"]
        Det --> Track["Bidirectional Pyramidal LK Tracking"]
        Pyr0 --> Track
        Track --> Filter["Forward-Backward Consistency &<br/>Eigenvalue Filtering"]
        Filter --> Ransac["Robust Model Fitting (RANSAC / MSAC)"]
        Ransac --> Out["Inter-Frame Transform H(t)"]
    end
```

#### 2.1.1 Multi-Scale Pyramidal Lucas-Kanade (LK) Flow
Given two consecutive grayscale images $I(x, y)$ and $J(x, y)$, the brightness constancy assumption asserts:
$$I(x, y) = J(x + u, y + v)$$
Linearizing via first-order Taylor expansion:
$$I_x u + I_y v + I_t \approx 0$$
where $I_x = \frac{\partial I}{\partial x}$, $I_y = \frac{\partial I}{\partial y}$, and $I_t = J(x, y) - I(x, y)$.

For an integration window $\Omega$ of size $(2w+1) \times (2w+1)$ around feature point $p$:
$$\begin{bmatrix} \sum I_x^2 & \sum I_x I_y \\ \sum I_x I_y & \sum I_y^2 \end{bmatrix} \begin{bmatrix} u \\ v \end{bmatrix} = - \begin{bmatrix} \sum I_x I_t \\ \sum I_y I_t \end{bmatrix}$$
Let spatial gradient matrix $G = \sum_{\Omega} \begin{bmatrix} I_x^2 & I_x I_y \\ I_x I_y & I_y^2 \end{bmatrix}$. The tracking vector is solvable when $\lambda_{\min}(G) \ge \tau_{\text{eigen}}$, satisfying the Shi-Tomasi corner criterion.

To track large displacements (e.g. up to $\pm 64$ pixels during fast vehicle vibration), an image pyramid $\{I^L\}_{L=0}^{L_{\max}}$ is constructed with Gaussian downsampling ($L_{\max}=3$). The motion vector computed at level $L$ initializes level $L-1$:
$$d^{L-1}_{\text{init}} = 2 \cdot d^L$$

#### 2.1.2 Bidirectional Error Verification
To guarantee zero spurious feature matches:
1. Track forward from $I_{t-1}$ to $I_t$ obtaining point $p'$.
2. Track backward from $I_t$ to $I_{t-1}$ obtaining point $p''$.
3. If $\|p - p''\|_2 > \epsilon_{\text{fb}}$ (default $0.5$ pixels), prune correspondence as an outlier.

#### 2.1.3 Spatial Grid Bucketing
To prevent feature clustering on high-contrast local foreground objects (e.g., passing vehicles or birds), the image plane is divided into a $K \times M$ grid (e.g., $8 \times 6$ cells). Each cell extracts at most $N_{\text{cell}}$ strongest features, enforcing uniform geometric distribution across the field of view.

#### 2.1.4 Frequency-Domain 2D Phase Correlation Fallback
For texture-poor environments (maritime open water, featureless sky, smoke/haze) or low-contrast thermal (LWIR/MWIR) imagery where corner detectors fail ($\sum \text{corners} < N_{\min}$), the pipeline automatically switches to frequency-domain 2D phase correlation powered by `libs/Math/PhaseCorrelation.h`.
Given consecutive frame luma windows $I_t(x, y)$ and $I_{t-1}(x, y)$ with 2D discrete Fourier transforms $F_t(u, v)$ and $F_{t-1}(u, v)$:
$$\mathcal{R}(u, v) = \frac{F_t(u, v) \cdot F_{t-1}^*(u, v)}{|F_t(u, v) \cdot F_{t-1}^*(u, v)|}$$
The cross-correlation surface $r(x, y) = \mathcal{F}^{-1}\{\mathcal{R}(u, v)\}$ exhibits an impulse peak at the translation displacement $(\Delta x, \Delta y)$:
$$(\Delta x, \Delta y) = \arg\max_{x, y} \{ r(x, y) \}$$
Sub-pixel accuracy is extracted via 2D parabolic interpolation around the peak, yielding sub-pixel 2-DOF translation without discrete landmark detection.

#### 2.1.5 Dynamic Target & Static Reticle Exclusion Masking
To prevent foreground motion bias:
1. **Static Masking**: Configurable border bands (Sightline SLA format) and arbitrary static rectangular regions ignore stationary reticles, HUD symbols, timecodes, and chassis edges.
2. **Dynamic Target Exclusion**: Ingests active target bounding boxes from `libs/Tracking` (`TargetTrack`), masking out maneuvering targets so their motion does not corrupt the camera background motion estimation.

---

### 2.2 Robust Model Fitting & Outlier Rejection

#### 2.2.1 Motion Model Hierarchy
1. **Translation (2-DOF)**:
   $$\begin{bmatrix} x' \\ y' \end{bmatrix} = \begin{bmatrix} x \\ y \end{bmatrix} + \begin{bmatrix} t_x \\ t_y \end{bmatrix}$$
2. **Euclidean / Rigid (3-DOF)**:
   $$\begin{bmatrix} x' \\ y' \end{bmatrix} = \begin{bmatrix} \cos\theta & -\sin\theta \\ \sin\theta & \cos\theta \end{bmatrix} \begin{bmatrix} x \\ y \end{bmatrix} + \begin{bmatrix} t_x \\ t_y \end{bmatrix}$$
3. **Similarity (4-DOF)**:
   $$\begin{bmatrix} x' \\ y' \end{bmatrix} = \begin{bmatrix} s\cos\theta & -s\sin\theta \\ s\sin\theta & s\cos\theta \end{bmatrix} \begin{bmatrix} x \\ y \end{bmatrix} + \begin{bmatrix} t_x \\ t_y \end{bmatrix}$$
4. **Affine (6-DOF)**:
   $$\begin{bmatrix} x' \\ y' \end{bmatrix} = \begin{bmatrix} a_{11} & a_{12} \\ a_{21} & a_{22} \end{bmatrix} \begin{bmatrix} x \\ y \end{bmatrix} + \begin{bmatrix} t_x \\ t_y \end{bmatrix}$$
5. **Homography / Projective (8-DOF)**:
   $$\begin{bmatrix} x' \\ y' \\ 1 \end{bmatrix} \sim \begin{bmatrix} h_{11} & h_{12} & h_{13} \\ h_{21} & h_{22} & h_{23} \\ h_{31} & h_{32} & 1 \end{bmatrix} \begin{bmatrix} x \\ y \\ 1 \end{bmatrix}$$

#### 2.2.2 MSAC Outlier Scoring
Unlike standard RANSAC which applies a binary 0/1 loss, M-Estimator Sample Consensus (MSAC) applies a bounded quadratic error penalty:
$$\rho(e_i^2) = \begin{cases} e_i^2 & \text{if } e_i^2 < T^2 \\ T^2 & \text{if } e_i^2 \ge T^2 \end{cases}$$
where $e_i = \|p_i' - H p_i\|_2$ is the reprojection error. This avoids ties between degenerate candidate models and converges to superior accuracy with fewer iterations ($N_{\text{iter}} \le 200$).

#### 2.2.3 Projective / Homography 8-DOF Direct Linear Transformation (DLT)
For wide-angle optics or oblique low-altitude aerial perspectives with perspective keystone distortion, an 8-DOF projective homography is solved via 4-point Direct Linear Transformation (DLT):
$$\mathbf{A}_i \mathbf{h} = \mathbf{0}, \quad \mathbf{A}_i = \begin{bmatrix} -x_i & -y_i & -1 & 0 & 0 & 0 & x_i x_i' & y_i x_i' & x_i' \\ 0 & 0 & 0 & -x_i & -y_i & -1 & x_i y_i' & y_i y_i' & y_i' \end{bmatrix}$$
Solved via singular value decomposition (SVD) of $\mathbf{A} \in \mathbb{R}^{2N \times 9}$ with normalization (Hartley pre-conditioning) to eliminate numerical instability.

#### 2.2.4 Spatially-Variant Parallax Mesh Warping (MeshFlow Grid Homography)
When the scene contains 3D parallax (e.g. low-altitude drone flight with foreground trees and distant terrain), a single 2D parametric matrix produces local wobbles. The image is tessellated into an $M \times N$ control mesh (e.g. $16 \times 12$ vertices).
Each vertex displacement $\mathbf{V}_{i,j}$ minimizes a combined data and spatial smoothness energy:
$$\mathcal{E}(\mathbf{V}) = \sum_{i,j} \mathcal{E}_{\text{data}}(\mathbf{V}_{i,j}) + \gamma \sum_{(i,j) \sim (k,l)} \|\mathbf{V}_{i,j} - \mathbf{V}_{k,l}\|_2^2$$
Regularized using sparse Jacobi iteration on stack memory with zero dynamic allocations.

---

### 2.3 Motion Filtering: Intentional vs. Jitter Trajectory

```
+-----------------------------------------------------------------------------------------------+
|                            TRAJECTORY SMOOTHING ARCHITECTURE                                  |
|                                                                                               |
|   Cumulative Motion           Intentional Trajectory           Correction Offset              |
|   $P_t = \prod_{k=1}^t H_k$   $S_t = \text{Filter}(P_t)$       $C_t = P_t^{-1} \cdot S_t$     |
|              |                             |                               |                  |
|              ▼                             ▼                               ▼                  |
|   +---------------------+       +---------------------+       +-------------------------+     |
|   | Raw Trajectory Accum| ----> | Trajectory Filter:  | ----> | Warp Generator:         |     |
|   |  - Translation (X,Y)|       |  - Dual-State EKF   |       |  - Affine/Perspective   |     |
|   |  - Rotation (\theta)|       |  - Gaussian FIR     |       |  - Auto-Crop Margin     |     |
|   |  - Scale (s)        |       |  - L1 Optimization  |       |  - Anti-Border Clamp    |     |
|   +---------------------+       +----------+----------+       +-------------------------+     |
|                                            ^                                                  |
|                                            |                                                  |
|                                 +----------+----------+                                       |
|                                 | Gimbal PTZ Velocity |                                       |
|                                 | Feedforward Sensor  |                                       |
|                                 +---------------------+                                       |
+-----------------------------------------------------------------------------------------------+
```

```mermaid
flowchart LR
    Ht["Frame Transform H(t)"] --> Accum["Accumulate Absolute Path P(t)"]
    PTZ["Gimbal Pan/Tilt Telemetry"] --> FF["Feedforward Bias Gate"]
    FF --> Filt
    Accum --> Filt["Trajectory Smoothing<br/>(Kalman / Gaussian / L1)"]
    Filt --> St["Smoothed Path S(t)"]
    Accum --> Comp["Compute Correction Transform<br/>C(t) = P(t)^(-1) * S(t)"]
    St --> Comp
    Comp --> Bound["Crop Margin & Clamping Validator"]
    Bound --> Warp["Bilinear / Bicubic Warp Engine"]
```

#### 2.3.1 Dual-State Extended Kalman Filter (EKF)
For zero-latency online stabilization, each motion parameter $x \in \{t_x, t_y, \theta, s\}$ is tracked via a discrete constant-acceleration state vector $\mathbf{x}_k = [p_k, v_k, a_k]^T$:
$$\mathbf{x}_{k} = \mathbf{F} \mathbf{x}_{k-1} + \mathbf{w}_{k-1}, \quad \mathbf{z}_k = \mathbf{H} \mathbf{x}_k + \mathbf{v}_k$$
$$\mathbf{F} = \begin{bmatrix} 1 & \Delta t & \frac{1}{2}\Delta t^2 \\ 0 & 1 & \Delta t \\ 0 & 0 & 1 \end{bmatrix}, \quad \mathbf{H} = \begin{bmatrix} 1 & 0 & 0 \end{bmatrix}$$
Process noise covariance $\mathbf{Q}$ is dynamically modulated by gimbal telemetry:
- When PTZ gimbal is stationary: $\mathbf{Q}$ is minimized ($q_a \approx 10^{-4}$), yielding heavy low-pass filtering that cancels camera jitter completely.
- When PTZ gimbal initiates an intentional slew ($\dot{\theta}_{\text{pan}} > \omega_{\text{thresh}}$): $\mathbf{Q}$ is scaled by $1 + \alpha \|\vec{\omega}_{\text{ptz}}\|^2$, allowing the smoothed trajectory to track the operator's command without lag or rubber-banding.

#### 2.3.2 Constrained $L_1$ Optimal Path Smoothing
For recorded streams or short buffering windows ($W \approx 15\text{--}30$ frames), $L_1$ path smoothing solves a convex optimization problem using linear programming:
$$\min_{S} \sum_{t=1}^T \left( \|S_t - P_t\|_1 + \lambda_1 \|S_{t+1} - S_t\|_1 + \lambda_2 \|S_{t+2} - 2S_{t+1} + S_t\|_1 + \lambda_3 \|S_{t+3} - 3S_{t+2} + 3S_{t+1} - S_t\|_1 \right)$$
Subject to maximum crop bounds:
$$-M_{\max} \le S_t - P_t \le M_{\max}$$
The $L_1$ objective promotes sparsity in first, second, and third derivatives, automatically generating transitions composed of:
1. **Static hold** ($\dot{S} = 0$).
2. **Constant-velocity panning** ($\ddot{S} = 0$).
3. **Constant-acceleration slews** ($\dddot{S} = 0$).

#### 2.3.3 High-Rate Gyro/IMU Fusion & Tactical Horizon Leveling
To counteract high-frequency vehicle engine vibration ($>30\text{ Hz}$) that exceeds camera frame rates:
1. **High-Rate Prediction (500 Hz–1 kHz)**: Discrete integration of 3-axis angular rates $\vec{\omega} = [\omega_x, \omega_y, \omega_z]^T$ from onboard IMU / DJI telemetry / MAVLink estimates inter-exposure camera rotation.
2. **Low-Rate Visual Correction (30/60 Hz)**: Pyramidal optical flow estimates update gyro bias drift $\vec{b}_g$.
3. **Tactical Horizon Lock**: By extracting the gravity vector from IMU accelerometers or STANAG 4609 KLV platform roll metadata (`libs/Klv`), the stabilizer cancels all roll rotation ($\theta_{\text{roll}} \to 0$) to maintain a permanently level horizon regardless of aircraft bank angle or vessel wave roll.

#### 2.3.4 Staring Lock (Virtual Tripod) & Target-Centric Stabilization
1. **Staring Lock Mode**: For stationary observation or long-range mast surveillance, the trajectory smoother locks the line-of-sight to the scene origin, suppressing all intentional drift until the cumulative offset reaches $95\%$ of the crop margin, whereupon a smooth sigmoid transition recenters the canvas.
2. **Target-Centric Stabilization**: Interfacing directly with `libs/Tracking`, the reference frame is anchored to a tracked object's bounding box centroid $[x_{\text{tgt}}, y_{\text{tgt}}]^T$. This removes both platform jitter and relative target jitter, keeping the target locked at the center of display.

#### 2.3.5 Look-Ahead Low-Latency Trajectory Smoothing Buffer
When streaming or recording with low-latency buffering ($3\text{--}5$ frames, $\approx 100\text{ ms}$ latency), an asymmetric non-causal trajectory filter processes future frame poses $P_{t+1} \dots P_{t+k}$. This completely avoids crop boundary collisions, eliminates abrupt deceleration jerk, and yields broadcast-grade cinematic motion.

---

### 2.4 Motion Compensation & Frame Synthesis

#### 2.4.1 Inverse Mapping Warp with SIMD Acceleration
To synthesize the stabilized frame $I_{\text{stab}}$, backward mapping is performed:
$$I_{\text{stab}}(x, y) = I_{\text{raw}}(C_t^{-1} \cdot [x, y, 1]^T)$$
where $C_t = P_t^{-1} \cdot S_t$ is the stabilizing transformation.
- Sub-pixel samples are interpolated using clamped bilinear or 4-tap Catmull-Rom bicubic interpolation.
- Inner pixel rendering loops utilize 8-way AVX2 / 4-way NEON SIMD vectorization with fixed-point $8.8$ arithmetic.

#### 2.4.2 Dynamic Margin Auto-Cropping & Boundary Clamping
To prevent black undefined borders when the camera pans away from the stabilized pose:
1. The canvas is zoomed by an adaptive margin factor $\kappa = \frac{1}{1 - 2\mu_{\text{crop}}}$, where $\mu_{\text{crop}} \in [0.03, 0.15]$ (3% to 15% margin).
2. Clamping bounds are continuously enforced on correction transform $C_t$:
   $$|t_x| \le W \cdot \mu_{\text{crop}}, \quad |t_y| \le H \cdot \mu_{\text{crop}}, \quad |\theta| \le \theta_{\max}$$
3. Soft margin damping pushes $S_t$ back toward $P_t$ as displacements approach $90\%$ of the crop limit.

#### 2.4.3 Rolling Shutter Distortion Correction
CMOS image sensors expose scanlines sequentially from top to bottom with readout delay $\tau_{\text{readout}}$:
$$t_{\text{row}}(y) = t_{\text{frame}} + y \cdot \frac{\tau_{\text{readout}}}{H}$$
For high-frequency vibration, each row $y$ undergoes a different transformation $H(y)$. The row-dependent transform is computed via linear interpolation of the inter-frame transform:
$$H(y) = (1 - \alpha_y) H_t + \alpha_y H_{t+1}, \quad \alpha_y = \frac{y}{H}$$
Straightening vertical structures and eliminating the characteristic "jello" wobble.

#### 2.4.4 Unified Lens Distortion Dewarping (Single-Pass Resampling)
Wide-angle lenses exhibit radial and tangential barrel/pincushion distortions ($k_1, k_2, p_1, p_2$). Stabilizing distorted imagery causes barrel curvature to oscillate dynamically. Rather than executing two consecutive resampling passes (dewarp then warp), radial distortion correction is fused directly into the backward mapping kernel:
$$x_n = \frac{x - c_x}{f_x}, \quad y_n = \frac{y - c_y}{f_y}, \quad r^2 = x_n^2 + y_n^2$$
$$x_d = x_n (1 + k_1 r^2 + k_2 r^4) + 2 p_1 x_n y_n + p_2 (r^2 + 2 x_n^2)$$
$$[x_{\text{src}}, y_{\text{src}}, 1]^T = C_t^{-1} \cdot [f_x x_d + c_x, f_y y_d + c_y, 1]^T$$
Synthesizes stabilized, geometrically undistorted video in a single memory read/write cycle.

#### 2.4.5 Temporal Background Mosaic Inpainting (Zero-Crop Border Synthesis)
To eliminate FOV loss from auto-crop zooming, out-of-boundary pixels exposed during camera excursions are reconstructed by inpainting from a persistent, registered temporal background buffer. Pixels entering the frame boundary are sampled from recent valid frames registered into world coordinates, providing full-canvas stabilized video with zero black borders and 0% crop loss.

#### 2.4.6 Multi-Format SIMD Surface Warping (NV12 & RGB24 Support)
Expands beyond 8-bit luma to support direct hardware decoder video surfaces:
1. **NV12 (Hardware Default)**: SIMD AVX2/NEON warping for semi-planar NV12 (full-resolution Y plane + half-resolution interleaved UV plane), eliminating color conversion overhead from GPU/V4L2/MediaFoundation decoders.
2. **RGB24 / RGBA32**: 3-channel and 4-channel SIMD pixel gathering for RGB pipelines.

---

## 3. System Architecture & Module Decomposition

### 3.1 Directory Structure (`libs/Stabilizer`)

```
libs/Stabilizer/
├── CMakeLists.txt
├── StabilizerTypes.h             # Strongly-typed domain value objects
├── FrameView.h                   # Non-owning 2D span, sub-ROI slicing & pixel access
├── FrameBuffer.h                 # 64-byte aligned RAII frame memory container
├── FrameBuffer.cpp
├── StabilizerArena.h             # High-speed zero-fragmentation monotonic scratchpad
├── StabilizerArena.cpp
├── FramePyramid.h                # Zero-heap multi-scale Gaussian image pyramid
├── FramePyramid.cpp
├── FeatureDetector.h             # Shi-Tomasi & FAST with grid bucketing (Phase 2)
├── FeatureDetector.cpp
├── OpticalFlowTracker.h          # Pyramidal Lucas-Kanade with forward-backward pruning (Phase 3)
├── OpticalFlowTracker.cpp
├── PhaseCorrelationEstimator.h   # FFT-based global translation fallback (Phase 8)
├── PhaseCorrelationEstimator.cpp
├── ExclusionMask.h               # Static HUD/reticle & dynamic target exclusion masks (Phase 9)
├── ExclusionMask.cpp
├── MotionModelEstimator.h        # RANSAC/MSAC Affine & Euclidean fitting (Phase 4)
├── MotionModelEstimator.cpp
├── HomographyEstimator.h         # 8-DOF Projective DLT Estimator with 4-pt RANSAC (Phase 10)
├── HomographyEstimator.cpp
├── MeshWarpEngine.h              # Spatially-variant MeshFlow parallax warping (Phase 10)
├── MeshWarpEngine.cpp
├── ImuFusionEngine.h             # High-rate IMU gyro EKF & horizon lock filter (Phase 11)
├── ImuFusionEngine.cpp
├── TrajectoryFilter.h            # EKF, Gaussian FIR, and L1 path smoothing (Phase 5, 12)
├── TrajectoryFilter.cpp
├── InpaintingEngine.h            # Temporal mosaic edge reconstruction (Phase 13)
├── InpaintingEngine.cpp
├── WarpEngine.h                  # SIMD inverse warp, lens dewarp & rolling shutter (Phase 6, 13)
├── WarpEngine.cpp
├── VideoStabilizerEngine.h       # Top-level orchestrator & state machine (Phase 7)
├── VideoStabilizerEngine.cpp
├── SightlineStabilizerBridge.h   # Sightline SLA 0x02/0x9E/0x9F protocol translation (Phase 14)
├── SightlineStabilizerBridge.cpp
├── StabilizerOverlay.h           # Motion vector quiver & diagnostic HUD generator (Phase 14)
├── StabilizerOverlay.cpp
└── tests/
    ├── CMakeLists.txt
    ├── TestStabilizerTypes.cpp   # Phase 1 unit tests
    ├── TestFrameView.cpp         # Phase 1 unit tests
    ├── TestStabilizerArena.cpp   # Phase 1 unit tests
    ├── TestFramePyramid.cpp      # Phase 1 unit tests
    ├── TestFeatureDetector.cpp   # Phase 2 unit tests
    ├── TestOpticalFlowTracker.cpp # Phase 3 unit tests
    ├── TestMotionModelEstimator.cpp # Phase 4 unit tests
    ├── TestTrajectoryFilter.cpp  # Phase 5 unit tests
    ├── TestWarpEngine.cpp        # Phase 6 unit tests
    ├── TestVideoStabilizerEngine.cpp # Phase 7 unit tests
    ├── TestPhaseCorrelationEstimator.cpp # Phase 8 unit tests
    ├── TestExclusionMask.cpp     # Phase 9 unit tests
    ├── TestHomographyEstimator.cpp # Phase 10 unit tests
    ├── TestMeshWarpEngine.cpp    # Phase 10 unit tests
    ├── TestImuFusionEngine.cpp   # Phase 11 unit tests
    ├── TestInpaintingEngine.cpp  # Phase 13 unit tests
    └── TestSightlineStabilizerBridge.cpp # Phase 14 unit tests
```

### 3.2 Strongly-Typed Domain Modeling (Prohibition of Primitive Obsession)

In strict compliance with `domain-modeling.md`, all physical entities are wrapped into zero-cost, invariant-enforcing C++17 value objects:

```cpp
/// @file StabilizerTypes.h
#pragma once

#include <cstddef>
#include <cstdint>

namespace Stabilizer {

/// @brief Strongly-typed image dimension bounds.
struct FrameSize {
    std::uint32_t width{0};
    std::uint32_t height{0};

    [[nodiscard]] constexpr bool isValid() const noexcept {
        return (width >= 64U) && (width <= 7680U) &&
               (height >= 64U) && (height <= 4320U);
    }
};

/// @brief Sub-pixel 2D coordinate.
struct SubPixelPoint {
    float x{0.0F};
    float y{0.0F};
};

/// @brief Inter-frame 2D motion vector.
struct MotionVector {
    float dx{0.0F};
    float dy{0.0F};
};

/// @brief Strongly-typed fractional crop ratio [0.0 .. 0.30].
class CropRatio {
public:
    constexpr CropRatio() noexcept = default;
    explicit constexpr CropRatio(float val) noexcept 
        : value_{(val < 0.0F) ? 0.0F : ((val > 0.30F) ? 0.30F : val)} {}

    [[nodiscard]] constexpr float value() const noexcept { return value_; }

private:
    float value_{0.05F}; // Default 5% border margin
};

/// @brief Operational stabilization mode selector.
enum class StabilizationMode : std::uint8_t {
    Disabled = 0U,
    TranslationOnly,      ///< 2-DOF (X, Y)
    TranslationRotation,  ///< 3-DOF Rigid (X, Y, Theta)
    Similarity,           ///< 4-DOF (X, Y, Theta, Scale)
    Affine,               ///< 6-DOF (X, Y, Theta, Scale, Shear)
    ProjectiveHomography, ///< 8-DOF Projective Perspective (Phase 10)
    SpatiallyVariantMesh, ///< MeshFlow multi-plane parallax (Phase 10)
    RollingShutterComp    ///< Affine + Row-wise scanline interpolation
};

/// @brief Tactical mission operational profile.
enum class StabilizationProfile : std::uint8_t {
    Standard = 0U,        ///< Standard cinematic smoothing
    HorizonLock,          ///< Tactical roll leveling to gravitational horizon (Phase 11)
    StaringLock,          ///< Virtual tripod / stationary observation (Phase 12)
    TargetCentric         ///< Target-anchored stabilization tracking (Phase 12)
};

/// @brief Affine transform 2x3 matrix: [a11 a12 tx; a21 a22 ty]
struct AffineTransform2D {
    float a11{1.0F};
    float a12{0.0F};
    float tx{0.0F};
    float a21{0.0F};
    float a22{1.0F};
    float ty{0.0F};

    [[nodiscard]] static constexpr AffineTransform2D identity() noexcept {
        return {1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F};
    }
};

/// @brief 3x3 Projective Homography Matrix (8-DOF).
struct HomographyTransform2D {
    float h11{1.0F}; float h12{0.0F}; float h13{0.0F};
    float h21{0.0F}; float h22{1.0F}; float h23{0.0F};
    float h31{0.0F}; float h32{0.0F}; float h33{1.0F};

    [[nodiscard]] static constexpr HomographyTransform2D identity() noexcept {
        return {1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    }
};

/// @brief High-rate 3-axis IMU angular velocity and acceleration telemetry sample.
struct ImuTelemetrySample {
    float gyroXRadSec{0.0F};
    float gyroYRadSec{0.0F};
    float gyroZRadSec{0.0F};
    float accelXG{0.0F};
    float accelYG{0.0F};
    float accelZG{1.0F};
    std::uint64_t timestampNs{0U};
};

} // namespace Stabilizer
```

---

## 4. Phased Implementation Roadmap

```
+---------------------------------------------------------------------------------------------------------------+
|                                         PHASED IMPLEMENTATION TIMELINE                                        |
|                                                                                                               |
|  [COMPLETED] Phase 1: Domain Types, Frame Buffers & Image Pyramids (Zero-Heap Infrastructure)                 |
|  [COMPLETED] Phase 2: Shi-Tomasi Feature Detector & Grid Bucketing (Uniform Spatial Distribution)             |
|  [COMPLETED] Phase 3: Multi-Scale Pyramidal Lucas-Kanade Optical Flow Tracker (Bidirectional Pruning)         |
|  [COMPLETED] Phase 4: RANSAC / MSAC Parametric Motion Model Estimator (Affine & Euclidean Fitting)            |
|  [COMPLETED] Phase 5: Trajectory Smoothing & PTZ Feedforward Sensor Fusion (EKF / L1 Path Smoother)          |
|  [COMPLETED] Phase 6: SIMD Warp Synthesis, Dynamic Margin Cropping & Rolling Shutter (Bilinear/Bicubic)       |
|  [COMPLETED] Phase 7: End-to-End Orchestrator, Conformance Verification & Benchmarking (Zero-Copy Engine)     |
|  [PLANNED]   Phase 8: 2D Phase Correlation Fallback & Thermal Low-Contrast Imagery (FFT-Based Translation)   |
|  [PLANNED]   Phase 9: Dynamic Target & Static Reticle Exclusion Masking (Sightline / libs/Tracking Fusion)   |
|  [PLANNED]   Phase 10: 8-DOF Projective Homography & Spatially-Variant Parallax Mesh (MeshFlow Grid)          |
|  [PLANNED]   Phase 11: High-Rate IMU/Gyro Extended Kalman Filter & Tactical Horizon Lock (STANAG 4609 KLV)    |
|  [PLANNED]   Phase 12: Mission Profiles: Staring Lock (Virtual Tripod), Target-Centric & Look-Ahead Buffer    |
|  [PLANNED]   Phase 13: Unified Single-Pass Lens Dewarping, Temporal Mosaic Inpainting & Native NV12 SIMD     |
|  [PLANNED]   Phase 14: Sightline SLA Protocol Bridge & Motion Quiver Vector HUD Overlay                       |
+---------------------------------------------------------------------------------------------------------------+
```

```mermaid
flowchart TD
    subgraph CoreFoundation["Core Stabilization Foundation (COMPLETED)"]
        P1["Phase 1: Domain Types & Memory Infrastructure<br/><b>[COMPLETED]</b>"] --> P2["Phase 2: Feature Detection & Grid Bucketing<br/><b>[COMPLETED]</b>"]
        P2 --> P3["Phase 3: Pyramidal LK Optical Flow<br/><b>[COMPLETED]</b>"]
        P3 --> P4["Phase 4: Robust Motion Model Estimator<br/><b>[COMPLETED]</b>"]
        P4 --> P5["Phase 5: Trajectory Smoothing & PTZ Fusion<br/><b>[COMPLETED]</b>"]
        P5 --> P6["Phase 6: SIMD Warp Engine & Rolling Shutter<br/><b>[COMPLETED]</b>"]
        P6 --> P7["Phase 7: Top-Level Engine & Benchmarking<br/><b>[COMPLETED]</b>"]
    end

    subgraph Extensions["Tactical & Advanced Extensions (PLANNED)"]
        P7 --> P8["Phase 8: 2D Phase Correlation (Thermal/LWIR)"]
        P7 --> P9["Phase 9: Dynamic Target & HUD Masking"]
        P8 & P9 --> P10["Phase 10: 8-DOF Homography & Parallax Mesh"]
        P7 --> P11["Phase 11: High-Rate IMU Fusion & Horizon Lock"]
        P11 --> P12["Phase 12: Staring Lock & Target-Centric Modes"]
        P10 & P12 --> P13["Phase 13: Lens Dewarping, Mosaic & NV12"]
        P7 --> P14["Phase 14: Sightline SLA Bridge & Vector HUD"]
    end
```

### 4.1 Phase Execution & Status Matrix

| Phase | Description | Status | Primary Artifacts | Detailed Plan |
| :--- | :--- | :--- | :--- | :--- |
| **Phase 1** | Domain Types, Frame Buffers & Image Pyramids | **COMPLETED** | `StabilizerTypes.h`, `FrameView.h`, `FrameBuffer.h/.cpp`, `StabilizerArena.h/.cpp`, `FramePyramid.h/.cpp` | [stabilizer_phase1_plan.md](stabilizer_phase1_plan.md) |
| **Phase 2** | Feature Detection & Grid Bucketing | **COMPLETED** | `FeatureDetector.h`, `FeatureDetector.cpp`, `TestFeatureDetector.cpp` | [stabilizer_phase2_plan.md](stabilizer_phase2_plan.md) |
| **Phase 3** | Pyramidal Lucas-Kanade Optical Flow Tracker | **COMPLETED** | `OpticalFlowTracker.h`, `OpticalFlowTracker.cpp`, `TestOpticalFlowTracker.cpp` | [stabilizer_phase3_plan.md](stabilizer_phase3_plan.md) |
| **Phase 4** | Robust Parametric Motion Model Estimator | **COMPLETED** | `MotionModelEstimator.h`, `MotionModelEstimator.cpp`, `TestMotionModelEstimator.cpp` | [stabilizer_phase4_plan.md](stabilizer_phase4_plan.md) |
| **Phase 5** | Trajectory Smoothing & PTZ Telemetry Fusion | **COMPLETED** | `TrajectoryFilter.h`, `TrajectoryFilter.cpp`, `TestTrajectoryFilter.cpp` | [stabilizer_phase5_plan.md](stabilizer_phase5_plan.md) |
| **Phase 6** | SIMD Warp Synthesis & Rolling Shutter Compensation | **COMPLETED** | `WarpEngine.h`, `WarpEngine.cpp`, `TestWarpEngine.cpp` | [stabilizer_phase6_plan.md](stabilizer_phase6_plan.md) |
| **Phase 7** | Top-Level Engine Orchestration & Benchmark Harness | **COMPLETED** | `VideoStabilizerEngine.h`, `VideoStabilizerEngine.cpp`, `TestVideoStabilizerEngine.cpp` | [stabilizer_phase7_plan.md](stabilizer_phase7_plan.md) |
| **Phase 8** | 2D Phase Correlation Fallback (Thermal / Infrared) | **PLANNED** | `PhaseCorrelationEstimator.h/.cpp`, `TestPhaseCorrelationEstimator.cpp` | Planned Specification |
| **Phase 9** | Dynamic Target & Static Reticle Exclusion Masking | **PLANNED** | `ExclusionMask.h/.cpp`, `TestExclusionMask.cpp` | Planned Specification |
| **Phase 10** | 8-DOF Projective Homography & Parallax MeshFlow | **PLANNED** | `HomographyEstimator.h/.cpp`, `MeshWarpEngine.h/.cpp`, `TestHomographyEstimator.cpp` | Planned Specification |
| **Phase 11** | High-Rate IMU/Gyro Fusion & Tactical Horizon Lock | **PLANNED** | `ImuFusionEngine.h/.cpp`, `TestImuFusionEngine.cpp` | Planned Specification |
| **Phase 12** | Advanced Mission Profiles (Staring, Target-Centric, Look-Ahead) | **PLANNED** | `TrajectoryFilter.h/.cpp`, `TestTrajectoryFilter.cpp` | Planned Specification |
| **Phase 13** | Unified Lens Dewarping, Temporal Mosaic & NV12 SIMD | **PLANNED** | `InpaintingEngine.h/.cpp`, `WarpEngine.h/.cpp`, `TestInpaintingEngine.cpp` | Planned Specification |
| **Phase 14** | Sightline SLA Protocol Bridge & Motion Quiver HUD Overlay | **PLANNED** | `SightlineStabilizerBridge.h/.cpp`, `StabilizerOverlay.h/.cpp`, `TestSightlineStabilizerBridge.cpp` | Planned Specification |

---

### Phase 1: Domain Types, Frame Buffers & Image Pyramids `[COMPLETED]`
- **Objective**: Establish `StabilizerTypes.h`, zero-allocation memory workspace, and 3-level Gaussian image pyramid downsampler.
- **Delivered Classes**:
  - `StabilizerTypes.h`: Strongly-typed value objects (`FrameDimensions`, `SubPixelPoint`, `MotionVector`, `CropRatio`, `SmoothingFactor`, `AffineTransform2D`, `StabilizerStatus`, `PixelFormat`, `StabilizationMode`).
  - `FrameView.h`: Non-owning 2D span, sub-ROI slicing, clamped pixel access, zero heap allocations.
  - `FrameBuffer.h` / `FrameBuffer.cpp`: 64-byte aligned RAII frame memory container.
  - `StabilizerArena.h` / `StabilizerArena.cpp`: High-speed zero-fragmentation monotonic scratchpad arena.
  - `FramePyramid.h` / `FramePyramid.cpp`: 3-level Gaussian image pyramid downsampler ($L_0, L_1, L_2$) using fixed-point SIMD binomial kernel downsampling.
- **Verification**: Complete GoogleTest suite (`TestStabilizerTypes.cpp`, `TestFrameView.cpp`, `TestStabilizerArena.cpp`, `TestFramePyramid.cpp`).
- **Detailed Specification**: [stabilizer_phase1_plan.md](stabilizer_phase1_plan.md).

### Phase 2: Feature Detection & Grid Bucketing `[COMPLETED]`
- **Objective**: High-speed, deterministic corner and feature detection with uniform spatial dispersion.
- **Delivered Classes**:
  - `FeatureDetector.h` / `FeatureDetector.cpp`:
    - Strongly-typed `FeaturePoint`, `QualityThreshold`, `GridBucketingConfig`, and `FeatureDetectorConfig`.
    - Sobel spatial gradient convolution with fixed-point integer arithmetic.
    - Shi-Tomasi minimum eigenvalue corner score calculation:
      $$\lambda_{\min} = \frac{1}{2} \left( (I_{xx} + I_{yy}) - \sqrt{(I_{xx} - I_{yy})^2 + 4 I_{xy}^2} \right)$$
    - Configurable $K \times M$ spatial grid bucketing enforcing at most $N_{\text{cell}}$ features per cell with round-robin rank extraction, eliminating foreground clustering.
    - $3 \times 3$ Non-Maximum Suppression (NMS) with directional tie-breaking and sub-pixel parabolic peak refinement.
    - Zero dynamic allocation runtime execution powered by `StabilizerArena` (`CERT ERR55-CPP`).
- **Verification**: Complete GoogleTest suite (`TestFeatureDetector.cpp`: configuration validation, flat image rejection, synthetic corner detection, border margin exclusion, grid bucketing caps, minimum distance enforcement, and arena exhaustion protection).
- **Detailed Specification**: [stabilizer_phase2_plan.md](stabilizer_phase2_plan.md).

### Phase 3: Pyramidal Lucas-Kanade Optical Flow Tracker `[COMPLETED]`
- **Objective**: Sub-pixel feature tracking with bidirectional forward-backward error validation.
- **Delivered Classes**:
  - `OpticalFlowTracker.h` / `OpticalFlowTracker.cpp`:
    - Strongly-typed `TrackingStatus` (`Tracked`, `LostOutOfBounds`, `LostIllConditioned`, `LostConvergence`, `LostForwardBackward`), `FlowWindowSize`, `TrackedCorrespondence`, and `OpticalFlowConfig`.
    - Sub-pixel bilinear sampling on 8-bit luma frame views.
    - Analytical spatial gradient tensor accumulation, conditioning check, and minimum eigenvalue verification ($\lambda_{\min}(G) \ge \tau_{\text{eigen}}$).
    - Multi-scale coarse-to-fine propagation across `FramePyramid` levels ($L_2 \to L_1 \to L_0$).
    - Iterative Newton-Raphson optical flow solver with sub-pixel convergence criterion.
    - Bidirectional forward-backward loop-closure error check ($e_{\text{fb}} = \|p - p''\|_2 \le \tau_{\text{fb}}$) pruning occlusions and aperture errors.
    - Zero dynamic allocation runtime execution powered by stack workspace and `StabilizerArena` (`CERT ERR55-CPP`).
- **Verification**: Complete GoogleTest suite (`TestOpticalFlowTracker.cpp`: config validation, window bounds, identity zero motion, sub-pixel translation recovery, large displacement pyramid tracking, ill-conditioned patch rejection, boundary exclusion, forward-backward occlusion pruning, and arena exhaustion protection).
- **Detailed Specification**: [stabilizer_phase3_plan.md](stabilizer_phase3_plan.md).

### Phase 4: Robust Parametric Motion Model Estimator `[COMPLETED]`
- **Objective**: Fit 2-DOF, 3-DOF, and 6-DOF motion models with high outlier rejection.
- **Key Classes**: `MotionModelEstimator`.
- **Algorithms**: MSAC outlier scoring, singular value decomposition (SVD) closed-form affine solver, collinearity and degenerate configuration detection.
- **Tests**: `TestMotionModelEstimator.cpp` verifying model fitting with up to 60% synthetic outlier contamination.
- **Detailed Specification**: [stabilizer_phase4_plan.md](stabilizer_phase4_plan.md).

### Phase 5: Trajectory Smoothing & PTZ Telemetry Fusion `[COMPLETED]`
- **Objective**: Separate unwanted jitter from intentional camera sweeps.
- **Key Classes**: `TrajectoryFilter`, `KalmanPathSmoother`, `GaussianPathSmoother`, `L1PathSmoother`.
- **Algorithms**: Discrete constant-acceleration Extended Kalman Filter with adaptive $\mathbf{Q}$ matrix, PTZ velocity feedforward gate (`PelcoDCore` pan/tilt rate ingestion), $L_1$ total-variation proximal smoothing.
- **Tests**: `TestTrajectoryFilter.cpp` testing synthetic sinusoidal vibration rejection while tracking constant-velocity steps.
- **Detailed Specification**: [stabilizer_phase5_plan.md](stabilizer_phase5_plan.md).

### Phase 6: SIMD Warp Synthesis & Rolling Shutter Compensation `[COMPLETED]`
- **Objective**: High-throughput image reconstruction and border handling.
- **Key Classes**: `WarpEngine`.
- **Algorithms**: Backward-mapping bilinear and bicubic interpolation with AVX2/NEON optimizations, fixed-point 16.16 DDA scanline stepping, dynamic margin auto-cropping, row-dependent rolling shutter transformation interpolation.
- **Tests**: `TestWarpEngine.cpp` validating geometric fidelity, SIMD vs. scalar equivalence, edge clamping, rolling shutter rectification, and zero memory overwrites.
- **Detailed Specification**: [stabilizer_phase6_plan.md](stabilizer_phase6_plan.md).

### Phase 7: Top-Level Engine Orchestration & Benchmark Harness `[COMPLETED]`
- **Objective**: Integrate all subsystems into `VideoStabilizerEngine`, provide zero-copy frame pipeline, and verify real-time performance.
- **Key Classes**: `VideoStabilizerEngine`.
- **Deliverables**: End-to-end processing pipeline, integration with `libs/VideoFilters/GeometricFilters.h`, performance benchmarks confirming $\le 4.5$ ms per 1080p frame on x86_64/ARM64.
- **Tests**: `TestVideoStabilizerEngine.cpp` verifying complete streaming workflows.
- **Detailed Specification**: [stabilizer_phase7_plan.md](stabilizer_phase7_plan.md).

### Phase 8: 2D Phase Correlation Fallback & Thermal Low-Contrast Imagery `[PLANNED]`
- **Objective**: Robust frequency-domain global translation estimation when Shi-Tomasi feature count drops below threshold in low-texture or thermal scenes.
- **Key Classes**: `PhaseCorrelationEstimator`.
- **Algorithms**:
  - Integration with `libs/Math/PhaseCorrelation.h`.
  - Normalized cross-power spectrum surface computation via 2D FFT:
    $$\mathcal{R}(u, v) = \frac{F_t(u, v) \cdot F_{t-1}^*(u, v)}{|F_t(u, v) \cdot F_{t-1}^*(u, v)|}$$
  - 2D parabolic sub-pixel peak interpolation extracting translation vector $(\Delta x, \Delta y)$ with $< 0.05$ pixel error.
  - Zero dynamic heap allocation using `StabilizerArena` scratchpad workspace.
  - Automatic fallback policy transition inside `VideoStabilizerEngine`.
- **Verification**: `TestPhaseCorrelationEstimator.cpp` testing synthetic translations on featureless gradient fields, synthetic low-contrast thermal noise, sub-pixel accuracy, and automatic engine switching.
- **Detailed Specification**: Planned Specification (Execution Roadmap).

### Phase 9: Dynamic Target & Static Reticle Exclusion Masking `[PLANNED]`
- **Objective**: Eliminate motion model estimation contamination caused by static HUD/reticle overlays and maneuvering tracked foreground targets.
- **Key Classes**: `ExclusionMask`.
- **Algorithms**:
  - Non-owning rectangular and bitmask spatial exclusion zones.
  - Sightline SLA registration ignore margins (`left, right, top, bottom`).
  - Real-time ingestion of active bounding boxes from `libs/Tracking` (`TargetTrack`), culling corner candidates located within target bounds before optical flow tracking.
  - Fast coordinate point-in-rect rejection on stack arrays ($O(1)$ per feature point).
- **Verification**: `TestExclusionMask.cpp` verifying feature candidate rejection within masked rectangles, border margin exclusion, tracking immunity to moving synthetic targets, and zero heap allocation.
- **Detailed Specification**: Planned Specification (Execution Roadmap).

### Phase 10: 8-DOF Projective Homography & Spatially-Variant Parallax Mesh `[PLANNED]`
- **Objective**: Extend motion modeling to 8-DOF perspective keystone transformations and spatially-regularized MeshFlow parallax warping.
- **Key Classes**: `HomographyEstimator`, `MeshWarpEngine`.
- **Algorithms**:
  - Normalized 4-point Direct Linear Transformation (DLT) for 8-DOF projective homography:
    $$\begin{bmatrix} x' \\ y' \\ 1 \end{bmatrix} \sim \begin{bmatrix} h_{11} & h_{12} & h_{13} \\ h_{21} & h_{22} & h_{23} \\ h_{31} & h_{32} & 1 \end{bmatrix} \begin{bmatrix} x \\ y \\ 1 \end{bmatrix}$$
  - $M \times N$ control grid vertex optimization minimizing motion data fidelity and Laplacian spatial grid smoothness for 3D multi-plane parallax scenes.
  - Fixed-point SIMD mesh rendering on stack memory.
- **Verification**: `TestHomographyEstimator.cpp` and `TestMeshWarpEngine.cpp` testing synthetic perspective tilts, multi-depth parallax synthetic sequences, SVD numerical stability, and degenerate point checks.
- **Detailed Specification**: Planned Specification (Execution Roadmap).

### Phase 11: High-Rate IMU/Gyroscope Fusion & Tactical Horizon Lock `[PLANNED]`
- **Objective**: Sub-frame predictive stabilization and gravity-aligned horizon leveling via high-rate inertial sensor telemetry.
- **Key Classes**: `ImuFusionEngine`.
- **Algorithms**:
  - High-rate (500 Hz–1 kHz) strapdown gyro angular velocity integration ($\vec{\omega} = [\omega_x, \omega_y, \omega_z]^T$) predicting inter-frame camera rotational displacement before frame arrival.
  - Low-rate visual optical flow measurement updates correcting gyroscope rate bias $\vec{b}_g$.
  - Horizon Lock mode: Ingests gravitational vector from IMU accelerometers or STANAG 4609 KLV platform roll angle (`libs/Klv`), enforcing zero roll deviation ($\theta_{\text{roll}} = 0$) while preserving commanded pan/tilt.
- **Verification**: `TestImuFusionEngine.cpp` testing synthetic $50\text{ Hz}$ airframe vibration cancellation, gyro bias drift convergence, KLV telemetry horizon alignment, and numerical covariance stability.
- **Detailed Specification**: Planned Specification (Execution Roadmap).

### Phase 12: Advanced Mission Profiles (Staring Lock, Target-Centric, Look-Ahead) `[PLANNED]`
- **Objective**: Provide specialized tactical operational modes for observation, target engagement, and broadcast recording.
- **Key Classes**: `TrajectoryFilter` extensions, `VideoStabilizerEngine`.
- **Algorithms**:
  - **Staring Lock (Virtual Tripod)**: Infinite-decay pose anchor holding line-of-sight fixed until reaching $95\%$ margin threshold, followed by smooth sigmoid recentering.
  - **Target-Centric Stabilization**: Stabilizes video coordinates relative to target track centroid $[x_{\text{tgt}}, y_{\text{tgt}}]^T$, eliminating platform jitter and maintaining target lock in the screen center.
  - **Look-Ahead Trajectory Smoothing**: $3\text{--}5$ frame non-causal ring buffer for zero-overshoot cinematic $L_1$ smoothing on recorded or streaming pipelines.
- **Verification**: `TestTrajectoryFilter.cpp` testing synthetic stationary holding, target track tracking jitter isolation, look-ahead latency guarantees, and boundary clamping.
- **Detailed Specification**: Planned Specification (Execution Roadmap).

### Phase 13: Unified Lens Distortion Dewarping, Temporal Mosaic & Native NV12 SIMD `[PLANNED]`
- **Objective**: Optimize output optical quality, eliminate crop loss, and support hardware video decoder formats directly.
- **Key Classes**: `InpaintingEngine`, `WarpEngine` extensions.
- **Algorithms**:
  - Single-pass fused lens dewarping (Brown-Conrady radial $k_1, k_2$ and tangential $p_1, p_2$ polynomial undistortion) inside SIMD backward mapping.
  - Registered temporal background mosaic buffer inpainting missing border pixels to achieve 0% field-of-view crop loss.
  - Native AVX2/NEON SIMD warp kernels for semi-planar NV12 and packed RGB24 pixel layouts.
- **Verification**: `TestInpaintingEngine.cpp` and `TestWarpEngine.cpp` validating distortion correction grid geometry, NV12 bit-exactness against scalar reference, and mosaic edge seamlessness.
- **Detailed Specification**: Planned Specification (Execution Roadmap).

### Phase 14: Sightline SLA Protocol Bridge & Motion Quiver HUD Overlay `[PLANNED]`
- **Objective**: Seamless integration with hardware Sightline SLA protocols and real-time diagnostic operator visualization.
- **Key Classes**: `SightlineStabilizerBridge`, `StabilizerOverlay`.
- **Algorithms**:
  - Bidirectional translation between Sightline SLA messages (`MsgSetStabilizationParameters` [0x02], `MsgSetRegistrationParameters` [0x9E], `MsgSetStabilizationBias` [0x9F], `MsgResetStabilizationParameters` [0x04]) and `VideoStabilizerEngine` configurations.
  - Motion quiver vector field generator rendering inlier vectors (green), outlier vectors (red), crop safety boundaries, and live vibration frequency spectrum on 8-bit or 32-bit overlays.
- **Verification**: `TestSightlineStabilizerBridge.cpp` verifying SLA message parsing/generation round-trip and overlay coordinate alignment.
- **Detailed Specification**: Planned Specification (Execution Roadmap).

---

## 5. Verification Checklist & Compliance Matrix

| Standard / Requirement | Compliance Mechanism |
| :--- | :--- |
| **MISRA C++:2023 Rule 7.0.6** | Explicit `static_cast` for all integer conversions; strict `<cstdint>` usage. |
| **SEI CERT INT31-C** | Range-verified integer conversions with bounds checking. |
| **SEI CERT ERR55-CPP** | All frame processing functions marked `noexcept` with zero dynamic allocations. |
| **SEI CERT STR37-C** | ASCII-only token handling without `<cctype>` negative `char` UB. |
| **IWYU & `<cstdint>`** | Direct header inclusion for every type; types scoped with `std::`. |
| **Zero Qt Dependency** | Pure standard C++17; `#include <Q...>` strictly prohibited. |
| **Relative Documentation Paths**| All markdown paths strictly relative to document root. |
| **Function Name Length** | All identifiers strictly $< 30$ characters. |

