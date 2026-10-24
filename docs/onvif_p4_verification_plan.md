# Implementation Plan: P4 – ONVIF Verification & Quality Assurance

**Scope:** `libs/Onvif/` test infrastructure (`tests/`, `fuzzing/`, `tools/onvif_conformance/`, `.github/workflows/`, `scripts/`)  
**Standard Baseline:** MISRA C++:2023, SEI CERT C++, ONVIF Profile S Specification v1.3, ONVIF Core Specification v24.12, IEEE 754-2019, Project Rules in [`.agents/rules/`](../.agents/rules/)  
**Reference Document:** [`docs/onvif_review.md`](onvif_review.md) (§5 Coding Standards, §6 Test Assessment, §8 Remediation Roadmap P4)

---

## 1. Executive Summary & Problem Analysis

In [`docs/onvif_review.md`](onvif_review.md), **Phase P4** focuses on comprehensive verification, automated regression prevention, negative fault validation, continuous fuzz testing, standards conformance, and multi-platform CI sanitizer coverage across the `libs/Onvif` subsystem.

While Phases P0 (Security Hardening), P1 (Protocol Correctness), P2 (Profile S Completeness), and P3 (Architecture & Hygiene) addressed functional vulnerabilities and modularized the server architecture, the current test suite exhibits severe verification blind spots:

1. **Monolithic Mega-Tests:** 52 coarse "kitchen-sink" tests reside within two massive files: `TestOnvifServer.cpp` (3,317 lines, 186 KB, 15 tests) and `TestPelcoDOnvif.cpp` (2,342 lines, 119 KB, 37 tests). Tests average ~5 KB each and combine unrelated assertions into single blocks. A failure in one assertion halts the entire test case, masking subsequent bugs and preventing parallel CTest execution.
2. **Absence of Negative & Fault Testing:** As documented in `docs/onvif_review.md` §6, the test suite historically had zero assertions for `ter:NotAuthorized`, `ter:ActionNotSupported`, HTTP 4xx/5xx status codes, XML injection payloads, and malformed SOAP envelopes.
3. **No Fuzz Testing on Untrusted Ingress:** Network-facing entry points (pugixml document parsing, UDP 3702 multicast WS-Discovery packet processing, HTTP client response deserializers, and Authorization header decoders) have never been fuzzed with LLVM `libFuzzer`, leaving parser edge cases and memory exhaustion vectors undetected.
4. **No Automated Standards Conformance Run:** Conformance claims rely entirely on internal unit mocks. There is no automated test pipeline running against the official ONVIF Device Test Tool (ODTT) or an open-source WSDL/XML-schema conformance runner (such as `python-onvif-zeep` or `xmlschema`).
5. **Incomplete Multi-Platform CI & Sanitizers:** The project only runs a Windows MSVC workflow in CI (`.github/workflows/windows-msvc.yml`). There is no Linux GCC/Clang workflow, and memory safety sanitizers (AddressSanitizer, LeakSanitizer, ThreadSanitizer, UndefinedBehaviorSanitizer) are not systematically exercised on push or pull requests.

```
+-----------------------------------------------------------------------------------------------------------------------+
|                                    P4 VERIFICATION & QUALITY ASSURANCE MATRIX                                         |
+----------------------+------------------------------------+-------------------------------------------+---------------+
| Verification Area    | Defect / Technical Debt            | Target Architecture / Verification Method | Target Phase  |
+----------------------+------------------------------------+-------------------------------------------+---------------+
| Test Granularity     | 52 coarse tests in 2 monolithic    | Decompose into 13 modular GoogleTest      | [Phase 1](onvif_p4_phase1_test_decomposition_plan.md) (Completed) |
|                      | files (> 300 KB total size)        | fixtures aligned with P3 service domains  |               |
+----------------------+------------------------------------+-------------------------------------------+---------------+
| Negative & Security  | Sparse negative edge-case tests;   | Systematic negative test matrix: malformed| [Phase 2](onvif_p4_phase2_negative_security_plan.md) (Completed) |
| Fault Coverage       | Missing SOAP 1.2 fault subcodes;   | XML, fault subcodes, replay attacks, SSRF |               |
|                      | Unchecked numeric conversions      | injection, NaN/Inf bounds (CERT FLP34-C)  |               |
+----------------------+------------------------------------+-------------------------------------------+---------------+
| Ingress Fuzzing      | Zero fuzz testing on untrusted     | LLVM libFuzzer harnesses for SOAP parsing,| [Phase 3](onvif_p4_phase3_fuzz_testing_plan.md) |
|                      | network inputs (pugixml, UDP 3702) | WS-Discovery, client responses, & auth    |               |
+----------------------+------------------------------------+-------------------------------------------+---------------+
| Standards            | No ODTT or schema conformance runs;| Formal ODTT test procedure + automated    | [Phase 4](onvif_p4_phase4_conformance_runner_plan.md) |
| Conformance          | Unverified WSDL XML compliance     | Python WSDL/XSD schema conformance runner |               |
+----------------------+------------------------------------+-------------------------------------------+---------------+
| CI & Sanitizers      | Windows-only CI; No Linux build;   | Multi-platform CI matrix: GCC/Clang, MSVC,| [Phase 5](onvif_p4_phase5_ci_sanitizers_plan.md) |
|                      | Sanitizers (ASan/TSan/UBSan) unused| ASan/LSan, TSan, UBSan, and -Werror /WX   |               |
+----------------------+------------------------------------+-------------------------------------------+---------------+
```

