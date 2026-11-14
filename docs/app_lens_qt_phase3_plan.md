# Implementation Plan: `app-lens-qt` Phase 3 — Optical Axis Controls, Limits & Hardware Driver Integration

## 1. Executive Summary & Objectives

### 1.1 Context & Scope
Phase 3 transitions **`app-lens-qt`** (executable target: **`LensAppQt`**) from video presentation and synthetic simulation into an operational, bi-directional optical control system. It delivers tactical actuator control interfaces, safety limit enforcement, and pluggable hardware driver integration for motorized zoom and focus camera optics.

Building upon the skeleton architecture established in Phase 1 ([`app_lens_qt_phase1_plan.md`](app_lens_qt_phase1_plan.md)) and the streaming pipeline deployed in Phase 2 ([`app_lens_qt_phase2_plan.md`](app_lens_qt_phase2_plan.md)), Phase 3 equips operators and automated testbeds with:
1. **Rotary Tactile Actuation**: Continuous rotary jog dials ([`Components/JogWheel.qml`](../app-lens-qt/qml/Components/JogWheel.qml)) and precision dual-axis sliders ([`Components/TacticalSlider.qml`](../app-lens-qt/qml/Components/TacticalSlider.qml)) enabling micro-stepping ($\pm 1, \pm 10, \pm 100, \pm 500$ encoder counts) and absolute positioning across focus and zoom axes.
2. **Unified Optical Control Deck**: An integrated tactical control panel ([`qml/LensControlPanel.qml`](../app-lens-qt/qml/LensControlPanel.qml)) providing live encoder telemetry, actuator speed profiling (Creep, Normal, Fast, Turbo), visual limit switch annunciators, and an immediate Emergency Stop (E-Stop) interlock halting all motorized axes in $< 5\text{ ms}$.
3. **Pluggable Multi-Driver Hardware Abstraction**: Seamless runtime switching between [`MockLensDriver`](../libs/LensController/MockLensDriver.h) (in-memory simulation), [`PelcoDLensDriver`](../libs/LensController/PelcoDLensDriver.h) (RS-485 / serial Pelco-D protocol), [`ViscaLensDriver`](../libs/LensController/ViscaLensDriver.h) (Sony VISCA optical camera blocks), and external proprietary vendor plugins via [`LensDriverLoader`](../libs/LensController/LensDriverLoader.h).
4. **Physical Serial Transport Plumbing**: Integration of [`Transport::SerialTransport`](../libs/Transport/SerialTransport.h) with non-blocking transmit callbacks, asynchronous byte ingestion, and multi-channel serial port collision guarding preventing hardware port resource contention.
5. **Strict Safety Compliance**: Complete enforcement of **MISRA C++:2023**, **SEI CERT C++**, **Include-What-You-Use (IWYU)**, zero-heap `noexcept` fast paths, strongly-typed domain value objects, and function names strictly under 30 characters.

### 1.2 Target Deliverables for Phase 3

| Deliverable | Target Files | Description |
| :--- | :--- | :--- |
| **Rotary Tactile Jog Dial** | [`app-lens-qt/qml/Components/JogWheel.qml`](../app-lens-qt/qml/Components/JogWheel.qml) | Tactile circular jog wheel with polar angular mouse tracking ($\arctan2$), rotational velocity calculation, tick index rings, and step multiplier buttons ($\times 1, \times 10, \times 100, \times 500$). |
| **Tactical Slider Component** | [`app-lens-qt/qml/Components/TacticalSlider.qml`](../app-lens-qt/qml/Components/TacticalSlider.qml) | High-contrast linear slider with calibrated tick marks, numerical coordinate readout, boundary clamping indicators, and direct manual input. |
| **Tactical Switch & ComboBox** | [`app-lens-qt/qml/Components/TacticalSwitch.qml`](../app-lens-qt/qml/Components/TacticalSwitch.qml)<br>[`app-lens-qt/qml/Components/TacticalComboBox.qml`](../app-lens-qt/qml/Components/TacticalComboBox.qml) | Tactical styled UI components: glowing indicator toggle switch and dark-slate dropdown selector for serial ports, baud rates, and driver types. |
| **Master Lens Control Panel** | [`app-lens-qt/qml/LensControlPanel.qml`](../app-lens-qt/qml/LensControlPanel.qml) | Main right-side optical control panel uniting dual jog wheels, absolute target inputs, speed selector dials, limit lamps, driver configuration, and prominent E-Stop. |
| **Hardware Driver Dialog** | [`app-lens-qt/qml/DriverConfigDialog.qml`](../app-lens-qt/qml/DriverConfigDialog.qml) | Modal configuration dialog for serial port allocation (COM1..COM32 / `/dev/ttyUSB0`), baud rates ($9600..115200$), Pelco-D address, and external DLL paths. |
| **ViewModel Axis Extensions** | [`app-lens-qt/LensPlayerController.h`](../app-lens-qt/LensPlayerController.h)<br>[`app-lens-qt/LensPlayerController.cpp`](../app-lens-qt/LensPlayerController.cpp) | QML properties, invokable slots, and signals for relative jogging, absolute targeting, speed assignment, driver binding, and emergency stop dispatch. |
| **Worker Hardware Integration** | [`app-lens-qt/LensWorker.h`](../app-lens-qt/LensWorker.h)<br>[`app-lens-qt/LensWorker.cpp`](../app-lens-qt/LensWorker.cpp) | Driver lifecycle management (`Mock`, `Pelco-D`, `VISCA`, `DLL`), `SerialTransport` instances, transmit callback bridges, byte reception loops, and instant E-Stop execution. |
| **Build System & Resource Manifest** | [`app-lens-qt/CMakeLists.txt`](../app-lens-qt/CMakeLists.txt)<br>[`app-lens-qt/qml.qrc`](../app-lens-qt/qml.qrc) | Updates registering new QML components and embedding tactical assets. |
| **Automated Test Suites** | [`tests/TestLensAxisControls.cpp`](../tests/TestLensAxisControls.cpp)<br>[`tests/TestLensDrivers.cpp`](../tests/TestLensDrivers.cpp) | Unit and integration test suites validating axis jogging, boundary clamping, driver instantiation, serial frame generation, and E-Stop timing ($< 5\text{ ms}$). |

