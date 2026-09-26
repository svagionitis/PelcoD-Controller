# New PayloadHal Features & Roadmap

### Executive Overview

The [PayloadHal](../libs/PayloadHal/PayloadHal.h) library provides a unified Hardware Abstraction Layer for multi-sensor surveillance and tactical payload stations. It coordinates:
- [IPanTiltUnit](../libs/PayloadHal/IPanTiltUnit.h): Pan-tilt heads, pedestals, and stabilized gimbals.
- [ICameraPayload](../libs/PayloadHal/ICameraPayload.h): Daylight visible and thermal LWIR/MWIR optical sensors.
- [ILaserRangeFinder](../libs/PayloadHal/ILaserRangeFinder.h): Tactical pulsed laser rangefinders with safety interlocks.
- [IPayload](../libs/PayloadHal/IPayload.h): Aggregate composite station binding all subsystems with geodetic target projection.
- [GeoreferenceUtils](../libs/PayloadHal/GeoreferenceUtils.h): Slant range projection, ground intersection, and look-angle computation.
- [PayloadFactory](../libs/PayloadHal/PayloadFactory.h): Instantiation engine for physical and simulated stations.

---

### Key Shortcomings in the Original Implementation (Status)

1. **Missing Synchronous State & Telemetry Getters across Interfaces** *(Resolved)*:
   - Added `currentTelemetry()` to [IPanTiltUnit](../libs/PayloadHal/IPanTiltUnit.h) and [ICameraPayload](../libs/PayloadHal/ICameraPayload.h), and `lastMeasurement()` to [ILaserRangeFinder](../libs/PayloadHal/ILaserRangeFinder.h).
   - Fixed [PelcoDCompositePayload::calculateTargetCoordinates](../libs/PayloadHal/PayloadFactory.cpp) to query live PTU pan/tilt angles directly.
2. **Adapter Ecosystem & URI Resolution** *(Resolved)*:
   - Implemented [OnvifPayloadAdapter](../libs/PayloadHal/adapters/OnvifPayloadAdapter.h) and [PelcoDViscaCompositePayload](../libs/PayloadHal/adapters/PelcoDViscaCompositePayload.h).
   - Implemented physical serial/stream LRF hardware driver [SerialLrfAdapter](../libs/PayloadHal/adapters/SerialLrfAdapter.h) with multi-protocol support (NMEA, ASCII, Binary), eye-safety interlocks, and auto-disarm watchdog timer.
   - Upgraded [PayloadFactory::createFromUri](../libs/PayloadHal/PayloadFactory.cpp) to resolve `sim://`, `onvif://`, `pelcod://`, `serial://`, `tcp://`, `udp://`, `fujinon://`, `pelcod-visca://`, and `lrf://` with streaming query parameters and `?lrf=` composite parameter binding.

---

### What Is Missing and Can Be Added

#### 1. Core Interface & Telemetry Enhancements
- **Synchronous Telemetry Access** *(Completed)*:
  - Added `[[nodiscard]] virtual GimbalTelemetry currentTelemetry() const = 0;` to [IPanTiltUnit](../libs/PayloadHal/IPanTiltUnit.h).
  - Added `[[nodiscard]] virtual CameraTelemetry currentTelemetry() const = 0;` to [ICameraPayload](../libs/PayloadHal/ICameraPayload.h).
  - Added `[[nodiscard]] virtual std::optional<LrfTargetMeasurement> lastMeasurement() const = 0;` to [ILaserRangeFinder](../libs/PayloadHal/ILaserRangeFinder.h).
  - Fixed `PelcoDCompositePayload::calculateTargetCoordinates` to query live PTU pan and tilt angles synchronously instead of hardcoded defaults.