---

## 2. Verification Architecture & Pipelines

### 2.1 Multi-Layered Verification Hierarchy

The P4 verification architecture organizes testing into 5 distinct defense-in-depth validation layers:

```
+---------------------------------------------------------------------------------------------------+
|                                 P4 VERIFICATION DEFENSE-IN-DEPTH                                  |
+---------------------------------------------------------------------------------------------------+
|                                                                                                   |
|  [ Layer 5: Multi-Platform Hardened CI ]                                                          |
|    - Linux GCC 12/13 & Clang 16/17 (-Wall -Wextra -Wpedantic -Werror -Wconversion)                |
|    - Windows MSVC 2022 (/W4 /WX /we4365 /guard:cf /CETCOMPAT)                                     |
|    - Dynamic Sanitizers: ASan (Memory), LSan (Leaks), TSan (Races), UBSan (UB)                    |
|    - Static Audits: verify_iwyu_cstdint.py, verify_function_lengths.py                            |
|                                     │                                                             |
|                                     ▼                                                             |
|  [ Layer 4: Standards Conformance & System Validation ]                                           |
|    - ONVIF Device Test Tool (ODTT) Profile S test scripts                                         |
|    - Automated Python Conformance Runner (tools/onvif_conformance/):                              |
|      * XML Schema Validation (onvif.xsd, media.xsd, ptz.xsd, b-2.xsd)                             |
|      * End-to-end WSDL operation coverage and fault compliance                                    |
|                                     │                                                             |
|                                     ▼                                                             |
|  [ Layer 3: Continuous Ingress Fuzz Testing (libFuzzer) ]                                         |
|    - fuzz_soap_router: Untrusted SOAP XML payloads                                                |
|    - fuzz_ws_discovery: Untrusted UDP multicast frames                                            |
|    - fuzz_client_xml: Corrupt HTTP responses from malicious cameras                              |
|    - fuzz_auth_headers: Hostile UsernameToken & Digest strings                                    |
|    - fuzz_url_validator: SSRF evasions and URI parse anomalies                                    |
|                                     │                                                             |
|                                     ▼                                                             |
|  [ Layer 2: Integration & Protocol Negative/Security Fault Suites ]                              |
|    - Wire-level HTTP client-server interaction via OnvifTestClient                                |
|    - SOAP 1.2 Fault verification (ter:ActionNotSupported, ter:InvalidArgVal, ter:NotAuthorized)   |
|    - Security attack replay: replay cache, SSRF filters, WS-Discovery reflection                  |
|                                     │                                                             |
|                                     ▼                                                             |
|  [ Layer 1: Modular Unit & Domain Port Component Tests ]                                          |
|    - Granular GoogleTest fixtures decomposed from legacy mega-tests                               |
|    - Direct verification of P3 decoupled services (Device, Media, PTZ, Event, Imaging)           |
|    - Strict adherence to <cstdint>, IWYU, zero raw pointers, < 30 char identifiers                |
|                                                                                                   |
+---------------------------------------------------------------------------------------------------+
```

#### Mermaid Diagram: Verification Ingress & CI Pipeline

```mermaid
flowchart TD
    subgraph L1["Layer 1: Unit & Domain Tests"]
        U1["DeviceServiceSuite"]
        U2["MediaServiceSuite"]
        U3["PtzServiceSuite"]
        U4["EventServiceSuite"]
        U5["ImagingServiceSuite"]
    end

    subgraph L2["Layer 2: Negative & Security Suites"]
        N1["ProtocolFaultSuite<br/>(ter: subcodes & HTTP codes)"]
        N2["SecurityHardeningSuite<br/>(SSRF, Replay, Auth, NaN)"]
    end

    subgraph L3["Layer 3: libFuzzer Harnesses"]
        F1["fuzz_soap_router"]
        F2["fuzz_ws_discovery"]
        F3["fuzz_client_xml"]
        F4["fuzz_auth_headers"]
    end

    subgraph L4["Layer 4: Standards Conformance"]
        C1["ODTT Profile S Suite"]
        C2["Python WSDL Conformance<br/>(Schema validation)"]
    end

    subgraph L5["Layer 5: Multi-Platform CI"]
        CI1["Linux GCC/Clang<br/>(-Werror -Wconversion)"]
        CI2["Windows MSVC<br/>(/W4 /WX /guard:cf)"]
        CI3["Sanitizers<br/>(ASan, TSan, UBSan)"]
        CI4["Static Audits<br/>(IWYU, <cstdint>, <30 char)"]
    end

    L1 --> L2
    L2 --> L3
    L3 --> L4
    L4 --> L5
```

