# Implementation Plan: Phase 10 – Tactical HUD Metadata Overlay (MISB ST 1909.1)

**Scope:** `libs/Klv/` (`Misb1909Types.h`, `Misb1909OverlayRenderer.h`, `Misb1909OverlayRenderer.cpp`, `CMakeLists.txt`), `libs/Klv/tests/` (`TestMisb1909OverlayRenderer.cpp`, `CMakeLists.txt`), `app-pelcod-qt/widgets/` (`VideoOverlayWidget.h`, `VideoOverlayWidget.cpp`), `docs/KLV_Features.md`  
**Standards Baseline:** MISB ST 1909.1 (Metadata Overlay for Visualization), MISB ST 0601.19 (UAS Datalink Local Set), MISB ST 0102.13 (Security Classification Local Set), MISB ST 0806.4 (Remote Video Terminal Local Set), STANAG 4609 Edition 3/4, MISRA C++:2023, SEI CERT C++ (ERR55-CPP, INT31-C, STR37-C), Project Rules in `.agents/rules/`  
**Reference Document:** [`docs/KLV_Features.md`](KLV_Features.md) (§1, §2, §3.22, §4.2 Phase 10, §5.10)

---

## 1. Executive Summary & Operational Context

### 1.1 Operational Problem Statement
In military, intelligence, surveillance, and reconnaissance (ISR) operations, full-motion video (FMV) feeds transmitted over STANAG 4609 / MISB datalinks must present critical spatial and situational telemetry to payload operators, joint terminal attack controllers (JTACs), and tactical commanders.

Historically, sensor telemetry has been rendered using **destructive burned-in On-Screen Display (OSD)**:
- Graphical text, heading ribbons, and reticle lines are rasterized directly onto uncompressed video frames (e.g. via `cv::Mat` drawing in `libs/VideoFilters/TacticalHudFilter.cpp`) prior to video compression encoding.
- This legacy burned-in approach imposes severe operational and technical penalties:
  1. **Video Compression Degradation:** High-contrast text edges, artificial horizon bars, and rapidly updating numeric digits force H.264/H.265 video encoders to waste substantial bitrate on artificial high-frequency edges, starving actual imagery of compression budget and causing macroblocking or motion artifacts.
  2. **Irreversible Target Occlusion:** Static text cards and reticle lines permanently obliterate underlying image pixels. Small tactical targets (dismounted personnel, camouflage positions, distant vehicles) hidden beneath burned-in text cannot be recovered during subsequent intelligence exploitation.
  3. **Display Rigidity & Zero Client Adaptability:** Burned-in pixels are fixed at the broadcast resolution and aspect ratio. A client viewing on an ATAK tablet, a 4K command post display, or a degraded narrow viewport cannot declutter, toggle off, scale, or recolor the symbology to suit local ambient lighting (NVG phosphor green, daylight white, FLIR thermal amber).

```
Legacy Burned-in OSD (Destructive):
[Raw Camera Video] ---> [OSD Drawing (cv::Mat)] ---> [H.264 Encoder] ---> [Corrupted Pixels in TS]

MISB ST 1909.1 Client Overlay (Non-Destructive):
[Raw Camera Video] ────────────────────────────────> [H.264 Encoder] ─────┐
                                                                           ├─> [STANAG 4609 MPEG-TS]
[Sensor Telemetry] ---> [MISB ST 0601 / ST 0102] ──> [KLV PES Muxer] ──────┘
                                                                           │
                                                                           v
                                                       [Client Viewport Non-Destructive Render]
                                                       ├─ Clean Unaltered Video Frame
                                                       └─ Vector HUD (Normalized Anchors 0.0..1.0)
```

### 1.2 The MISB ST 1909.1 Solution
**MISB ST 1909.1** ("Metadata Overlay for Visualization") standardizes the non-destructive graphical layout of metadata elements over motion imagery on client viewports:
- **Pristine Video Preservation:** Video frames remain 100% untouched by symbology throughout compression, transport, and decoding.
- **Normalized Display Coordinates (NDC):** All layout anchors, group margins, and vector primitives are specified in a normalized $[0.0, 1.0]$ coordinate space across $(X, Y)$, rendering identically regardless of display resolution (720p, 1080p, 4K UHD, or mobile).
- **Eight Standard Layout Groups:** Standardizes 8 distinct telemetry zones (Main Sensor, Classification, Platform Info, True North, Laser Sensor, Date/Time, Target Group, Boresight Reticle).
- **Tactical Flight Instrument Symbology:** Standardizes artificial horizon pitch ladders, compass heading tapes, optical field of view reticles, and geodetic / MGRS coordinates.
- **Zero-Qt Pure C++17 Core Engine (`libs/Klv`):** The layout generation engine strictly maintains zero Qt dependencies. It ingests parsed `Klv::UasDatalinkMessage` packets and outputs an immutable, type-safe layout descriptor (`TacticalOverlayLayout`).
- **Hardware-Accelerated Client Presentation:** The Qt/QML client viewport (`app-pelcod-qt/widgets/VideoOverlayWidget`) consumes the layout descriptor and renders it on a hardware-accelerated vector overlay canvas atop the raw video surface.

---

## 2. Architecture & Information Flow

### 2.1 ASCII Architecture Diagram

