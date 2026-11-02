#!/usr/bin/env python3
"""
verify_relative_doc_paths.py - Automated verification for relative paths in documentation and comments.

Enforces Project Rule 13: All paths of filenames in documents and comments must be relative.

Checks performed:
1. Rejects absolute filesystem paths (e.g., /home/..., /Users/..., C:\\Users\\...).
2. Rejects leading root slashes in repository links (e.g., [Text](/libs/...) -> [Text](../libs/...)).
3. Rejects Windows backslashes in Markdown link targets (e.g., ..\\libs\\... -> ../libs/...).
4. Validates that relative link targets exist on disk (when pointing to local files).
5. Verifies Doxygen '/// @file' declarations use file basenames or relative paths.

Usage:
    python3 scripts/verify_relative_doc_paths.py [options] [paths...]

Options:
    --files FILE...     Verify specific files only.
    --strict            Fail on dead link targets and path formatting issues.
    -v, --verbose       Show verbose diagnostic output.
"""

import argparse
import os
import re
import sys
from typing import List, Tuple

# Workstation absolute path patterns that must never appear in documentation
WORKSTATION_PATH_PATTERNS = [
    re.compile(r'(?i)(?:^|[\s"\'`(\[])(/home/[A-Za-z0-9_.-]+)'),
    re.compile(r'(?i)(?:^|[\s"\'`(\[])(/Users/[A-Za-z0-9_.-]+)'),
    re.compile(r'(?i)(?:^|[\s"\'`(\[])([A-Za-z]:[/\\]Users[/\\][A-Za-z0-9_.-]+)'),
]

# Standard CI/container environment paths that may be referenced in CI configs
CI_ENV_EXCEPTIONS = ("runneradmin", "/root/.cache")

# Markdown link regex: [label](url)
MARKDOWN_LINK_PATTERN = re.compile(r'\[([^\]]+)\]\(([^)\s]+)(?:\s+"[^"]*")?\)')

# Doxygen @file pattern: /// @file [optional filename]
DOXYGEN_FILE_PATTERN = re.compile(r'^\s*///\s*@file(?:\s+([^\s*]+))?')

# Ignored link schemes (external protocols or special schemes)
IGNORED_SCHEMES = (
    "http://", "https://", "ftp://", "ftps://", "mailto:",
    "tel:", "file://", "onvif://", "rtsp://", "sim://", "pelcod://",
    "memory://", "test://", "mock://", "data:"
)

class PathViolation:
    def __init__(self, filepath: str, line_num: int, violation_type: str, detail: str, is_warning: bool = False):
        self.filepath = filepath
        self.line_num = line_num
        self.violation_type = violation_type
        self.detail = detail
        self.is_warning = is_warning

    def __str__(self) -> str:
        level = "WARN" if self.is_warning else "FAIL"
        return f"[{level}] {self.filepath}:{self.line_num} ({self.violation_type}): {self.detail}"


def check_markdown_file(filepath: str, strict: bool, repo_root: str) -> List[PathViolation]:
    violations: List[PathViolation] = []
    file_dir = os.path.dirname(os.path.abspath(filepath))

    try:
        with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()
    except OSError as err:
        violations.append(PathViolation(filepath, 0, "IO_ERROR", str(err)))
        return violations

    in_fenced_code_block = False

    for idx, raw_line in enumerate(lines, 1):
        line = raw_line.rstrip()

        # Track markdown code fence
        if line.strip().startswith("```") or line.strip().startswith("~~~"):
            in_fenced_code_block = not in_fenced_code_block
            continue

        # Check for workstation absolute paths outside fenced code blocks
        if not in_fenced_code_block:
            for pattern in WORKSTATION_PATH_PATTERNS:
                match = pattern.search(line)
                if match:
                    captured = match.group(1)
                    is_ci_exception = any(exc in captured for exc in CI_ENV_EXCEPTIONS)
                    violations.append(PathViolation(
                        filepath, idx, "ABSOLUTE_WORKSTATION_PATH",
                        f"Found absolute user path: '{captured}'. Use relative or generic path.",
                        is_warning=is_ci_exception
                    ))

        # Do not extract markdown links from fenced code blocks
        if in_fenced_code_block:
            continue

        # Strip inline code spans (`...`) so code examples containing markdown syntax are not treated as active links
        line_without_inline_code = re.sub(r'`[^`]*`', '', line)

        # Markdown link validation
        for match in MARKDOWN_LINK_PATTERN.finditer(line_without_inline_code):
            label = match.group(1)
            target = match.group(2).strip()

            # Ignore URL schemes and pure in-page anchors
            if target.startswith(IGNORED_SCHEMES) or target.startswith("#"):
                continue

            # Strip target anchors if present (e.g., file.md#section -> file.md)
            target_path = target.split("#")[0].strip()
            # Also strip line number annotations (e.g., file.cpp:12-34 -> file.cpp)
            clean_target_path = re.sub(r':\d+(?:-\d+)?$', '', target_path)
            if not clean_target_path:
                continue

            # Check 1: Leading slash (absolute repository root link)
            if target_path.startswith("/") or target_path.startswith("\\"):
                violations.append(PathViolation(
                    filepath, idx, "ABSOLUTE_ROOT_LINK",
                    f"Markdown link '{target}' has leading root slash. Use relative path (e.g. '../{target_path.lstrip('/')}')."
                ))
                continue

            # Check 2: Windows backslash in link target
            if "\\" in target_path:
                violations.append(PathViolation(
                    filepath, idx, "BACKSLASH_PATH_SEPARATOR",
                    f"Markdown link '{target}' contains backslashes. Use forward slashes '/' for cross-platform portability."
                ))
                clean_target_path = clean_target_path.replace("\\", "/")

            # Check 3: Drive letters (C:/...)
            if re.match(r'^[A-Za-z]:', target_path):
                violations.append(PathViolation(
                    filepath, idx, "WINDOWS_DRIVE_PATH",
                    f"Markdown link '{target}' uses absolute drive letter. Use relative path."
                ))
                continue

            # Check 4: Target existence verification
            resolved_target = os.path.normpath(os.path.join(file_dir, clean_target_path))
            if not os.path.exists(resolved_target):
                # Check if target is relative to repo_root by mistake
                repo_relative_candidate = os.path.normpath(os.path.join(repo_root, clean_target_path))
                if os.path.exists(repo_relative_candidate) and repo_relative_candidate != resolved_target:
                    rel_from_doc = os.path.relpath(repo_relative_candidate, file_dir)
                    violations.append(PathViolation(
                        filepath, idx, "NON_RELATIVE_TO_DOC_LINK",
                        f"Target exists at repo root, but link is not relative to current doc. Change '{target}' to '{rel_from_doc}'.",
                        is_warning=not strict
                    ))
                else:
                    violations.append(PathViolation(
                        filepath, idx, "BROKEN_RELATIVE_LINK",
                        f"Linked target does not exist: '{target}' (resolved to '{resolved_target}').",
                        is_warning=not strict
                    ))

    return violations


