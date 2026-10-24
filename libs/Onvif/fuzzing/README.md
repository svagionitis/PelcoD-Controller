# ONVIF Continuous Fuzz Testing via LLVM libFuzzer

This directory provides automated, coverage-guided continuous fuzz testing for the `libs/Onvif` subsystem using LLVM `libFuzzer` in combination with AddressSanitizer (ASan) and UndefinedBehaviorSanitizer (UBSan).

## 1. Fuzzing Target Harnesses

| Target Executable | Target Source | Ingress Tested |
| :--- | :--- | :--- |
| `fuzz_soap_router` | `harnesses/fuzz_soap_router.cpp` | Untrusted HTTP SOAP XML request payloads (port 8080) |
| `fuzz_ws_discovery` | `harnesses/fuzz_ws_discovery.cpp` | Untrusted UDP multicast Probe and ProbeMatches datagrams (port 3702) |
| `fuzz_client_xml` | `harnesses/fuzz_client_xml.cpp` | Untrusted HTTP SOAP XML camera responses |
| `fuzz_auth_headers` | `harnesses/fuzz_auth_headers.cpp` | Malformed WS-Security UsernameToken XML and HTTP Digest headers |
| `fuzz_url_validator` | `harnesses/fuzz_url_validator.cpp` | Hostile ConsumerReference URLs for SSRF filtering |

## 2. Running Continuous Fuzzing (Clang + libFuzzer)

### Prerequisites
- Clang compiler (version 15 or higher recommended)
- CMake 3.16+
- Linux or macOS (or Clang on Windows with compiler-rt libFuzzer)

### Build
```bash
cmake -B build -S . \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_C_COMPILER=clang \
  -DPELCOD_ENABLE_ONVIF=ON \
  -DPELCOD_ENABLE_FUZZING=ON \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo

cmake --build build --parallel $(nproc)
```

### Execution
Run an individual fuzz harness against its seed corpus with its syntax dictionary:
```bash
./build/libs/Onvif/fuzzing/fuzz_soap_router \
  -dict=libs/Onvif/fuzzing/dict/soap.dict \
  -max_len=65536 \
  -print_final_stats=1 \
  libs/Onvif/fuzzing/corpus/soap_router/
```

### Corpus Minimization
To merge and minimize newly discovered inputs into the authoritative corpus:
```bash
./build/libs/Onvif/fuzzing/fuzz_soap_router \
  -merge=1 \
  libs/Onvif/fuzzing/corpus/soap_router/ \
  /tmp/new_discovered_inputs/
```

## 3. Deterministic Regression Testing (All Compilers)

When building normally with `BUILD_TESTING=ON` (using GCC, MSVC, or Clang without `-fsanitize=fuzzer`), the `TestFuzzRegression` test target is compiled:
```bash
ctest --test-dir build -R TestFuzzRegression --output-on-failure
```
This ensures that every seed file in `corpus/*` is deterministically executed in CI on every push and pull request.