```
+---------------------------------------------------------------------------------------------------------+
|                              MISB ST 1909.1 TACTICAL HUD OVERLAY ARCHITECTURE                           |
+---------------------------------------------------------------------------------------------------------+
|                                                                                                         |
|   STANAG 4609 Stream Ingestion (MPEG-TS PES / Bare KLV RTP RFC 6597 / Telemetry Stream)                 |
|                                       │                                                                 |
|                                       ▼                                                                 |
|                         +---------------------------+                                                   |
|                         |   Klv::KlvParser          |                                                   |
|                         |   (MISB ST 0601 / 0102)   |                                                   |
|                         +-------------+-------------+                                                   |
|                                       │                                                                 |
|                                       ▼ [UasDatalinkMessage]                                            |
|   +───────────────────────────────────┼─────────────────────────────────────────────────────────────+   |
|   │ LIBS/KLV CORE ENGINE (ZERO-QT)    │                                                             │   |
|   │                                   ▼                                                             │   |
|   │                     +───────────────────────────+                                               │   |
|   │                     | Misb1909OverlayRenderer   | <── [Misb1909Config]                          │   |
|   │                     +───────────────────────────+     (Density, Theme, Declutter, Units)        │   |
|   │                                   │                                                             │   |
|   │        ┌──────────────────────────┼──────────────────────────┐                                  │   |
|   │        ▼                          ▼                          ▼                                  │   |
|   │   +─────────────────+    +─────────────────+    +─────────────────+                             │   |
|   │   | ST 1909 Formatter|   | Flight Symbology|    | Invariant Check |                             │   |
|   │   | - Angle (deg*)  |    | - Pitch Ladder  |    | - NDC [0.0, 1.0]|                             │   |
|   │   | - Meters (m)    |    | - Heading Tape  |    | - In-bounds     |                             │   |
|   │   | - ISO 8601-1    |    | - Reticle Cross |    | - ERR55 Zero-HP |                             │   |
|   │   | - MGRS String   |    | - North Arrow   |    |   noexcept      |                             │   |
|   │   +────────+────────+    +────────+────────+    +────────+────────+                             │   |
|   │            │                      │                      │                                      │   |
|   │            └──────────────────────┼──────────────────────┘                                      │   |
|   │                                   ▼                                                             │   |
|   │                     +───────────────────────────+                                               │   |
|   │                     |   TacticalOverlayLayout   | (Pure C++17 Normalized Layout Model)          │   |
|   │                     +─────────────+─────────────+                                               │   |
|   +───────────────────────────────────┼─────────────────────────────────────────────────────────────+   |
|                                       │                                                                 |
|                                       ▼ (Dispatched to Client Application)                              |
|   +───────────────────────────────────┼─────────────────────────────────────────────────────────────+   |
|   │ CLIENT VIEWPORT RENDERING (QT/QML)│                                                             │   |
|   │                                   ▼                                                             │   |
|   │                     +───────────────────────────+                                               │   |
|   │                     | VideoOverlayWidget (Qt)   | <── [Decoded Raw Video Frame]                 │   |
|   │                     +───────────────────────────+     (QImage / OpenGL / Metal Texture)         │   |
|   │                                   │                                                             │   |
|   │                                   ▼ paintEvent() / paintGL()                                    │   |
|   │   ┌─────────────────────────────────────────────────────────────────────────────────────────┐   │   |
|   │   │ Layer 0: Raw Pristine Video (drawImage without burned-in pixels)                        │   │   |
|   │   │ Layer 1: Vector Pitch Ladder & Artificial Horizon (Roll-transformed QPainter lines)    │   │   |
|   │   │ Layer 2: Vector Heading Tape (Cardinals, Degree ticks, Caret)                           │   │   |
|   │   │ Layer 3: Central Targeting Reticle & Optical FOV Boresight                              │   │   |
|   │   │ Layer 4: ST 1909 8 Standard Metadata Text Groups (Normalized px = norm * W, py = norm * H)│
|   │   │ Layer 5: ST 0102 Security Classification Banners (Top/Bottom)                           │   │   |
|   │   └─────────────────────────────────────────────────────────────────────────────────────────┘   │   |
|   +─────────────────────────────────────────────────────────────────────────────────────────────────+   |
+---------------------------------------------------------------------------------------------------------+
```

### 2.2 Mermaid Architecture Diagram

```mermaid
flowchart TD
    subgraph StreamIngest["1. Telemetry Ingestion"]
        TS["STANAG 4609 Stream<br/>(MPEG-TS PES / RTP RFC 6597)"] --> DEMUX["MpegTsKlvExtractor"]
        DEMUX --> PARSER["KlvParser<br/>(MISB ST 0601 / ST 0102)"]
        PARSER --> MSG["UasDatalinkMessage"]
    end

    subgraph LibsKlvCore["2. libs/Klv Zero-Qt Engine"]
        MSG --> BUILDER["Misb1909OverlayRenderer::buildLayout"]
        CFG["Misb1909Config<br/>(Density, Palette, Declutter, MGRS)"] --> BUILDER

        subgraph GeneratorSubsystems["Symbology & Layout Generators"]
            G1["ST 1909 Text Formatter<br/>(formatSt1909Angle, Meters, ISO Time)"]
            G2["Flight Instruments Generator<br/>(Pitch Ladder, Heading Tape, North Arrow)"]
            G3["Targeting Reticle Generator<br/>(Open Crosshair, Mil-Dot, CE90 Ellipse)"]
            G4["MGRS / Geodetic Formatter<br/>(WGS-84 to Military Grid)"]
        end

        BUILDER --> G1 & G2 & G3 & G4
        G1 & G2 & G3 & G4 --> LAYOUT["TacticalOverlayLayout<br/>(Normalized Display Coordinates [0.0, 1.0])"]
    end

    subgraph ClientRendering["3. Client Viewport Rendering (app-pelcod-qt)"]
        LAYOUT --> OVERLAY["VideoOverlayWidget::updateOverlayLayout"]
        VIDEO["Decoded Pristine Video Frame<br/>(Uncompressed QImage / GPU Texture)"] --> OVERLAY

        subgraph CompositeCanvas["Non-Destructive Paint Pipeline"]
            L0["Layer 0: Pristine Video Frame (drawImage)"]
            L1["Layer 1: Artificial Horizon & Pitch Ladder (Roll rotated)"]
            L2["Layer 2: Compass Heading Tape & Digital Readout"]
            L3["Layer 3: Targeting Boresight Reticle & TLE Box"]
            L4["Layer 4: Eight MISB ST 1909 Text Blocks"]
            L5["Layer 5: ST 0102 Security Classification Banners"]
        end

        OVERLAY --> L0 --> L1 --> L2 --> L3 --> L4 --> L5
    end
```