def check_source_header(filepath: str) -> List[PathViolation]:
    violations: List[PathViolation] = []
    basename = os.path.basename(filepath)

    try:
        with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
            for idx, line in enumerate(f, 1):
                match = DOXYGEN_FILE_PATTERN.match(line)
                if match:
                    declared_name = match.group(1)
                    if declared_name:
                        # Check for absolute path
                        if os.path.isabs(declared_name) or re.match(r'^[A-Za-z]:', declared_name):
                            violations.append(PathViolation(
                                filepath, idx, "DOXYGEN_ABSOLUTE_FILE",
                                f"@file declaration uses absolute path '{declared_name}'. Use basename '{basename}'."
                            ))
                        elif "/" in declared_name or "\\" in declared_name:
                            # It's a path rather than basename
                            violations.append(PathViolation(
                                filepath, idx, "DOXYGEN_NON_BASENAME_FILE",
                                f"@file declaration uses path '{declared_name}'. Prefer basename '{basename}'.",
                                is_warning=True
                            ))
                # Stop checking header banner after first 30 lines
                if idx > 30:
                    break
    except OSError as err:
        violations.append(PathViolation(filepath, 0, "IO_ERROR", str(err)))

    return violations


def find_files_to_scan(targets: List[str], repo_root: str) -> Tuple[List[str], List[str]]:
    md_files: List[str] = []
    src_headers: List[str] = []

    for target in targets:
        if os.path.isfile(target):
            if target.endswith((".md", ".markdown")):
                md_files.append(target)
            elif target.endswith((".h", ".hpp")):
                src_headers.append(target)
        elif os.path.isdir(target):
            for root, dirs, files in os.walk(target):
                # Skip build, vcpkg, git, and third-party directories
                if any(x in root for x in ["build", ".git", "vcpkg", "node_modules", "third_party"]):
                    continue
                for f in files:
                    full_p = os.path.join(root, f)
                    if f.endswith((".md", ".markdown")):
                        md_files.append(full_p)
                    elif f.endswith((".h", ".hpp")):
                        src_headers.append(full_p)

    return sorted(list(set(md_files))), sorted(list(set(src_headers)))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Verify relative paths in documentation, markdown files, and source code headers."
    )
    parser.add_argument(
        "paths", nargs="*", default=["docs", ".agents", "README.md"],
        help="Paths, directories, or files to inspect (default: docs, .agents, README.md)"
    )
    parser.add_argument(
        "--files", nargs="+", default=[],
        help="Specific files to inspect exclusively."
    )
    parser.add_argument(
        "--strict", action="store_true",
        help="Treat warnings (e.g. broken target links, non-basename @file) as fatal errors."
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true",
        help="Print verbose scanning progress."
    )

    args = parser.parse_args()

    # Determine repository root (assumed to be directory where scripts/ is located or current cwd)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.abspath(os.path.join(script_dir, ".."))

    targets = args.files if args.files else args.paths
    md_files, src_headers = find_files_to_scan(targets, repo_root)

    if args.verbose:
        print(f"Repository root: {repo_root}")
        print(f"Scanning {len(md_files)} Markdown files and {len(src_headers)} source headers...")

    all_violations: List[PathViolation] = []

    for md_path in md_files:
        violations = check_markdown_file(md_path, args.strict, repo_root)
        all_violations.extend(violations)

    for h_path in src_headers:
        violations = check_source_header(h_path)
        all_violations.extend(violations)

    errors = [v for v in all_violations if not v.is_warning]
    warnings = [v for v in all_violations if v.is_warning]

    # Report results
    for v in all_violations:
        print(v)

    print("\n" + "=" * 70)
    print(f"Documentation Path Audit Completed:")
    print(f"  Scanned Markdown files : {len(md_files)}")
    print(f"  Scanned Source headers : {len(src_headers)}")
    print(f"  Errors                 : {len(errors)}")
    print(f"  Warnings               : {len(warnings)}")
    print("=" * 70)

    if errors or (args.strict and warnings):
        print("\nFAILURE: Path violations detected. Enforce relative paths in all documents.")
        return 1

    print("\nSUCCESS: All document links and file paths are relative and valid.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