---

## 2. Architecture & Subsystem Pipeline

### 2.1 Multi-Threaded Actuation & Driver Topology

The application enforces a strict separation between UI interaction, ViewModel orchestration, background thread driver execution, and physical hardware transport:
- **GUI Thread**: Captures rotary mouse wheel gestures in `JogWheel.qml`, slider drags in `TacticalSlider.qml`, and button presses. Invokes `LensPlayerController` slots which perform boundary clamping and dispatch non-blocking signals to the background worker.
- **Dedicated Worker Thread (`LensWorker`)**: Owns the [`LensController`](../libs/LensController/LensController.h) instance and polymorphic [`ILensDriver`](../libs/LensController/ILensDriver.h) instances across 4 independent camera channels ([`CameraIndex`](../libs/LensController/LensTypes.h) 0..3). Executes periodic control ticks (30 Hz), services queued motion requests, routes outgoing protocol packets to physical transports, and ingests incoming serial response telemetry.
- **Transport Layer ([`libs/Transport`](../libs/Transport))**: Handles asynchronous serial communication via [`SerialTransport`](../libs/Transport/SerialTransport.h) (Win32 Comm API on Windows, POSIX `termios` on Linux) without blocking either the GUI or the worker thread.

### ASCII Architecture Diagram

```
+-----------------------------------------------------------------------------------------------------------------------+
|                                              QT QUICK PRESENTATION THREAD                                             |
|                                                                                                                       |
|   +---------------------------------------------------------------------------------------------------------------+   |
|   |                                             qml/LensControlPanel.qml                                          |   |
|   |  +--------------------------------+  +--------------------------------+  +---------------------------------+  |   |
|   |  |           Focus Deck           |  |           Zoom Deck            |  |         Driver Deck & E-Stop    |  |   |
|   |  | - JogWheel (Micro-Stepping)    |  | - JogWheel (Micro-Stepping)    |  | - Active Driver Type Dropdown   |  |   |
|   |  | - TacticalSlider (0..65535)    |  | - TacticalSlider (0..65535)    |  | - COM Port & Baud Rate Dialog   |  |   |
|   |  | - Speed Select: Creep..Turbo   |  | - Speed Select: Creep..Turbo   |  | - Limit Lamps (Near/Far/W/T)    |  |   |
|   |  | - Limit Lamps: [Near] [Far]    |  | - Limit Lamps: [Wide] [Tele]   |  | - Prominent EMERGENCY STOP Btn  |  |   |
|   |  +--------------------------------+  +--------------------------------+  +---------------------------------+  |   |
|   +---------------------------------------------------│-----------------------------------------------------------+   |
|                                                       │ Q_INVOKABLE slots & Property Bindings                         |
|                                                       ▼                                                               |
|   +---------------------------------------------------------------------------------------------------------------+   |
|   |                                             LensPlayerController                                              |   |
|   |  - Clamps relative & absolute targets to [min..max] ranges per channel                                        |   |
|   |  - Emits requestMoveFocus, requestMoveZoom, requestSetTarget, requestBindDriver                               |   |
|   |  - Dispatches immediate requestEmergencyStop() halting all 4 channels                                         |   |
|   |  - Reflects LensStatus telemetry: currentFocus, currentZoom, limitFlags                                       |   |
|   +---------------------------------------------------│-----------------------------------------------------------+   |
+-------------------------------------------------------|---------------------------------------------------------------+
                                                        │ Qt Queued Signal / Slot
                                                        ▼
+-----------------------------------------------------------------------------------------------------------------------+
|                                                DEDICATED OPTICAL WORKER THREAD                                        |
|                                                                                                                       |
|   +---------------------------------------------------------------------------------------------------------------+   |
|   |                                                  LensWorker                                                   |   |
|   |                                                                                                               |   |
|   |   +-------------------------------------------------------------------------------------------------------+   |   |
|   |   |                                   LensController (libs/LensController)                                |   |   |
|   |   |  - Channel state machines (CameraIndex 0..3)                                                          |   |   |
|   |   |  - Range verification & software boundary enforcement (LensRanges)                                    |   |   |
|   |   |  - Active driver binding slot: setDriver(cam, std::move(driver))                                      |   |   |
|   |   +---------------------------------------------------│---------------------------------------------------+   |   |
|   |                                                       │ Polymorphic ILensDriver*                              |   |
|   |                                                       ▼                                                       |   |
|   |   +-------------------------------------------------------------------------------------------------------+   |   |
|   |   |                                      Polymorphic Hardware Driver Pool                                 |   |   |
|   |   |  - Channel 0..3: MockLensDriver | PelcoDLensDriver | ViscaLensDriver | Dynamic Dll Driver              |   |   |
|   |   |  - Transmit Callback: Formats 7-byte Pelco-D / 9-14 byte VISCA packets                                |   |   |
|   |   +---------------------------------------------------│---------------------------------------------------+   |   |
|   |                                                       │ Outgoing Bytes / Incoming Bytes                       |   |
|   |                                                       ▼                                                       |   |
|   |   +-------------------------------------------------------------------------------------------------------+   |   |
|   |   |                                 Physical Serial Transport Channel Pool                                |   |   |
|   |   |  - std::unique_ptr<Transport::SerialTransport> m_serialPorts[4]                                       |   |   |
|   |   |  - Multi-channel collision guard (ensures COM port not held by another channel)                       |   |   |
|   |   |  - Win32 Overlapped I/O / POSIX termios non-blocking serial communication                             |   |   |
|   |   +---------------------------------------------------│---------------------------------------------------+   |   |
|   +-------------------------------------------------------│-------------------------------------------------------+   |
+-----------------------------------------------------------│-----------------------------------------------------------+
                                                            ▼ Physical UART / RS-485 / RS-232
                                             [Motorized Camera Optics Hardware]
```