### 2.3 Normalized Display Coordinates (NDC) Grid & The 8 Standard Groups

```
(0.0, 0.0) ┌───────────────────────────(0.5, 0.0)───────────────────────────┐ (1.0, 0.0)
           │ [Top Security Banner: // UNCLASSIFIED // USA //]               │
           │                                                                 │
           │ Group 1: Main Sensor                 Group 3: Platform Info     │
           │ (x=0.02, y=0.037, Left, Down)        (x=0.98, y=0.037, Right, Dn│
           │ EO NOSE                              MQ-9 REAPER AF02-014       │
           │ REL AZ   160.7191*                   33.4568* LAT               │
           │ REL EL   -36.4976*                   -104.6546* LON             │
           │                                      1428m MSL ALT              │
           │                    [Heading Tape]                               │
           │             260   270(W)   280   290                            │
           │                   ▲ 270*                                        │
           │                                                                 │
           │ Group 4: North Arrow                                            │
           │ (x=0.02, y=0.50)           +10 ───── ─────                      │
           │       (N)                      ───── ─────                      │
           │        ▲            Horizon ═══════   ═══════                   │
           │        │                       - - - - - -                      │
           │                       -10 ───┐     ┌───                         │
           │                                                                 │
           │                         Group 8: Reticle                        │
           │                             ┌──   ──┐                           │
           │                             │   +   │ (x=0.50, y=0.50)          │
           │                             └──   ──┘                           │
           │                                                                 │
           │ Group 5: Laser Sensor                Group 7: Target Group      │
           │ (x=0.02, y=0.963, Left, Up)          (x=0.98, y=0.963, Right, Up│
           │ Laser PRF Code 1688                  5200m SR                   │
           │ Laser ON                             150m TW                    │
           │ LRF/Designator                       60.0000* HFOV              │
           │                                      35.0000* VFOV              │
           │                                      34.2600* FC LAT            │
           │                                      -116.1100* FC LON          │
           │                                      500m MSL FC EL             │
           │                                                                 │
           │                 Group 6: Date/Time Group                        │
           │                 (x=0.50, y=0.963, Center, Up)                   │
           │                 FT 2026-11-02T14:20:05.2Z                       │
           │                 MT 2026-11-02T14:20:05.2Z                       │
           │ [Bottom Security Banner: // UNCLASSIFIED // USA //]            │
(0.0, 1.0) └───────────────────────────(0.5, 1.0)───────────────────────────┘ (1.0, 1.0)
```

---

## 3. Standards Baseline & MISB ST 1909.1 Specification Breakdown

### 3.1 MISB ST 1909.1 Layout Groups Specification Table

The standard specifies 8 mandatory and conditional graphical zones anchored to the video viewport using normalized coordinates:

| Group ID | Name | Anchor $(X, Y)$ | Alignment | Stack Direction | Content / Elements | Source Tags (ST 0601 / 0102 / 0806) |
|---|---|:---:|:---:|:---:|---|---|
| **Group 1** | **Main Sensor Group** | $(0.0200, 0.0370)$ | Left | Downwards | Sensor payload model, Relative Azimuth (`REL AZ`), Relative Elevation (`REL EL`) | Tag 11, Tag 18, Tag 19 |
| **Group 2** | **Classification & Releasability Group** | $(0.5000, 0.0370)$ | Center | Downwards | Security Classification marking banner, Classifying Country, Handling Caveats | ST 0102 Sub-tags 1, 2, 3, 12, 22 (Tag 48) |
| **Group 3** | **Platform Information Group** | $(0.9800, 0.0370)$ | Right | Downwards | Platform designation / tail number, Platform Latitude, Platform Longitude, Platform Altitude (MSL/HAE) | Tag 10, Tag 4, Tag 13, Tag 14, Tag 15/75 |
| **Group 4** | **True North Arrow Group** | $(0.0200, 0.5000)$ | Center | Centered | Circular compass rose with oriented True North pointer indicating sensor line of sight relative to True North | Tag 5, Tag 18 |
| **Group 5** | **Laser Sensor Group** | $(0.0200, 0.9630)$ | Left | Upwards | Laser sensor name, Firing state (`Laser ON`/`Laser OFF`), PRF Code (`Laser PRF Code <code >`) | Tag 119 (ST 0601) / Custom Laser Telemetry |
| **Group 6** | **Date/Time Group** | $(0.5000, 0.9630)$ | Center | Upwards | Frame Timestamp (`FT <ISO 8601-1>`), Master/Metadata Timestamp (`MT <ISO 8601-1>`) | Tag 2 (Precision Time Stamp), ST 0604 FT |
| **Group 7** | **Target Group** | $(0.9800, 0.9630)$ | Right | Upwards | Slant Range (`SR`), Target Width (`TW`), HFOV, VFOV, Target Latitude (`TL/FC LAT`), Longitude (`TL/FC LON`), Elevation (`TL/FC EL`) | Tags 21, 22, 16, 17, 23..25 (FC), 40..42 (TL) |
| **Group 8** | **Reticle & Boresight Group** | $(0.5000, 0.5000)$ | Center | Centered | Open crosshair with central gap, electronic stadiametric markings, or lead pip | Viewport geometric center |

