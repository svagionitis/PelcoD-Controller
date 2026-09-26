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

### Key Shortcomings in the Current Implementation

1. **Missing Synchronous State & Telemetry Getters across Interfaces**:
   - [IPanTiltUnit](../libs/PayloadHal/IPanTiltUnit.h), [ICameraPayload](../libs/PayloadHal/ICameraPayload.h), and [ILaserRangeFinder](../libs/PayloadHal/ILaserRangeFinder.h) provide push callbacks (`registerTelemetryCallback`, `registerMeasurementCallback`), but **no synchronous snapshot getters** (`currentTelemetry()` or `lastMeasurement()`).
   - Consequently, in [PelcoDCompositePayload::calculateTargetCoordinates](../libs/PayloadHal/PayloadFactory.cpp#L116-L131), gimbal angles were **hardcoded to `pan = 0.0°, tilt = -10.0°`** because it could not query [PelcoDPtzAdapter](../libs/PayloadHal/adapters/PelcoDPtzAdapter.h) directly.
2. **Incomplete Adapter Ecosystem**:
   - [ViscaSonyAdapter](../libs/PayloadHal/adapters/ViscaSonyAdapter.h) exists, but there is no composite payload binding a Sony FCB camera to a pan-tilt head (e.g. `PelcoDViscaCompositePayload`).
   - [PayloadFactory::createFromUri](../libs/PayloadHal/PayloadFactory.cpp#L141-L146) only handles `"sim://"`. URI parsing for serial, TCP, ONVIF, or Pelco-D is unimplemented.
   - [SimulatedPayload::SimLrf](../libs/PayloadHal/sim/SimulatedPayload.cpp#L347) is the only LRF implementation; there is no physical LRF hardware driver (e.g., NMEA/ASCII or binary serial).

---

### What Is Missing and Can Be Added

#### 1. Core Interface & Telemetry Enhancements
- **Synchronous Telemetry Access**:
  - Add `[[nodiscard]] virtual GimbalTelemetry currentTelemetry() const noexcept = 0;` to [IPanTiltUnit](../libs/PayloadHal/IPanTiltUnit.h).
  - Add `[[nodiscard]] virtual CameraTelemetry currentTelemetry() const noexcept = 0;` to [ICameraPayload](../libs/PayloadHal/ICameraPayload.h).
  - Add `[[nodiscard]] virtual std::optional<LrfTargetMeasurement> lastMeasurement() const noexcept = 0;` to [ILaserRangeFinder](../libs/PayloadHal/ILaserRangeFinder.h).
- **3-Axis Gimbal Support (Roll Axis & Horizon Leveling)**:
  - Tactical turrets and aerial gimbals include a roll axis. Expand [GimbalTelemetry](../libs/PayloadHal/PayloadTypes.h#L58-L69) and [IPanTiltUnit](../libs/PayloadHal/IPanTiltUnit.h) with `rollAngleDeg`, `rollRateDegPerSec`, `setRollAngle(double rollDeg)`, and horizon leveling modes.
- **Continuous Focus & Exposure / Iris Controls**:
  - Add `focusContinuous(float velocity)` and `focusStop()` to [ICameraPayload](../libs/PayloadHal/ICameraPayload.h) for continuous Near/Far joystick slewing (supported by Pelco-D and VISCA).
  - Add Iris/Exposure controls (`setIrisAuto(bool)`, `setIrisNormalized(double)`, exposure compensation).
- **Video Stream Binding & Frame Synchronization** *(Completed)*:
  - Added [VideoStreamTypes.h](../libs/PayloadHal/VideoStreamTypes.h) with `VideoStreamProfile` (Primary, Secondary, Thermal, Snapshot), `StreamTransportProtocol` (RTSP, V4L2, DirectShow, UDP/MPEG-TS, WebRTC, Simulated), and `VideoStreamDescriptor`.
  - Added video streaming methods (`videoStreamUri()`, `setVideoStreamUri()`, `availableStreams()`, `streamDescriptor()`) to [ICameraPayload](../libs/PayloadHal/ICameraPayload.h) and convenience shortcuts to [IPayload](../libs/PayloadHal/IPayload.h).
  - Implemented streaming across all camera adapters (`SimulatedPayload`, `OnvifCameraAdapter`, `PelcoDFujinonCameraAdapter`, `ViscaSonyCameraAdapter`, `PelcoDCameraUnit`) and query parameter parsing (`?video=`, `?substream=`, `?thermal=`, `?snapshot=`) in [PayloadFactory::createFromUri](../libs/PayloadHal/PayloadFactory.cpp).
  - Implemented [CameraStreamBinder](../libs/PayloadHal/CameraStreamBinder.h) for pairing raw/decoded video frames with live camera optics, gimbal orientation, and ground georeferencing/DEM projections.

#### 2. Tactical & Subsystem Additions
- **Laser Pointer / Illuminator Subsystem (`ILaserIlluminator`)**:
  - Tactical payloads have near-infrared target pointers / illuminators (830nm/850nm/1064nm).
  - Add an interface with safety interlock arming, continuous/pulsed/strobe modes, and power level adjustments.
- **Ground Sensor Footprint (Frustum) Georeferencing**:
  - [GeoreferenceUtils](../libs/PayloadHal/GeoreferenceUtils.h) calculates frame center, but does not compute the 4-corner ground projection quadrilateral ([Klv::FrustumCorners](../libs/Mapping/TacticalOverlay.h#L38)).
  - Adding `computeFrustumCorners(...)` in [GeoreferenceUtils](../libs/PayloadHal/GeoreferenceUtils.h) bridges camera HFOV/VFOV and gimbal orientation directly with [TacticalOverlay](../libs/Mapping/TacticalOverlay.h) in `libs/Mapping`.
- **Digital Elevation Model (DEM) Ray Intersection** *(Completed)*:
  - Added [IDemProvider](../libs/PayloadHal/IDemProvider.h), [GridDemProvider](../libs/PayloadHal/GridDemProvider.h), and [DemRayCaster](../libs/PayloadHal/DemRayCaster.h) implementing 2-phase numerical ray-marching with Illinois secant root refinement and foreground occlusion handling.
  - Upgraded [GeoreferenceUtils](../libs/PayloadHal/GeoreferenceUtils.h), [IPayload](../libs/PayloadHal/IPayload.h), and [PayloadKlvGenerator](../libs/PayloadHal/PayloadKlvGenerator.h) to support high-fidelity terrain ray intersection.

#### 3. New Adapters & Factory URIs
- **ONVIF Client Adapter (`OnvifPayloadAdapter`)**:
  - The repo contains an extensive ONVIF client in [OnvifClient.h](../libs/Onvif/OnvifClient.h).
  - Create an adapter mapping `OnvifClient` into [IPanTiltUnit](../libs/PayloadHal/IPanTiltUnit.h) and [ICameraPayload](../libs/PayloadHal/ICameraPayload.h), enabling any Profile S/T/G network camera to be driven by `PayloadHal`.
- **Sony FCB + Pelco-D Composite Adapter**:
  - Combine [PelcoDPtzAdapter](../libs/PayloadHal/adapters/PelcoDPtzAdapter.h) and [ViscaSonyAdapter](../libs/PayloadHal/adapters/ViscaSonyAdapter.h) into `PelcoDViscaCompositePayload` in [PayloadFactory](../libs/PayloadHal/PayloadFactory.h).
- **Physical Serial LRF Driver (`SerialLrfAdapter`)**:
  - Support common rangefinder protocols (e.g. NMEA 0183 `$GPLRF`, Vectronix LDM, or ASCII serial) communicating over [libs/Transport](../libs/Transport).
- **Extended URI Schemes in `PayloadFactory::createFromUri`**:
  - Support `onvif://user:pass@host:port`, `pelcod://host:port?addr=1`, and `serial:///dev/ttyUSB0?baud=9600&protocol=pelcod`.

#### 4. Subsystem Integrations
- **Click-to-Point / Geo-Lock Controller**:
  - Add a high-level command to [IPayload](../libs/PayloadHal/IPayload.h):
    ```cpp
    virtual bool slewToGeoTarget(const Klv::GeoPoint3D& platformPos,
                                 double platformHeadingDeg,
                                 const Klv::GeoPoint3D& targetPos) = 0;
    ```
  - Provide an active **GeoHold tracking loop** that automatically compensates for platform movement to keep the payload pointed at fixed coordinates.
- **Auto-Tracker Bridge with `libs/Tracking`**:
  - Connect [PtzAutoTracker](../libs/Tracking/PtzAutoTracker.h) directly to [IPanTiltUnit::setRate](../libs/PayloadHal/IPanTiltUnit.h#L27) and [ICameraPayload::zoomContinuous](../libs/PayloadHal/ICameraPayload.h#L33) for automated visual target tracking.
- **STANAG 4609 / MISB ST 0601 Metadata Generator**:
  - Create a utility in `PayloadHal` that serializes current platform GPS, [GimbalTelemetry](../libs/PayloadHal/PayloadTypes.h#L58), [CameraTelemetry](../libs/PayloadHal/PayloadTypes.h#L119), and [LrfTargetMeasurement](../libs/PayloadHal/PayloadTypes.h#L86) into MISB KLV packets using `libs/Klv`.

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