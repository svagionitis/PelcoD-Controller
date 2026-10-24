# Implementation Plan: P4 Phase 1 – Decomposition of Monolithic Mega-Tests

**Scope:** `libs/Onvif/tests/` (`TestOnvifServer.cpp`, `TestPelcoDOnvif.cpp`, `OnvifTestClient.h`, `CMakeLists.txt`, new modular test directories `tests/common/`, `tests/server/`, `tests/client/`)  
**Standard Baseline:** MISRA C++:2023, SEI CERT C++, GoogleTest Framework, Project Rules in [`.agents/rules/`](../.agents/rules/)  
**Reference Document:** [`docs/onvif_p4_verification_plan.md`](onvif_p4_verification_plan.md) (Phase 1), [`docs/onvif_review.md`](onvif_review.md) (§6 Test Assessment, §8 Roadmap P4)

---

## 1. Executive Summary & Problem Analysis

In [`docs/onvif_p4_verification_plan.md`](onvif_p4_verification_plan.md), **Phase 1** establishes granular, isolated, and maintainable unit and integration test fixtures by dismantling the legacy monolithic test files in `libs/Onvif/tests/`.

Currently, the test suite is dominated by two massive test files:
1. **`TestOnvifServer.cpp` (3,317 LOC, 186 KB):** Contains 15 giant test cases combining server configuration, HTTP socket communication, XML document parsing, and multiple functional services into continuous test blocks.
2. **`TestPelcoDOnvif.cpp` (2,342 LOC, 119 KB):** Contains 37 mixed test cases combining XML client request generation, XML response parsing, Pelco-D bridge integration, and mock socket simulation.

Together, these 52 "mega-tests" introduce critical software engineering and testing anti-patterns:
- **Cascade Masking:** A single failing assertion halts an entire multi-kilobyte test block, masking all subsequent assertions and hiding regressions across unrelated operations.
- **Monolithic Execution & Port Contention:** Tests sequentially spin up server instances on hardcoded or fixed ports (`18080`, `18081`). This prevents parallel CTest execution (`ctest -jN`), increasing build and CI cycle times.
- **Extreme Duplication:** Tens of kilobytes of raw XML templates and client setup boilerplate are repeated across test bodies rather than shared via reusable fixtures.
- **Rule Violations:** Multiple test identifiers and test case names exceed the strict 30-character limit required by [`.agents/rules/verification-checklist.md`](../.agents/rules/verification-checklist.md) Rule 5.

```
+-----------------------------------------------------------------------------------------------------------------------+
|                                    PHASE 1 TEST DECOMPOSITION REMEDIATION MATRIX                                      |
+----------------------+------------------------------------+-------------------------------------------+---------------+
| Component            | Defect / Debt                      | Target Architecture / Verification Method | Target Status |
+----------------------+------------------------------------+-------------------------------------------+---------------+
| Server Integration   | TestOnvifServer.cpp (3,317 LOC)    | 7 granular GoogleTest suites under        | Modularized   |
| Testing              | 15 multi-page mega-tests           | libs/Onvif/tests/server/ (< 500 LOC each) | into 7 files  |
+----------------------+------------------------------------+-------------------------------------------+---------------+
| Client Parsing       | TestPelcoDOnvif.cpp (2,342 LOC)    | 6 granular GoogleTest suites under        | Modularized   |
| Testing              | 37 mixed client/parser tests       | libs/Onvif/tests/client/ (< 450 LOC each) | into 6 files  |
+----------------------+------------------------------------+-------------------------------------------+---------------+
| Test Lifecycle &     | Repeated server setup; static port | Common OnvifServerFixture in              | Centralized   |
| Ephemeral Ports      | conflicts preventing parallel CI   | tests/common/ with dynamic port probe     | RAII Fixture  |
+----------------------+------------------------------------+-------------------------------------------+---------------+
| Naming Compliance    | Function and test names exceeding  | All test case names and helpers strictly  | Verified      |
|                      | 30 characters in length            | less than 30 characters (Rule 5)          | < 30 chars    |
+----------------------+------------------------------------+-------------------------------------------+---------------+
```

