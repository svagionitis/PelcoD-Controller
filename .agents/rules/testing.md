# Testing & Verification

## Test-Driven Bug Fixing
When a bug is being fixed:
1. Do not write the fix right away.
2. First, write the test that reproduces the bug.
3. Observe it failing (Red).
4. Write the fix.
5. Observe the test passing (Green).

## Repository Testing Conventions
- **Automated CTest Suite:** Run tests via `ctest --test-dir build --output-on-failure`. All tests must pass 100%.
- **Hardware-Free Simulation:** Use `MockPelcoDDevice` for end-to-end testing of `PelcoDDevice` without physical serial or network hardware connected.
- **Protocol Conformance:** Verify packet sizes, Pelco-D checksum calculations, and command encoding against the official Pelco-D specification (Pelco PTZ Protocols: D Protocol Version 5.0.1).
- **Concurrency & Lock-Free Validation:** Concurrency data structures (e.g., `CircularByteRing`) must include multi-threaded producer/consumer stress tests verifying zero byte loss or race conditions under high volume.

