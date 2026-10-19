#!/usr/bin/env python3
"""
verify_iwyu_cstdint.py - Automated verification for <cstdint> inclusion and IWYU compliance.

Performs static analysis on C++ header (.h, .hpp) and source (.cpp, .cc) files to verify:
1. Strict inclusion of <cstdint> whenever fixed-width integer types are referenced.
2. Proper std:: namespace qualification for fixed-width types (e.g., std::uint8_t vs bare uint8_t).
3. Direct inclusion of essential standard headers (IWYU: std::vector -> <vector>, std::unique_ptr -> <memory>, etc.).
4. Detection of missing <cstddef> when size_t/ptrdiff_t are used without standard library definitions.

Usage:
    python3 scripts/verify_iwyu_cstdint.py [options] [paths...]

Options:
    --files FILE...     Verify specific files only (useful in git hooks or subagents).
    --fix-cstdint       Automatically insert '#include <cstdint>' into files that use fixed-width types but lack it.
    --strict            Fail if bare types (e.g. uint8_t without std::) are detected.
    --tool              Run include-what-you-use binary if available.
    -v, --verbose       Show verbose diagnostic output.
"""

import argparse
import os
import re
import subprocess
import sys
from typing import Dict, List, Set, Tuple

FIXED_WIDTH_TYPES = [
    "int8_t", "uint8_t",
    "int16_t", "uint16_t",
    "int32_t", "uint32_t",
    "int64_t", "uint64_t",
    "intptr_t", "uintptr_t",
    "intmax_t", "uintmax_t",
    "int_least8_t", "uint_least8_t",
    "int_least16_t", "uint_least16_t",
    "int_least32_t", "uint_least32_t",
    "int_least64_t", "uint_least64_t",
    "int_fast8_t", "uint_fast8_t",
    "int_fast16_t", "uint_fast16_t",
    "int_fast32_t", "uint_fast32_t",
    "int_fast64_t", "uint_fast64_t",
]

# Essential standard library symbols mapped to their canonical headers
STD_IWYU_MAP = {
    r"\bstd::unique_ptr\b": "<memory>",
    r"\bstd::make_unique\b": "<memory>",
    r"\bstd::shared_ptr\b": "<memory>",
    r"\bstd::make_shared\b": "<memory>",
    r"\bstd::weak_ptr\b": "<memory>",
    r"\bstd::vector\b": "<vector>",
    r"\bstd::string\b": "<string>",
    r"\bstd::string_view\b": "<string_view>",
    r"\bstd::optional\b": "<optional>",
    r"\bstd::nullopt\b": "<optional>",
    r"\bstd::array\b": "<array>",
    r"\bstd::atomic\b": "<atomic>",
    r"\bstd::mutex\b": "<mutex>",
    r"\bstd::lock_guard\b": "<mutex>",
    r"\bstd::unique_lock\b": "<mutex>",
    r"\bstd::chrono\b": "<chrono>",
    r"\bstd::function\b": "<functional>",
}

IGNORE_DIRS = {
    ".git", ".cache", "build", "build_test", "build_tsan",
    "CMakeFiles", "Testing", "sample-videos"
}

def strip_comments_and_strings(code: str) -> str:
    """Strips block comments, line comments, and string literals from C++ code."""
    # Pattern matches:
    # 1. Raw string literals: R"delim(...)delim"
    # 2. Regular string literals: "..."
    # 3. Character literals: '.'
    # 4. Block comments: /* ... */
    # 5. Line comments: // ...
    pattern = re.compile(
        r'(R"([a-zA-Z0-9_]*)\((?:.|\n)*?\)\2")|'
        r'("(?:\\.|[^"\\])*")|'
        r"('(?:\\.|[^'\\])*')|"
        r'(/\*.*?\*/)|'
        r'(//[^\r\n]*)',
        re.DOTALL
    )
    def replacer(match):
        s = match.group(0)
        if s.startswith('/') or s.startswith('"') or s.startswith("'") or s.startswith('R"'):
            # Replace comment or literal with spaces or newlines to preserve line numbers
            return re.sub(r'[^\r\n]', ' ', s)
        return s
    return pattern.sub(replacer, code)