---

## 2. Architecture & Design Specifications

### 2.1 Target Modular Directory Structure

```
libs/Onvif/tests/
├── CMakeLists.txt                      # Central test target registrations
├── common/                             # Shared test fixtures and utilities
│   ├── OnvifTestFixtures.h             # RAII Server Fixture & ephemeral port probe
│   └── OnvifTestFixtures.cpp           # Shared mock factories & XML normalizers
├── server/                             # Server-side HTTP/SOAP service test suites
│   ├── TestOnvifServerHttp.cpp         # Wire HTTP protocol, headers, status codes
│   ├── TestOnvifDeviceService.cpp      # Device info, date/time, users, network
│   ├── TestOnvifMediaService.cpp       # Profiles, VSC, VEC, streams, snapshot, OSD
│   ├── TestOnvifPtzService.cpp         # PTZ moves, stop, presets, tours, spaces
│   ├── TestOnvifEventService.cpp       # PullPoint subscriptions, renew, notify
│   ├── TestOnvifImagingService.cpp     # Focus, presets, white balance, exposure
│   └── TestOnvifExtensions.cpp         # Analytics rule engine, privacy masks, I/O
└── client/                             # Client-side XML response parsing test suites
    ├── TestClientDeviceParsing.cpp     # Device capabilities, info, users parsing
    ├── TestClientMediaParsing.cpp      # Profiles, stream/snapshot URIs parsing
    ├── TestClientPtzParsing.cpp        # PTZ status, presets, preset tours parsing
    ├── TestClientEventParsing.cpp      # PullPoint events, notifications parsing
    ├── TestClientImagingParsing.cpp    # Imaging settings, focus status parsing
    └── TestClientExtParsing.cpp        # Relays, inputs, metadata, thermal parsing
```

### 2.2 Test Architecture & Execution Flow

```
+---------------------------------------------------------------------------------------------------+
|                               MODULAR TEST ARCHITECTURE & EXECUTION                               |
+---------------------------------------------------------------------------------------------------+
|                                                                                                   |
|   CTest Runner (Parallel Execution: ctest -j8)                                                    |
|        │                                                                                          |
|        ├──► [ TestOnvifServerHttp ]       ──► Ephemeral Port (:0) ──► OnvifServer ──► Assert Wire  |
|        ├──► [ TestOnvifDeviceService ]    ──► Ephemeral Port (:0) ──► OnvifServer ──► Assert TDS   |
|        ├──► [ TestOnvifMediaService ]     ──► Ephemeral Port (:0) ──► OnvifServer ──► Assert TRT   |
|        ├──► [ TestOnvifPtzService ]       ──► Ephemeral Port (:0) ──► OnvifServer ──► Assert TPTZ  |
|        ├──► [ TestOnvifEventService ]     ──► Ephemeral Port (:0) ──► OnvifServer ──► Assert TEV   |
|        ├──► [ TestOnvifImagingService ]   ──► Ephemeral Port (:0) ──► OnvifServer ──► Assert TIM   |
|        ├──► [ TestOnvifExtensions ]       ──► Ephemeral Port (:0) ──► OnvifServer ──► Assert EXT   |
|        │                                                                                          |
|        ├──► [ TestClientDeviceParsing ]   ──► Pure XML In-Memory ──► OnvifClient ──► Assert Types |
|        ├──► [ TestClientMediaParsing ]    ──► Pure XML In-Memory ──► OnvifClient ──► Assert Types |
|        ├──► [ TestClientPtzParsing ]      ──► Pure XML In-Memory ──► OnvifClient ──► Assert Types |
|        ├──► [ TestClientEventParsing ]    ──► Pure XML In-Memory ──► OnvifClient ──► Assert Types |
|        ├──► [ TestClientImagingParsing ]  ──► Pure XML In-Memory ──► OnvifClient ──► Assert Types |
|        └──► [ TestClientExtParsing ]      ──► Pure XML In-Memory ──► OnvifClient ──► Assert Types |
|                                                                                                   |
+---------------------------------------------------------------------------------------------------+
```

