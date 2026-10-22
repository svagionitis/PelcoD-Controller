#!/usr/bin/env python3
"""
parse_audit_log.py - Clang System-Headers Diagnostic Parser & Filter

Parses Clang compiler output produced with -Wsign-conversion -Wsystem-headers
and filters diagnostics to isolate:
1. Direct warnings occurring in project code (libs/ and app-*/).
2. Warnings occurring inside system or external headers whose template
   instantiation trace originates from project code (libs/ and app-*/).

Part of Work Package 5: Clang System-Headers Audit Tooling.
"""

import argparse
import json
import os
import re
import sys
from dataclasses import asdict, dataclass, field
from typing import Dict, List, Optional, Set, Tuple

# ANSI Color Codes
CYAN = "\033[0;36m"
GREEN = "\033[0;32m"
YELLOW = "\033[1;33m"
RED = "\033[0;31m"
MAGENTA = "\033[0;35m"
BOLD = "\033[1m"
GRAY = "\033[0;90m"
NC = "\033[0m"


def disable_colors_if_needed():
    global CYAN, GREEN, YELLOW, RED, MAGENTA, BOLD, GRAY, NC
    if not sys.stdout.isatty() or os.environ.get("NO_COLOR"):
        CYAN = ""
        GREEN = ""
        YELLOW = ""
        RED = ""
        MAGENTA = ""
        BOLD = ""
        GRAY = ""
        NC = ""


@dataclass
class DiagnosticNote:
    file: str
    line: int
    column: int
    message: str
    is_project: bool = False
    is_instantiation: bool = False


@dataclass
class DiagnosticRecord:
    file: str
    line: int
    column: int
    level: str  # 'warning', 'error', 'fatal error'
    message: str
    flag: Optional[str] = None
    notes: List[DiagnosticNote] = field(default_factory=list)
    raw_lines: List[str] = field(default_factory=list)

    @property
    def is_project_direct(self) -> bool:
        return is_project_path(self.file)

    @property
    def project_instantiations(self) -> List[DiagnosticNote]:
        return [n for n in self.notes if n.is_project and n.is_instantiation]

    @property
    def is_project_associated(self) -> bool:
        return self.is_project_direct or len(self.project_instantiations) > 0


def is_project_path(filepath: str) -> bool:
    """Returns True if the given path belongs to project code (libs/ or app-*/)."""
    norm = filepath.replace("\\", "/")
    # Filter out build dirs or external dependencies that might contain 'libs' in their path
    parts = norm.split("/")
    for i, p in enumerate(parts):
        if p == "libs" or (p.startswith("app-") and not p.endswith(".log")):
            # Ensure it's not inside a build directory
            prefix = parts[:i]
            if any(b.startswith("build") or b == "vcpkg_installed" or b == ".git" for b in prefix):
                continue
            return True
    return False


def normalize_to_project(filepath: str, workspace_root: str) -> str:
    """Converts an absolute path to a workspace-relative path if inside workspace."""
    norm_file = os.path.abspath(filepath)
    norm_root = os.path.abspath(workspace_root)
    if norm_file.startswith(norm_root):
        rel = os.path.relpath(norm_file, norm_root)
        return rel.replace("\\", "/")
    return filepath.replace("\\", "/")


# Regex for Clang diagnostic header: file:line:col: (warning|error|fatal error): message [-Wflag]
DIAG_HEADER_RE = re.compile(
    r"^([A-Za-z0-9_/.\-\\:]+?):(\d+):(\d+):\s+(warning|error|fatal error):\s+(.*?)(?:\s+\[(-W[^\]]+)\])?$"
)

# Regex for Clang note: file:line:col: note: message
NOTE_HEADER_RE = re.compile(
    r"^([A-Za-z0-9_/.\-\\:]+?):(\d+):(\d+):\s+note:\s+(.*?)$"
)