def parse_includes(raw_content: str) -> Set[str]:
    """Extracts all included header strings, e.g. '<cstdint>', '\"Foo.h\"'."""
    includes = set()
    for match in re.finditer(r'#\s*include\s+([<"][^>"]+[>"])', raw_content):
        includes.add(match.group(1))
    return includes

def check_file(filepath: str, strict: bool = False) -> List[str]:
    """Inspects a single file and returns a list of violation messages."""
    violations = []
    try:
        with open(filepath, "r", encoding="utf-8", errors="replace") as f:
            raw_content = f.read()
    except Exception as e:
        return [f"Could not read {filepath}: {e}"]

    clean_content = strip_comments_and_strings(raw_content)
    includes = parse_includes(raw_content)

    # 1. Fixed-width integer checks
    fixed_type_pattern = r'\b(std::)?(' + '|'.join(FIXED_WIDTH_TYPES) + r')\b'
    matches = list(re.finditer(fixed_type_pattern, clean_content))

    if matches:
        has_cstdint = any("<cstdint>" in inc or "cstdint" in inc for inc in includes)
        if not has_cstdint:
            violations.append(
                f"[MISSING <cstdint>] File uses fixed-width integer types ({matches[0].group(2)}) "
                f"without explicitly including <cstdint>."
            )

        # Check for bare types (unqualified with std::)
        if strict:
            bare_matches = [
                m.group(2) for m in matches
                if not m.group(1)  # Group 1 is 'std::'
            ]
            if bare_matches:
                unique_bare = sorted(list(set(bare_matches)))
                violations.append(
                    f"[UNQUALIFIED TYPE] Bare fixed-width types used without 'std::': {', '.join(unique_bare)}. "
                    f"MISRA C++:2023 requires explicit 'std::' qualification (e.g., std::{unique_bare[0]})."
                )

    # 2. IWYU checks for standard library symbols
    for pattern, required_header in STD_IWYU_MAP.items():
        if re.search(pattern, clean_content):
            if required_header not in includes:
                symbol_name = pattern.replace(r"\b", "").replace(r"\b", "")
                violations.append(
                    f"[IWYU VIOLATION] Symbol '{symbol_name}' used without direct inclusion of {required_header}."
                )

    # 3. Check for size_t / ptrdiff_t
    if re.search(r'\b(std::)?(size_t|ptrdiff_t)\b', clean_content):
        has_cstddef = any("<cstddef>" in inc or "<cstdint>" in inc or "<cstring>" in inc for inc in includes)
        if not has_cstddef:
            violations.append(
                "[MISSING <cstddef>] 'size_t' or 'ptrdiff_t' used without direct inclusion of <cstddef> or <cstdint>."
            )

    return violations

def fix_cstdint_in_file(filepath: str) -> bool:
    """Inserts #include <cstdint> into a file if missing and needed."""
    try:
        with open(filepath, "r", encoding="utf-8") as f:
            content = f.read()
    except Exception:
        return False

    clean_content = strip_comments_and_strings(content)
    includes = parse_includes(content)

    fixed_type_pattern = r'\b(std::)?(' + '|'.join(FIXED_WIDTH_TYPES) + r')\b'
    if not re.search(fixed_type_pattern, clean_content):
        return False

    if any("<cstdint>" in inc for inc in includes):
        return False

    lines = content.splitlines(keepends=True)
    insert_idx = 0

    # If file begins with #pragma once, insert after #pragma once
    for i, line in enumerate(lines):
        if line.strip() == "#pragma once":
            insert_idx = i + 1
            break
        elif line.startswith("#include"):
            insert_idx = i
            break

    # Look for existing C library include block or insert at include start
    for i in range(insert_idx, len(lines)):
        line = lines[i]
        if line.startswith("#include <c") or line.startswith("#include <"):
            insert_idx = i
            break

    lines.insert(insert_idx, "#include <cstdint>\n")

    try:
        with open(filepath, "w", encoding="utf-8") as f:
            f.writelines(lines)
        return True
    except Exception:
        return False