- **3-Axis Gimbal Support (Roll Axis & Horizon Leveling)** *(Completed)*:
  - Added 3-axis attitude data structures (`GimbalAttitude3D`, `GimbalAxisCapabilities`, `StabilizationMode::HorizonLevel`) and expanded [GimbalTelemetry](../libs/PayloadHal/PayloadTypes.h) with `rollAngleDeg`, `rollRateDegPerSec`, and `isHorizonLeveled`.
  - Added 3-axis roll motion, limits, and horizon leveling control methods to [IPanTiltUnit](../libs/PayloadHal/IPanTiltUnit.h) with backward-compatible defaults.
  - Implemented analytical horizon counter-roll kinematics (`computeLevelingRoll`) and roll-aware sensor frustum rotation in [GeoreferenceUtils](../libs/PayloadHal/GeoreferenceUtils.h).
  - Upgraded [SimulatedPayload](../libs/PayloadHal/sim/SimulatedPayload.h) with 3-axis gimbal dynamics, $\pm 60^\circ$ physical roll limits, and closed-loop horizon leveling under platform bank/pitch angles.
  - Integrated roll telemetry into MISB ST 0601 Tag 20 (`SensorRelativeRoll`) in [PayloadKlvGenerator](../libs/PayloadHal/PayloadKlvGenerator.h) and temporal roll interpolation in [CameraStreamBinder](../libs/PayloadHal/CameraStreamBinder.h).
- **Continuous Focus & Exposure / Iris Controls** *(Completed)*:
  - Added `focusContinuous(float velocity)`, `focusStop()`, and `triggerOnePushFocus()` to [ICameraPayload](../libs/PayloadHal/ICameraPayload.h) for motorized manual and one-push focus slewing.
  - Added `setIrisAuto(bool)`, `setIrisNormalized(double)`, `irisContinuous(float velocity)`, and `irisStop()` to [ICameraPayload](../libs/PayloadHal/ICameraPayload.h) for exposure and aperture adjustment across optical sensors.
- **Video Stream Binding & Frame Synchronization** *(Completed)*:
  - Added [VideoStreamTypes.h](../libs/PayloadHal/VideoStreamTypes.h) with `VideoStreamProfile` (Primary, Secondary, Thermal, Snapshot), `StreamTransportProtocol` (RTSP, V4L2, DirectShow, UDP/MPEG-TS, WebRTC, Simulated), and `VideoStreamDescriptor`.
  - Added video streaming methods (`videoStreamUri()`, `setVideoStreamUri()`, `availableStreams()`, `streamDescriptor()`) to [ICameraPayload](../libs/PayloadHal/ICameraPayload.h) and convenience shortcuts to [IPayload](../libs/PayloadHal/IPayload.h).
  - Implemented streaming across all camera adapters (`SimulatedPayload`, `OnvifCameraAdapter`, `PelcoDFujinonCameraAdapter`, `ViscaSonyCameraAdapter`, `PelcoDCameraUnit`) and query parameter parsing (`?video=`, `?substream=`, `?thermal=`, `?snapshot=`) in [PayloadFactory::createFromUri](../libs/PayloadHal/PayloadFactory.cpp).
  - Implemented [CameraStreamBinder](../libs/PayloadHal/CameraStreamBinder.h) for pairing raw/decoded video frames with live camera optics, gimbal orientation, and ground georeferencing/DEM projections.

#### 2. Tactical & Subsystem Additions
- **Laser Pointer / Illuminator Subsystem (`ILaserIlluminator`)** *(Completed)*:
  - Implemented [ILaserIlluminator](../libs/PayloadHal/ILaserIlluminator.h) with dual-action safety interlock arming, continuous/pulsed/strobe firing modes, adjustable duty cycles/frequencies, and optical power percentage controls.
  - Fully implemented in [SimulatedPayload](../libs/PayloadHal/sim/SimulatedPayload.h) and verified with unit tests in [TestLaserIlluminator.cpp](../libs/PayloadHal/tests/TestLaserIlluminator.cpp).
- **Ground Sensor Footprint (Frustum) Georeferencing** *(Completed)*:
  - Implemented `computeFrustumCorners(...)` in [GeoreferenceUtils](../libs/PayloadHal/GeoreferenceUtils.h) projecting 4-corner ground polygons on the WGS-84 ellipsoid and DEM terrain meshes.
  - Bridges camera HFOV/VFOV, gimbal azimuth, elevation, and roll directly with [TacticalOverlay](../libs/Mapping/TacticalOverlay.h) in `libs/Mapping`.
- **Digital Elevation Model (DEM) Ray Intersection** *(Completed)*:
  - Added [IDemProvider](../libs/PayloadHal/IDemProvider.h), [GridDemProvider](../libs/PayloadHal/GridDemProvider.h), and [DemRayCaster](../libs/PayloadHal/DemRayCaster.h) implementing 2-phase numerical ray-marching with Illinois secant root refinement and foreground occlusion handling.
  - Upgraded [GeoreferenceUtils](../libs/PayloadHal/GeoreferenceUtils.h), [IPayload](../libs/PayloadHal/IPayload.h), and [PayloadKlvGenerator](../libs/PayloadHal/PayloadKlvGenerator.h) to support high-fidelity terrain ray intersection.

