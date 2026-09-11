# Verification Checklist (Run Before Outputting Code)

Before presenting any C++17 code snippet or committing changes, internally verify against this checklist:
1. Did I use any raw `new` or `delete`? (If yes, rewrite using RAII/Smart Pointers).
2. Are headers using `#pragma once`? (Never use `#ifndef / #define` include guards).
3. Are header (`.h`) and implementation (`.cpp`) files co-located in the same directory?
4. Does `PelcoDCore` maintain **zero Qt dependencies** (`#include <Q...>` is strictly banned in Core)?
5. Are all function names concise (strictly less than 30 characters)?
6. Does transport code support both **Linux** (POSIX termios/sockets) and **Windows** (Win32 Comm API/Winsock2)?
7. Are all variables explicitly initialized using `{}` where possible?
8. Did I apply `[[nodiscard]]` to functions returning status or safety-critical data?
9. Are all public APIs documented with full Doxygen templates (`/// @brief`, `@details`, `@param`, `@return`)?
10. Does this code violate any MISRA/AUTOSAR rules (e.g., dynamic casting, implicit conversions)?
11. Did I write a failing test first if fixing a bug?
12. Does code compile with zero warnings under `-Wall -Wextra -pedantic` (`-Werror`) and `/W4` on MSVC, with hardening flags applied?