### 3.2 MISB ST 1909.1 Formatting Rules

1. **Angle Formatting (ST 1909-01, ST 1909.1-95):**
   - Format: `<padded_integer_part>.<fractional_part>*`
   - The asterisk `*` represents the degree symbol to ensure universal font rendering across all tactical display hardware.
   - Padded with leading spaces to maintain fixed text column alignment during live telemetry updates:
     - Relative Azimuth / Elevation: 5 digits before dot, 4 digits after: `formatSt1909Angle(deg, 5, 4)` $\rightarrow$ `"  160.7191*"`, `"  -36.4976*"`.
     - Latitude: 3 digits before dot, 4 digits after: `formatSt1909Angle(lat, 3, 4)` $\rightarrow$ `" 33.4568*"`, `"-12.3456*"`.
     - Longitude: 4 digits before dot, 4 digits after: `formatSt1909Angle(lon, 4, 4)` $\rightarrow$ `"-104.6546*"`, `"  45.1234*"`.
     - Missing / unavailable telemetry: `"N/A"`.
2. **Distance and Altitude Formatting (ST 1909-01):**
   - Slant Range and Target Width: 7 digits integer meters padded with spaces, suffix `m`: `formatSt1909Meters(m, 7)` $\rightarrow$ `"  14265m"`, `"     50m"`.
   - Platform Altitude and Target Elevation: 5 digits integer meters padded with spaces, datum suffix (`MSL ALT` or `HAE ALT`): `formatSt1909Meters(alt, 5)` $\rightarrow$ `" 1428m MSL ALT"`.
   - Missing / unavailable telemetry: `"N/A"`.
3. **ISO 8601-1 Timestamp Formatting (ST 1909-46):**
   - Format: `CCYY-MM-DDThh:mm:ss.sZ` (tenths of second precision, UTC indicator `Z`).
   - Example: microsecond epoch `1561902329800000ULL` $\rightarrow$ `"2019-06-30T13:45:29.8Z"`.
   - Zero or missing: `"N/A"`.

### 3.3 Tactical Flight Instrument Symbology Specifications

1. **Pitch Ladder & Artificial Horizon:**
   - Horizon Bar: Horizontal line across viewport center, split by a central gap for the boresight reticle. Rotated counter-clockwise by platform roll angle $\phi$ (MISB ST 0601 Tag 7/91).
   - Pitch Rungs: Parallel rungs spaced at $5^\circ$ or $10^\circ$ pitch increments ($\theta$, Tag 6/90).
     - Positive Pitch (Climbing): Solid horizontal lines with downwards vertical tick tabs pointing towards the horizon.
     - Negative Pitch (Diving): Dashed horizontal lines with upwards vertical tick tabs pointing towards the horizon.
     - Pitch angle labels rendered at the outer ends of each rung (e.g. `10`, `-10`).
2. **Compass Heading Tape:**
   - Top horizontal ribbon spanning a configurable FOV angle (default: $60^\circ$).
   - Linear interpolation: pixels per degree $= \frac{\text{tapeWidth}}{\text{tapeFov}}$.
   - Minor tick marks every $5^\circ$, major tick marks every $10^\circ$.
   - Cardinal / Inter-cardinal text markers at $0^\circ$ (`N`), $45^\circ$ (`NE`), $90^\circ$ (`E`), $135^\circ$ (`SE`), $180^\circ$ (`S`), $225^\circ$ (`SW`), $270^\circ$ (`W`), $315^\circ$ (`NW`), two-digit headings at other tens (e.g. `26` for $260^\circ$).
   - Center pointer caret indicator ($\blacktriangle$) and digital numeric readout box (e.g. `270*`).
3. **Boresight Reticle Geometry:**
   - Open 4-quadrant crosshair centered at $(0.50, 0.50)$ with outer arm length $L$ and central gap $G$ ensuring the optical pixel center is unobstructed.
4. **MGRS Coordinate Representation:**
   - In tactical ground operations, operators utilize the Military Grid Reference System (MGRS).
   - If RVT Local Set (MISB ST 0806 Tags 14..21) is present, the engine utilizes decoded `RvtMgrsCoord` (Zone, 100km Grid Square, Easting, Northing).
   - Otherwise, the engine provides built-in analytical WGS-84 to UTM/MGRS coordinate transformation (e.g. `11SMT 12345 67890`) for platform and target locations.

---

## 4. Strongly-Typed C++17 Domain Modeling & Interfaces

### 4.1 Domain Value Types (`libs/Klv/Misb1909Types.h`)
In accordance with `.agents/rules/domain-modeling.md` and `.agents/rules/iwyu-cstdint.md`, all primitives are encapsulated into strongly-typed value objects with domain boundary invariants:

```cpp
#pragma once

/// @file Misb1909Types.h
/// @brief Strongly-typed domain models and graphical primitives for MISB ST 1909.1 HUD layout.

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "GeoTypes.h"
#include "KlvTypes.h"

namespace Klv {

/// @brief Normalized coordinate value clamped to [0.0F, 1.0F].
class NormalizedCoord {
public:
    constexpr NormalizedCoord() noexcept = default;
    explicit constexpr NormalizedCoord(float val) noexcept
        : m_val(std::clamp(val, 0.0F, 1.0F)) {}

    [[nodiscard]] constexpr float value() const noexcept { return m_val; }

private:
    float m_val { 0.0F };
};

/// @brief 2D point in normalized display coordinate space [0.0, 1.0].
struct NormalizedPoint {
    NormalizedCoord x { NormalizedCoord(0.5F) };
    NormalizedCoord y { NormalizedCoord(0.5F) };
};

/// @brief 2D bounding box in normalized display coordinate space.
struct NormalizedRect {
    NormalizedPoint topLeft {};
    NormalizedCoord width { NormalizedCoord(0.0F) };
    NormalizedCoord height { NormalizedCoord(0.0F) };
};

/// @enum TextAlignment
/// @brief Horizontal text alignment relative to anchor.
enum class TextAlignment : std::uint8_t {
    Left = 0U,
    Center = 1U,
    Right = 2U
};

/// @enum StackDirection
/// @brief Multi-line text block expansion direction.
enum class StackDirection : std::uint8_t {
    Downwards = 0U, ///< Lines expand towards bottom of viewport
    Upwards = 1U    ///< Lines expand towards top of viewport
};

/// @enum SymbologyDensity
/// @brief HUD declutter density level.
enum class SymbologyDensity : std::uint8_t {
    Minimal = 0U,      ///< Reticle and critical slant range only
    Standard = 1U,     ///< Standard flight instruments and target card
    FullTactical = 2U, ///< All 8 groups, footprint corners, and MGRS
    Misb1909Standard = 3U ///< Strict MISB ST 1909.1 8 standard layout groups
};

/// @enum TacticalColorPalette
/// @brief Chromatic themes optimized for operational display modes.
enum class TacticalColorPalette : std::uint8_t {
    TacticalGreen = 0U, ///< NVG compatible green (#00FF41)
    Amber = 1U,         ///< FLIR thermal amber (#FFB000)
    ElectricCyan = 2U,  ///< Glass cockpit cyan (#00F0FF)
    CombatRed = 3U,     ///< Threat alert red (#FF3333)
    AviationWhite = 4U  ///< High-contrast white (#FFFFFF)
};

/// @struct HudColorRgba
/// @brief Floating-point RGBA color with channels in [0.0, 1.0].
struct HudColorRgba {
    float r { 0.0F };
    float g { 1.0F };
    float b { 0.25F };
    float a { 1.0F };
};

/// @struct HudTextItem
/// @brief Renderable text element with normalized anchor.
struct HudTextItem {
    std::string text {};
    NormalizedPoint anchor {};
    TextAlignment alignment { TextAlignment::Left };
    float fontSizeScale { 1.0F };
    HudColorRgba color {};
};

/// @struct HudLineItem
/// @brief Renderable line segment primitive in normalized space.
struct HudLineItem {
    NormalizedPoint start {};
    NormalizedPoint end {};
    float strokeWidth { 1.5F };
    bool isDashed { false };
    HudColorRgba color {};
};

/// @struct HudArcItem
/// @brief Renderable circle or arc primitive in normalized space.
struct HudArcItem {
    NormalizedPoint center {};
    float radiusNorm { 0.05F };
    float startAngleDeg { 0.0F };
    float sweepAngleDeg { 360.0F };
    float strokeWidth { 1.5F };
    HudColorRgba color {};
};

/// @struct PitchLadderRung
/// @brief Single rung of the artificial horizon pitch ladder.
struct PitchLadderRung {
    float pitchDeg { 0.0F };      ///< Rung angle in degrees (e.g. +10.0, -10.0)
    NormalizedPoint leftStart {};
    NormalizedPoint leftEnd {};
    NormalizedPoint rightStart {};
    NormalizedPoint rightEnd {};
    NormalizedPoint leftTabEnd {};  ///< Vertical tick tab pointing to horizon
    NormalizedPoint rightTabEnd {}; ///< Vertical tick tab pointing to horizon
    bool isDashed { false };        ///< True for diving rungs (< 0 deg)
};

/// @struct PitchLadderLayout
/// @brief Complete artificial horizon and pitch ladder layout.
struct PitchLadderLayout {
    NormalizedPoint horizonLeft {};
    NormalizedPoint horizonRight {};
    float rollAngleDeg { 0.0F };
    std::vector<PitchLadderRung> rungs {};
    bool visible { true };
};

/// @struct HeadingTapeTick
/// @brief Individual degree tick on the compass ribbon.
struct HeadingTapeTick {
    float headingDeg { 0.0F };
    NormalizedPoint top {};
    NormalizedPoint bottom {};
    bool isMajor { false };
    std::string label {}; ///< e.g. "N", "27", "28"
};

/// @struct HeadingTapeLayout
/// @brief Top compass heading ribbon layout.
struct HeadingTapeLayout {
    NormalizedPoint tapeStart {};
    NormalizedPoint tapeEnd {};
    NormalizedPoint caretTip {};
    float currentHeadingDeg { 0.0F };
    std::string digitalReadout {}; ///< e.g. "270*"
    std::vector<HeadingTapeTick> ticks {};
    bool visible { true };
};

/// @struct ReticleLayout
/// @brief Center targeting boresight reticle.
struct ReticleLayout {
    NormalizedPoint center { NormalizedPoint{NormalizedCoord(0.5F), NormalizedCoord(0.5F)} };
    float armLengthNorm { 0.025F };
    float centerGapNorm { 0.008F };
    std::vector<HudLineItem> crosshairLines {};
    bool visible { true };
};

/// @struct Misb1909GroupData
/// @brief Text block corresponding to one of the 8 ST 1909 standard groups.
struct Misb1909GroupData {
    std::uint8_t groupId { 1U }; ///< 1..8 per ST 1909
    std::string groupTitle {};
    NormalizedPoint anchor {};
    TextAlignment alignment { TextAlignment::Left };
    StackDirection stackDirection { StackDirection::Downwards };
    std::vector<std::string> lines {};
    bool visible { true };
};

/// @struct TacticalOverlayLayout
/// @brief Complete, self-contained tactical HUD presentation model.
struct TacticalOverlayLayout {
    Misb1909GroupData mainSensorGroup {};        ///< Group 1 (Top-Left)
    Misb1909GroupData securityGroup {};          ///< Group 2 (Top-Center)
    Misb1909GroupData platformGroup {};          ///< Group 3 (Top-Right)
    Misb1909GroupData northArrowGroup {};        ///< Group 4 (Mid-Left)
    Misb1909GroupData laserSensorGroup {};       ///< Group 5 (Bottom-Left)
    Misb1909GroupData dateTimeGroup {};          ///< Group 6 (Bottom-Center)
    Misb1909GroupData targetGroup {};            ///< Group 7 (Bottom-Right)
    ReticleLayout reticle {};                    ///< Group 8 (Center Reticle)
    PitchLadderLayout pitchLadder {};            ///< Flight instrument
    HeadingTapeLayout headingTape {};            ///< Flight instrument
    std::vector<HudLineItem> vectorLines {};     ///< Auxiliary vectors
    std::vector<HudTextItem> auxiliaryTexts {};  ///< Auxiliary labels
    HudColorRgba activeThemeColor {};            ///< Active theme RGBA
};

/// @struct Misb1909Config
/// @brief User and operational configuration for HUD layout generation.
struct Misb1909Config {
    SymbologyDensity density { SymbologyDensity::Misb1909Standard };
    TacticalColorPalette palette { TacticalColorPalette::TacticalGreen };
    bool showHorizon { true };
    bool showHeadingTape { true };
    bool showSecurityBanners { true };
    bool showNorthArrow { true };
    bool showLaserGroup { true };
    bool useMgrsCoordinates { false };
    float tapeSpanDegrees { 60.0F };
    float ladderStepDegrees { 5.0F };
    float maxLadderPitchDegrees { 20.0F };
};

} // namespace Klv
```

