Listed directory PayloadHal
Listed directory adapters
Listed directory sim
Viewed PayloadHal.h:1-18
Viewed IPayload.h:1-52
Viewed IPanTiltUnit.h:1-100
Viewed ICameraPayload.h:1-94
Viewed ILaserRangeFinder.h:1-68
Viewed IDevice.h:1-45
Viewed PayloadTypes.h:1-131
Viewed GeoreferenceUtils.h:1-67
Viewed PayloadFactory.h:1-44
Listed directory libs
Listed directory apps
Listed directory PelcoD-Controller
Viewed PelcoDPtzAdapter.h:1-67
Viewed ViscaSonyAdapter.h:1-65
Viewed PayloadFactory.cpp:1-165
Viewed PelcoDFujinonPayloadAdapter.h:1-66
Viewed SimulatedPayload.h:1-63
Listed directory Onvif
Listed directory adapters
Listed directory Tracking
Viewed PtzAutoTracker.h:1-230
Viewed PelcoDFujinonPayloadAdapter.cpp:1-413
Listed directory tests
Viewed SimulatedPayload.cpp:1-618
Viewed ViscaSonyAdapter.cpp:1-189
Viewed PelcoDPtzAdapter.cpp:1-235
Listed directory Mapping
Viewed TacticalOverlay.h:1-72
Viewed GeoreferenceUtils.cpp:1-119
Searched for "IPayload"
Listed directory app-video-qt
Viewed VideoPlayerController.h:1-70
Listed directory app-pelcod-qt

### Executive Overview

The [PayloadHal](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/PayloadHal.h) library provides a unified Hardware Abstraction Layer for multi-sensor surveillance and tactical payload stations. It coordinates:
- [IPanTiltUnit](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/IPanTiltUnit.h): Pan-tilt heads, pedestals, and stabilized gimbals.
- [ICameraPayload](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/ICameraPayload.h): Daylight visible and thermal LWIR/MWIR optical sensors.
- [ILaserRangeFinder](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/ILaserRangeFinder.h): Tactical pulsed laser rangefinders with safety interlocks.
- [IPayload](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/IPayload.h): Aggregate composite station binding all subsystems with geodetic target projection.
- [GeoreferenceUtils](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/GeoreferenceUtils.h): Slant range projection, ground intersection, and look-angle computation.
- [PayloadFactory](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/PayloadFactory.h): Instantiation engine for physical and simulated stations.

---

### Key Shortcomings in the Current Implementation