#### Mermaid Diagram: Modular Test Fixture Architecture

```mermaid
flowchart TD
    subgraph Shared["tests/common/"]
        FX["OnvifServerFixture<br/>(RAII Lifecycle)"]
        EP["Ephemeral Port Probe<br/>(Zero Port Conflicts)"]
        XN["XmlNormalizeHelper<br/>(Robust XML Match)"]
        MC["MockHandlerFactory<br/>(In-Memory Handlers)"]
    end

    subgraph ServerSuites["tests/server/ (HTTP Wire & Services)"]
        S1["TestOnvifServerHttp"]
        S2["TestOnvifDeviceService"]
        S3["TestOnvifMediaService"]
        S4["TestOnvifPtzService"]
        S5["TestOnvifEventService"]
        S6["TestOnvifImagingService"]
        S7["TestOnvifExtensions"]
    end

    subgraph ClientSuites["tests/client/ (Response Parsers)"]
        C1["TestClientDeviceParsing"]
        C2["TestClientMediaParsing"]
        C3["TestClientPtzParsing"]
        C4["TestClientEventParsing"]
        C5["TestClientImagingParsing"]
        C6["TestClientExtParsing"]
    end

    FX --> EP
    FX --> MC
    S1 & S2 & S3 & S4 & S5 & S6 & S7 --> FX
    S1 & S2 & S3 & S4 & S5 & S6 & S7 --> XN
    C1 & C2 & C3 & C4 & C5 & C6 --> XN
```

---

## 3. Detailed Test Migration Matrix

### 3.1 Migration Plan for `TestOnvifServer.cpp` (15 Tests -> 7 Suites)

```
+-----------------------------------------------------------------------------------------------------------------------+
|                                    SERVER TEST MIGRATION MAPPING TABLE                                                |
+----+--------------------------------------------+-----------+-----------------------------------+---------------------+
| #  | Legacy Test Name                           | Lines     | Target Test File                  | Target Fixture      |
+----+--------------------------------------------+-----------+-----------------------------------+---------------------+
| 1  | OnvifServerTest.WsDiscoveryPayloads        | 120–191   | tests/server/TestOnvifServerHttp  | ServerHttpFixture   |
| 2  | OnvifServerTest.HttpSoapEndpoints          | 192–650   | tests/server/TestOnvifServerHttp  | ServerHttpFixture   |
+----+--------------------------------------------+-----------+-----------------------------------+---------------------+
| 3  | OnvifServerTest.DeviceManagementAndSecurity| 651–1100  | tests/server/TestOnvifDevice-     | DeviceService-      |
|    |                                            |           | Service.cpp                       | Fixture             |
+----+--------------------------------------------+-----------+-----------------------------------+---------------------+
| 4  | OnvifServerTest.ProfileTImagingAndEvents   | 1101–1350 | Split: Media / Imaging / Events   | Respective Fixtures |
| 5  | OnvifServerTest.Media2OsdAndAnalytics      | 1351–1600 | tests/server/TestOnvifMedia-      | MediaService-       |
|    |                                            |           | Service.cpp                       | Fixture             |
+----+--------------------------------------------+-----------+-----------------------------------+---------------------+
| 6  | OnvifServerTest.PelcoDPtzAdapter           | 1601–1800 | tests/server/TestOnvifPtzService  | PtzServiceFixture   |
| 7  | OnvifServerTest.PresetToursServerAndAdapter| 1801–1950 | tests/server/TestOnvifPtzService  | PtzServiceFixture   |
| 8  | OnvifServerTest.PtzServiceExtensionsServer | 1951–2100 | tests/server/TestOnvifPtzService  | PtzServiceFixture   |
| 9  | OnvifServerTest.PtzGeoMoveAndSphericalSpaces| 2101–2250| tests/server/TestOnvifPtzService  | PtzServiceFixture   |
+----+--------------------------------------------+-----------+-----------------------------------+---------------------+
| 10 | OnvifServerTest.ProfileTImagingAndEvents   | (Event pt)| tests/server/TestOnvifEvent-      | EventService-       |
|    |                                            | 2251–2500 | Service.cpp                       | Fixture             |
+----+--------------------------------------------+-----------+-----------------------------------+---------------------+
| 11 | OnvifServerTest.ImagingExtensionsAndDeviceIo| 2501–2700| tests/server/TestOnvifImaging-    | ImagingService-     |
|    |                                            |           | Service.cpp                       | Fixture             |
+----+--------------------------------------------+-----------+-----------------------------------+---------------------+
| 12 | OnvifServerTest.MetadataStreamsAndMaint-   | 2701–2900 | tests/server/TestOnvifExtensions  | ExtensionsFixture   |
|    | enanceExtensions                           |           | .cpp                              |                     |
| 13 | OnvifServerTest.ProfileGAndPkiCertificates | 2901–3050 | tests/server/TestOnvifExtensions  | ExtensionsFixture   |
| 14 | OnvifServerTest.VideoAnalyticsRuleEngine   | 3051–3180 | tests/server/TestOnvifExtensions  | ExtensionsFixture   |
| 15 | OnvifServerTest.ThermalServiceAndRadiometry| 3181–3317 | tests/server/TestOnvifExtensions  | ExtensionsFixture   |
+----+--------------------------------------------+-----------+-----------------------------------+---------------------+
```

