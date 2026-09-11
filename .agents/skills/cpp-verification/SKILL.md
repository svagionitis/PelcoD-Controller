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