1. **Missing Synchronous State & Telemetry Getters across Interfaces**:
   - [IPanTiltUnit](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/IPanTiltUnit.h), [ICameraPayload](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/ICameraPayload.h), and [ILaserRangeFinder](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/ILaserRangeFinder.h) provide push callbacks (`registerTelemetryCallback`, `registerMeasurementCallback`), but **no synchronous snapshot getters** (`currentTelemetry()` or `lastMeasurement()`).
   - Consequently, in [PelcoDCompositePayload::calculateTargetCoordinates](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/PayloadFactory.cpp#L116-L131), gimbal angles are **hardcoded to `pan = 0.0°, tilt = -10.0°`** because it cannot query [PelcoDPtzAdapter](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/adapters/PelcoDPtzAdapter.h) directly.
2. **Incomplete Adapter Ecosystem**:
   - [ViscaSonyAdapter](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/adapters/ViscaSonyAdapter.h) exists, but there is no composite payload binding a Sony FCB camera to a pan-tilt head (e.g. `PelcoDViscaCompositePayload`).
   - [PayloadFactory::createFromUri](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/PayloadFactory.cpp#L141-L146) only handles `"sim://"`. URI parsing for serial, TCP, ONVIF, or Pelco-D is unimplemented.
   - [SimulatedPayload::SimLrf](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/sim/SimulatedPayload.cpp#L347) is the only LRF implementation; there is no physical LRF hardware driver (e.g., NMEA/ASCII or binary serial).

---

### What Is Missing and Can Be Added

#### 1. Core Interface & Telemetry Enhancements
- **Synchronous Telemetry Access**:
  - Add `[[nodiscard]] virtual GimbalTelemetry currentTelemetry() const noexcept = 0;` to [IPanTiltUnit](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/IPanTiltUnit.h).
  - Add `[[nodiscard]] virtual CameraTelemetry currentTelemetry() const noexcept = 0;` to [ICameraPayload](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/ICameraPayload.h).
  - Add `[[nodiscard]] virtual std::optional<LrfTargetMeasurement> lastMeasurement() const noexcept = 0;` to [ILaserRangeFinder](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/ILaserRangeFinder.h).
- **3-Axis Gimbal Support (Roll Axis & Horizon Leveling)**:
  - Tactical turrets and aerial gimbals include a roll axis. Expand [GimbalTelemetry](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/PayloadTypes.h#L58-L69) and [IPanTiltUnit](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/IPanTiltUnit.h) with `rollAngleDeg`, `rollRateDegPerSec`, `setRollAngle(double rollDeg)`, and horizon leveling modes.
- **Continuous Focus & Exposure / Iris Controls**:
  - Add `focusContinuous(float velocity)` and `focusStop()` to [ICameraPayload](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/ICameraPayload.h) for continuous Near/Far joystick slewing (supported by Pelco-D and VISCA).
  - Add Iris/Exposure controls (`setIrisAuto(bool)`, `setIrisNormalized(double)`, exposure compensation).
- **Video Stream Binding**:
  - Add `[[nodiscard]] virtual std::string videoStreamUri() const = 0;` to [ICameraPayload](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/ICameraPayload.h) so UI and pipeline controllers know which RTSP or V4L2 stream belongs to which sensor.

#### 2. Tactical & Subsystem Additions
- **Laser Pointer / Illuminator Subsystem (`ILaserIlluminator`)**:
  - Tactical payloads have near-infrared target pointers / illuminators (830nm/850nm/1064nm).
  - Add an interface with safety interlock arming, continuous/pulsed/strobe modes, and power level adjustments.
- **Ground Sensor Footprint (Frustum) Georeferencing**:
  - [GeoreferenceUtils](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/GeoreferenceUtils.h) calculates frame center, but does not compute the 4-corner ground projection quadrilateral ([Klv::FrustumCorners](file:///home/theon/Development/PelcoD-Controller/libs/Mapping/TacticalOverlay.h#L38)).
  - Adding `computeFrustumCorners(...)` in [GeoreferenceUtils](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/GeoreferenceUtils.h) bridges camera HFOV/VFOV and gimbal orientation directly with [TacticalOverlay](file:///home/theon/Development/PelcoD-Controller/libs/Mapping/TacticalOverlay.h) in `libs/Mapping`.
- **Digital Elevation Model (DEM) Ray Intersection**:
  - Upgrade [GeoreferenceUtils::computeTargetFromGroundIntersection](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/GeoreferenceUtils.h#L48) to support ray-casting against a DEM elevation grid instead of assuming a flat plane.

#### 3. New Adapters & Factory URIs
- **ONVIF Client Adapter (`OnvifPayloadAdapter`)**:
  - The repo contains an extensive ONVIF client in [libs/Onvif/OnvifClient.h](file:///home/theon/Development/PelcoD-Controller/libs/Onvif/OnvifClient.h).
  - Create an adapter mapping `OnvifClient` into [IPanTiltUnit](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/IPanTiltUnit.h) and [ICameraPayload](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/ICameraPayload.h), enabling any Profile S/T/G network camera to be driven by `PayloadHal`.
- **Sony FCB + Pelco-D Composite Adapter**:
  - Combine [PelcoDPtzAdapter](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/adapters/PelcoDPtzAdapter.h) and [ViscaSonyAdapter](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/adapters/ViscaSonyAdapter.h) into `PelcoDViscaCompositePayload` in [PayloadFactory](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/PayloadFactory.h).
- **Physical Serial LRF Driver (`SerialLrfAdapter`)**:
  - Support common rangefinder protocols (e.g. NMEA 0183 `$GPLRF`, Vectronix LDM, or ASCII serial) communicating over [libs/Transport](file:///home/theon/Development/PelcoD-Controller/libs/Transport).
- **Extended URI Schemes in `PayloadFactory::createFromUri`**:
  - Support `onvif://user:pass@host:port`, `pelcod://host:port?addr=1`, and `serial:///dev/ttyUSB0?baud=9600&protocol=pelcod`.

#### 4. Subsystem Integrations
- **Click-to-Point / Geo-Lock Controller**:
  - Add a high-level command to [IPayload](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/IPayload.h):
    ```cpp
    virtual bool slewToGeoTarget(const Klv::GeoPoint3D& platformPos,
                                 double platformHeadingDeg,
                                 const Klv::GeoPoint3D& targetPos) = 0;
    ```
  - Provide an active **GeoHold tracking loop** that automatically compensates for platform movement to keep the payload pointed at fixed coordinates.
- **Auto-Tracker Bridge with `libs/Tracking`**:
  - Connect [PtzAutoTracker](file:///home/theon/Development/PelcoD-Controller/libs/Tracking/PtzAutoTracker.h) directly to [IPanTiltUnit::setRate](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/IPanTiltUnit.h#L27) and [ICameraPayload::zoomContinuous](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/ICameraPayload.h#L33) for automated visual target tracking.
- **STANAG 4609 / MISB ST 0601 Metadata Generator**:
  - Create a utility in `PayloadHal` that serializes current platform GPS, [GimbalTelemetry](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/PayloadTypes.h#L58), [CameraTelemetry](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/PayloadTypes.h#L119), and [LrfTargetMeasurement](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/PayloadTypes.h#L86) into MISB KLV packets using `libs/Klv`.

---

### Suggested Prioritized Roadmap

| Priority | Feature / Addition | Files to Touch | Benefit |
|---|---|---|---|
| **P1** | Add synchronous telemetry getters (`currentTelemetry()`, `lastMeasurement()`) & fix `calculateTargetCoordinates` | [IPanTiltUnit.h](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/IPanTiltUnit.h), [ICameraPayload.h](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/ICameraPayload.h), [ILaserRangeFinder.h](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/ILaserRangeFinder.h), [PayloadFactory.cpp](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/PayloadFactory.cpp) | Fixes hardcoded angles and enables instant state queries. |
| **P1** | Add `OnvifPayloadAdapter` | `libs/PayloadHal/adapters/OnvifPayloadAdapter.h/.cpp` | Unlocks standard IP PTZ cameras using existing `libs/Onvif`. |
| **P2** | Add `PelcoDViscaCompositePayload` & URI scheme resolution in `PayloadFactory` | [PayloadFactory.h](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/PayloadFactory.h), [PayloadFactory.cpp](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/PayloadFactory.cpp) | Supports standard Sony FCB + Pelco-D PT setups and dynamic connection. |
| **P2** | Add `computeFrustumCorners` & Click-to-Point / Geo-Lock | [GeoreferenceUtils.h](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/GeoreferenceUtils.h), [IPayload.h](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/IPayload.h) | Connects directly to [TacticalOverlay](file:///home/theon/Development/PelcoD-Controller/libs/Mapping/TacticalOverlay.h) for map projection. |
| **P3** | Continuous manual focus, Iris controls, and `ILaserIlluminator` | [ICameraPayload.h](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/ICameraPayload.h), [ILaserRangeFinder.h](file:///home/theon/Development/PelcoD-Controller/libs/PayloadHal/ILaserRangeFinder.h) | Complete tactical camera control set. |
| **P3** | `PayloadAutoTrackerBridge` & KLV telemetry generator | `libs/PayloadHal/` | Unifies `libs/Tracking` and `libs/Klv` under the HAL. |