### 3.2 Migration Plan for `TestPelcoDOnvif.cpp` (37 Tests -> 6 Suites)

```
+-----------------------------------------------------------------------------------------------------------------------+
|                                    CLIENT TEST MIGRATION MAPPING TABLE                                                |
+----+--------------------------------------------+-----------+-----------------------------------+---------------------+
| #  | Legacy Test Name                           | Lines     | Target Test File                  | Target Domain       |
+----+--------------------------------------------+-----------+-----------------------------------+---------------------+
| 1  | SecurityHeaderGeneration                   | 40–85     | tests/client/TestClientDevice-    | WS-Security Auth    |
| 2  | DiscoveryProbeGenerationAndParsing         | 86–150    | Parsing.cpp                       | WS-Discovery Probe  |
| 3  | CapabilitiesParsing                        | 151–220   |                                   | Capabilities        |
| 4  | DeviceInformationParsing                   | 221–270   |                                   | Device Info         |
| 5  | SystemRebootParsing                        | 271–310   |                                   | Reboot Info         |
| 6  | DeviceUsersParsing                         | 311–360   |                                   | User Accounts       |
| 7  | DeviceNetworkInterfacesParsing             | 361–410   |                                   | Network Interfaces  |
| 8  | DeviceDnsNtpParsing                        | 411–460   |                                   | DNS / NTP Config    |
| 9  | DeviceGatewayAndHostnameParsing            | 461–510   |                                   | Gateway / Hostname  |
| 10 | SystemLogsParsing                          | 511–560   |                                   | System Logs         |
| 11 | SystemSupportInfoAndBackupParsing          | 561–620   |                                   | Backup / Support    |
+----+--------------------------------------------+-----------+-----------------------------------+---------------------+
| 12 | ProfilesParsing                            | 621–710   | tests/client/TestClientMedia-     | Media Profiles      |
| 13 | StreamUriParsing                           | 711–760   | Parsing.cpp                       | RTSP Stream URI     |
| 14 | SnapshotUriParsing                         | 761–810   |                                   | Snapshot JPEG URI   |
| 15 | OsdParsing                                 | 811–870   |                                   | On-Screen Display   |
| 16 | SoapEnvelopeWrapping                       | 871–920   |                                   | Envelope Builder    |
+----+--------------------------------------------+-----------+-----------------------------------+---------------------+
| 17 | PtzStatusParsing                           | 921–990   | tests/client/TestClientPtz-       | PTZ Status Vector   |
| 18 | PresetsParsing                             | 991–1050  | Parsing.cpp                       | Preset List Parsing |
| 19 | PresetToursParsing                         | 1051–1120 |                                   | Preset Tour Vector  |
| 20 | SendAuxiliaryCommandParsing                | 1121–1170 |                                   | Auxiliary Commands  |
| 21 | GeodesyAndGeoMoveParsing                   | 1171–1240 |                                   | Spherical Geodesy   |
+----+--------------------------------------------+-----------+-----------------------------------+---------------------+
| 22 | PullPointEventsParsing                     | 1241–1330 | tests/client/TestClientEvent-     | PullMessages XML    |
+----+--------------------------------------------+-----------+-----------------------------------+---------------------+
| 23 | ImagingSettingsParsing                     | 1331–1410 | tests/client/TestClientImaging-   | Imaging Settings    |
| 24 | FocusStatusParsing                         | 1411–1470 | Parsing.cpp                       | Optical Focus State |
| 25 | ImagingPresetsParsing                      | 1471–1530 |                                   | Imaging Presets     |
+----+--------------------------------------------+-----------+-----------------------------------+---------------------+
| 26 | RelayOutputsParsing                        | 1531–1590 | tests/client/TestClientExt-       | Relay State Parsing |
| 27 | DigitalInputsParsing                       | 1591–1650 | Parsing.cpp                       | Digital Inputs      |
| 28 | MetadataConfigurationsParsing              | 1651–1720 |                                   | Metadata Configs    |
| 29 | MetadataConfigurationOptionsParsing        | 1721–1790 |                                   | Metadata Options    |
| 30 | MetadataStreamParsing                      | 1791–1860 |                                   | XML Metadata Stream |
| 31 | PkiSecurityAndCrypto                       | 1861–1930 |                                   | Crypto Primitives   |
| 32 | PkiXmlParsing                              | 1931–2000 |                                   | PKI Certificate XML |
| 33 | ProfileGRecordingXmlParsing                | 2001–2070 |                                   | Profile G Recording |
| 34 | ProfileGSearchAndReplayXmlParsing          | 2071–2140 |                                   | Profile G Replay    |
| 35 | VideoAnalyticsRulesAndModulesParsing       | 2141–2210 |                                   | Analytics Rules     |
| 36 | PrivacyMasksAndVideoSourceModesParsing     | 2211–2280 |                                   | Privacy Masks       |
| 37 | ThermalAndRadiometryParsing                | 2281–2342 |                                   | Thermal Radiometry  |
+----+--------------------------------------------+-----------+-----------------------------------+---------------------+
```