def parse_compiler_log(log_path: str, workspace_root: str) -> List[DiagnosticRecord]:
    """Parses a compiler output log into a list of DiagnosticRecord objects."""
    records: List[DiagnosticRecord] = []
    current_record: Optional[DiagnosticRecord] = None

    if not os.path.isfile(log_path):
        return records

    with open(log_path, "r", encoding="utf-8", errors="replace") as f:
        for raw_line in f:
            line = raw_line.rstrip("\r\n")

            # Check if this line is a new diagnostic header
            diag_match = DIAG_HEADER_RE.match(line)
            if diag_match:
                fpath, s_line, s_col, level, msg, flag = diag_match.groups()
                norm_fpath = normalize_to_project(fpath, workspace_root)
                current_record = DiagnosticRecord(
                    file=norm_fpath,
                    line=int(s_line),
                    column=int(s_col),
                    level=level,
                    message=msg.strip(),
                    flag=flag.strip() if flag else None,
                    raw_lines=[line],
                )
                records.append(current_record)
                continue

            # Check if this line is a note attached to the current diagnostic
            note_match = NOTE_HEADER_RE.match(line)
            if note_match and current_record is not None:
                n_fpath, ns_line, ns_col, n_msg = note_match.groups()
                norm_n_fpath = normalize_to_project(n_fpath, workspace_root)
                is_proj = is_project_path(norm_n_fpath)
                is_inst = "instantiation of" in n_msg or "required from" in n_msg or "requested here" in n_msg
                note_obj = DiagnosticNote(
                    file=norm_n_fpath,
                    line=int(ns_line),
                    column=int(ns_col),
                    message=n_msg.strip(),
                    is_project=is_proj,
                    is_instantiation=is_inst,
                )
                current_record.notes.append(note_obj)
                current_record.raw_lines.append(line)
                continue

            # Continuation line (code snippet, caret, etc.)
            if current_record is not None:
                current_record.raw_lines.append(line)

    return records


@dataclass
class AuditFinding:
    project_file: str
    project_line: int
    project_col: int
    warning_file: str
    warning_line: int
    warning_col: int
    flag: str
    message: str
    instantiation_context: Optional[str]
    is_direct: bool


def extract_audit_findings(
    records: List[DiagnosticRecord],
    sign_conversion_only: bool = False,
    all_warnings: bool = False,
    filter_pattern: Optional[str] = None,
) -> List[AuditFinding]:
    """Extracts and deduplicates project-associated audit findings from records."""
    findings: List[AuditFinding] = []
    seen: Set[Tuple[str, int, str, str]] = set()

    filter_re = re.compile(filter_pattern, re.IGNORECASE) if filter_pattern else None

    for r in records:
        if sign_conversion_only and r.flag != "-Wsign-conversion":
            continue

        if not r.is_project_associated:
            continue

        if r.is_project_direct:
            if filter_re and not filter_re.search(r.file):
                continue
            key = (r.file, r.line, r.flag or "", r.message)
            if key not in seen:
                seen.add(key)
                findings.append(
                    AuditFinding(
                        project_file=r.file,
                        project_line=r.line,
                        project_col=r.column,
                        warning_file=r.file,
                        warning_line=r.line,
                        warning_col=r.column,
                        flag=r.flag or "warning",
                        message=r.message,
                        instantiation_context=None,
                        is_direct=True,
                    )
                )
        else:
            # System/external header warning instantiated from project code
            # Focus on -Wsign-conversion unless --all-warnings was requested
            if not all_warnings and r.flag != "-Wsign-conversion":
                continue

            for inst in r.project_instantiations:
                if filter_re and not filter_re.search(inst.file):
                    continue
                key = (inst.file, inst.line, r.flag or "", r.message)
                if key not in seen:
                    seen.add(key)
                    findings.append(
                        AuditFinding(
                            project_file=inst.file,
                            project_line=inst.line,
                            project_col=inst.column,
                            warning_file=r.file,
                            warning_line=r.line,
                            warning_col=r.column,
                            flag=r.flag or "warning",
                            message=r.message,
                            instantiation_context=inst.message,
                            is_direct=False,
                        )
                    )

    # Sort findings by project file and line
    findings.sort(key=lambda x: (x.project_file, x.project_line))
    return findings


