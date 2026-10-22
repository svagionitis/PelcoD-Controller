#!/usr/bin/env bash
# ==============================================================================
# Pelco-D Controller - Clang System-Headers Audit Tool
#
# Runs non-blocking Clang audits with -Wsign-conversion and -Wsystem-headers
# to detect signed/unsigned conversions inside external and standard templates
# (e.g. <vector>, <string>, <algorithm>) instantiated from project code.
#
# Part of Work Package 5: Clang System-Headers Audit Tooling.
# ==============================================================================

# Re-execute with bash if launched via basic /bin/sh (e.g. dash)
if [ -z "${BASH_VERSION:-}" ]; then
    if command -v bash >/dev/null 2>&1; then
        exec bash "$0" "$@"
    else
        echo "[ERROR] bash is required to execute this script." >&2
        exit 1
    fi
fi

set -euo pipefail

# ANSI color codes (enabled if stdout is a terminal)
if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    CYAN='\033[0;36m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    RED='\033[0;31m'
    BOLD='\033[1m'
    GRAY='\033[0;90m'
    NC='\033[0m'
else
    CYAN=''
    GREEN=''
    YELLOW=''
    RED=''
    BOLD=''
    GRAY=''
    NC=''
fi

log_header() {
    echo -e "${CYAN}${BOLD}$1${NC}"
}

log_info() {
    echo -e "${GRAY}$1${NC}"
}

log_ok() {
    echo -e "${GREEN}$1${NC}"
}

log_warn() {
    echo -e "${YELLOW}$1${NC}"
}

log_error() {
    echo -e "${RED}[ERROR] $1${NC}" >&2
}

show_help() {
    cat << 'EOF'
Pelco-D Controller - Clang System-Headers Audit Tool

Runs an opt-in non-blocking Clang audit build with -Wsign-conversion and
-Wsystem-headers, capturing signed/unsigned conversions inside standard
and external templates instantiated by project code (libs/ and app-*/).

Usage:
    ./scripts/audit_system_headers.sh [OPTIONS]

Options:
    -t, --target <Target>
        Specific CMake target to audit (e.g. "Onvif", "PelcoDCore", "Nmea").
        Builds all targets if omitted.
        (Also accepts -Target <Target>)

    -b, --build-dir <Dir>
        Build directory for audit build tree relative to workspace root
        (default: "build-audit-clang").
        (Also accepts -BuildDir <Dir>)

    --clean
        Removes the audit build directory before configuring.
        (Also accepts -Clean)

    --reconfigure
        Force re-running CMake configuration even if CMakeCache.txt exists.
        (Also accepts -Reconfigure)

    -j, --parallel-jobs <N>
        Number of parallel compilation processes. Defaults to logical CPU count.
        (Also accepts -ParallelJobs <N>)

    --compiler <Compiler>
        Clang C++ executable to use: "clang++" (default) or custom path.
        (Also accepts -Compiler <Compiler>)

    --sign-conversion-only
        Filter report strictly to -Wsign-conversion diagnostics.

    --fail-on-warning
        Exit with code 1 if any project-associated conversion warnings are found.
        (Default: exit 0 to allow non-blocking developer audits).

    --filter <Pattern>
        Regex pattern to filter project findings (e.g. "Onvif" or "NmeaDevice").

    --json <Path>
        Export structured audit findings to a JSON file.

    -v, --verbose
        Show detailed raw compiler output.

    -h, --help
        Display this help message and exit.

Examples:
    ./scripts/audit_system_headers.sh --target Onvif
        Audits the Onvif subsystem for signed/unsigned template conversions.

    ./scripts/audit_system_headers.sh --clean --target PelcoDCore
        Performs a clean audit of PelcoDCore.

    ./scripts/audit_system_headers.sh --fail-on-warning --target Onvif
        Audits Onvif and fails with exit code 1 if warnings are found (CI mode).

    ./scripts/audit_system_headers.sh --json audit_results.json
        Audits all targets and exports findings to JSON.
EOF
}

# ------------------------------------------------------------------------------
# Workspace Resolution
# ------------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# ------------------------------------------------------------------------------
# Default Options
# ------------------------------------------------------------------------------
TARGET=""
BUILD_DIR="build-audit-clang"
CLEAN=false
REBUILD=false
RECONFIGURE=false
PARALLEL_JOBS=0
COMPILER="clang++"
SIGN_CONVERSION_ONLY=false
ALL_WARNINGS=false
FAIL_ON_WARNING=false
FILTER_PATTERN=""
JSON_OUTPUT=""
VERBOSE=false

# ------------------------------------------------------------------------------
# Argument Parsing
# ------------------------------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --target|-t|-Target)
            if [ -z "${2:-}" ]; then
                log_error "Option '$1' requires a target name."
                exit 1
            fi
            TARGET="$2"
            shift 2
            ;;
        --target=*|-Target=*)
            TARGET="${1#*=}"
            shift
            ;;
        --build-dir|-b|-BuildDir)
            if [ -z "${2:-}" ]; then
                log_error "Option '$1' requires a directory path."
                exit 1
            fi
            BUILD_DIR="$2"
            shift 2
            ;;
        --build-dir=*|-BuildDir=*)
            BUILD_DIR="${1#*=}"
            shift
            ;;
        --clean|-Clean)
            CLEAN=true
            shift
            ;;
        --rebuild|-Rebuild)
            REBUILD=true
            shift
            ;;
        --reconfigure|-Reconfigure)
            RECONFIGURE=true
            shift
            ;;
        --compiler|-Compiler)
            if [ -z "${2:-}" ]; then
                log_error "Option '$1' requires a compiler path."
                exit 1
            fi
            COMPILER="$2"
            shift 2
            ;;
        --compiler=*|-Compiler=*)
            COMPILER="${1#*=}"
            shift
            ;;
        -j|--parallel-jobs|-ParallelJobs)
            if [ -z "${2:-}" ]; then
                log_error "Option '$1' requires an integer argument."
                exit 1
            fi
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        -j*|--parallel-jobs=*)
            if [[ "$1" == -j* ]] && [ "$1" != "-j" ]; then
                PARALLEL_JOBS="${1#-j}"
            else
                PARALLEL_JOBS="${1#*=}"
            fi
            shift
            ;;
        --sign-conversion-only)
            SIGN_CONVERSION_ONLY=true
            shift
            ;;
        --all-warnings)
            ALL_WARNINGS=true
            shift
            ;;
        --fail-on-warning)
            FAIL_ON_WARNING=true
            shift
            ;;
        --filter)
            if [ -z "${2:-}" ]; then
                log_error "Option '$1' requires a regex pattern."
                exit 1
            fi
            FILTER_PATTERN="$2"
            shift 2
            ;;
        --filter=*)
            FILTER_PATTERN="${1#*=}"
            shift
            ;;
        --json)
            if [ -z "${2:-}" ]; then
                log_error "Option '$1' requires a file path."
                exit 1
            fi
            JSON_OUTPUT="$2"
            shift 2
            ;;
        --json=*)
            JSON_OUTPUT="${1#*=}"
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            log_error "Unrecognized option: $1"
            echo "Use ./scripts/audit_system_headers.sh --help for usage information." >&2
            exit 1
            ;;
    esac