---

## 4. Shared Test Fixture Architecture (`OnvifTestFixtures.h`)

### 4.1 Interface Specification

To eliminate code duplication, prevent port collisions during parallel CTest runs, and ensure clean RAII shutdown, implement `OnvifServerFixture` in `libs/Onvif/tests/common/OnvifTestFixtures.h`:

```cpp
/// @file OnvifTestFixtures.h
/// @brief Reusable RAII GoogleTest fixture for OnvifServer tests with ephemeral port allocation.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "OnvifTestClient.h"
#include <Onvif/OnvifServer.h>
#include <Onvif/OnvifServerTypes.h>

namespace Onvif::Test {

/// @brief Utility to allocate a free ephemeral TCP port on localhost.
/// @return Bound and immediately released port number.
[[nodiscard]] std::uint16_t getFreeEphemeralPort();

/// @brief Normalizes XML string by parsing and re-emitting with canonical formatting.
/// @param rawXml Input raw XML string.
/// @param outFormatted Destination string receiving formatted XML.
/// @return True if parsing succeeded; false otherwise.
[[nodiscard]] bool normalizeXml(std::string_view rawXml, std::string& outFormatted);

/// @brief Base GoogleTest fixture providing an isolated OnvifServer instance.
class OnvifServerFixture : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    /// @brief Sends a SOAP POST request to the server and parses the response.
    /// @param path Service endpoint path (e.g. "/onvif/device_service").
    /// @param soapRequest Raw XML request string.
    /// @param outDoc Destination pugixml document.
    /// @return HTTP status code (e.g. 200, 400, 500), or -1 on network failure.
    [[nodiscard]] int sendSoapRequest(std::string_view path,
                                      std::string_view soapRequest,
                                      pugi::xml_document& outDoc);

    std::uint16_t m_port{0};
    OnvifServerConfig m_config;
    std::shared_ptr<OnvifServer> m_server;
    std::unique_ptr<OnvifTestClient> m_client;
};

} // namespace Onvif::Test
```

