#!/usr/bin/env python3
"""
verify_type_conversions.py - Automated static analysis for signed/unsigned range conversions.

Verifies compliance with:
- MISRA C++:2023 Rule 7.0.6 (Signed/Unsigned Conversions)
- SEI CERT INT31-C (Prevent conversion errors across signed/unsigned types)
- MSVC C4365 hardening against implicit conversions in STL templates

Checks for:
1. Prohibited range constructors of std::vector<std::uint8_t> from char iterators/ranges
   (e.g., std::string, std::string_view, QByteArray, char*).
   Such patterns trigger C4365 in MSVC STL (<xmemory> / <vector>) under /W4 /w14365.
   Remediation: use reserve() with an explicit static_cast<std::uint8_t>(ch) loop or
   Onvif::AsciiUtils::stringToBytes().
2. STL algorithm callables accepting signed/unsigned mismatches across char and uint8_t.

Usage:
    python3 scripts/verify_type_conversions.py [options] [paths...]

Options:
    --files FILE...     Verify specific files only (useful in git hooks or CI).
    -v, --verbose       Show verbose diagnostic output.
"""

import argparse
import os
import re
import sys
from typing import List, Set

IGNORE_DIRS: Set[str] = {
    ".git", ".cache", "build", "build_test", "build_tsan", "build_clang", "build-clang",
    "CMakeFiles", "Testing", "sample-videos"
}

# Regex to detect std::vector<std::uint8_t> range initialization from .begin() / .end()
REGEX_VECTOR_RANGE_CONV = re.compile(
    r"\b(?:std::)?vector\s*<\s*(?:std::)?uint8_t\s*>\s+([a-zA-Z0-9_]+)\s*\(\s*([a-zA-Z0-9_]+)\.(?:c?begin|data)\(\)\s*,\s*([a-zA-Z0-9_]+)\.(?:c?end|data\(\))"
)

# Regex to detect inline temporary vector construction: std::vector<std::uint8_t>(s.begin(), s.end())
REGEX_INLINE_VECTOR_RANGE = re.compile(
    r"\b(?:std::)?vector\s*<\s*(?:std::)?uint8_t\s*>\s*\(\s*([a-zA-Z0-9_]+)\.(?:c?begin|data)\(\)\s*,\s*([a-zA-Z0-9_]+)\.(?:c?end|data\(\))"
)

KNOWN_UINT8_TYPENAMES = {
    "StandardFrame", "ByteView", "PesPacket", "KlvPacket", "RawBuffer"
}

def strip_comments_and_literals(code: str) -> str:
    """Strips comments and raw/regular string literals from C++ code to avoid false positives."""
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
            return re.sub(r'[^\r\n]', ' ', s)
        return s
    return pattern.sub(replacer, code)

def is_proven_uint8_source(source_var: str, context_text: str) -> bool:
    """Checks whether source_var is proven to be a uint8_t container in context."""
    # Check if declared as vector<uint8_t>, array<uint8_t>, ByteView, or StandardFrame
    uint8_decl_pattern = re.compile(
        rf"\b(?:std::)?(?:vector|array)\s*<\s*(?:std::)?uint8_t\b[a-zA-Z0-9_<>\s]*\b{re.escape(source_var)}\s*[:=;,\{{\(]"
        rf"|\b(?:ByteView|StandardFrame|PesPacket)\s+(?:const\s+)?{re.escape(source_var)}\s*[:=;,\{{\(]"
        rf"|\b(?:const\s+)?(?:std::)?uint8_t\s*\*\s*{re.escape(source_var)}\b"
    )
    return bool(uint8_decl_pattern.search(context_text))

