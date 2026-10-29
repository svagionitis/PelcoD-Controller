---
trigger: always_on
---

# Verification Checklist (Run Before Outputting Code)

Before presenting any C++17 code snippet or committing changes, internally verify against this checklist:
1. Did I use any raw `new` or `delete`? (If yes, rewrite using RAII/Smart Pointers).
2. Are headers using `#pragma once`? (Never use `#ifndef / #define` include guards).
3. Are header (`.h`) and implementation (`.cpp`) files co-located in the same directory?
4. Do all Core libraries maintain **zero Qt dependencies** (`#include <Q...>` is strictly banned in all non-Qt libraries: `PelcoDCore`, `ViscaCore`, `SightlineCore`, `Onvif`, `Tracking`, `PayloadHal`, `Klv`, `Nmea`, and any newly added Core library)?
5. Are all function names concise (strictly less than 30 characters)?
6. Does transport code support both **Linux** (POSIX termios/sockets) and **Windows** (Win32 Comm API/Winsock2)?
7. Are all variables explicitly initialized using `{}` where possible?
8. Did I apply `[[nodiscard]]` to functions returning status or safety-critical data?
9. Are all public APIs documented with full Doxygen templates (`/// @brief`, `@details`, `@param`, `@return`)?
10. Does this code violate any MISRA/AUTOSAR rules (e.g., dynamic casting, implicit conversions)?
11. Did I write a failing test first if fixing a bug?
12. Does code compile with zero warnings under `-Wall -Wextra -pedantic` (`-Werror`) and `/W4` on MSVC, with hardening flags applied?
13. All the paths of the filenames in the docs should be relative.
14. If there are diagrams involved, add both ASCII and mermaid diagrams.
15. Did I explicitly `#include <cstdint>` in every file using fixed-width integers, and are all integer types strictly qualified with `std::` (e.g., `std::uint8_t`, `std::int32_t`)?
16. Does the code adhere to **IWYU (Include-What-You-Use)**: directly including every header for symbols, types, or templates used, never relying on transitive/indirect includes, and using forward declarations in headers where pointer/reference usage suffices?
17. Do callables passed to STL algorithms (and iterator-range constructors) use the range's exact value_type, with any conversion made explicit inside project code?
18. Are functions marked `noexcept` on fast paths (network receive, protocol parsing, tracking frame loops) completely free of heap allocations (`std::string`, `std::vector`, `new`, `std::make_unique`) to prevent `std::terminate` under memory exhaustion (CERT ERR55-CPP)?