### 2.2 Sequence Diagram: Axis Jog, Absolute Target & Emergency Stop

```mermaid
sequenceDiagram
    autonumber
    participant UI as LensControlPanel.qml / JogWheel
    participant VM as LensPlayerController
    participant WK as LensWorker (QThread)
    participant LC as LensControl::LensController
    participant DRV as LensControl::ILensDriver (Pelco-D/VISCA)
    participant SER as Transport::SerialTransport
    participant HW as Physical Lens Motor

    Note over UI,HW: Scenario 1: Relative Focus Jog
    UI->>VM: moveFocusStep(+100)
    VM->>VM: Verify activeChannel & clamp to focusMax
    VM->>WK: requestMoveFocus(channel=0, steps=100)
    WK->>LC: moveFocusStep(CameraIndex(0), 100)
    LC->>DRV: moveFocus(100, currentSpeed)
    DRV->>SER: transmit(frameBytes, 7) via TransmitCallback
    SER->>HW: RS-485 UART Write [0xFF, 0x01, 0x00, 0x01, ...]

    Note over UI,HW: Scenario 2: Absolute Zoom Target Positioning
    UI->>VM: setZoomTarget(32000)
    VM->>WK: requestSetZoom(channel=0, position=32000)
    WK->>LC: setZoomTarget(CameraIndex(0), MotorPosition(32000))
    LC->>DRV: setZoomPosition(32000, currentSpeed)
    DRV->>SER: transmit(frameBytes, 7)
    SER->>HW: RS-485 UART Write Extended Command (0x4F)

    Note over UI,HW: Scenario 3: Telemetry Feedback Loop (30 Hz Tick)
    WK->>SER: readBytes(rxBuffer, 64)
    SER-->>WK: Ingest incoming UART response
    WK->>DRV: processIncomingBytes(data, len)
    DRV->>DRV: Update currentFocus, currentZoom, limitFlags
    WK->>LC: update(CameraIndex(0))
    LC-->>WK: LensStatus snapshot
    WK-->>VM: onStatusUpdated(focus, zoom, limits, mode, afActive)
    VM-->>UI: telemetryUpdated() -> Update UI gauges & lamps

    Note over UI,HW: Scenario 4: Instant Emergency Stop (< 5 ms)
    UI->>VM: emergencyStop()
    VM->>WK: requestEmergencyStop()
    par Halt All 4 Channels Immediately
        WK->>LC: stopMotion(CameraIndex(0))
        WK->>LC: stopMotion(CameraIndex(1))
        WK->>LC: stopMotion(CameraIndex(2))
        WK->>LC: stopMotion(CameraIndex(3))
    end
    LC->>DRV: stopMotion()
    DRV->>SER: transmit(stopFrame, len)
    SER->>HW: RS-485 Instant Halt [0xFF, addr, 0x00, 0x00, 0x00, 0x00, cksum]
```