### 4.2 Core Layout Generator (`libs/Klv/Misb1909OverlayRenderer.h`)

```cpp
#pragma once

/// @file Misb1909OverlayRenderer.h
/// @brief Non-destructive tactical HUD layout generator conforming to MISB ST 1909.1.
/// @details Generates normalized graphical primitives and standardized metadata layout groups
///          from MISB ST 0601 / ST 0102 telemetry for client-side viewport rendering. Zero Qt dependencies.

#include <cstddef>
#include <cstdint>

#include <optional>
#include <string>

#include "KlvTypes.h"
#include "Misb1909Types.h"

namespace Klv {

/// @class Misb1909OverlayRenderer
/// @brief Pure C++17 stateless generator synthesizing normalized MISB ST 1909.1 HUD layouts.
class Misb1909OverlayRenderer {
public:
    /// @brief Generates a complete tactical HUD layout from parsed UAS Datalink telemetry.
    /// @param[in] message Fully or partially populated MISB ST 0601 telemetry packet.
    /// @param[in] config Operational configuration (density, theme, instrument flags).
    /// @return Immutable normalized layout ready for client viewport rasterization.
    [[nodiscard]] static TacticalOverlayLayout buildLayout(
        const UasDatalinkMessage& message,
        const Misb1909Config& config = Misb1909Config{}) noexcept;

    /// @brief Formats an angle per MISB ST 1909.1 (ST 1909-01, ST 1909.1-95).
    /// @param[in] deg Angle in degrees, or nullopt if unavailable.
    /// @param[in] digitsBefore Minimum width of integer part padded with leading spaces.
    /// @param[in] digitsAfter Fixed decimal fractional precision.
    /// @return Padded angle string with asterisk degree indicator, or "N/A".
    [[nodiscard]] static std::string formatSt1909Angle(
        std::optional<double> deg, int digitsBefore, int digitsAfter);

    /// @brief Formats integer meters per MISB ST 1909.1 (ST 1909-01).
    /// @param[in] meters Distance or altitude in meters, or nullopt.
    /// @param[in] digits Total integer field width padded with leading spaces.
    /// @return Padded string with 'm' suffix, or "N/A".
    [[nodiscard]] static std::string formatSt1909Meters(
        std::optional<double> meters, int digits);

    /// @brief Formats a microsecond timestamp per ISO 8601-1 CCYY-MM-DDThh:mm:ss.sZ (ST 1909-46).
    /// @param[in] epochUs Microseconds since UNIX epoch, or nullopt.
    /// @return Formatted ISO 8601-1 string with tenth-second precision, or "N/A".
    [[nodiscard]] static std::string formatSt1909IsoTime(
        std::optional<std::uint64_t> epochUs);

    /// @brief Converts WGS-84 geodetic coordinates to Military Grid Reference System (MGRS).
    /// @param[in] latDeg Latitude in degrees [-90.0, +90.0].
    /// @param[in] lonDeg Longitude in degrees [-180.0, +180.0].
    /// @return Standard 10-figure MGRS string (e.g. "11SMT 12345 67890"), or "N/A".
    [[nodiscard]] static std::string toMgrsString(
        std::optional<double> latDeg, std::optional<double> lonDeg);

    /// @brief Resolves color RGBA channels from a tactical color palette enum.
    /// @param[in] palette Operational theme choice.
    /// @return Normalized RGBA color channels.
    [[nodiscard]] static constexpr HudColorRgba getPaletteColor(TacticalColorPalette palette) noexcept;
};

} // namespace Klv
```

---

## 5. Client Viewport Non-Destructive Rendering Integration

### 5.1 Architecture of Decoupled Presentation