---

## 3. Phase 1: Decomposition of Monolithic Mega-Tests ([Phase 1 Detailed Plan](onvif_p4_phase1_test_decomposition_plan.md))

### 3.1 Problem Analysis of Legacy Test Files

1. **`TestOnvifServer.cpp` (3,317 LOC, 186 KB):**
   - Contains 15 giant test cases that mix server configuration, HTTP wire communication, XML validation, and multiple functional services into continuous test blocks.
   - For example, `TEST(OnvifServerTest, HttpSoapEndpoints)` spans over 450 lines, sequentially executing 12 distinct SOAP calls. If the first call fails, the remaining 11 calls never execute.
   - Tests instantiate full server instances with default mock handlers, leading to heavy thread synchronization overhead and slow test execution.
2. **`TestPelcoDOnvif.cpp` (2,342 LOC, 119 KB):**
   - Contains 37 mixed test cases combining XML client request generation, XML response parsing, Pelco-D bridge integration, and mock socket simulation.
   - Response parsing tests embed multi-kilobyte hardcoded XML strings directly inside test bodies without reusable fixtures.

### 3.2 Target Modular Test Suites

Both monolithic files will be decomposed into 13 modular, domain-specific GoogleTest suites matching the P3 decoupled architecture:

```
+---------------------------------------------------------------------------------------------------+
|                              TEST DECOMPOSITION MAPPING TABLE                                     |
+-----------------------------+------------------------------------+--------------------------------+
| Legacy Monolith File        | Target Modular Test Suite          | Responsibilities Covered       |
+-----------------------------+------------------------------------+--------------------------------+
| TestOnvifServer.cpp         | TestOnvifServerHttpEndpoints.cpp   | HTTP wire transport, headers,  |
| (Lines 192–650)             |                                    | Content-Type, status codes     |
+-----------------------------+------------------------------------+--------------------------------+
| TestOnvifServer.cpp         | TestOnvifDeviceServiceSuite.cpp    | GetDeviceInformation, date/time|
| (Lines 651–1100)            |                                    | network, NTP, DNS, reboot      |
+-----------------------------+------------------------------------+--------------------------------+
| TestOnvifServer.cpp         | TestOnvifMediaServiceSuite.cpp     | Profiles, VSC, VEC, options,   |
| (Lines 1101–1600)           |                                    | stream URIs, snapshot, OSD     |
+-----------------------------+------------------------------------+--------------------------------+
| TestOnvifServer.cpp         | TestOnvifPtzServiceSuite.cpp       | Continuous/Absolute/Relative,  |
| (Lines 1601–2100)           |                                    | Stop, Presets, Tours, spaces   |
+-----------------------------+------------------------------------+--------------------------------+
| TestOnvifServer.cpp         | TestOnvifEventServiceSuite.cpp     | PullPoint, Renew, Unsubscribe, |
| (Lines 2101–2600)           |                                    | Push Notify, topic filters     |
+-----------------------------+------------------------------------+--------------------------------+
| TestOnvifServer.cpp         | TestOnvifImagingServiceSuite.cpp   | Focus, white balance, presets, |
| (Lines 2601–2900)           |                                    | imaging options & settings     |
+-----------------------------+------------------------------------+--------------------------------+
| TestOnvifServer.cpp         | TestOnvifExtensionsSuite.cpp       | Analytics engine, privacy masks|
| (Lines 2901–3317)           |                                    | device I/O relays and inputs   |
+-----------------------------+------------------------------------+--------------------------------+
| TestPelcoDOnvif.cpp         | TestOnvifClientDeviceParsing.cpp   | Client XML parsing for Device  |
| (Lines 1–450)               |                                    | capabilities, info, interfaces |
+-----------------------------+------------------------------------+--------------------------------+
| TestPelcoDOnvif.cpp         | TestOnvifClientMediaParsing.cpp    | Client XML parsing for Media   |
| (Lines 451–900)             |                                    | profiles, streams, snapshots   |
+-----------------------------+------------------------------------+--------------------------------+
| TestPelcoDOnvif.cpp         | TestOnvifClientPtzParsing.cpp      | Client XML parsing for PTZ     |
| (Lines 901–1350)            |                                    | status, presets, preset tours  |
+-----------------------------+------------------------------------+--------------------------------+
| TestPelcoDOnvif.cpp         | TestOnvifClientEventParsing.cpp    | Client XML parsing for events, |
| (Lines 1351–1700)           |                                    | notification messages, ops     |
+-----------------------------+------------------------------------+--------------------------------+
| TestPelcoDOnvif.cpp         | TestOnvifClientImagingParsing.cpp  | Client XML parsing for focus,  |
| (Lines 1701–2000)           |                                    | imaging settings and presets   |
+-----------------------------+------------------------------------+--------------------------------+
| TestPelcoDOnvif.cpp         | TestOnvifClientExtensionsParsing   | Client XML parsing for relays, |
| (Lines 2001–2342)           | .cpp                               | metadata, analytics, thermal   |
+-----------------------------+------------------------------------+--------------------------------+
```