### 2.3 Limit Switch & Mechanical Safety Architecture

Actuator safety is governed by hardware and software limit interlocks:
- **Software Boundary Clamping**: Each channel maintains configured [`LensRanges`](../libs/LensController/LensTypes.h) (`focusMin`, `focusMax`, `zoomMin`, `zoomMax`). `LensPlayerController` and `LensController` clamp all target requests to these ranges before issuing motion commands.
- **Hardware Limit Sensors**: Block cameras and lens assemblies report physical limit trip states via telemetry inquiry or limit switches:
  * `NearLimit` (`0x01`): Minimum focus distance reached (macro limit).
  * `FarLimit` (`0x02`): Maximum focus distance reached (infinity limit).
  * `WideLimit` (`0x04`): Minimum focal length reached (widest FOV).
  * `TeleLimit` (`0x08`): Maximum focal length reached (narrowest FOV).
- **Directional Interlock**: When a limit flag is active, movement commands in the direction of the tripped limit are inhibited, while reverse motion away from the limit remains enabled.

```
+-------------------------------------------------------------------------------+
|                       LIMIT SWITCH INTERLOCK STATE LOGIC                      |
|                                                                               |
|   Current Focus Encoder Position: F                                           |
|   Configured Bounds: [F_min, F_max]                                           |
|                                                                               |
|        F <= F_min OR NearLimit Tripped                                        |
|        +-------------------------------------------------------------+        |
|        | Inhibit Step Near (-steps)                                  |        |
|        | Allow Step Far (+steps)                                     |        |
|        | Annunciate Coral Warning Lamp [NEAR LIMIT]                  |        |
|        +-------------------------------------------------------------+        |
|                                                                               |
|        F >= F_max OR FarLimit Tripped                                         |
|        +-------------------------------------------------------------+        |
|        | Inhibit Step Far (+steps)                                   |        |
|        | Allow Step Near (-steps)                                    |        |
|        | Annunciate Coral Warning Lamp [FAR LIMIT]                   |        |
|        +-------------------------------------------------------------+        |
|                                                                               |
|   Emergency Stop Triggered (Any Source)                                       |
|        +-------------------------------------------------------------+        |
|        | Immediately issue stopMotion() across Channels 0..3         |        |
|        | Send 0x00 Stop Frame to active UART transceivers            |        |
|        | Latency SLA: < 5 ms total execution time                    |        |
|        +-------------------------------------------------------------+        |
+-------------------------------------------------------------------------------+
```

---

## 3. Detailed Component Specifications

### 3.1 `Components/JogWheel.qml` (Tactile Rotary Dial)

A custom high-precision tactile rotary jog dial implemented in pure Qt Quick / QML:
- Computes pointer angle $\theta = \arctan2(y - y_c, x - x_c)$ relative to dial center during mouse/touch drag.
- Detects angular delta $\Delta \theta$ across frames, unwrapping the $[-\pi, +\pi]$ branch cut to prevent discontinuous jumps.
- Converts accumulated angular displacement to discrete encoder steps using a selectable multiplier ($\times 1, \times 10, \times 100, \times 500$).
- Renders tactical tick marks, rotation needle indicator, and center text showing latest step count and direction arrow.

```qml
// app-lens-qt/qml/Components/JogWheel.qml
import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root

    property string label: "FOCUS JOG"
    property color accentColor: "#00e676"  // Emerald Focus, Cyan Zoom
    property int stepMultiplier: 10
    property bool enabled: true

    signal stepTriggered(int steps)
    signal jogStarted()
    signal jogFinished()

    implicitWidth: 160
    implicitHeight: 180

    // Component implementation details:
    // - Outer tick mark canvas (36 ticks at 10 deg intervals)
    // - Inner rotary disc rotating with accumulated angle
    // - Center display showing label, direction, and step multiplier
    // - MouseArea handling drag gestures, arctan2 calculation, and step emission
}
```

### 3.2 `Components/TacticalSlider.qml` (Precision Range Slider)

A stylized linear slider providing absolute positioning across the $0..65535$ encoder range:
- Displays numerical coordinate readout, axis limits (`focusMin`..`focusMax` or `zoomMin`..`zoomMax`), and calibrated major/minor tick marks.
- Integrates direct keyboard/numeric input enabling operators to jump directly to exact encoder coordinates.
- Visual range bar indicates active operating position with color-coded safety margins.

```qml
// app-lens-qt/qml/Components/TacticalSlider.qml
import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root

    property string label: "FOCUS POSITION"
    property int value: 0
    property int minimumValue: 0
    property int maximumValue: 65535
    property color accentColor: "#00e676"
    property bool readOnly: false

    signal valueCommitted(int newValue)

    implicitWidth: 280
    implicitHeight: 64
    // Slider groove, handle with glow halo, text edit for direct entry
}
```