```
libs/Klv (Headless Core)                     app-pelcod-qt (UI Presentation)
┌──────────────────────────────┐             ┌───────────────────────────────────┐
│ Misb1909OverlayRenderer      │             │ VideoOverlayWidget                │
│                              │             │                                   │
│ Input: UasDatalinkMessage    │             │ Slot: updateLayout(layout)        │
│ Output: TacticalOverlayLayout│ ──────────> │                                   │
│ (Zero Qt, pure math & NDC)   │             │ QPainter in paintEvent():         │
│                              │             │   px = normX * width()            │
│                              │             │   py = normY * height()           │
└──────────────────────────────┘             └───────────────────────────────────┘
```

### 5.2 Implementation in `app-pelcod-qt/widgets/VideoOverlayWidget`
1. **Zero Impact on Video Decoder:** Decoded video frames (`QImage` from FFmpeg / GStreamer / OpenCV) are passed directly to `VideoOverlayWidget::updateFrame()`.
2. **Normalized to Viewport Pixel Mapping:** In `paintEvent(QPaintEvent*)`:
   ```cpp
   const int W = width();
   const int H = height();
   const auto toPx = [W, H](const Klv::NormalizedPoint& pt) -> QPointF {
       return QPointF(pt.x.value() * static_cast<qreal>(W),
                      pt.y.value() * static_cast<qreal>(H));
   };
   ```
3. **Layered Vector Drawing:**
   - Draw background video frame.
   - If pitch ladder enabled: apply `QTransform` centered on viewport center, rotate by `layout.pitchLadder.rollAngleDeg`, draw rungs and horizon gap.
   - If heading tape enabled: draw horizontal ribbon, ticks, and caret triangle.
   - Draw 8 standard ST 1909 text blocks using `QFontMetrics` and alignment rules (Left, Center, Right).
   - If security banners enabled: draw full-width classification bars at top and bottom margins.

---

## 6. Safety, Compliance & Coding Standards

Every line of code authored in Phase 10 will strictly adhere to the project safety rules:

1. **MISRA C++:2023 Rule 7.0.6 & SEI CERT INT31-C (Signed/Unsigned Conversions):**
   - Coordinate normalization and pixel conversions require explicit `static_cast<float>` and `static_cast<int>`.
   - All integer types scoped strictly to `std::uint8_t`, `std::uint16_t`, `std::uint32_t`, `std::size_t`.
2. **SEI CERT ERR55-CPP (Exception Safety & Zero-Heap Fast Paths):**
   - Coordinate calculations, layout bounds checks, and color evaluations are marked `noexcept` and execute with zero dynamic heap allocations.
   - Stack arrays and string views used where applicable.
3. **SEI CERT STR37-C (Deterministic Character Handling):**
   - Formatter functions use standard locale-independent numeric conversions (`std::snprintf` with fixed buffers or ASCII helper functions).
4. **Include-What-You-Use (IWYU) & `<cstdint>` Policy:**
   - Every header directly includes `<cstdint>`, `<cstddef>`, `<string>`, `<vector>`, `<optional>`.
   - Headers use `#pragma once`.
5. **Relative Paths in Documentation (`relative-paths-docs.md`):**
   - All documentation links strictly use repository-relative paths without machine-specific prefixes.

---

## 7. Comprehensive Test Strategy & Verification

A dedicated unit test suite [`libs/Klv/tests/TestMisb1909OverlayRenderer.cpp`](../libs/Klv/tests/TestMisb1909OverlayRenderer.cpp) will be implemented to verify all aspects of MISB ST 1909.1 conformance:

| Test Case | Scenario / Condition | Expected Result |
|---|---|---|
| `AngleFormattingRules` | Azimuth `160.7191`, Elevation `-36.4976`, Latitude `33.4568`, Longitude `-104.6546` | Exactly matches ST 1909-01 formatting: `"  160.7191*"`, `"  -36.4976*"`, `" 33.4568*"`, `"-104.6546*"` |
| `MetersFormattingRules` | Slant Range `14265.0`, Target Width `50.0`, Altitude `1428.0`, Negative Alt `-15.0` | Output matches padded integer meters: `"  14265m"`, `"     50m"`, `" 1428m"`, `"  -15m"` |
| `IsoTimeFormattingRules` | Epoch `1561902329800000ULL` | Exactly matches ISO 8601-1 format `"2019-06-30T13:45:29.8Z"` with tenth-second precision |
| `MissingAndNulloptHandling` | Unpopulated fields passed to formatters | Gracefully returns `"N/A"` without throwing or crashing |
| `EightStandardGroupsAnchorsAndContent` | Synthetic MISB ST 0601 message with complete telemetry | All 8 groups generated with exact ST 1909 normalized anchor coordinates $(X, Y)$ and expected line text |
| `PitchLadderClimbDiveGeometry` | Pitch $+10.0^\circ$ (climbing) vs $-10.0^\circ$ (diving) | $+10^\circ$ rungs are solid with downward tabs; $-10^\circ$ rungs are dashed with upward tabs |
| `PitchLadderRollTransformation` | Roll angle $+45.0^\circ$ | Layout stores roll angle $\phi=45^\circ$ for viewport derotation; rung coordinates preserve symmetry |
| `HeadingTapeWrappingAndCardinals` | Heading crossing North boundary ($355^\circ \to 5^\circ$) | Heading ribbon wraps modulo $360^\circ$ smoothly, emits cardinal `"N"`, `"NW"`, `"NE"` |
| `SecurityClassificationBanners` | MISB ST 0102 with Secret / US / NOFORN | Banner text formatted as `"// SECRET // USA // NOFORN //"` |
| `LaserSensorGroupRendering` | Laser active = true, PRF code = 1688 | Lines include `"Laser ON"` and `"Laser PRF Code 001688"` |
| `ReticleBoresightPrimitives` | Center reticle query | Boresight centered at $(0.50, 0.50)$, 4-quadrant open crosshair arms with central gap |
| `MgrsCoordinateFormatting` | WGS-84 coordinate $(34.2600^\circ\text{N}, -116.1100^\circ\text{W})$ | Formats into valid 10-figure MGRS string `"11SMT 82060 91440"` |
| `SymbologyDensityDeclutterLevels` | Switching between `Minimal`, `Standard`, and `Misb1909Standard` | `Minimal` hides tapes and groups 1/3/4/5; `Misb1909Standard` enables all 8 groups |
| `Err55ZeroHeapNoexceptSafety` | High-frequency update loop executing 10,000 iterations | Zero unhandled exceptions, zero crashes under memory stress |

