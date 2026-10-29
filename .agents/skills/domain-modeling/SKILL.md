---
name: domain-modeling
description: >-
  Use this skill to model domain concepts into strongly-typed C++17 entities, enforce ubiquitous language, and eliminate Primitive Obsession across all current and future codebase libraries.
---

# Domain Modeling & Ubiquitous Language

This skill establishes a disciplined, type-driven domain modeling workflow that applies universally across the **entire codebase**—including all existing libraries (`libs/PelcoDCore`, `libs/Onvif`, `libs/Tracking`, `libs/SightlineCore`, `libs/ViscaCore`, `libs/Klv`, `libs/Nmea`, `libs/Mapping`, etc.) as well as **any future or newly added libraries** in this repository.

It actively translates protocol specifications, algorithm semantics, and business logic into strongly-typed C++17 entities, maintains a unified ubiquitous language, and eliminates **Primitive Obsession** while adhering strictly to MISRA C++:2023 and SEI CERT C++ safety standards.

---

## 1. Core Principles

1. **Eliminate Primitive Obsession:**
   - Raw primitives (`std::uint8_t`, `int`, `float`, `double`, `std::string`) must never be passed freely across domain or API boundaries.
   - Wrap raw values into strongly-typed value objects with domain semantics and invariant checks.
2. **Make Illegal States Unrepresentable:**
   - Enforce domain invariants upon construction. If a value is invalid, reject it immediately or make out-of-range states unconstructible.
3. **Zero Dynamic Allocation (CERT ERR55-CPP):**
   - Invariants and value objects on real-time loops, packet parsers, and network discovery paths must be `noexcept` and execute with zero heap allocations. Use `std::string_view`, stack arrays, or pre-allocated buffers.
4. **Explicit Typing & Conversions (MISRA C++:2023 Rule 7.0.6 & SEI CERT INT31-C):**
   - Fixed-width integers must explicitly include `<cstdint>` and use `std::` qualification.
   - Conversions between units, coordinate frames, or integer ranges must be explicit, clamped, and range-verified via `static_cast`.

---

## 2. Onboarding New Libraries & Future Domains

Whenever a new library or module is added to `libs/`, this domain modeling protocol must be applied from day one:

```
+-------------------------------------------------------------------------------+
|                    NEW LIBRARY DOMAIN ONBOARDING PROTOCOL                     |
|                                                                               |
|   New Library (`libs/<Name>`)                                                 |
|         │                                                                     |
|         ▼                                                                     |
|   1. Classify Architectural Tier                                              |
|         ├── Core Library (Default): Zero Qt, pure C++17, zero-heap paths      |
|         └── UI Adapter (`*Qt`): Qt signals/slots wrapping Core value types    |
|         │                                                                     |
|         ▼                                                                     |
|   2. Extract Domain Concepts & Ubiquitous Language                           |
|         • Identify domain nouns, measurements, units, and ranges              |
|         • Forbid raw primitives (e.g. wrap angles, IDs, baud rates, tokens)   |
|         │                                                                     |
|         ▼                                                                     |
|   3. Enforce Invariants in C++17 Types                                        |
|         • Construction validation with explicit constexpr constructors        |
|         • No implicit conversion operators                                    |
|         • `[[nodiscard]] noexcept` state queries                              |
+-------------------------------------------------------------------------------+
```

```mermaid
flowchart TD
    A["New Library Added: libs/&lt;NewModule&gt;"] --> B{"Determine Architecture Tier"}
    B -->|Ends with 'Qt'| C1["UI Adapter: Wraps Core types with Qt signals/slots"]
    B -->|Non-Qt (Default)| C2["Core Library: Strict 0-Qt, MISRA/CERT, 0-heap fast paths"]
    
    C2 --> D["Identify Ubiquitous Language & Domain Terms"]
    D --> E["Ban Primitive Obsession: Wrap raw int/float/string"]
    E --> F["Enforce Construction Invariants & Zero-Cost Value Types"]
    F --> G["Add Unit Tests Validating Boundary & Out-of-Range Rejection"]
```

---

## 3. Multi-Domain Ubiquitous Language Catalog

Every domain entity, class, function, and document across current and future repository modules must adhere to canonical terminology:

### A. Computer Vision & Tracking (`libs/Tracking`, `libs/Video`, and future vision modules)
| Domain Term | Canonical Type | Allowed Invariants | Purpose |
| :--- | :--- | :--- | :--- |
| `TargetId` | `struct TargetId` | Unique `std::uint32_t` identifier | Differentiates distinct tracked objects. |
| `NormalizedCoord` | `class NormalizedCoord` | Float $\in [0.0f, 1.0f]$ | Resolution-independent sensor coordinate. |
| `PixelCoord` | `class PixelCoord` | Integer $\ge 0$, bounded by image dims | Discrete sensor pixel location. |
| `BoundingBox` | `struct BoundingBox` | $w > 0$, $h > 0$, bounds inside frame | Detected object bounding envelope. |
| `ConfidenceScore` | `class ConfidenceScore` | Float $\in [0.0f, 1.0f]$ | Detection / classification certainty. |
| `TrackerState` | `enum class TrackerState` | `Searching`, `Acquired`, `Tracking`, `Lost` | Finite state machine tracking lifecycle. |

### B. ONVIF & Network Discovery (`libs/Onvif`, and future web service modules)
| Domain Term | Canonical Type | Allowed Invariants | Purpose |
| :--- | :--- | :--- | :--- |
| `ProfileToken` | `class ProfileToken` | Non-empty ASCII string view | Targets specific media/PTZ profile on camera. |
| `PtzVector` | `struct PtzVector` | Normalized velocities $[-1.0, 1.0]$ | Velocity or absolute position vector. |
| `WsDiscoveryProbe` | `struct WsDiscoveryProbe` | Zero heap in validation path | Local broadcast discovery request. |

### C. Serial & PTZ Protocols (`libs/PelcoDCore`, `libs/ViscaCore`, `libs/SightlineCore`, and future protocols)
| Domain Term | Canonical Type | Allowed Invariants | Purpose |
| :--- | :--- | :--- | :--- |
| `CameraAddress` | `class CameraAddress` | Integer $1 \le \text{address} \le 255$ | Device address on multi-drop bus. |
| `PanSpeed` / `TiltSpeed`| `class PanSpeed` | Protocol-specific speed bounds | Horizontal/vertical velocity. |
| `CommandField` | `struct CommandField` | Bitmask with conflict rejection | Validated motor and auxiliary state bits. |

### D. Telemetry & Navigation (`libs/Klv`, `libs/Nmea`, `libs/DjiTelemetry`, and future sensor modules)
| Domain Term | Canonical Type | Allowed Invariants | Purpose |
| :--- | :--- | :--- | :--- |
| `Latitude` / `Longitude`| `class Latitude` | Valid geo degrees $[-90.0, 90.0]$ | Geodetic coordinates. |
| `AltitudeMeters` | `class AltitudeMeters` | Bounded physical elevations | Height above sea level / ellipsoid. |
| `HeadingDegrees` | `class HeadingDegrees` | Angle in $[0.0, 360.0)$ | Compass azimuth. |

---

## 4. Modeling Workflow for Any Subsystem

When authoring or modifying code in any library (existing or future):

1. **Extract Domain Concepts:**
   - Review standards (STANAG, ONVIF, NMEA, MISRA) or mathematical models.
   - Distinguish Value Objects from Stateful Entities.
2. **Define Invariants and Preconditions:**
   - Are bounding dimensions positive? Are coordinates in bounds? Are directional commands conflict-free?
3. **Formulate Strongly-Typed Value Objects:**
   - Use explicit constructors (`explicit constexpr ... noexcept`).
   - Eliminate implicit type conversions.
   - Mark accessors `[[nodiscard]] noexcept`.
4. **Audit MISRA & CERT Compliance:**
   - Integer types: explicitly qualify `std::uint8_t`, `std::uint32_t`, `std::size_t` with `<cstdint>` and `<cstddef>`.
   - Floating-point calculations: verify absence of undefined casts (e.g. negative float to unsigned integer).
   - Fast paths: 0-heap allocations in `noexcept` functions (CERT ERR55-CPP).
5. **Update Repository Glossary & Tests:**
   - Add unit tests validating invariant enforcement and boundary rejections.