### 3.3 Implementation Details & Refactoring Steps

1. **Create Common Test Fixture Helpers (`libs/Onvif/tests/OnvifTestFixtures.h`):**
   - Provide RAII server fixture `OnvifServerFixture` that binds to an ephemeral localhost port (`127.0.0.1:0` or dynamic port probe), starts the server, ensures clean shutdown in destructor, and provides pre-configured `OnvifTestClient` instances.
   - Centralize valid XML payload templates and XML string comparison helpers with namespace normalization.
2. **Decompose `TestOnvifServer.cpp`:**
   - Extract individual service tests into isolated translation units under `libs/Onvif/tests/server/`.
   - Ensure each test file is under 400 lines and contains focused `TEST_F` test cases testing one operation or state change at a time.
3. **Decompose `TestPelcoDOnvif.cpp`:**
   - Extract client response parsers into isolated translation units under `libs/Onvif/tests/client/`.
   - Use parameterized tests (`TEST_P`) where applicable to test multiple valid and edge-case response XML structures.
4. **Update CMake Configuration (`libs/Onvif/tests/CMakeLists.txt`):**
   - Register each new test file with `add_onvif_test`.
   - Retire the old monolithic targets `TestOnvifServer` and `TestPelcoDOnvif` once all test cases are cleanly migrated and verified.
5. **Verification:**
   - Run CTest in parallel: `ctest --test-dir build --output-on-failure -j8`.
   - Verify all test names are strictly less than 30 characters and follow the project verification checklist.

---

## 4. Phase 2: Negative, Boundary & Security Vulnerability Suites ([Phase 2 Detailed Plan](onvif_p4_phase2_negative_security_plan.md))

### 4.1 Protocol Negative Fault Suite (`TestOnvifProtocolFaults.cpp`)

Implement dedicated negative tests to verify that invalid protocol inputs are rejected with exact SOAP 1.2 Fault structures per ONVIF Core §5.11:

```
+---------------------------------------------------------------------------------------------------+
|                              NEGATIVE FAULT TEST MATRIX                                           |
+------------------------+------------------------------------+--------------------+----------------+
| Input Fault Condition  | Trigger Payload                    | Expected HTTP Code | Expected Fault |
+------------------------+------------------------------------+--------------------+----------------+
| Non-XML Body           | Random binary or non-XML text      | 400 Bad Request    | None / 400     |
+------------------------+------------------------------------+--------------------+----------------+
| Malformed XML          | `<Envelope><Body><Unclosed>`       | 400 Bad Request    | env:Sender /   |
|                        |                                    |                    | ter:InvalidArg |
+------------------------+------------------------------------+--------------------+----------------+
| Missing SOAP Body      | `<Envelope><Header/></Envelope>`   | 400 Bad Request    | env:Sender /   |
|                        |                                    |                    | ter:InvalidArg |
+------------------------+------------------------------------+--------------------+----------------+
| Unknown Operation      | `<Body><tds:UnknownOp/></Body>`    | 500 Internal Error | env:Receiver / |
|                        |                                    |                    | ter:ActionNot- |
|                        |                                    |                    | Supported      |
+------------------------+------------------------------------+--------------------+----------------+
| Mismatched Namespace   | `<Body><tptz:Stop/></Body>` sent   | 500 Internal Error | env:Receiver / |
|                        | to `/onvif/device_service`         |                    | ter:ActionNot- |
|                        |                                    |                    | Supported      |
+------------------------+------------------------------------+--------------------+----------------+
| Non-Existent Profile   | `<GetProfile><ProfileToken>Invalid | 500 Internal Error | env:Sender /   |
|                        | Token</ProfileToken></GetProfile>` |                    | ter:NoProfile  |
+------------------------+------------------------------------+--------------------+----------------+
| Invalid Config Token   | `<GetConfiguration><ConfigToken>  | 500 Internal Error | env:Sender /   |
|                        | BadToken</ConfigToken></GetConfig>`|                    | ter:NoToken    |
+------------------------+------------------------------------+--------------------+----------------+
| Non-Existent Preset    | `<GotoPreset><PresetToken>NoSuch   | 500 Internal Error | env:Sender /   |
|                        | Preset</PresetToken></GotoPreset>` |                    | ter:InvalidArg |
+------------------------+------------------------------------+--------------------+----------------+
| Out of Range Values    | `ContinuousMove` PanSpeed = 5.0    | 500 Internal Error | env:Sender /   |
|                        | (allowed range: [-1.0, 1.0])       |                    | ter:OutofRange |
+------------------------+------------------------------------+--------------------+----------------+
| Missing Mand. Args     | `<SetSystemDateAndTime>` missing   | 500 Internal Error | env:Sender /   |
|                        | `DateTimeType` element             |                    | ter:InvalidArg |
+------------------------+------------------------------------+--------------------+----------------+
```