def collect_files(target_paths: List[str]) -> List[str]:
    """Finds all C++ files under target paths, filtering out build directories."""
    collected = []
    extensions = (".h", ".hpp", ".cpp", ".cc", ".cxx")

    for p in target_paths:
        if os.path.isfile(p):
            if p.endswith(extensions):
                collected.append(os.path.normpath(p))
        elif os.path.isdir(p):
            for root, dirs, files in os.walk(p):
                dirs[:] = [d for d in dirs if d not in IGNORE_DIRS]
                for f in files:
                    if f.endswith(extensions):
                        collected.append(os.path.normpath(os.path.join(root, f)))
    return sorted(list(set(collected)))

def run_iwyu_binary(filepath: str) -> None:
    """Invokes include-what-you-use on a single file if binary exists."""
    iwyu_path = "/usr/bin/include-what-you-use"
    if not os.path.exists(iwyu_path):
        print("[WARNING] include-what-you-use not found at /usr/bin/include-what-you-use.")
        return

    cmd = [iwyu_path, "-std=c++17", filepath]
    print(f"[IWYU TOOL] Running: {' '.join(cmd)}")
    subprocess.run(cmd)

def main():
    parser = argparse.ArgumentParser(
        description="Verify <cstdint> inclusion and IWYU compliance for C++ files."
    )
    parser.add_argument("paths", nargs="*", default=["libs", "app-pelcod-qt", "app-visca-qt", "app-fujinon-qt", "app-sightline-qt", "app-onvif-qt", "app-video-qt", "app-pelcod-tui"], help="Directories or files to check.")
    parser.add_argument("--files", nargs="+", help="Specific files to verify.")
    parser.add_argument("--strict", action="store_true", help="Enforce std:: prefix on all fixed-width integer types.")
    parser.add_argument("--fix-cstdint", action="store_true", help="Auto-insert #include <cstdint> where missing.")
    parser.add_argument("--tool", action="store_true", help="Run include-what-you-use binary on target files.")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output.")

    args = parser.parse_args()

    target_files = []
    if args.files:
        target_files = collect_files(args.files)
    else:
        target_files = collect_files(args.paths)

    if not target_files:
        print("[INFO] No C++ files matched for verification.")
        sys.exit(0)

    if args.fix_cstdint:
        fixed_count = 0
        for f in target_files:
            if fix_cstdint_in_file(f):
                fixed_count += 1
                if args.verbose:
                    print(f"[FIXED] Inserted <cstdint> in: {f}")
        print(f"[AUTO-FIX COMPLETE] Successfully updated {fixed_count} file(s).")
        sys.exit(0)

    if args.tool:
        for f in target_files:
            run_iwyu_binary(f)
        sys.exit(0)

    total_violations = 0
    violating_files = 0

    for filepath in target_files:
        violations = check_file(filepath, strict=args.strict)
        if violations:
            violating_files += 1
            total_violations += len(violations)
            print(f"\n[VIOLATIONS IN] {filepath}")
            for v in violations:
                print(f"  - {v}")
        elif args.verbose:
            print(f"[PASS] {filepath}")

    print("\n" + "=" * 60)
    print(f"Verification Summary: Checked {len(target_files)} file(s).")
    if total_violations > 0:
        print(f"FAILED: Found {total_violations} violation(s) across {violating_files} file(s).")
        sys.exit(1)
    else:
        print("SUCCESS: 100% compliance with <cstdint> and IWYU rules.")
        sys.exit(0)

if __name__ == "__main__":
    main()
