---
name: cpp-verification
description: >-
  Use this skill before outputting C++17 code snippets to verify safety, style, and compliance against the project checklist.
---

# C++17 Verification Checklist

Before presenting any C++17 code snippet, internally verify it against this checklist:

1. **RAII / Memory:** Did I use any raw `new` or `delete`? (If yes, rewrite using RAII/Smart Pointers).
2. **Initialization:** Are all variables explicitly initialized using `{}` where possible?
3. **Safety Attributes:** Did I apply `[[nodiscard]]` to functions returning safety-critical data?
4. **Documentation:** Are all public APIs documented with full Doxygen templates?
5. **Standards Compliance:** Does this code violate any MISRA/AUTOSAR rules (e.g., dynamic casting, implicit conversions)?
6. **TDD:** Did I write a failing test first if fixing a bug?
7. **Header Guards:** Did I use `#pragma once` at the top of headers instead of legacy `#ifndef` / `#define` guards?
8. **`<cstdint>` & Types:** Did I explicitly `#include <cstdint>` for all fixed-width integer types and strictly qualify with `std::` (e.g., `std::uint8_t`, `std::int32_t`)? Did I use `<cstddef>` for `std::size_t`?
9. **IWYU (Include-What-You-Use):** Does every file directly include all headers it depends on without assuming transitive includes, using forward declarations in headers where pointer/reference usage suffices?
10. **Docs & Diagrams:** Are all file paths in documentation relative? If diagrams are involved, are both ASCII and Mermaid diagrams included?
11. **Verification Script:** Did I run `python3 scripts/verify_iwyu_cstdint.py --files <files>` to ensure zero compliance violations?