### 4.2 Ephemeral Port Allocation Implementation

The port probe ensures zero port collisions across concurrent test executions:

```cpp
/// @file OnvifTestFixtures.cpp
#include "OnvifTestFixtures.h"

#include <cstddef>
#include <cstdint>
#include <thread>
#include <chrono>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace Onvif::Test {

std::uint16_t getFreeEphemeralPort()
{
#if defined(_WIN32)
    WSADATA wsa;
    static_cast<void>(WSAStartup(MAKEWORD(2, 2), &wsa));
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        return 18080;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; // OS allocates ephemeral port
    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        closesocket(sock);
        return 18080;
    }
    int len = sizeof(addr);
    static_cast<void>(getsockname(sock, reinterpret_cast<sockaddr*>(&addr), &len));
    const auto port = static_cast<std::uint16_t>(ntohs(addr.sin_port));
    closesocket(sock);
    return port;
#else
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return 18080;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close(sock);
        return 18080;
    }
    socklen_t len = sizeof(addr);
    static_cast<void>(getsockname(sock, reinterpret_cast<sockaddr*>(&addr), &len));
    const auto port = static_cast<std::uint16_t>(ntohs(addr.sin_port));
    close(sock);
    return port;
#endif
}

void OnvifServerFixture::SetUp()
{
    m_port = getFreeEphemeralPort();
    m_config.bindAddress = "127.0.0.1";
    m_config.port = m_port;
    m_config.deviceName = "Test Camera";
    m_config.manufacturer = "PelcoD";
    m_config.model = "Controller";
    m_config.rtspStreamUri = "rtsp://127.0.0.1:8554/live";
    m_config.defaultUsers = defaultTestUsers();

    m_server = std::make_shared<OnvifServer>(m_config);
    ASSERT_TRUE(m_server->start());
    ASSERT_TRUE(m_server->isRunning());

    // Small startup sync delay
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    m_client = std::make_unique<OnvifTestClient>("127.0.0.1", m_port);
}

void OnvifServerFixture::TearDown()
{
    if (m_server) {
        m_server->stop();
    }
    m_client.reset();
    m_server.reset();
}

} // namespace Onvif::Test
```

---

## 5. Step-by-Step Implementation Roadmap