#### 3. New Adapters & Factory URIs
- **ONVIF Client Adapter (`OnvifPayloadAdapter`)** *(Completed)*:
  - Implemented [OnvifPayloadAdapter](../libs/PayloadHal/adapters/OnvifPayloadAdapter.h) mapping `libs/Onvif` ([OnvifClient](../libs/Onvif/OnvifClient.h)) into [IPanTiltUnit](../libs/PayloadHal/IPanTiltUnit.h) and [ICameraPayload](../libs/PayloadHal/ICameraPayload.h), enabling any Profile S/T/G network camera to be driven by `PayloadHal`.
- **Sony FCB + Pelco-D Composite Adapter** *(Completed)*:
  - Implemented [PelcoDViscaCompositePayload](../libs/PayloadHal/adapters/PelcoDViscaCompositePayload.h) combining [PelcoDPtzAdapter](../libs/PayloadHal/adapters/PelcoDPtzAdapter.h) and [ViscaSonyAdapter](../libs/PayloadHal/adapters/ViscaSonyAdapter.h) in [PayloadFactory](../libs/PayloadHal/PayloadFactory.h).
- **Extended URI Schemes in `PayloadFactory::createFromUri`** *(Completed)*:
  - Supported `sim://`, `onvif://user:pass@host:port`, `pelcod://host:port?addr=1`, `serial:///dev/ttyUSB0?baud=9600&protocol=pelcod`, `tcp://`, `udp://`, `fujinon://`, and `pelcod-visca://` with streaming query parameters.
- **Physical Serial LRF Driver (`SerialLrfAdapter`)** *(Completed)*:
  - Implemented [SerialLrfAdapter](../libs/PayloadHal/adapters/SerialLrfAdapter.h) over `libs/Transport` with [LrfProtocols](../libs/PayloadHal/adapters/LrfProtocols.h) supporting NMEA-0183 (`$GPLRF`, `$PLRF` with XOR checksums), ASCII delimited (`R: <dist>`, `DIST: <dist>`), and Binary framed (`0xAA 0x55` sync, command ID, payload, CRC-16 CCITT).
  - Enforced ANSI Z136 eye-safety interlocks: strict disarmed default state, guarded pulse emission (rejection when disarmed), inactivity auto-disarm watchdog timer, and immediate disarm on link failure/disconnect.
  - Implemented configurable range gating (`minRangeMeters`, `maxRangeMeters`) to filter near-field atmospheric clutter and backscatter.
  - Upgraded [PayloadFactory](../libs/PayloadHal/PayloadFactory.h) with `createLrfFromUri` (`lrf://serial/COM3...`, `lrf://tcp/...`, `lrf://sim`) and dynamic `?lrf=` composite query parameter binding across all composite adapters (`PelcoDCompositePayload`, `PelcoDFujinonPayloadAdapter`, `PelcoDViscaCompositePayload`, `OnvifPayloadAdapter`), unlocking precise 3D slant-range target georeferencing via [GeoreferenceUtils](../libs/PayloadHal/GeoreferenceUtils.h) and MISB Tag 21 telemetry.

#### 4. Subsystem Integrations
- **Click-to-Point / Geo-Lock Controller** *(Completed)*:
  - Added high-level command [`slewToGeoTarget`](../libs/PayloadHal/IPayload.h) and full Geo-Lock management (`engageGeoLock`, `disengageGeoLock`, `isGeoLocked`, `geoLockTarget`, `updateGeoLock`) in [IPayload](../libs/PayloadHal/IPayload.h).
  - Implemented [GeoLockController](../libs/PayloadHal/GeoLockController.h) with active background thread tracking and deadband-filtered slew loop holding stationary geodetic coordinates under dynamic host platform motion.
- **Auto-Tracker Bridge with `libs/Tracking`** *(Completed)*:
  - Implemented [PayloadAutoTrackerBridge](../libs/PayloadHal/PayloadAutoTrackerBridge.h) bridging [Tracking::PtzAutoTracker](../libs/Tracking/PtzAutoTracker.h) directly to [IPanTiltUnit](../libs/PayloadHal/IPanTiltUnit.h) motion commands (`setNormalizedVelocity` or `setRate`) and [ICameraPayload](../libs/PayloadHal/ICameraPayload.h) optical zoom framing.
  - Supports normalized error ingestion (`updateVisual`), bounding-box ingestion (`updateBoundingBox`), spherical angular tracking (`updateAngular`), adaptive optical zoom gain scheduling, deadband filtering, and autonomous threaded tracking loops.