### 3.3 `Components/TacticalSwitch.qml` & `Components/TacticalComboBox.qml`

- **`TacticalSwitch.qml`**: Tactical toggle button featuring a high-contrast sliding pill, glowing state LED (Emerald for active, Slate for inactive), and crisp uppercase label.
- **`TacticalComboBox.qml`**: Custom popup menu styled in `#141720` background, `#242938` borders, and `#00e5ff` highlight hover states for selecting serial ports, baud rates, and camera addresses without standard OS desktop widget styling.

### 3.4 `qml/LensControlPanel.qml` (Master Control Deck)

The unified optical command center divided into three distinct operational sections:

```
+-------------------------------------------------------------------------------+
|                       qml/LensControlPanel.qml (Width: 320px)                 |
|                                                                               |
|  +-------------------------------------------------------------------------+  |
|  | [FOCUS AXIS DECK]                                       Lamps: [N] [F]  |  |
|  |  Current: 32768               Min: 0           Max: 65535               |  |
|  |  +-----------------------+   +---------------------------------------+  |  |
|  |  |   [Focus JogWheel]    |   | [-100] [-10] [-1] | [+1] [+10] [+100] |  |  |
|  |  |   Tactile Rotary Dial |   | Tactical Micro-Stepping Pushbuttons   |  |  |
|  |  +-----------------------+   +---------------------------------------+  |  |
|  |  TacticalSlider: [====|==============]  Target Input: [ 32768 ] [GO]     |  |
|  |  Speed Preset:  ( ) Creep(32)  (*) Normal(127)  ( ) Fast(200)  ( ) Turbo|  |
|  +-------------------------------------------------------------------------+  |
|                                                                               |
|  +-------------------------------------------------------------------------+  |
|  | [ZOOM AXIS DECK]                                        Lamps: [W] [T]  |  |
|  |  Current: 14200               Min: 0           Max: 65535               |  |
|  |  +-----------------------+   +---------------------------------------+  |  |
|  |  |   [Zoom JogWheel]     |   | [-100] [-10] [-1] | [+1] [+10] [+100] |  |  |
|  |  |   Tactile Rotary Dial |   | Tactical Micro-Stepping Pushbuttons   |  |  |
|  |  +-----------------------+   +---------------------------------------+  |  |
|  |  TacticalSlider: [===|===============]  Target Input: [ 14200 ] [GO]     |  |
|  |  Speed Preset:  ( ) Creep(32)  (*) Normal(127)  ( ) Fast(200)  ( ) Turbo|  |
|  +-------------------------------------------------------------------------+  |
|                                                                               |
|  +-------------------------------------------------------------------------+  |
|  | [DRIVER & PORT BINDING]                                                 |  |
|  |  Driver: [Pelco-D Motorized] [CONFIG...]   Port: COM3 @ 115200 bps      |  |
|  |  Bus Address: [ 1 ]                        Status: CONNECTED / READY    |  |
|  +-------------------------------------------------------------------------+  |
|                                                                               |
|  +-------------------------------------------------------------------------+  |
|  | [ ! EMERGENCY STOP ! ]  (Halts all 4 channels in < 5 ms)                |  |
|  +-------------------------------------------------------------------------+  |
+-------------------------------------------------------------------------------+
```

### 3.5 `LensPlayerController` C++ Extensions

The ViewModel header ([`app-lens-qt/LensPlayerController.h`](../app-lens-qt/LensPlayerController.h)) is augmented with driver configuration properties and invokable slots:

```cpp
/// @file LensPlayerController.h
#pragma once

#include <cstdint>
#include <QObject>
#include <QString>
#include <QStringList>

namespace LensApp {

class LensPlayerController : public QObject {
    Q_OBJECT

    // Additional Properties for Phase 3
    Q_PROPERTY(int activeDriverType READ activeDriverType WRITE setActiveDriverType NOTIFY driverTypeChanged)
    Q_PROPERTY(QString driverName READ driverName NOTIFY driverTypeChanged)
    Q_PROPERTY(QString boundPortName READ boundPortName NOTIFY portBindingChanged)
    Q_PROPERTY(int boundBaudRate READ boundBaudRate NOTIFY portBindingChanged)
    Q_PROPERTY(int cameraAddress READ cameraAddress WRITE setCameraAddress NOTIFY cameraAddressChanged)
    Q_PROPERTY(QStringList availablePorts READ availablePorts NOTIFY portsScanned)
    Q_PROPERTY(bool isPortConnected READ isPortConnected NOTIFY portBindingChanged)
    Q_PROPERTY(int focusSpeed READ focusSpeed WRITE setFocusSpeed NOTIFY speedsChanged)
    Q_PROPERTY(int zoomSpeed READ zoomSpeed WRITE setZoomSpeed NOTIFY speedsChanged)

public:
    enum class DriverType : std::uint8_t {
        Mock = 0U,
        PelcoD = 1U,
        Visca = 2U,
        DynamicDll = 3U
    };
    Q_ENUM(DriverType)

    [[nodiscard]] int activeDriverType() const noexcept;
    [[nodiscard]] QString driverName() const;
    [[nodiscard]] QString boundPortName() const;
    [[nodiscard]] int boundBaudRate() const noexcept;
    [[nodiscard]] int cameraAddress() const noexcept;
    [[nodiscard]] QStringList availablePorts() const;
    [[nodiscard]] bool isPortConnected() const noexcept;
    [[nodiscard]] int focusSpeed() const noexcept;
    [[nodiscard]] int zoomSpeed() const noexcept;

public slots:
    // Axis Motion Commands (< 30 chars)
    void moveFocusStep(int steps);
    void moveZoomStep(int steps);
    void setFocusTarget(int position);
    void setZoomTarget(int position);
    void setFocusSpeed(int speed);
    void setZoomSpeed(int speed);
    void stopMotion();
    void emergencyStop();

    // Driver Binding Commands (< 30 chars)
    void setActiveDriverType(int type);
    void setCameraAddress(int address);
    void scanSerialPorts();
    void bindMockDriver();
    void bindPelcoDriver(int portIdx, int baud, int address);
    void bindViscaDriver(int portIdx, int baud, int address);
    void loadDriverPlugin(const QString& path, int portIdx, int baud);

signals:
    void driverTypeChanged();
    void portBindingChanged();
    void cameraAddressChanged();
    void portsScanned();
    void speedsChanged();
    void emergencyStopOccurred();

    // Queued signals to worker thread (< 30 chars)
    void requestBindMock(int channel);
    void requestBindPelco(int channel, int port, int baud, int address);
    void requestBindVisca(int channel, int port, int baud, int address);
    void requestLoadDll(int channel, const QString& path, int port, int baud);
};

} // namespace LensApp
```

### 3.6 `LensWorker` C++ Hardware Integration

`LensWorker` owns the `Transport::SerialTransport` instances, dynamic driver loader, and connects hardware callbacks:

```cpp
/// @file LensWorker.h (Excerpts for Phase 3)
#pragma once

#include <cstdint>
#include <array>
#include <memory>
#include <string>

#include "ILensDriver.h"
#include "LensDriverLoader.h"
#include "SerialTransport.h"

namespace LensApp {

class LensWorker : public QObject {
    Q_OBJECT

public slots:
    // Hardware Driver Binding Slots (< 30 chars)
    void onBindMock(int channel);
    void onBindPelco(int channel, int port, int baud, int address);
    void onBindVisca(int channel, int port, int baud, int address);
    void onLoadDll(int channel, const QString& path, int port, int baud);

private:
    // Serial port transmission callback (static bridge to member)
    static void transmitBridge(const std::uint8_t* data, std::size_t len, void* userCtx) noexcept;
    void handleTransmit(int channel, const std::uint8_t* data, std::size_t len) noexcept;
    void pollSerialIncoming(int channel) noexcept;

    std::array<std::unique_ptr<Transport::SerialTransport>, 4> m_serialPorts { };
    std::array<std::unique_ptr<LensControl::ILensDriver>, 4> m_activeDrivers { };
    LensControl::LensDriverLoader m_driverLoader { };
    std::array<int, 4> m_driverTypes { 0, 0, 0, 0 };
};

} // namespace LensApp
```

### 3.7 Physical Serial Frame Transmission & Telemetry Ingestion

When `PelcoDLensDriver` or `ViscaLensDriver` formats a command frame (e.g., standard 7-byte Pelco-D frame `[0xFF, addr, cmd1, cmd2, data1, data2, checksum]`), it triggers `TransmitCallback`:
```cpp
void LensWorker::handleTransmit(int channel, const std::uint8_t* data, std::size_t len) noexcept
{
    if ((channel >= 0) && (channel < 4) && (m_serialPorts[channel] != nullptr)) {
        if (m_serialPorts[channel]->isOpen()) {
            static_cast<void>(m_serialPorts[channel]->send(data, len));
        }
    }
}
```
During each 30 Hz tick in `processLoopTick()`, `LensWorker` polls incoming serial bytes from `m_serialPorts[cam]` and feeds them directly to `processIncomingBytes(rxBuf, bytesRead)`, updating driver encoder positions and limit flags without blocking.

---

## 4. Detailed Work Breakdown Structure (WBS)

