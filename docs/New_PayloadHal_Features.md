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