- **STANAG 4609 / MISB ST 0601 Metadata Generator** *(Completed)*:
  - Implemented [PayloadKlvGenerator](../libs/PayloadHal/PayloadKlvGenerator.h) serializing platform navigation, [GimbalTelemetry](../libs/PayloadHal/PayloadTypes.h) (including Tag 20 Roll), [CameraTelemetry](../libs/PayloadHal/PayloadTypes.h), LRF slant range echo, DEM terrain line-of-sight ray intersection, and 4-corner frustum footprints into standard MISB KLV packets using `libs/Klv`.

#### 5. Advanced Next-Generation Capabilities & Roadmap
- **Preset & Automated Patrol/Tour Engine (`IPtzPresetManager` / `TourEngine`)** *(Completed)*:
  - Unified preset storage, recall, and management across Pan-Tilt heads, optical zoom, and focus (`savePreset`, `recallPreset`, `clearPreset`, `listPresets`) via [IPtzPresetManager](../libs/PayloadHal/IPtzPresetManager.h) and [LocalPresetManager](../libs/PayloadHal/LocalPresetManager.h).
  - Automated cyclical guard tour / patrol engine ([TourEngine](../libs/PayloadHal/TourEngine.h)) with configurable dwell times, slew velocities, angle arrival tolerance checking, pause/resume on user intervention or auto-tracker engagement, and return-to-home fail-safes.
  - Integrated into [IPayload](../libs/PayloadHal/IPayload.h) (`presetManager()`, `tourEngine()`) and fully supported in [SimulatedPayload](../libs/PayloadHal/sim/SimulatedPayload.h). Verified with comprehensive unit test coverage in [TestPtzPresetManager.cpp](../libs/PayloadHal/tests/TestPtzPresetManager.cpp) and [TestTourEngine.cpp](../libs/PayloadHal/tests/TestTourEngine.cpp).
- **Spatial Sector Blanking & Laser Safety Keep-Out Zones (`GimbalSectorBlanking`)** *(Completed)*:
  - 3D angular exclusion polygons and soft mechanical limit masking preventing gimbal collisions with host platform structures (cabin, masts, antennas, rotor blades).
  - ANSI Z136 eye-safety software laser interlock automatically inhibiting pulsed LRF firing and tactical near-IR illuminator emission when pointing into restricted sectors or personnel decks.
  - Video privacy / structural masking flag (`isVideoBlanked`) for sensitive azimuth/elevation sectors.
  - Wraparound handling across $0^\circ / 360^\circ$ boundaries, arbitrary spherical polygon containment, safety margin buffer zones, and trajectory path traversal validation.
  - Integrated into [IPayload](../libs/PayloadHal/IPayload.h) (`sectorBlanking()`) and [SimulatedPayload](../libs/PayloadHal/sim/SimulatedPayload.h) with automatic interlock callbacks and mechanical path clamping. Verified with comprehensive unit test coverage in [TestGimbalSectorBlanking.cpp](../libs/PayloadHal/tests/TestGimbalSectorBlanking.cpp).
- **Dual-Sensor Parallax & Boresight Alignment (`SensorParallaxCompensator`)** *(Completed)*:
  - 3D physical baseline modeling ($b_x, b_y, b_z$) and static boresight calibration ($\Delta \text{Az}_0, \Delta \text{El}_0, \Delta \text{Roll}_0$) between Daylight Visible, Thermal LWIR/MWIR, and LRF optical axes.
  - Range-dependent angular disparity ($\Delta \theta = \arctan(b / (R + b_z))$) and normalized screen/pixel displacement as a function of LRF slant range or DEM terrain distance.
  - Dynamic crosshair and reticle convergence offsets, cross-spectrum visual tracking bounding box transfer with optical FOV scaling, and LRF beam convergence angles.
  - Integrated into [IPayload](../libs/PayloadHal/IPayload.h) (`parallaxCompensator()`) and [SimulatedPayload](../libs/PayloadHal/sim/SimulatedPayload.h). Verified with comprehensive unit test coverage in [TestSensorParallaxCompensator.cpp](../libs/PayloadHal/tests/TestSensorParallaxCompensator.cpp).