done

# ------------------------------------------------------------------------------
# Dependency & Compiler Validation
# ------------------------------------------------------------------------------
log_header "================================================================================"
log_header "               Clang System-Headers Audit Tooling (WP 5)                        "
log_header "================================================================================"

if ! command -v cmake >/dev/null 2>&1; then
    log_error "CMake executable not found in PATH."
    exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
    log_error "Python 3 is required for diagnostic parsing."
    exit 1
fi

# Locate Clang C and C++ compilers
if ! command -v "$COMPILER" >/dev/null 2>&1; then
    log_error "Clang++ compiler not found: $COMPILER"
    echo "Clang is required for system headers audit (-Wsign-conversion -Wsystem-headers)." >&2
    exit 1
fi

CLANG_C_COMPILER="clang"
if [[ "$COMPILER" == *"clang++"* ]]; then
    CLANG_C_COMPILER="${COMPILER/clang++/clang}"
fi
if ! command -v "$CLANG_C_COMPILER" >/dev/null 2>&1; then
    CLANG_C_COMPILER="clang"
fi

CLANG_VERSION=$("$COMPILER" --version 2>/dev/null | head -n 1 || echo "unknown")
log_ok "[OK] Compiler              : $COMPILER ($CLANG_VERSION)"
log_ok "[OK] Workspace Root        : $WORKSPACE_ROOT"

