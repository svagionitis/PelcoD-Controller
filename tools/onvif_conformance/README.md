# ONVIF Profile S Conformance Runner & ODTT Integration

This directory contains the automated test harness, offline W3C XML schemas, and test procedures for verifying compliance of the Pelco-D ONVIF implementation against **ONVIF Profile S Specification v1.3** and **ONVIF Core Specification v24.12**.

---

## 1. Directory Structure

```
tools/onvif_conformance/
├── CMakeLists.txt              # CMake build targets for OnvifServerApp and CTest
├── README.md                   # This documentation and ODTT execution guide
├── requirements.txt            # Python dependencies (pytest, xmlschema, requests)
├── conformance_config.json     # Default target and Profile S test parameters
├── run_conformance.py          # Main CLI orchestrator and server supervisor
├── OnvifServerApp.cpp          # Standalone ONVIF server daemon executable
├── schemas/                    # Bundled official W3C and ONVIF XML schemas
│   ├── xml.xsd                 # W3C XML namespace
│   ├── soap-envelope.xsd       # SOAP 1.2 Envelope & Fault schema
│   ├── ws-addr.xsd             # WS-Addressing 1.0 schema
│   ├── ws-discovery.xsd        # WS-Discovery 2005/04 schema
│   ├── b-2.xsd                 # WS-BaseNotification 1.3 schema
│   ├── onvif.xsd               # ONVIF Core Types schema
│   ├── devicemgmt.xsd          # ONVIF Device Management service schema
│   ├── media.xsd               # ONVIF Media service schema
│   ├── ptz.xsd                 # ONVIF PTZ service schema
│   └── event.xsd               # ONVIF Event service schema
└── suites/                     # Pytest conformance test suites
    ├── conftest.py             # Shared fixtures and authenticated SOAP client
    ├── test_discovery.py       # WS-Discovery multicast probe and response tests
    ├── test_device_mgmt.py     # Device management and SOAP fault tests
    ├── test_media_profile_s.py # Profile S invariants, RTSP stream, JPEG snapshot
    ├── test_ptz_ops.py         # PTZ movements, normalized spaces, presets
    └── test_events_pullpoint.py# PullPoint subscription lifecycle and renew
```

---

## 2. Automated Python Conformance Runner

The automated conformance runner executes unattended in CI or locally to validate all SOAP XML responses against the strict official W3C XML schemas using `xmlschema`.

### Prerequisites

```bash
pip install -r tools/onvif_conformance/requirements.txt
```

### Running the Conformance Runner

#### A. Managed Mode (Spawns and Supervises Server Binary)

```bash
python3 tools/onvif_conformance/run_conformance.py \
    --server-binary build/bin/OnvifServerApp \
    --output-junit build/conformance_report.xml
```

#### B. Standalone / Existing Device Mode

```bash
python3 tools/onvif_conformance/run_conformance.py \
    --host 192.168.10.100 \
    --port 8080 \
    --output-junit conformance_report.xml
```

#### C. Running via CMake / CTest

When built with `PELCOD_ENABLE_ONVIF=ON`:

```bash
# Build standalone server
cmake --build build --target OnvifServerApp

# Run custom target
cmake --build build --target onvif_conformance

# Or run via CTest
ctest --test-dir build -R OnvifConformanceTest --output-on-failure
```

---

## 3. Official ONVIF Device Test Tool (ODTT) Test Plan

The ONVIF Device Test Tool (ODTT v24.x) is the official compliance tool for formal Profile S certification.

### 3.1 Network Topology Setup

```
+-------------------------------------------------------------------------------+
|                            ODTT LAB TOPOLOGY                                  |
+-------------------------------------------------------------------------------+
|                                                                               |
|                      [ Isolated Switch / VLAN 10 ]                            |
|                            (192.168.10.0/24)                                  |
|                                │         │                                    |
|              ┌─────────────────┘         └──────────────────┐                 |
|              ▼                                              ▼                 |
|   +--------------------------+                  +-------------------------+   |
|   | Device Under Test (DUT)  |                  | ODTT Test Host          |   |
|   | IP: 192.168.10.100       |                  | IP: 192.168.10.200      |   |
|   | Port: 8080 (SOAP)        |                  | OS: Windows 11 64-bit   |   |
|   | Discovery: 3702 (UDP)    |                  | Tool: ODTT v24.12       |   |
|   +--------------------------+                  +-------------------------+   |
|                                                                               |
+-------------------------------------------------------------------------------+
```

### 3.2 Execution Steps

1. Launch `OnvifServerApp --port 8080 --test-mode` on the DUT.
2. Verify host reachability from the ODTT workstation (`ping 192.168.10.100`).
3. Open ODTT, navigate to **Profile S**, and configure the Device Descriptor:
   - Device Service XAddr: `http://192.168.10.100:8080/onvif/device_service`
   - Administrator Credentials: `admin` / `AdminPass123!`
4. Run the full Profile S test suite.
5. Save the generated XML log (`ODTT_ProfileS_Results.xml`).

---

## 4. Troubleshooting Common Failures

- **Clock Drift (`ter:NotAuthorized`):** ONVIF WS-Security enforces a maximum clock skew of ±300 seconds. Synchronize clocks via NTP before testing.
- **Scope Mismatch:** Ensure the server advertises `onvif://www.onvif.org/Profile/Streaming` in discovery scopes.
- **Port Conflicts:** If `OnvifServerApp` fails to bind, ensure port 8080 is not occupied by another process, or specify `--port <free_port>`.