- **Tactical Search Patterns & Slew-to-Cue Engine (`TacticalSearchEngine`)** *(Completed)*:
  - Automated wide-area scanning routines: Sector Scan (back-and-forth oscillation with elevation stepped rasters), Expanding Square (IAMSAR datum search with adaptive FOV overlap), Creeping Line (parallel cross-track sweeps), and Archimedean Spiral Search.
  - Slew-to-Cue priority queue integrating external target tracks (Radar, ADS-B, AIS transponders, acoustic gunshot detectors) with strict preemption (`Flash` > `Immediate` > `Priority` > `Routine`).
  - Arrival tolerance verification, observation dwell hold, automatic target acquisition callbacks (handing off to auto-tracker or rangefinders), and seamless auto-resumption of interrupted search patterns.
  - Integrated into [IPayload](../libs/PayloadHal/IPayload.h) (`tacticalSearch()`) and [SimulatedPayload](../libs/PayloadHal/sim/SimulatedPayload.h). Verified with 100% test coverage in [TestTacticalSearchEngine.cpp](../libs/PayloadHal/tests/TestTacticalSearchEngine.cpp).
- **Platform Lever-Arm & Coordinate Frame Transformations (`PlatformLeverArmCompensator`)** *(Completed)*:
  - Rigid-body translations accounting for physical distances between GPS/INS antenna, gimbal pivot base, and sensor nodal points ($\mathbf{P}_{\text{optical}} = \mathbf{P}_{\text{GPS}} + \mathbf{R}_{\text{body}}(\mathbf{L}_{\text{leverarm}}) + \mathbf{R}_{\text{gimbal}}(\mathbf{L}_{\text{sensor}})$) for sub-millimeter geodetic targeting precision.
  - Forward kinematics: `computeSensorPosition`, `computeSensorPositionNed`, `computeLineOfSightNed`, `computeTargetFromSlantRange`, and `computeTargetFromGroundIntersection`.
  - Inverse kinematics: `computeLookAnglesToTarget` resolving gimbal pan/tilt look angles from the true physical sensor position.
  - Support for `Upright` (pedestal/mast), `Inverted` (aircraft/drone belly mount), and `Custom` 3D Euler mounting orientations.
  - Integrated into [IPayload](../libs/PayloadHal/IPayload.h) (`leverArmCompensator()`) and [SimulatedPayload](../libs/PayloadHal/sim/SimulatedPayload.h). Verified with comprehensive unit test coverage in [TestPlatformLeverArmCompensator.cpp](../libs/PayloadHal/tests/TestPlatformLeverArmCompensator.cpp).
- **Built-In-Test & Health Monitoring Subsystem (`PayloadHealthMonitor` / BIT)** *(Completed)*:
  - Standardized diagnostic telemetry: Power-On BIT (PBIT - boot integrity, hardware loopbacks, calibration tables), Continuous BIT (CBIT - background periodic thread tracking motor currents, stall detection, temperatures, thermal throttling at 60°C/75°C, transport packet drop counts), and Initiated BIT (IBIT - operator-triggered multi-stage diagnostic routine with monotonic progress reporting 0.0 to 1.0 and operator abort capability).
  - Telemetry structures: `DiagnosticFaultRecord`, `DiagnosticFaultCode`, `BitSeverity` (`Info`, `Warning`, `Critical`, `Fatal`), `SystemHealthReport`, and automatic device state escalation (`Ready` -> `Degraded` -> `Fault`).
  - Integrated into [IPayload](../libs/PayloadHal/IPayload.h) (`healthMonitor()`) and [SimulatedPayload](../libs/PayloadHal/sim/SimulatedPayload.h) with automatic PBIT run and CBIT thread lifecycle management upon `connect()`/`disconnect()`. Verified with 100% test coverage in [TestPayloadHealthMonitor.cpp](../libs/PayloadHal/tests/TestPayloadHealthMonitor.cpp).