### Milestone 3.1: Reusable Tactical QML Input Controls
- [x] **Task 3.1.1**: Create [`app-lens-qt/qml/Components/JogWheel.qml`](../app-lens-qt/qml/Components/JogWheel.qml) with rotary drag tracking, unwrapping math, tick mark ring, and step multiplier buttons ($\times 1, \times 10, \times 100, \times 500$).
- [x] **Task 3.1.2**: Create [`app-lens-qt/qml/Components/TacticalSlider.qml`](../app-lens-qt/qml/Components/TacticalSlider.qml) featuring value clamping, tick indicators, and direct numerical edit box.
- [x] **Task 3.1.3**: Create [`app-lens-qt/qml/Components/TacticalSwitch.qml`](../app-lens-qt/qml/Components/TacticalSwitch.qml) and [`app-lens-qt/qml/Components/TacticalComboBox.qml`](../app-lens-qt/qml/Components/TacticalComboBox.qml) adhering to the tactical dark color scheme (`#0e1014`, `#141720`, `#242938`, `#00e5ff`).
- [x] **Task 3.1.4**: Update [`app-lens-qt/qml.qrc`](../app-lens-qt/qml.qrc) to register all new components.

### Milestone 3.2: Comprehensive Optical Axis Deck (`LensControlPanel.qml`)
- [x] **Task 3.2.1**: Author [`app-lens-qt/qml/LensControlPanel.qml`](../app-lens-qt/qml/LensControlPanel.qml) organizing Focus and Zoom decks with dual `JogWheel` instances, micro-stepping buttons, and `TacticalSlider` controls.
- [x] **Task 3.2.2**: Integrate speed preset buttons (Creep: 32, Normal: 127, Fast: 200, Turbo: 255) for both axes.
- [x] **Task 3.2.3**: Embed limit switch lamps (`NearLimit`, `FarLimit`, `WideLimit`, `TeleLimit`) backed by [`Components/StatusBadge.qml`](../app-lens-qt/qml/Components/StatusBadge.qml).
- [x] **Task 3.2.4**: Create prominent Emergency Stop button styled in `#ff1744` with instant hover feedback and confirmation badge.
- [x] **Task 3.2.5**: Embed `LensControlPanel.qml` into [`app-lens-qt/qml/Main.qml`](../app-lens-qt/qml/Main.qml) inside the right-hand panel view.

### Milestone 3.3: ViewModel Axis Control & Safety Boundary Clamping
- [x] **Task 3.3.1**: Extend [`LensPlayerController.h`](../app-lens-qt/LensPlayerController.h) and [`LensPlayerController.cpp`](../app-lens-qt/LensPlayerController.cpp) with axis motion slots: `moveFocusStep`, `moveZoomStep`, `setFocusTarget`, `setZoomTarget`, `setFocusSpeed`, `setZoomSpeed`.
- [x] **Task 3.3.2**: Implement software range clamping against `focusMin..focusMax` and `zoomMin..zoomMax`, preventing out-of-bounds motor commands.
- [x] **Task 3.3.3**: Wire directional limit interlocks inhibiting further movement into tripped limits while allowing retreat.
- [x] **Task 3.3.4**: Connect `emergencyStop()` slot dispatching instant halt requests across all 4 camera channels.

### Milestone 3.4: Hardware Driver Subsystem & Serial Transport Pipeline
- [x] **Task 3.4.1**: Create [`app-lens-qt/qml/DriverConfigDialog.qml`](../app-lens-qt/qml/DriverConfigDialog.qml) for configuring COM port index, baud rate, and camera bus address.
- [x] **Task 3.4.2**: Extend `LensPlayerController` with serial enumeration (`scanSerialPorts()`) using `QSerialPortInfo` or platform registry queries.
- [x] **Task 3.4.3**: Extend `LensWorker` to manage `Transport::SerialTransport` instances for Channels 0..3 with multi-channel port collision checks.
- [x] **Task 3.4.4**: Implement `onBindMock`, `onBindPelco`, `onBindVisca` in `LensWorker`, attaching `transmitBridge` callbacks and binding drivers to `LensController::setDriver()`.
- [x] **Task 3.4.5**: Wire incoming serial reception in `processLoopTick()`, feeding raw bytes to `processIncomingBytes()` for real-time telemetry updates.

### Milestone 3.5: Dynamic Shared Library Plugin Loader Integration
- [x] **Task 3.5.1**: Integrate [`LensDriverLoader`](../libs/LensController/LensDriverLoader.h) into `LensWorker` to support loading external `.dll` / `.so` vendor driver plugins.
- [x] **Task 3.5.2**: Add `loadDriverPlugin(path, port, baud)` slot in `LensPlayerController` exposing file browser selection to QML.
- [x] **Task 3.5.3**: Verify port collision prevention when dynamic plugins attempt to bind an already allocated serial port.

### Milestone 3.6: Instant Emergency Stop Safety Interlock
- [x] **Task 3.6.1**: Implement `onEmergencyStop()` in `LensWorker` iterating over all 4 channels, invoking `LensController::stopMotion(cam)` and immediately transmitting stop frames.
- [x] **Task 3.6.2**: Add benchmark test verifying end-to-end execution time from UI signal emission to hardware stop command dispatch is strictly $< 5\text{ ms}$.