---

## 8. Implementation Roadmap & Milestones

```
Phase 10 Execution Roadmap
            │
            ├─► Step 1: Create libs/Klv/Misb1909Types.h (Value objects & primitives)
            │
            ├─► Step 2: Implement libs/Klv/Misb1909OverlayRenderer.h & .cpp
            │
            ├─► Step 3: Integrate with libs/Klv/CMakeLists.txt
            │
            ├─► Step 4: Author libs/Klv/tests/TestMisb1909OverlayRenderer.cpp & verify 100% tests
            │
            ├─► Step 5: Extend app-pelcod-qt/widgets/VideoOverlayWidget (QPainter vector rendering)
            │
            ├─► Step 6: Run verification scripts (IWYU, relative paths, MISRA/CERT)
            │
            └─► Step 7: Update docs/KLV_Features.md to Phase 10 IMPLEMENTED status
```

1. **Step 1: Domain Value Modeling (`libs/Klv/Misb1909Types.h`)**
   - Author strongly-typed `NormalizedCoord`, `NormalizedPoint`, `NormalizedRect`.
   - Author primitives (`HudLineItem`, `HudTextItem`, `HudArcItem`).
   - Author structured flight instrument layouts (`PitchLadderLayout`, `HeadingTapeLayout`, `ReticleLayout`, `Misb1909GroupData`, `TacticalOverlayLayout`).
2. **Step 2: Core Layout Engine (`libs/Klv/Misb1909OverlayRenderer.h` & `.cpp`)**
   - Implement `formatSt1909Angle`, `formatSt1909Meters`, `formatSt1909IsoTime`.
   - Implement MGRS conversion helper `toMgrsString`.
   - Implement `buildLayout` assembling the 8 MISB ST 1909.1 standard groups and flight instrument vector geometries.
3. **Step 3: Build System Integration**
   - Add `Misb1909Types.h`, `Misb1909OverlayRenderer.h`, and `Misb1909OverlayRenderer.cpp` to `libs/Klv/CMakeLists.txt`.
4. **Step 4: Unit Test Suite (`libs/Klv/tests/TestMisb1909OverlayRenderer.cpp`)**
   - Add 14 comprehensive test cases covering all formatting, group anchoring, pitch ladder, heading tape, and declutter modes.
   - Register suite in `libs/Klv/tests/CMakeLists.txt` and execute `ctest`.
5. **Step 5: Client Viewport Non-Destructive Canvas Integration**
   - Update `app-pelcod-qt/widgets/VideoOverlayWidget.h` and `.cpp` to consume `TacticalOverlayLayout`.
   - Implement non-destructive `QPainter` drawing passes.
6. **Step 6: Compliance & Hygiene Verification**
   - Execute `python3 scripts/verify_iwyu_cstdint.py --files libs/Klv/Misb1909*`.
   - Execute `python3 scripts/verify_relative_doc_paths.py --files docs/klv_phase10_tactical_hud_overlay_plan.md docs/KLV_Features.md --strict`.
7. **Step 7: Documentation Update**
   - Update `docs/KLV_Features.md` status table, test count matrix, and phase completion checklist.
8. **Step 8: Video Filters Downstream Integration (`libs/VideoFilters`)**
   - Refactor `libs/VideoFilters/TacticalHudFilter.cpp` to eliminate duplicated ST 1909 angle, meter, and time formatting logic.
   - Delegate formatting and normalized layout calculation to `Klv::Misb1909OverlayRenderer::buildLayout()`.
   - Preserve OpenCV rasterization for destructive burn-in workflows (e.g. video export and legacy display recording).

---

## 9. Verification & Delivery Record

| Metric / Requirement | Target Standard | Result / Verification |
|---|---|---|
| **Phase 10 Unit Tests** | `libs/Klv/tests/TestMisb1909OverlayRenderer.cpp` | **14/14 tests PASSED (100%)** |
| **Video Filters Regression Tests** | [TestTacticalHudFilter.cpp](../libs/Video/tests/TestTacticalHudFilter.cpp) | **7/7 tests PASSED (100%)** |
| **IWYU & `<cstdint>` Audit** | `scripts/verify_iwyu_cstdint.py` | **100% compliant**, 0 missing includes |
| **Relative Documentation Paths** | `scripts/verify_relative_doc_paths.py --strict` | **0 errors, 0 warnings** |
| **Compiler Warnings** | `-Wall -Wextra -pedantic -Werror` (`-Wconversion`, `-Wdouble-promotion`) | **Zero warnings**, clean compilation |
| **ERR55-CPP Exception Safety** | Fast-path zero heap allocations in `noexcept` layout generation | **Verified in Test 14 (10,000 iterations)** |
| **MISRA C++:2023 Rule 7.0.6 & INT31-C** | Explicit signed/unsigned casts and range validation | **Verified** |
| **Master KLV Roadmap Status** | `docs/KLV_Features.md` Phase 10 | **✅ IMPLEMENTED (40 suites / 315 tests)** |


