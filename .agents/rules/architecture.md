# Architecture & Error Handling

## Project Architecture & Layer Boundaries
- **Three-Tier Architecture:**
  1. `PelcoDCore`: Standalone, pure C++17 library implementing the standard Pelco-D protocol (Version 5.0.1). **Zero Qt dependency** allowed. Must be usable by any client (Qt, Dear ImGui, CLI, ROS).
  2. `PelcoDQt`: Qt 6 adapter layer providing `QPelcoDDevice` (`QObject`, signals/slots).
  3. `app-qt/`: Qt 6 desktop GUI client with modern dark-theme dashboard.
- **Co-located File Organization:** Headers (`.h`) and implementations (`.cpp`) must reside in the same directory. Never create separate `include/` and `src/` directories.
- **Modular CMake & Security Hardening:** Each layer (`libs/PelcoDCore`, `libs/PelcoDQt`, `app-qt`, `tests`) has its own `CMakeLists.txt` and applies `apply_compiler_flags` from `cmake/CompilerFlags.cmake` (`-Werror`, `-Wconversion`, `-fstack-protector-strong`, sanitizers).
- **Header Inclusion:** Always use `#pragma once` across all headers. Never use `#ifndef / #define` include guards.
- **Cross-Platform Portability:** All transport drivers (`SerialTransport`, `TcpTransport`) and core logic must compile and run on both **Linux** (POSIX termios, BSD sockets) and **Windows** (Win32 Comm API, Winsock2).
- **Concurrency & Streaming:**
  - Inbound I/O streaming uses the Single-Producer Single-Consumer (SPSC) lock-free circular byte ring (`CircularByteRing`) with cacheline padding (`alignas(64)`). Transport read threads push raw bytes lock-free and zero-alloc.
  - Outbound commands are managed via a thread-safe pacing queue enforcing ~15–20 ms inter-command delay per Pelco-D RS-485 specifications with a bounded capacity (256 commands).

## Abstraction & Layer Boundaries
- **Program to Levels of Abstraction:** Lower-level mechanics (raw hardware I/O, socket descriptors, serial port handles, packet checksums) must be encapsulated in transport and framing layers (`ITransport`, `PelcoDFrame`, `CircularByteRing`). Expose high-level domain concepts (`panLeft`, `tiltUp`, `setPreset`, `DeviceStatus`) in `PelcoDDevice`.
- **Strict Layer Boundary Hierarchy:** Each layer may only communicate with its immediate neighbor directly below it. UI components must interact through `QPelcoDDevice` or `PelcoDDevice`, never touching raw transport handles directly.
- **Member Visibility:** Treat member visibility changes as a breaking design shift. Keep all fields and functions private unless external access is strictly required by the design. Prompt the user for explicit approval before changing any access modifier from private to internal or public.

## Error Handling & Safety
- **No Exceptions in Critical Paths:** Core library operations and packet parsers must not throw exceptions. Report errors via `bool`, `std::optional`, or error status codes.
- **`noexcept` Specification:** Mark all functions that are guaranteed not to throw as `noexcept` (especially destructors, move constructors, move assignment operators, and pure getters/parsers).

## OOP & Expressions
- **Virtual Destructors:** Every base class with virtual functions (e.g. `ITransport`) must explicitly declare a `virtual` destructor. Always use the `override` specifier for overridden virtual functions without repeating the `virtual` keyword.
- **No C-Style Casts:** Use `static_cast`, `const_cast`, or `reinterpret_cast` (only when absolutely necessary). Never use `(Type)value`.
- **No Side Effects in Evaluated Contexts:** Avoid pre/post-increment expressions mixed inside complex expressions (e.g., `array[i++] = ++j;` is banned). Keep statements atomic.

