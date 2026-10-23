#!/usr/bin/env python3
"""
verify_function_lengths.py - Audit C++ function and method identifier lengths.
Enforces Project Rule 5: All function names must be strictly < 30 characters.
"""

import os
import re
import sys
from typing import List, Tuple

IGNORED_PREFIXES = (
    "TEST", "TEST_F", "TEST_P", "EXPECT_", "ASSERT_", "OPENSSL_", "EVP_", "X509_",
    "ASN1_", "BIO_", "BN_", "CURL", "HTTP_", "PELCOD_", "PelcoD_"
)

# Regex matching C++ function declarations or definitions: return_type func_name(
FUNCTION_PATTERN = re.compile(
    r'\b([A-Za-z_][A-Za-z0-9_]{29,})\s*\('
)

def scan_file(filepath: str) -> List[Tuple[int, str, int]]:
    violations = []
    with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
        for idx, line in enumerate(f, 1):
            clean = line.strip()
            if clean.startswith("//") or clean.startswith("*") or clean.startswith("/*"):
                continue
            for match in FUNCTION_PATTERN.finditer(line):
                name = match.group(1)
                if not any(name.startswith(p) for p in IGNORED_PREFIXES):
                    violations.append((idx, name, len(name)))
    return violations

def main() -> int:
    target_dir = sys.argv[1] if len(sys.argv) > 1 else "libs/Onvif"
    total_violations = 0
    unique_violations = set()

    for root, dirs, files in os.walk(target_dir):
        # Skip build and hidden directories
        if "build" in root or ".git" in root:
            continue
        for file in files:
            if file.endswith((".h", ".cpp")):
                path = os.path.join(root, file)
                v = scan_file(path)
                for line, name, length in v:
                    print(f"VIOLATION: {path}:{line} '{name}' ({length} chars >= 30)")
                    total_violations += 1
                    unique_violations.add(name)

    print(f"\nAudit completed for directory: {target_dir}")
    print(f"Total violations found: {total_violations} (Unique identifiers: {len(unique_violations)})")

    if total_violations > 0:
        print("\nFAILURE: One or more function identifiers violate Project Rule 5 (< 30 characters).")
        return 1

    print("\nSUCCESS: All function identifiers are strictly < 30 characters.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