### 4.2 Security Abuse & Vulnerability Test Suite (`TestOnvifSecurityHardening.cpp`)

Implement tests targeting security attack vectors to verify that protections built in Phase P0 remain impenetrable:

1. **Authentication Abuse & Replay Tests:**
   - Attempt authentication with expired UsernameToken timestamp (`Created` older than -300s or newer than +300s) -> verify `ter:NotAuthorized` fault.
   - Attempt replay of previously used `Nonce` within the replay window -> verify rejection with `ter:NotAuthorized` fault.
   - Send HTTP Digest request with invalid MD5/SHA-256 response hash -> verify HTTP 401 Unauthorized with fresh `nonce` challenge.
   - Rapid-fire authentication failure attempts -> verify exponential back-off / rate-limiting triggers and blocks subsequent requests.
2. **Credential & Provisioning Security:**
   - Attempt to set passwords violating complexity rules (under 8 characters, missing digits or special characters) -> verify rejection.
   - Verify that when the server is in `ProvisioningState::Unprovisioned`, all endpoints except `GetSystemDateAndTime` and initial commissioning reject requests.
3. **SSRF Mitigation Verification:**
   - Send `Subscribe` and `Renew` requests with malicious `ConsumerURL` values:
     * IPv4 Loopback: `http://127.0.0.1:8080/hook`, `http://127.0.0.2/hook`
     * IPv6 Loopback: `http://[::1]:8080/hook`
     * AWS / Cloud Metadata: `http://169.254.169.254/latest/meta-data/`
     * Sensitive Ports: `http://192.168.1.50:22/` (SSH), `http://192.168.1.50:25/` (SMTP), `http://192.168.1.50:445/` (SMB)
     * Bad Schemes: `file:///etc/passwd`, `gopher://192.168.1.50/`
     * Userinfo injection: `http://admin:pass@192.168.1.50/`
   - Verify that all malicious URLs are rejected by `NotificationUrlValidator` and return SOAP 1.2 `ter:InvalidArgVal` faults.
4. **WS-Discovery Storm & Amplification Rejection:**
   - Inject UDP multicast probe with source port 3702 -> verify reflection suppression drops the probe.
   - Inject probe originating from the server's own IP/MAC -> verify self-loop suppression drops the probe.
   - Flood UDP socket with probe bursts exceeding token-bucket limit -> verify excess probes are dropped without CPU starvation.
5. **Float-to-Integer Undefined Behavior (CERT FLP34-C):**
   - Inject `NaN`, `+Infinity`, `-Infinity`, and `1e38` into PTZ `panSpeed`, `tiltSpeed`, `zoomSpeed`, `pan`, and `tilt` parameters.
   - Verify that the server validates and clamps all floating-point inputs before casting, preventing undefined behavior and server termination.

---

## 5. Phase 3: Continuous Fuzz Testing via LLVM libFuzzer ([Phase 3 Detailed Plan](onvif_p4_phase3_fuzz_testing_plan.md))

### 5.1 Fuzz Targets & Attack Surfaces

To protect against untrusted network inputs, 5 specialized libFuzzer harnesses will be implemented under `libs/Onvif/fuzzing/`:

```
+---------------------------------------------------------------------------------------------------+
|                                 LIBFUZZER TARGET HARNESSES                                        |
+----------------------+------------------------------------+---------------------------------------+
| Harness Executable   | Targeted Component                 | Input Data Description                |
+----------------------+------------------------------------+---------------------------------------+
| fuzz_soap_router     | SoapRouter::dispatch,              | Raw HTTP body byte streams representing|
|                      | XmlUtils::parseXmlDocument         | arbitrary, mutated SOAP XML documents |
+----------------------+------------------------------------+---------------------------------------+
| fuzz_ws_discovery    | WsDiscoveryValidator::validateProbe| Raw UDP datagram byte streams received|
|                      | and parseProbeMatches              | on multicast port 3702                |
+----------------------+------------------------------------+---------------------------------------+
| fuzz_client_xml      | OnvifClient response parsers       | Mutated HTTP SOAP XML response bodies |
|                      | (parseGetProfilesResponse, etc.)   | received by client from camera        |
+----------------------+------------------------------------+---------------------------------------+
| fuzz_auth_headers    | UsernameToken & HttpDigest parsers | Malformed, hostile, and boundary-value|
|                      | in AuthMiddleware / Authenticator  | Authorization header string sequences |
+----------------------+------------------------------------+---------------------------------------+
| fuzz_url_validator   | NotificationUrlValidator           | Raw URI/URL strings with unusual enc- |
|                      | ::validateUrl                      | odings, IPv6 literals, and delimiters |
+----------------------+------------------------------------+---------------------------------------+
```

### 5.2 Harness Implementation Architecture

Each fuzz harness follows standard LLVM `LLVMFuzzerTestOneInput` conventions with zero heap leaks:

```cpp
/// @file fuzz_soap_router.cpp
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <memory>

#include "Onvif/SoapRouter.h"
#include "Onvif/XmlUtils.h"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (size == 0 || size > 65536) {
        return 0; // Bound input size to prevent trivial OOM
    }

    const std::string_view payload(reinterpret_cast<const char*>(data), size);
    
    // Parse XML DOM
    pugi::xml_document doc;
    if (!Onvif::XmlUtils::parseXmlDocument(payload, doc)) {
        return 0;
    }

    // Attempt operation routing and dispatch against mock registry
    Onvif::SoapRouter router;
    Onvif::HttpRequest req;
    req.body = std::string(payload);
    req.path = "/onvif/device_service";
    
    Onvif::HttpResponse res;
    static_cast<void>(router.dispatch(req, res));

    return 0;
}
```

### 5.3 Corpus Generation & CMake Integration

1. **Seed Corpus Directory (`libs/Onvif/fuzzing/corpus/`):**
   - Provide valid sample SOAP requests extracted from ONVIF WSDL specifications (`GetDeviceInformation.xml`, `GetProfiles.xml`, `ContinuousMove.xml`, `Subscribe.xml`).
   - Provide valid UDP WS-Discovery probes and RFC 7616 Authorization header strings.
2. **CMake Build Option (`libs/Onvif/fuzzing/CMakeLists.txt`):**
   ```cmake
   option(PELCOD_ENABLE_FUZZING "Build libFuzzer harnesses for ONVIF" OFF)
   if(PELCOD_ENABLE_FUZZING)
       if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
           message(FATAL_ERROR "libFuzzer requires Clang compiler.")
       endif()
       set(FUZZ_FLAGS "-fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer")
       # Register fuzz targets...
   endif()
   ```
3. **Automated Fuzz Regression in CTest:**
   - Run short fuzz smoke tests (1,000 iterations per harness) during CTest runs to verify harnesses do not crash on seed inputs.

---

## 6. Phase 4: ONVIF Profile S Conformance Runner & ODTT Integration ([Phase 4 Detailed Plan](onvif_p4_phase4_conformance_runner_plan.md))

### 6.1 Formal ONVIF Device Test Tool (ODTT) Test Plan

The ONVIF Device Test Tool (ODTT) is the official compliance suite provided by the ONVIF consortium. The test procedure specifies:

1. **Test Environment Topology:**
   - Dedicated test workstation on an isolated physical subnet or VLAN (`192.168.10.0/24`).
   - Device Under Test (DUT): Pelco-D Controller with `OnvifServer` bound to `192.168.10.100:8080`.
   - Secondary host running ODTT v24.x at `192.168.10.200`.
2. **Device Descriptor & Commissioning:**
   - Execute initial administrator commissioning to set credentials (`admin` / `AdminPass123!`).
   - Export device XML capabilities and configure Profile S feature flags.