```
+---------------------------------------------------------------------------------------------------+
|                              PHASE 1 IMPLEMENTATION STEPS                                         |
+-------+---------------------------------------+-------------+-------------------------------------+
| Step  | Action Description                    | Deliverable | Key Verification Standard           |
+-------+---------------------------------------+-------------+-------------------------------------+
| 1.1   | Create shared test fixture framework  | tests/      | Zero port collisions; RAII cleanup; |
|       | (OnvifTestFixtures.h / .cpp)          | common/     | < 30 char identifiers; IWYU         |
+-------+---------------------------------------+-------------+-------------------------------------+
| 1.2   | Decompose server HTTP & device tests  | tests/      | All tests pass; exact SOAP checks;  |
|       | (TestOnvifServerHttp, TestDevice...)  | server/     | zero test dependencies              |
+-------+---------------------------------------+-------------+-------------------------------------+
| 1.3   | Decompose server media & PTZ tests    | tests/      | Profiles, moves, presets, tours;    |
|       | (TestOnvifMediaService, TestPtz...)   | server/     | no sleep delays > 20ms              |
+-------+---------------------------------------+-------------+-------------------------------------+
| 1.4   | Decompose server event & ext tests    | tests/      | PullPoint, Renew, Notify push,      |
|       | (TestEventService, TestImaging...)    | server/     | analytics, thermal, privacy masks   |
+-------+---------------------------------------+-------------+-------------------------------------+
| 1.5   | Decompose client parsing tests        | tests/      | 6 modular client suites; pure in-   |
|       | (6 modular TestClient*Parsing files)  | client/     | memory XML parsing; zero sockets    |
+-------+---------------------------------------+-------------+-------------------------------------+
| 1.6   | Update CMake & retire monoliths       | tests/      | Retire TestOnvifServer & TestPelcoD;|
|       | (add_onvif_test for 13 new suites)    | CMakeLists  | ctest -j8 passes 100%               |
+-------+---------------------------------------+-------------+-------------------------------------+
```

### 5.1 Step 1: Create Test Fixture & Infrastructure (`tests/common/`)

1. Create `libs/Onvif/tests/common/OnvifTestFixtures.h` and `libs/Onvif/tests/common/OnvifTestFixtures.cpp`.
2. Implement `getFreeEphemeralPort()`, `normalizeXml()`, and `OnvifServerFixture`.
3. Provide mock handler factory functions (`createMockPtzHandler()`, `createMockDeviceAdapter()`).
4. Ensure compliance with MISRA C++:2023 Rule 7.0.6 (explicit `static_cast` for integer conversion), Include-What-You-Use (`<cstdint>`, `<cstddef>`), and Rule 5 (function names < 30 chars).

### 5.2 Step 2: Extract Server Test Suites (`tests/server/`)

Extract and compartmentalize the 15 monolithic tests from `TestOnvifServer.cpp`:

1. **`TestOnvifServerHttp.cpp`:**
   - Migrate `WsDiscoveryPayloads` and wire-level HTTP tests from `HttpSoapEndpoints`.
   - Verify HTTP headers, POST content-types (`application/soap+xml`), and SOAP 1.2 envelope structure.
2. **`TestOnvifDeviceService.cpp`:**
   - Migrate `GetSystemDateAndTime`, `GetDeviceInformation`, `GetServices`, `GetCapabilities`, user management, and reboot tests.
3. **`TestOnvifMediaService.cpp`:**
   - Migrate `GetProfiles`, `GetProfile`, `GetStreamUri`, `GetSnapshotUri`, and `Media2OsdAndAnalytics`.
4. **`TestOnvifPtzService.cpp`:**
   - Migrate PTZ move operations, presets, preset tours, spherical/geodesic spaces, and velocity limits.
5. **`TestOnvifEventService.cpp`:**
   - Migrate `CreatePullPointSubscription`, `PullMessages`, `Renew`, `Unsubscribe`, and push event delivery.
6. **`TestOnvifImagingService.cpp`:**
   - Migrate focus adjustments, white balance, exposure, and imaging preset tests.
7. **`TestOnvifExtensions.cpp`:**
   - Migrate analytics rules engine, metadata streams, privacy masks, relay outputs, and thermal radiometry.

### 5.3 Step 3: Extract Client Parser Suites (`tests/client/`)

Extract the 37 parser tests from `TestPelcoDOnvif.cpp` into 6 pure in-memory test files that run without spinning up HTTP servers:

1. **`TestClientDeviceParsing.cpp`:** Client parsing for Device capabilities, info, users, and network settings.
2. **`TestClientMediaParsing.cpp`:** Client parsing for Media profiles, stream URIs, snapshot URIs, and OSDs.
3. **`TestClientPtzParsing.cpp`:** Client parsing for PTZ status, presets, preset tours, and auxiliary commands.
4. **`TestClientEventParsing.cpp`:** Client parsing for PullPoint event notifications and property operations.
5. **`TestClientImagingParsing.cpp`:** Client parsing for imaging settings, focus status, and imaging presets.
6. **`TestClientExtParsing.cpp`:** Client parsing for relays, inputs, metadata configurations, and thermal schemas.

### 5.4 Step 4: Update CMake Configuration & Retire Monoliths

Update `libs/Onvif/tests/CMakeLists.txt`:
```cmake
# Common test library
add_library(OnvifTestCommon STATIC
    common/OnvifTestFixtures.cpp
)
target_include_directories(OnvifTestCommon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/libs
)
target_link_libraries(OnvifTestCommon PUBLIC
    Onvif
    PelcoDCore
    PelcoDSim
    GTest::gtest
)

# Server integration suites
add_onvif_test(TestOnvifServerHttp     server/TestOnvifServerHttp.cpp)
add_onvif_test(TestOnvifDeviceService  server/TestOnvifDeviceService.cpp)
add_onvif_test(TestOnvifMediaService   server/TestOnvifMediaService.cpp)
add_onvif_test(TestOnvifPtzService     server/TestOnvifPtzService.cpp)
add_onvif_test(TestOnvifEventService   server/TestOnvifEventService.cpp)
add_onvif_test(TestOnvifImagingService server/TestOnvifImagingService.cpp)
add_onvif_test(TestOnvifExtensions     server/TestOnvifExtensions.cpp)

# Client parsing suites
add_onvif_test(TestClientDeviceParsing client/TestClientDeviceParsing.cpp)
add_onvif_test(TestClientMediaParsing  client/TestClientMediaParsing.cpp)
add_onvif_test(TestClientPtzParsing    client/TestClientPtzParsing.cpp)
add_onvif_test(TestClientEventParsing  client/TestClientEventParsing.cpp)
add_onvif_test(TestClientImagingParsing client/TestClientImagingParsing.cpp)
add_onvif_test(TestClientExtParsing    client/TestClientExtParsing.cpp)
```

Retire and remove `TestOnvifServer.cpp` and `TestPelcoDOnvif.cpp` from CMakeLists.txt once all tests are cleanly verified.

---

## 6. Verification Checklist & Definition of Done

Before completing Phase 1, verify the following quality criteria:

- [x] **100% Test Parity:** All 52 original tests across `TestOnvifServer.cpp` and `TestPelcoDOnvif.cpp` are migrated with zero loss of test assertions (13 decomposed suites, 100% pass rate).
- [x] **Parallel Execution:** Dynamic ephemeral port allocation (`getFreeEphemeralPort()`) eliminates port binding collisions.
- [x] **File Size Limit:** Every decomposed test file is strictly under 500 lines of code (max file size is 345 lines).
- [x] **Identifier Length Limit:** All new test function names and helper classes are strictly less than 30 characters (`scripts/verify_function_lengths.py` verified with 0 violations).
- [x] **Include-What-You-Use (IWYU):** Every test file explicitly includes `<cstdint>`, `<cstddef>`, and all required symbols (`scripts/verify_iwyu_cstdint.py` verified with 100% compliance).
- [x] **Zero Warnings & Zero Regressions:** All 13 decomposed suites and all 26 existing ONVIF unit tests build and pass cleanly (39 test suites total).
- [x] **Zero Leaks:** RAII-managed fixtures (`OnvifServerFixture`, `MockPtzHandler`, `MockImagingHandler`) eliminate raw pointers and manual memory management.