# Resolve Build Directory
if [[ "$BUILD_DIR" = /* ]]; then
    BUILD_FULL_PATH="$BUILD_DIR"
else
    BUILD_FULL_PATH="$WORKSPACE_ROOT/$BUILD_DIR"
fi
log_ok "[OK] Audit Build Tree      : $BUILD_FULL_PATH"

if [ -n "$TARGET" ]; then
    log_ok "[OK] Audit Target          : $TARGET"
else
    log_ok "[OK] Audit Target          : all targets"
fi

# Determine Parallel Jobs
if [ "$PARALLEL_JOBS" -le 0 ] 2>/dev/null; then
    if command -v nproc >/dev/null 2>&1; then
        PARALLEL_JOBS=$(nproc)
    elif command -v sysctl >/dev/null 2>&1; then
        PARALLEL_JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
    else
        PARALLEL_JOBS=4
    fi
fi
log_ok "[OK] Parallel Jobs         : $PARALLEL_JOBS"

# ------------------------------------------------------------------------------
# Auto-detect Generator (prefer Ninja for contiguous diagnostics)
# ------------------------------------------------------------------------------
GENERATOR=""
if command -v ninja >/dev/null 2>&1; then
    GENERATOR="Ninja"
    log_ok "[OK] CMake Generator       : Ninja"
else
    log_warn "[INFO] Ninja not found; falling back to default generator."
fi

# ------------------------------------------------------------------------------
# 1. Clean Build Directory (if requested)
# ------------------------------------------------------------------------------
if [ "$CLEAN" = true ]; then
    log_warn "\n[1/3] Cleaning audit build directory..."
    if [ -d "$BUILD_FULL_PATH" ]; then
        rm -rf "$BUILD_FULL_PATH"
        log_ok "[OK] Audit build directory removed."
    fi
fi

mkdir -p "$BUILD_FULL_PATH"

# ------------------------------------------------------------------------------
# 2. Configure CMake with Audit Flags
# ------------------------------------------------------------------------------
CACHE_FILE="$BUILD_FULL_PATH/CMakeCache.txt"
NEEDS_CONFIGURE=false

if [ "$CLEAN" = true ] || [ "$RECONFIGURE" = true ] || [ ! -f "$CACHE_FILE" ]; then
    NEEDS_CONFIGURE=true
fi

if [ "$NEEDS_CONFIGURE" = true ]; then
    log_warn "\n[2/3] Configuring CMake with -DENABLE_SYSTEM_HEADERS_AUDIT=ON..."

    CMAKE_ARGS=(
        "-B" "$BUILD_FULL_PATH"
        "-S" "$WORKSPACE_ROOT"
        "-DCMAKE_BUILD_TYPE=Debug"
        "-DCMAKE_C_COMPILER=$CLANG_C_COMPILER"
        "-DCMAKE_CXX_COMPILER=$COMPILER"
        "-DENABLE_SYSTEM_HEADERS_AUDIT=ON"
        "-DWARNINGS_AS_ERRORS=OFF"
        "-DPELCOD_ENABLE_ONVIF=ON"
    )

    if [ -n "$GENERATOR" ]; then
        CMAKE_ARGS+=("-G" "$GENERATOR")
    fi

    # Auto-detect Qt prefix path if available
    if [ -d "/usr/lib/x86_64-linux-gnu/cmake/Qt6" ] || [ -d "/usr/lib/cmake/Qt6" ]; then
        CMAKE_ARGS+=("-DCMAKE_PREFIX_PATH=/usr")
    fi

    log_info "Running: cmake ${CMAKE_ARGS[*]}"
    set +e
    cmake "${CMAKE_ARGS[@]}"
    EXIT_CODE=$?
    set -e

    if [ $EXIT_CODE -ne 0 ]; then
        log_error "CMake configuration failed with exit code $EXIT_CODE"
        exit $EXIT_CODE
    fi
    log_ok "[OK] CMake audit configuration succeeded."
else
    log_info "\n[2/3] Audit build tree already configured (skipping configure step)."
fi

# ------------------------------------------------------------------------------
# 3. Compile and Capture Diagnostics
# ------------------------------------------------------------------------------
log_warn "\n[3/3] Compiling targets and auditing system headers..."

LOG_FILE="$BUILD_FULL_PATH/audit_compiler_output.log"
: > "$LOG_FILE"

BUILD_ARGS=(
    "--build" "$BUILD_FULL_PATH"
    "--parallel" "$PARALLEL_JOBS"
)

if [ "$REBUILD" = true ]; then
    BUILD_ARGS+=("--clean-first")
fi

if [ -n "$TARGET" ]; then
    BUILD_ARGS+=("--target" "$TARGET")
fi

log_info "Running: cmake ${BUILD_ARGS[*]}"
log_info "Capturing compiler diagnostics to: $LOG_FILE"

set +e
if [ "$VERBOSE" = true ]; then
    cmake "${BUILD_ARGS[@]}" 2>&1 | tee "$LOG_FILE"
    BUILD_EXIT_CODE=${PIPESTATUS[0]}
else
    cmake "${BUILD_ARGS[@]}" > "$LOG_FILE" 2>&1
    BUILD_EXIT_CODE=$?
fi
set -e

if [ $BUILD_EXIT_CODE -ne 0 ]; then
    log_error "Compilation failed with exit code $BUILD_EXIT_CODE"
    echo "Check raw compiler output in: $LOG_FILE" >&2
    exit $BUILD_EXIT_CODE
fi

log_ok "[OK] Compilation completed successfully."

# ------------------------------------------------------------------------------
# 4. Parse and Filter Diagnostic Findings
# ------------------------------------------------------------------------------
PARSE_ARGS=(
    "$LOG_FILE"
    "--workspace-root" "$WORKSPACE_ROOT"
)

if [ -n "$TARGET" ]; then
    PARSE_ARGS+=("--target" "$TARGET")
fi

if [ "$SIGN_CONVERSION_ONLY" = true ]; then
    PARSE_ARGS+=("--sign-conversion-only")
fi

if [ "$ALL_WARNINGS" = true ]; then
    PARSE_ARGS+=("--all-warnings")
fi

if [ "$FAIL_ON_WARNING" = true ]; then
    PARSE_ARGS+=("--fail-on-warning")
fi

if [ -n "$FILTER_PATTERN" ]; then
    PARSE_ARGS+=("--filter" "$FILTER_PATTERN")
fi

if [ -n "$JSON_OUTPUT" ]; then
    PARSE_ARGS+=("--json" "$JSON_OUTPUT")
fi

if [ "$VERBOSE" = true ]; then
    PARSE_ARGS+=("--verbose")
fi

set +e
python3 "$SCRIPT_DIR/parse_audit_log.py" "${PARSE_ARGS[@]}"
AUDIT_EXIT_CODE=$?
set -e

exit $AUDIT_EXIT_CODE