3. **Target Profile S Test Groups:**
   - **Discovery:** Verify Hello on boot, Bye on shutdown, ProbeMatches within 500 ms, Scopes matching `onvif://www.onvif.org/Profile/Streaming`, AppSequence monotonic increments.
   - **Device Management:** Verify `GetSystemDateAndTime`, `GetDeviceInformation`, `GetServices`, `GetCapabilities`, user management, and reboot sequence.
   - **Media:** Verify `GetProfiles` returns at least one profile containing both `VideoSourceConfiguration` and `VideoEncoderConfiguration`; verify `GetStreamUri` (RTSP) and `GetSnapshotUri` (JPEG HTTP 200).
   - **PTZ:** Verify `ContinuousMove`, `AbsoluteMove`, `RelativeMove`, `Stop`, `GetPresets`, `SetPreset`, `RemovePreset`, and `GotoPreset`.
   - **Events:** Verify `CreatePullPointSubscription`, `PullMessages` event retrieval, `Renew`, `Unsubscribe`, and termination time enforcement.

### 6.2 Automated Python Conformance Runner (`tools/onvif_conformance/`)

To enable automated conformance verification in headless CI environments without manual ODTT GUI interaction, implement an open-source conformance test runner in Python 3:

```
tools/onvif_conformance/
├── run_conformance.py          # Main CLI test orchestrator
├── requirements.txt            # zeep, xmlschema, requests, pytest
├── schemas/                    # Official ONVIF WSDL and XSD schemas
│   ├── onvif.xsd
│   ├── media.xsd
│   ├── ptz.xsd
│   ├── b-2.xsd
│   └── ws-discovery.xsd
├── test_discovery.py           # UDP 3702 multicast probe and response tests
├── test_device_mgmt.py         # Device service schema and fault tests
├── test_media_profile_s.py     # Media service profile and streaming tests
├── test_ptz_ops.py             # PTZ movement and preset validation
└── test_events_pullpoint.py    # PullPoint lifecycle and topic filter tests
```

#### Test Runner Workflow:
1. Spawns `OnvifServer` as a subprocess with a test configuration file.
2. Performs automated WS-Discovery Probe to locate the test server endpoint.
3. Executes complete suite of SOAP operations over HTTP.
4. Validates all SOAP XML responses against the strict official W3C XML schemas (`.xsd`) using `xmlschema`.
5. Emits JUnit XML (`conformance_report.xml`) and console summary. If any response violates schema or protocol expectations, exits with a non-zero exit code to fail CI.

---

## 7. Phase 5: Multi-Platform CI Matrix & Hardened Sanitizers ([Phase 5 Detailed Plan](onvif_p4_phase5_ci_sanitizers_plan.md))

### 7.1 Multi-Platform GitHub Actions Architecture

Expand CI beyond Windows MSVC by creating `.github/workflows/onvif-verification.yml` with a comprehensive build and sanitizer matrix:

```
+---------------------------------------------------------------------------------------------------+
|                                  CI MATRIX ARCHITECTURE                                           |
+----------------------+--------------------+-----------------------+-------------------------------+
| Job Name             | OS Runner          | Compiler              | Build Type & Flags            |
+----------------------+--------------------+-----------------------+-------------------------------+
| linux-gcc-strict     | ubuntu-24.04       | GCC 13                | -Wall -Wextra -Wpedantic      |
|                      |                    |                       | -Werror -Wconversion          |
+----------------------+--------------------+-----------------------+-------------------------------+
| linux-clang-strict   | ubuntu-24.04       | Clang 17              | Strict warnings +             |
|                      |                    |                       | verify_iwyu_cstdint.py        |
+----------------------+--------------------+-----------------------+-------------------------------+
| windows-msvc-strict  | windows-2022       | MSVC 2022             | /W4 /WX /we4365 /guard:cf     |
|                      |                    |                       | /CETCOMPAT                    |
+----------------------+--------------------+-----------------------+-------------------------------+
| sanitizers-asan-lsan | ubuntu-24.04       | Clang 17              | -fsanitize=address            |
|                      |                    |                       | (Address & Leak Sanitizer)    |
+----------------------+--------------------+-----------------------+-------------------------------+
| sanitizers-tsan      | ubuntu-24.04       | Clang 17              | -fsanitize=thread             |
|                      |                    |                       | (Thread Sanitizer / Races)    |
+----------------------+--------------------+-----------------------+-------------------------------+
| sanitizers-ubsan     | ubuntu-24.04       | Clang 17              | -fsanitize=undefined          |
|                      |                    |                       | (Undefined Behavior / Bounds) |
+----------------------+--------------------+-----------------------+-------------------------------+
| fuzzing-regression   | ubuntu-24.04       | Clang 17              | libFuzzer smoke run (1k iter) |
+----------------------+--------------------+-----------------------+-------------------------------+
| onvif-conformance    | ubuntu-24.04       | GCC 13 (Release)      | Run Python WSDL/XSD test suite|
+----------------------+--------------------+-----------------------+-------------------------------+
```

### 7.2 Static Analysis & Quality Gate Integration