- **Gimbal S-Curve Motion Profiler & Jerk-Limited Kinematics (`GimbalMotionProfiler`)** *(Completed)*:
  - Analytical 7-segment S-curve (jerk-limited) and trapezoidal velocity profile generator preventing infinite jerk and mechanical gear shock.
  - Axis-independent limits ($\omega_{\max}$, $\alpha_{\max}$, $j_{\max}$) for Pan, Tilt, and Roll.
  - Smooth acceleration ramp-up, constant velocity cruise, and deceleration braking into target angles and presets.
  - Multi-axis arrival duration synchronization ($T_{\text{master}} = \max(T_{\text{pan}}, T_{\text{tilt}}, T_{\text{roll}})$) eliminating asymmetric dog-leg sweeps.
  - Real-time streaming kinematics filter smoothing manual joystick inputs and auto-tracker velocity feeds.
  - Integrated into [IPayload](../libs/PayloadHal/IPayload.h) (`motionProfiler()`) and [SimulatedPayload](../libs/PayloadHal/sim/SimulatedPayload.h). Verified with 100% test coverage in [TestGimbalMotionProfiler.cpp](../libs/PayloadHal/tests/TestGimbalMotionProfiler.cpp).
- **Target Kinematics Estimator & Predictive Lead-Angle Slaving (`TargetKinematicsFilter`)** *(Completed)*:
  - 9-state 3D Cartesian Kalman filter ($p, v, a$) in local NED coordinates with Singer acceleration dynamics and spherical-to-Cartesian Jacobian measurement covariance projection.
  - Multi-sensor ingestion: Full 3D observation (Bearing + Slant Range / DEM) and passive Bearing-Only tracking (synthesized range with cross-track / along-track uncertainty).
  - Predictive time-of-flight ($\text{TOF}$) lead solver and system latency compensation for laser designator slaving, weapon fire control, and fast drone / boat tracking.
  - Occlusion coasting state machine (`Unacquired` -> `Acquiring` -> `Tracking` -> `Coasting` -> `Lost`) maintaining predictive trajectory for up to $T_{\text{coast}}$ seconds with gated innovation reacquisition.
  - Integrated into [IPayload](../libs/PayloadHal/IPayload.h) (`targetKinematics()`) and [SimulatedPayload](../libs/PayloadHal/sim/SimulatedPayload.h). Verified with 100% test coverage in [TestTargetKinematicsFilter.cpp](../libs/PayloadHal/tests/TestTargetKinematicsFilter.cpp).
- **Atmospheric Refraction & Earth Curvature Optical Compensator (`AtmosphericRefractionCompensator`)**:
  - Corrects long-range line-of-sight elevation angles and DEM target ray intersections beyond $5\,\text{km}$ up to $40\,\text{km}$.
  - Effective Earth Radius ($4/3 R_E$) and ITU-R P.834 / Edlén index of refraction $n(z)$ modeling pressure, temperature, humidity, and wavelength (Visible, SWIR, MWIR, LWIR).
  - True vs Apparent Target Elevation Angle correction for both forward georeferencing and inverse look-angle calculation.
- **Multi-Payload Master/Slave Slaving & Blind-Zone Handover (`PayloadSlavingCoordinator`)**:
  - Real-time line-of-sight slaving: Station B mirrors Station A's target point in 3D geodetic space (automatically compensating for inter-station baseline lever-arm and parallax).
  - Blind-Zone Handover: Automatic handoff protocol transferring active target track and cueing Station B when Station A approaches its sector blanking limit or gimbal mechanical stop.
- **Optical Sensor Switching, Digital Match-Zoom & Fusion Manager (`SensorFusionManager`)**:
  - Digital Match-Zoom: Automatically matches instantaneous horizontal FOV when switching between Daylight visible and Thermal IR cameras.
  - Optical color-palette & LUT management (White-Hot, Black-Hot, Ironbow, Rainbow, Haze-Penetration).
  - Environmental auto-switch: Automatic day-to-thermal handoff triggered by ambient scene luminance drops or low optical contrast.
- **Payload Stow, De-Ice/Wiper Routine & Emergency Park Controller (`PayloadStowController`)**:
  - Configurable `Stow`, `Deploy`, and `Maintenance` orientation states with interlocks preventing vehicle motion while unstowed.
  - Autonomous lens de-icing, heater, and wiper/washer sequence automation.
  - Emergency Park / Zeroization routine: drives gimbal to safe stow position and purges sensitive mission preset coordinates upon tamper alert.

---

### Suggested Prioritized Roadmap