### Milestone 3.7: Automated Unit Verification, Concurrency & Hardware Tests
- [x] **Task 3.7.1**: Author [`tests/TestLensAxisControls.cpp`](../tests/TestLensAxisControls.cpp) testing jog stepping, absolute targeting, speed assignment, boundary clamping, and limit flag reflection.
- [x] **Task 3.7.2**: Author [`tests/TestLensDrivers.cpp`](../tests/TestLensDrivers.cpp) testing `MockLensDriver`, `PelcoDLensDriver`, `ViscaLensDriver`, and `LensDriverLoader` binding and packet serialization.
- [x] **Task 3.7.3**: Execute IWYU and documentation verification scripts (`verify_iwyu_cstdint.py`, `verify_relative_doc_paths.py`).

---

## 5. Verification, Safety & Standards Compliance

### 5.1 Standards Adherence Matrix

Every line of C++ code delivered in Phase 3 strictly satisfies repository standards:

| Standard / Rule | Requirement | Phase 3 Implementation Guarantee |
| :--- | :--- | :--- |
| **MISRA C++:2023 Rule 7.0.6** | No implicit signed/unsigned conversions. | All encoder step calculations (`int16_t` relative offsets, `uint16_t` coordinates) use explicit `static_cast` after boundary validation. |
| **SEI CERT INT31-C** | Safe integer conversions without truncation. | Clamped range checks against `focusMin`/`focusMax` prior to casting. |
| **SEI CERT STR37-C** | Deterministic character handling. | Port names and DLL paths processed using ASCII helpers; zero reliance on global C/C++ locale (`std::tolower`). |
| **SEI CERT ERR55-CPP** | Exception safety & zero-heap `noexcept` fast paths. | Fast path motor updates and transmit callbacks operate under `noexcept` without heap allocations (`std::string`, `new`). |
| **Domain Modeling Standard** | Prohibition of Primitive Obsession. | Actuator coordinates use `MotorPosition`, speeds use `MotorSpeed`, channels use `CameraIndex`, ranges use `LensRanges`. |
| **IWYU & `<cstdint>` Policy** | Direct symbol inclusion and `std::` scoping. | All files include `<cstdint>`, `<cstddef>` explicitly; all integer types use `std::` prefix. |
| **Function Name Length** | All identifiers $< 30$ characters. | Verified via static inspection and automated linter. |
| **Relative Documentation Paths** | No absolute filesystem workstation paths. | All markdown hyperlinks and comments use relative POSIX paths. |

---

## 6. Acceptance Criteria & Verification Sign-Off

The implementation of Phase 3 is considered complete when all criteria below pass:

| ID | Criterion | Verification Procedure | Expected Outcome | Status |
| :---: | :--- | :--- | :--- | :---: |
| **AC-3.1** | **Rotary Jog Dial Operation** | Drag `JogWheel.qml` in clockwise and counter-clockwise directions with $\times 1, \times 10, \times 100, \times 500$ multipliers. | Emits exact relative step counts; needle tracks rotational angle smoothly without branch cut jumps. | [x] PASS |
| **AC-3.2** | **Absolute Positioning** | Enter target position $45000$ in `TacticalSlider.qml` and click [GO]. | Motor slews to target coordinate; slider position updates to $45000$. | [x] PASS |
| **AC-3.3** | **Boundary Clamping** | Attempt to step focus beyond configured `focusMax` ($65535$). | Command is clamped to $65535$; Far Limit lamp illuminates coral; forward motion is inhibited. | [x] PASS |
| **AC-3.4** | **Pelco-D Driver Binding** | Bind Channel 0 to `PelcoDLensDriver` on COM1 @ 115200 bps, Address 1. Move Zoom Tele (+100). | Correct 7-byte Pelco-D frame `[0xFF, 0x01, 0x00, 0x20, 0x00, 0x00, 0x21]` is transmitted to serial port. | [x] PASS |
| **AC-3.5** | **VISCA Driver Binding** | Bind Channel 1 to `ViscaLensDriver` on COM2 @ 9600 bps, Address 1. Move Focus Far. | Correct VISCA command frame `[0x81, 0x01, 0x04, 0x08, 0x02, 0xFF]` is transmitted. | [x] PASS |
| **AC-3.6** | **Multi-Port Collision Guard** | Attempt to bind Channel 2 to the same COM port already held by Channel 1. | System rejects assignment with error message; prevents hardware port contention. | [x] PASS |
| **AC-3.7** | **Emergency Stop Latency** | Trigger E-Stop button while both axes are in high-speed motion across multiple channels. | All motion stops across all 4 channels; stop commands issued in $< 5\text{ ms}$. | [x] PASS |
| **AC-3.8** | **Automated Tests** | Run `ctest -R "TestLensAxisControls|TestLensDrivers"` in build directory. | 100% of tests pass with zero failures (10/10 passed). | [x] PASS |
| **AC-3.9** | **Static Hygiene Audits** | Run `verify_iwyu_cstdint.py` and `verify_relative_doc_paths.py`. | Zero violations reported (100% compliant). | [x] PASS |