Every CI execution runs strict static verification scripts before compiling test binaries:
1. `python3 scripts/verify_iwyu_cstdint.py --files libs/Onvif/*`: Enforces explicit `<cstdint>` inclusion and `std::` qualification.
2. `python3 scripts/verify_function_lengths.py`: Verifies all function names are strictly under 30 characters.
3. `python3 scripts/verify_type_conversions.py`: Audits signed/unsigned conversions for explicit `static_cast`.

---

## 8. Compliance, Quality Gates & Verification Checklist

Before any test code or harness is merged into master, it must satisfy all rules in [`.agents/rules/verification-checklist.md`](../.agents/rules/verification-checklist.md):

- [ ] **No Raw `new` or `delete`:** All mock objects, test fixtures, and network buffers use RAII and smart pointers (`std::unique_ptr`, `std::make_shared`).
- [ ] **Include-What-You-Use (IWYU):** Every test file explicitly includes `<cstdint>`, `<cstddef>`, and all required standard headers. No relying on transitive includes.
- [ ] **Strict `<cstdint>` Qualification:** All fixed-width integer types use `std::` (e.g., `std::uint8_t`, `std::int32_t`).
- [ ] **Identifier Length Limit:** All test names, fixture classes, and helper functions are strictly under 30 characters in length.
- [ ] **Zero Qt Dependencies:** All tests in `libs/Onvif/tests/` depend strictly on standard library, OpenSSL, pugixml, and GoogleTest. Zero `#include <Q...>`.
- [ ] **Zero Warnings:** All test code compiles cleanly with `-Wall -Wextra -Wpedantic -Werror` on GCC/Clang and `/W4 /WX` on MSVC.
- [ ] **Zero Leaks & Zero Races:** All tests pass with zero errors under ASan, LSan, and TSan.
- [ ] **Documentation:** Test fixtures and helper utilities are fully documented with Doxygen comments (`/// @brief`, `@details`, `@param`, `@return`).

---

## 9. Implementation Schedule, Effort Estimation & Risk Management

### 9.1 Phased Execution Timeline

```
+---------------------------------------------------------------------------------------------------+
|                                 P4 EXECUTION SCHEDULE & EFFORT                                    |
+-------+---------------------------------------+-------------+--------------------+----------------+
| Phase | Scope / Deliverable                   | Effort (Est)| Dependencies       | Status         |
+-------+---------------------------------------+-------------+--------------------+----------------+
| P4.1  | Mega-Test Decomposition               | 4–5 days    | P3 Modularization  | Planned        |
|       | (13 granular test fixtures)           |             |                    |                |
+-------+---------------------------------------+-------------+--------------------+----------------+
| P4.2  | Negative & Security Fault Suites      | 4–5 days    | P4.1 Fixtures      | Planned        |
|       | (Protocol faults, SSRF, replay, NaN)  |             |                    |                |
+-------+---------------------------------------+-------------+--------------------+----------------+
| P4.3  | Continuous libFuzzer Harnesses        | 3–4 days    | Clang toolchain    | Planned        |
|       | (5 harnesses, corpora, CMake)         |             |                    |                |
+-------+---------------------------------------+-------------+--------------------+----------------+
| P4.4  | ONVIF Conformance Runner (Python)     | 4–5 days    | P2 Profile S       | Planned        |
|       | (WSDL/XSD schema test pipeline)       |             |                    |                |
+-------+---------------------------------------+-------------+--------------------+----------------+
| P4.5  | Multi-Platform CI & Sanitizers Matrix | 2–3 days    | GitHub Actions     | Planned        |
|       | (Linux GCC/Clang, ASan/TSan/UBSan)    |             |                    |                |
+-------+---------------------------------------+-------------+--------------------+----------------+
```

### 9.2 Risk Management & Mitigation

1. **Port Collisions During Parallel CTest Execution:**
   - *Risk:* Multiple tests attempting to bind to `127.0.0.1:8080` concurrently fail due to `EADDRINUSE`.
   - *Mitigation:* `OnvifServerFixture` uses ephemeral OS-assigned ports (port `0`) or dynamic port probing to ensure zero port conflicts across parallel test runners.
2. **Slow Sanitizer Builds in CI:**
   - *Risk:* TSan and ASan builds may exceed GitHub Actions runner timeouts if executing long sleeps.
   - *Mitigation:* Mock time sources (`std::chrono`) and minimize polling intervals in test clients (reduce sleep intervals from 100 ms to 5–10 ms).
3. **Flaky Network Tests in Virtualized CI:**
   - *Risk:* Multicast UDP discovery frames may be dropped or delayed in containerized CI environments.
   - *Mitigation:* Provide unicast fallback test modes and loopback multicast socket configuration (`IP_MULTICAST_LOOP`) specifically enabled in test configurations.