| Priority | Feature / Addition | Files to Touch | Benefit |
|---|---|---|---|
| **P1** | Add synchronous telemetry getters (`currentTelemetry()`, `lastMeasurement()`) & fix `calculateTargetCoordinates` *(Completed)* | [IPanTiltUnit.h](../libs/PayloadHal/IPanTiltUnit.h), [ICameraPayload.h](../libs/PayloadHal/ICameraPayload.h), [ILaserRangeFinder.h](../libs/PayloadHal/ILaserRangeFinder.h), [PayloadFactory.cpp](../libs/PayloadHal/PayloadFactory.cpp) | Fixes hardcoded angles and enables instant state queries. |
| **P1** | Add `OnvifPayloadAdapter` *(Completed)* | `libs/PayloadHal/adapters/OnvifPayloadAdapter.h/.cpp` | Unlocks standard IP PTZ cameras using existing `libs/Onvif`. |
| **P2** | Add `PelcoDViscaCompositePayload` & URI scheme resolution in `PayloadFactory` *(Completed)* | [PayloadFactory.h](../libs/PayloadHal/PayloadFactory.h), [PayloadFactory.cpp](../libs/PayloadHal/PayloadFactory.cpp), `libs/PayloadHal/adapters/PelcoDViscaCompositePayload.h/.cpp` | Supports standard Sony FCB + Pelco-D PT setups and dynamic connection. |
| **P2** | Add `computeFrustumCorners` & Click-to-Point / Geo-Lock *(Completed)* | [GeoreferenceUtils.h](../libs/PayloadHal/GeoreferenceUtils.h), [IPayload.h](../libs/PayloadHal/IPayload.h), `libs/PayloadHal/GeoLockController.h/.cpp` | Connects directly to [TacticalOverlay](../libs/Mapping/TacticalOverlay.h) for map projection, closed-loop tracking, and click-to-point. |
| **P3** | Continuous manual focus, Iris controls, and `ILaserIlluminator` *(Completed)* | [ICameraPayload.h](../libs/PayloadHal/ICameraPayload.h), [ILaserIlluminator.h](../libs/PayloadHal/ILaserIlluminator.h), [IPayload.h](../libs/PayloadHal/IPayload.h) | Complete tactical camera control set and tactical laser pointer/illuminator subsystem. |
| **P3** | `PayloadAutoTrackerBridge` & KLV telemetry generator (`PayloadKlvGenerator`) *(Completed)* | [PayloadAutoTrackerBridge.h](../libs/PayloadHal/PayloadAutoTrackerBridge.h), [PayloadKlvGenerator.h](../libs/PayloadHal/PayloadKlvGenerator.h), `libs/PayloadHal/` | Unifies `libs/Tracking` and `libs/Klv` under the HAL. |
| **P3** | Digital Elevation Model (DEM) Ray Intersection *(Completed)* | [IDemProvider.h](../libs/PayloadHal/IDemProvider.h), [GridDemProvider.h](../libs/PayloadHal/GridDemProvider.h), [DemRayCaster.h](../libs/PayloadHal/DemRayCaster.h), [GeoreferenceUtils.h](../libs/PayloadHal/GeoreferenceUtils.h) | Exact terrain target georeferencing and occlusion detection without LRF. |
| **P3** | Video Stream Binding & Frame Synchronization *(Completed)* | [VideoStreamTypes.h](../libs/PayloadHal/VideoStreamTypes.h), [CameraStreamBinder.h](../libs/PayloadHal/CameraStreamBinder.h), [ICameraPayload.h](../libs/PayloadHal/ICameraPayload.h), [IPayload.h](../libs/PayloadHal/IPayload.h) | Ties video frames with live camera telemetry and line-of-sight georeferencing. |
| **P3** | 3-Axis Gimbal Support & Horizon Leveling *(Completed)* | [IPanTiltUnit.h](../libs/PayloadHal/IPanTiltUnit.h), [PayloadTypes.h](../libs/PayloadHal/PayloadTypes.h), [GeoreferenceUtils.h](../libs/PayloadHal/GeoreferenceUtils.h), [SimulatedPayload.h](../libs/PayloadHal/sim/SimulatedPayload.h) | Full roll axis control, automatic horizon counter-roll kinematics, roll-aware frustum footprints, and MISB Tag 20 telemetry. |
| **P3** | Physical Serial LRF Driver (`SerialLrfAdapter`) *(Completed)* | [SerialLrfAdapter.h](../libs/PayloadHal/adapters/SerialLrfAdapter.h), [LrfProtocols.h](../libs/PayloadHal/adapters/LrfProtocols.h), [PayloadFactory.h](../libs/PayloadHal/PayloadFactory.h) | Multi-protocol physical LRF support (NMEA, ASCII, Binary), eye-safety interlocks, and 3D slant-range georeferencing. |
| **P4** | Preset & Automated Patrol/Tour Engine (`IPtzPresetManager` / `TourEngine`) *(Completed)* | `IPtzPresetManager.h`, `LocalPresetManager.h/.cpp`, `TourEngine.h/.cpp`, `IPayload.h`, `SimulatedPayload.h/.cpp` | Unified spatial/optical preset storage, recall, and cyclical automated guard patrol routes. |
| **P4** | Spatial Sector Blanking & Laser Safety Keep-Out Zones *(Completed)* | `GimbalSectorBlanking.h/.cpp`, `IPanTiltUnit.h`, `IPayload.h`, `SimulatedPayload.h/.cpp` | Prevents mechanical vehicle collisions, masks sensitive video, and inhibits laser emission into hazard sectors. |
| **P4** | Dual-Sensor Parallax & Boresight Alignment (`SensorParallaxCompensator`) *(Completed)* | `SensorParallaxCompensator.h/.cpp`, `IPayload.h`, `SimulatedPayload.h/.cpp` | Range-dependent angular alignment, reticle convergence, and bounding box transfer between Daylight EO and Thermal IR. |
| **P4** | Tactical Search Patterns & Slew-to-Cue Engine (`TacticalSearchEngine`) *(Completed)* | `TacticalSearchEngine.h/.cpp`, `IPayload.h`, `SimulatedPayload.h/.cpp` | Automated wide-area search sweeps (Sector, Expanding Square, Spiral, Creeping Line) and prioritized Radar/AIS/Acoustic cueing with auto-resume. |
| **P4** | Platform Lever-Arm & Coordinate Frame Transformations *(Completed)* | `PlatformLeverArmCompensator.h/.cpp`, `IPayload.h`, `SimulatedPayload.h/.cpp` | Offsets GPS antenna, gimbal pivot, and optical center for high-precision georeferencing under dynamic vehicle roll/pitch. |
| **P4** | Built-In-Test & Health Monitoring Subsystem *(Completed)* | `PayloadHealthMonitor.h/.cpp`, `IPayload.h`, `SimulatedPayload.h/.cpp`, `PayloadHal.h` | Three pillars of BIT (PBIT/CBIT/IBIT), motor stall detection, thermal throttling alerts, and health-based state escalation. |
| **P5** | Gimbal S-Curve Motion Profiler & Jerk-Limited Kinematics *(Completed)* | [GimbalMotionProfiler.h](../libs/PayloadHal/GimbalMotionProfiler.h), [IPayload.h](../libs/PayloadHal/IPayload.h), [SimulatedPayload.h](../libs/PayloadHal/sim/SimulatedPayload.h) | Prevents mechanical shock, eliminates motor overcurrent spikes, and provides cinema-smooth tracking. |
| **P5** | Target Kinematics & Predictive Lead-Angle Slaving *(Completed)* | [TargetKinematicsFilter.h](../libs/PayloadHal/TargetKinematicsFilter.h), [IPayload.h](../libs/PayloadHal/IPayload.h), [SimulatedPayload.h](../libs/PayloadHal/sim/SimulatedPayload.h) | Real-time target velocity estimation, lead angle pointing, and occlusion coasting. |
| **P5** | Atmospheric Refraction & Earth Curvature Compensator | `AtmosphericRefractionCompensator.h/.cpp`, `GeoreferenceUtils.h` | High-fidelity long-range over-the-horizon elevation and DEM georeferencing. |
| **P5** | Multi-Payload Master/Slave Coordinator & Handover | `PayloadSlavingCoordinator.h/.cpp`, `IPayload.h` | Multi-turret LOS slaving and automated blind-zone handover. |
| **P5** | Optical Sensor Switching & Digital Match-Zoom | `SensorFusionManager.h/.cpp`, `ICameraPayload.h` | Seamless FOV match-zoom, color palettes, and day/thermal auto-handoff. |
| **P5** | Payload Stow, Environmental De-Ice & Emergency Park | `PayloadStowController.h/.cpp`, `IPayload.h` | Safe transport stowing, lens de-icing, and emergency zeroization. |