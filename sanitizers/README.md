# Sanitizer Suppression Configurations

This directory contains suppression rule files for LLVM dynamic sanitizers:
- `lsan.supp`: LeakSanitizer (LSan) suppressions for unavoidable third-party allocations.
- `tsan.supp`: ThreadSanitizer (TSan) suppressions for third-party runtime race conditions.
- `ubsan.supp`: UndefinedBehaviorSanitizer (UBSan) suppressions.

---

## Strict Policy: Zero Suppressions for Project Code

1. **In-Tree Code is Strictly Unsuppressed:**
   - Suppressions are **strictly prohibited** for any file, function, namespace, or symbol inside `libs/Onvif/` or `libs/PelcoDCore/`.
   - Any memory leak, thread race, undefined behavior, or bounds violation within project code is considered a critical bug and must be fixed at the source.

2. **Third-Party Whitelist Criteria:**
   - Every suppression rule must target an external third-party library (e.g. OpenSSL, glibc, GoogleTest, pugixml).
   - Every rule must be preceded by an explanatory comment detailing why the suppression is necessary (e.g., one-time static allocations in OpenSSL, dynamic linker TLS initialization in glibc).