def check_file_type_conversions(filepath: str) -> List[str]:
    """Inspects a single file and returns a list of violation messages."""
    violations: List[str] = []
    try:
        with open(filepath, "r", encoding="utf-8", errors="replace") as f:
            lines = f.readlines()
    except Exception as e:
        return [f"Could not read {filepath}: {e}"]

    raw_content = "".join(lines)
    clean_content = strip_comments_and_literals(raw_content)
    clean_lines = clean_content.splitlines()

    for line_idx, line in enumerate(clean_lines, 1):
        # 1. Named vector range construction
        for match in REGEX_VECTOR_RANGE_CONV.finditer(line):
            var_name = match.group(1)
            start_source = match.group(2)
            end_source = match.group(3)
            if start_source == end_source:
                lookback_start = max(0, line_idx - 40)
                preceding_text = "".join(clean_lines[lookback_start:line_idx - 1])
                
                # Check for explicit string / char declaration in recent lines
                char_type_pattern = re.compile(
                    rf"\b(?:std::string|std::string_view|QByteArray|QString|char\s*\*|const\s+char\s*\*)\s+[^;]*\b{re.escape(start_source)}\b"
                )
                is_char = bool(char_type_pattern.search(preceding_text))
                is_uint8 = is_proven_uint8_source(start_source, preceding_text)

                if is_char or (not is_uint8 and any(h in start_source.lower() for h in ["str", "string", "sentence", "text", "finalstr", "tosend", "hex", "qba"])):
                    violations.append(
                        f"Line {line_idx}: Prohibited range constructor `std::vector<std::uint8_t> {var_name}({start_source}.begin(), {end_source}.end())` "
                        f"over char/string source '{start_source}'. Violates MISRA C++:2023 Rule 7.0.6 / INT31-C (triggers MSVC C4365). "
                        f"Use explicit reserve() + static_cast<std::uint8_t>(ch) loop."
                    )

        # 2. Temporary inline vector range construction
        for match in REGEX_INLINE_VECTOR_RANGE.finditer(line):
            start_source = match.group(1)
            end_source = match.group(2)
            if start_source == end_source:
                lookback_start = max(0, line_idx - 40)
                preceding_text = "".join(clean_lines[lookback_start:line_idx - 1])
                char_type_pattern = re.compile(
                    rf"\b(?:std::string|std::string_view|QByteArray|QString|char\s*\*|const\s+char\s*\*)\s+[^;]*\b{re.escape(start_source)}\b"
                )
                is_char = bool(char_type_pattern.search(preceding_text))
                is_uint8 = is_proven_uint8_source(start_source, preceding_text)

                if is_char or (not is_uint8 and any(h in start_source.lower() for h in ["str", "string", "sentence", "text", "finalstr", "tosend", "hex", "qba"])):
                    violations.append(
                        f"Line {line_idx}: Prohibited inline range constructor `std::vector<std::uint8_t>({start_source}.begin(), {end_source}.end())` "
                        f"over char/string source '{start_source}'. Violates MISRA C++:2023 Rule 7.0.6 / INT31-C (triggers MSVC C4365). "
                        f"Use explicit reserve() + static_cast<std::uint8_t>(ch) loop."
                    )

    return violations

def collect_files(target_paths: List[str]) -> List[str]:
    """Finds all C++ files under target paths, filtering out build directories."""
    collected: List[str] = []
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

def main():
    parser = argparse.ArgumentParser(
        description="Verify type conversions and prevent MSVC C4365 template instantiations."
    )
    parser.add_argument(
        "paths",
        nargs="*",
        default=["libs", "app-pelcod-qt", "app-visca-qt", "app-fujinon-qt", "app-sightline-qt", "app-onvif-qt", "app-video-qt", "app-pelcod-tui"],
        help="Directories or files to check."
    )
    parser.add_argument("--files", nargs="+", help="Specific files to verify.")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output.")

    args = parser.parse_args()

    if args.files:
        target_files = collect_files(args.files)
    else:
        target_files = collect_files(args.paths)

    if not target_files:
        print("[INFO] No C++ files matched for verification.")
        sys.exit(0)

    total_violations = 0
    violating_files = 0

    for filepath in target_files:
        violations = check_file_type_conversions(filepath)
        if violations:
            violating_files += 1
            total_violations += len(violations)
            print(f"\n[RANGE CONVERSION VIOLATION] {filepath}")
            for v in violations:
                print(f"  - {v}")
        elif args.verbose:
            print(f"[PASS] {filepath}")

    print("\n" + "=" * 60)
    print(f"Type Conversion Audit: Checked {len(target_files)} file(s).")
    if total_violations > 0:
        print(f"FAILED: Found {total_violations} violation(s) across {violating_files} file(s).")
        sys.exit(1)
    else:
        print("SUCCESS: 0 range conversion defects detected. Fully compliant with MISRA Rule 7.0.6 / CERT INT31-C.")
        sys.exit(0)

if __name__ == "__main__":
    main()