def print_audit_report(
    records: List[DiagnosticRecord],
    findings: List[AuditFinding],
    target: Optional[str],
    verbose: bool = False,
):
    """Prints a formatted terminal audit report."""
    total_diags = len(records)
    total_sign = sum(1 for r in records if r.flag == "-Wsign-conversion")
    direct_proj = sum(1 for f in findings if f.is_direct)
    template_proj = sum(1 for f in findings if not f.is_direct)

    target_desc = f"Target: {target}" if target else "All Targets"

    print(f"\n{BOLD}{CYAN}================================================================================{NC}")
    print(f"{BOLD}{CYAN}                  CLANG SYSTEM-HEADERS AUDIT REPORT                             {NC}")
    print(f"{BOLD}{CYAN}================================================================================{NC}")
    print(f"{GRAY}Audited Target     : {BOLD}{target_desc}{NC}")
    print(f"{GRAY}Total Diagnostics  : {total_diags} (in system & external headers)")
    print(f"{GRAY}Sign-Conversions   : {total_sign} total detected across all templates")
    print(f"{GRAY}Project Direct     : {direct_proj}")
    print(f"{GRAY}Project Templates  : {template_proj} instantiated from project code")
    print(f"{CYAN}--------------------------------------------------------------------------------{NC}")

    if not findings:
        print(f"\n{GREEN}{BOLD}[PASS] 0 project-associated sign-conversion issues found.{NC}")
        print(f"{GREEN}All project code and template instantiations are clean under Clang audit.{NC}\n")
        return

    print(f"\n{YELLOW}{BOLD}Project-Associated Instantiations & Diagnostics ({len(findings)} unique sites):{NC}\n")

    current_file = ""
    for f in findings:
        if f.project_file != current_file:
            current_file = f.project_file
            print(f"\n{BOLD}{MAGENTA}[FILE] {current_file}{NC}")

        kind_badge = f"{YELLOW}[DIRECT]{NC}" if f.is_direct else f"{CYAN}[TEMPLATE]{NC}"
        flag_badge = f"{RED}[{f.flag}]{NC}" if f.flag else ""
        print(f"  {BOLD}Line {f.project_line}:{f.project_col}{NC} {kind_badge} {flag_badge} {f.message}")

        if not f.is_direct:
            print(f"    {GRAY}--> System Header : {f.warning_file}:{f.warning_line}{NC}")
            if f.instantiation_context:
                print(f"    {GRAY}--> Instantiation : {f.instantiation_context}{NC}")

    print(f"\n{CYAN}--------------------------------------------------------------------------------{NC}")
    print(f"{YELLOW}{BOLD}Audit Summary:{NC} {len(findings)} project code site(s) trigger template conversion warnings.")
    print(f"{GRAY}Remediation: Review each callsite and use explicit static_cast or AsciiUtils helpers.{NC}\n")


def export_json_report(findings: List[AuditFinding], json_path: str, target: Optional[str]):
    """Exports findings to a JSON file."""
    data = {
        "target": target or "all",
        "total_findings": len(findings),
        "findings": [asdict(f) for f in findings],
    }
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
    print(f"{GREEN}[OK] Exported JSON audit report to: {json_path}{NC}")


def main() -> int:
    disable_colors_if_needed()

    parser = argparse.ArgumentParser(
        description="Clang System-Headers Audit Diagnostic Parser & Filter"
    )
    parser.add_argument("logfile", help="Path to raw compiler output log file")
    parser.add_argument(
        "--workspace-root",
        default=".",
        help="Path to workspace root directory (default: current directory)",
    )
    parser.add_argument(
        "--target",
        default=None,
        help="Name of target audited (for reporting purposes)",
    )
    parser.add_argument(
        "--filter",
        default=None,
        help="Optional regex pattern to filter project findings by path",
    )
    parser.add_argument(
        "--sign-conversion-only",
        action="store_true",
        default=False,
        help="Only report -Wsign-conversion diagnostics (ignores other warnings)",
    )
    parser.add_argument(
        "--all-warnings",
        action="store_true",
        default=False,
        help="Include non-sign-conversion warnings from system headers (default: sign-conversions only)",
    )
    parser.add_argument(
        "--fail-on-warning",
        action="store_true",
        default=False,
        help="Exit with code 1 if any project-associated findings are detected",
    )
    parser.add_argument(
        "--json",
        default=None,
        help="Path to export structured findings as JSON",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        default=False,
        help="Show verbose diagnostic output",
    )

    args = parser.parse_args()

    workspace_root = os.path.abspath(args.workspace_root)
    log_path = os.path.abspath(args.logfile)

    if not os.path.isfile(log_path):
        print(f"{RED}[ERROR] Log file not found: {log_path}{NC}", file=sys.stderr)
        return 1

    records = parse_compiler_log(log_path, workspace_root)
    findings = extract_audit_findings(
        records,
        sign_conversion_only=args.sign_conversion_only,
        all_warnings=args.all_warnings,
        filter_pattern=args.filter,
    )

    print_audit_report(records, findings, target=args.target, verbose=args.verbose)

    if args.json:
        export_json_report(findings, args.json, target=args.target)

    if args.fail_on_warning and findings:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
