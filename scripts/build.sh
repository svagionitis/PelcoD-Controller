#!/usr/bin/env bash
# ==============================================================================
# Pelco-D Controller - Build & Pipeline Automator
#
# Cross-platform build script mirroring scripts/Build.ps1 for Linux, macOS,
# and POSIX environments (WSL, Git Bash, MSYS2).
#
# Automates CMake configuration, compilation, dependency resolution (vcpkg or system),
# Qt 6 integration, runtime dependency deployment, and automated test execution.
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
if [ -t 1 ]; then
    CYAN='\033[0;36m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    RED='\033[0;31m'
    GRAY='\033[0;90m'
    NC='\033[0m'
else
    CYAN=''
    GREEN=''
    YELLOW=''
    RED=''
    GRAY=''
    NC=''
fi

log_header() {
    echo -e "${CYAN}$1${NC}"
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
    echo -e "${RED}$1${NC}" >&2
}

show_help() {
    cat << 'EOF'
Cross-platform build script for Pelco-D Controller, Sightline SLA Suite, and libraries.

Usage:
    ./scripts/build.sh [OPTIONS]

Options:
    -c, --config <Config>
        Build configuration: "Release" (default), "Debug", "RelWithDebInfo", or "MinSizeRel".
        (Also accepts -Config <Config>)

    -b, --build-dir <Dir>
        Build directory path relative to workspace root (default: "build").
        (Also accepts -BuildDir <Dir>)

    -t, --target <Target>
        Optional specific CMake target to build (e.g. "SightlineAppQt", "TestSightlineVideo", "PelcoDCore").
        Builds all targets if omitted.
        (Also accepts -Target <Target>)

    --clean
        Removes the build directory before configuring.
        (Also accepts -Clean)

    --reconfigure
        Force re-running CMake configuration even if CMakeCache.txt already exists.
        (Also accepts -Reconfigure)

    --run-tests, --tests
        Executes CTest after a successful build.
        (Also accepts -RunTests)

    -R, --test-filter <Pattern>
        Filter regex to select specific tests to execute via CTest (implies --run-tests).
        (Also accepts -TestFilter <Pattern>)

    --qt-dir <Path>
        Path to Qt 6 directory. Auto-detected via qmake6/qmake or standard search paths if omitted.
        (Also accepts -QtDir <Path>)

    --vcpkg-toolchain <Path>
        Path to vcpkg CMake toolchain file (vcpkg.cmake). Auto-detected if VCPKG_ROOT is set or present.
        (Also accepts -VcpkgToolchain <Path>)

    -j, --parallel-jobs <N>
        Number of parallel compilation processes. Defaults to logical CPU count.
        (Also accepts -ParallelJobs <N>)

    -h, --help
        Display this help message and exit.

Examples:
    ./scripts/build.sh
        Builds all targets in Release configuration.

    ./scripts/build.sh --config Debug --run-tests
        Builds all targets in Debug configuration and runs all unit tests.

    ./scripts/build.sh --run-tests -R "TestTacticalHud.*"
        Runs unit tests matching the specified filter regex.

    ./scripts/build.sh --target SightlineAppQt
        Builds only the Sightline SLA Qt control application.

    ./scripts/build.sh --clean --config Release
        Performs a clean build from scratch.
EOF
}

# ------------------------------------------------------------------------------
# Default Options
# ------------------------------------------------------------------------------
CONFIG="Release"
BUILD_DIR="build"
TARGET=""
CLEAN=false
RECONFIGURE=false
RUN_TESTS=false
TEST_FILTER=""
QT_DIR=""
VCPKG_TOOLCHAIN=""
PARALLEL_JOBS=0

# ------------------------------------------------------------------------------
# Argument Parsing (supports both GNU --option and PowerShell -Option formats)
# ------------------------------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --config|-c|-Config)
            if [ -z "${2:-}" ]; then
                log_error "Option '$1' requires an argument."
                exit 1
            fi
            CONFIG="$2"
            shift 2
            ;;
        --config=*|-Config=*)
            CONFIG="${1#*=}"
            shift
            ;;
        --build-dir|-b|-BuildDir)
            if [ -z "${2:-}" ]; then
                log_error "Option '$1' requires an argument."
                exit 1
            fi
            BUILD_DIR="$2"
            shift 2
            ;;
        --build-dir=*|-BuildDir=*)
            BUILD_DIR="${1#*=}"
            shift
            ;;
        --target|-t|-Target)
            if [ -z "${2:-}" ]; then
                log_error "Option '$1' requires an argument."
                exit 1
            fi
            TARGET="$2"
            shift 2
            ;;
        --target=*|-Target=*)
            TARGET="${1#*=}"
            shift
            ;;
        --clean|-Clean)
            CLEAN=true
            shift
            ;;
        --reconfigure|-Reconfigure)
            RECONFIGURE=true
            shift
            ;;
        --run-tests|--tests|-RunTests|-run-tests)
            RUN_TESTS=true
            shift
            ;;
        --test-filter|-R|-TestFilter)
            if [ -z "${2:-}" ]; then
                log_error "Option '$1' requires an argument."
                exit 1
            fi
            TEST_FILTER="$2"
            RUN_TESTS=true
            shift 2
            ;;
        --test-filter=*|-TestFilter=*)
            TEST_FILTER="${1#*=}"
            RUN_TESTS=true
            shift
            ;;
        --qt-dir|-QtDir)
            if [ -z "${2:-}" ]; then
                log_error "Option '$1' requires an argument."
                exit 1
            fi
            QT_DIR="$2"
            shift 2
            ;;
        --qt-dir=*|-QtDir=*)
            QT_DIR="${1#*=}"
            shift
            ;;
        --vcpkg-toolchain|-VcpkgToolchain)
            if [ -z "${2:-}" ]; then
                log_error "Option '$1' requires an argument."
                exit 1
            fi
            VCPKG_TOOLCHAIN="$2"
            shift 2
            ;;
        --vcpkg-toolchain=*|-VcpkgToolchain=*)
            VCPKG_TOOLCHAIN="${1#*=}"
            shift
            ;;
        --parallel-jobs|-j|-ParallelJobs)
            if [ -z "${2:-}" ]; then
                log_error "Option '$1' requires an argument."
                exit 1
            fi
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --parallel-jobs=*|-ParallelJobs=*)
            PARALLEL_JOBS="${1#*=}"
            shift
            ;;
        -h|--help|-Help)
            show_help
            exit 0
            ;;
        *)
            log_error "Unknown argument: $1"
            echo "Run '$0 --help' for usage information." >&2
            exit 1
            ;;
    esac
done

# Validate Configuration
CONFIG_LOWER=$(echo "$CONFIG" | tr '[:upper:]' '[:lower:]')
case "$CONFIG_LOWER" in
    release)
        CONFIG="Release"
        ;;
    debug)
        CONFIG="Debug"
        ;;
    relwithdebinfo)
        CONFIG="RelWithDebInfo"
        ;;
    minsizerel)
        CONFIG="MinSizeRel"
        ;;
    *)
        log_error "Invalid configuration: '$CONFIG'. Allowed values: Release, Debug, RelWithDebInfo, MinSizeRel."
        exit 1
        ;;
esac

# ------------------------------------------------------------------------------
# Path Resolution & Timer Setup
# ------------------------------------------------------------------------------
START_TIME=$(date +%s)
START_TIME_MS=$(python3 -c "import time; print(int(time.time() * 1000))" 2>/dev/null || true)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

if [[ "$BUILD_DIR" = /* ]]; then
    BUILD_FULL_PATH="$BUILD_DIR"
else
    BUILD_FULL_PATH="$WORKSPACE_ROOT/$BUILD_DIR"
fi

log_header "================================================================="
log_header " Pelco-D Controller - Build & Pipeline Automator                 "
log_info " Configuration : $CONFIG"
log_info " Workspace     : $WORKSPACE_ROOT"
log_info " Build Dir     : $BUILD_FULL_PATH"
if [ -n "$TARGET" ]; then
    log_info " Target        : $TARGET"
fi
log_header "================================================================="

# ------------------------------------------------------------------------------
# 1. Detect CMake & CTest
# ------------------------------------------------------------------------------
if ! command -v cmake >/dev/null 2>&1; then
    log_error "CMake is not found in PATH! Please install CMake 3.16+ from https://cmake.org/download/"
    exit 1
fi

if [ "$RUN_TESTS" = true ] && ! command -v ctest >/dev/null 2>&1; then
    log_error "CTest is not found in PATH! Please install CMake/CTest."
    exit 1
fi

# ------------------------------------------------------------------------------
# 2. Detect vcpkg CMake Toolchain
# ------------------------------------------------------------------------------
if [ -z "$VCPKG_TOOLCHAIN" ]; then
    vcpkg_candidates=(
        "${VCPKG_ROOT:-}/scripts/buildsystems/vcpkg.cmake"
        "$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake"
        "/opt/vcpkg/scripts/buildsystems/vcpkg.cmake"
        "/usr/local/share/vcpkg/scripts/buildsystems/vcpkg.cmake"
        "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
        "/c/vcpkg/scripts/buildsystems/vcpkg.cmake"
    )
    for cand in "${vcpkg_candidates[@]}"; do
        if [ -n "$cand" ] && [ -f "$cand" ]; then
            VCPKG_TOOLCHAIN="$cand"
            break
        fi
    done
fi

if [ -n "$VCPKG_TOOLCHAIN" ]; then
    if [ ! -f "$VCPKG_TOOLCHAIN" ]; then
        log_error "vcpkg.cmake toolchain not found at specified path: $VCPKG_TOOLCHAIN"
        exit 1
    fi
    log_ok "[OK] Found vcpkg toolchain : $VCPKG_TOOLCHAIN"
else
    case "$(uname -s 2>/dev/null || true)" in
        MINGW*|MSYS*|CYGWIN*)
            log_error "vcpkg.cmake toolchain not found! Specify --vcpkg-toolchain or set VCPKG_ROOT."
            exit 1
            ;;
        *)
            log_info "[INFO] No vcpkg toolchain found; using system package manager & CMake defaults."
            ;;
    esac
fi

# ------------------------------------------------------------------------------
# 3. Detect Qt 6
# ------------------------------------------------------------------------------
if [ -z "$QT_DIR" ]; then
    if [ -n "${QTDIR:-}" ] && [ -d "$QTDIR" ]; then
        QT_DIR="$QTDIR"
    elif command -v qmake6 >/dev/null 2>&1; then
        detected_prefix=$(qmake6 -query QT_INSTALL_PREFIX 2>/dev/null || true)
        if [ -n "$detected_prefix" ] && [ -d "$detected_prefix" ]; then
            QT_DIR="$detected_prefix"
        fi
    elif command -v qmake >/dev/null 2>&1; then
        detected_prefix=$(qmake -query QT_INSTALL_PREFIX 2>/dev/null || true)
        if [ -n "$detected_prefix" ] && [ -d "$detected_prefix" ]; then
            QT_DIR="$detected_prefix"
        fi
    fi

    if [ -z "$QT_DIR" ]; then
        qt_candidates=(
            "$HOME"/Qt/6.*/gcc_64
            /opt/Qt/6.*/gcc_64
            /usr/lib/qt6
            /usr/lib/x86_64-linux-gnu/qt6
            /c/Qt/6.*/msvc*
            "C:/Qt/6."*/msvc*
        )
        for cand in "${qt_candidates[@]}"; do
            if [ -d "$cand" ] && { [ -f "$cand/bin/qmake" ] || [ -f "$cand/bin/qmake.exe" ] || [ -d "$cand/lib/cmake/Qt6" ]; }; then
                QT_DIR="$cand"
                break
            fi
        done
    fi
fi

if [ -n "$QT_DIR" ]; then
    if [ ! -d "$QT_DIR" ]; then
        log_error "Qt directory does not exist: $QT_DIR"
        exit 1
    fi
    log_ok "[OK] Found Qt 6 prefix     : $QT_DIR"
else
    # On Linux, Qt might reside in standard system paths (/usr)
    if [ -d "/usr/lib/x86_64-linux-gnu/cmake/Qt6" ] || [ -d "/usr/lib/cmake/Qt6" ]; then
        QT_DIR="/usr"
        log_ok "[OK] Found system Qt 6 prefix : $QT_DIR"
    else
        log_warn "[WARN] Qt 6 prefix not detected. CMake will attempt to find Qt 6 in standard search locations."
    fi
fi

# ------------------------------------------------------------------------------
# Parallel Jobs Calculation
# ------------------------------------------------------------------------------
if [ "$PARALLEL_JOBS" -le 0 ] 2>/dev/null; then
    if command -v nproc >/dev/null 2>&1; then
        PARALLEL_JOBS=$(nproc)
    elif command -v sysctl >/dev/null 2>&1; then
        PARALLEL_JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
    elif command -v getconf >/dev/null 2>&1; then
        PARALLEL_JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
    else
        PARALLEL_JOBS=4
    fi
fi
log_ok "[OK] Parallel build jobs   : $PARALLEL_JOBS"

# ------------------------------------------------------------------------------
# 4. Clean Build Directory (if requested)
# ------------------------------------------------------------------------------
if [ "$CLEAN" = true ]; then
    log_warn "\n[1/4] Cleaning build directory..."
    if [ -d "$BUILD_FULL_PATH" ]; then
        rm -rf "$BUILD_FULL_PATH"
        log_ok "[OK] Build directory removed."
    else
        log_ok "[OK] Build directory does not exist, nothing to clean."
    fi
fi

# ------------------------------------------------------------------------------
# 5. CMake Configure
# ------------------------------------------------------------------------------
CACHE_FILE="$BUILD_FULL_PATH/CMakeCache.txt"
NEEDS_CONFIGURE=false

if [ "$CLEAN" = true ] || [ "$RECONFIGURE" = true ] || [ ! -f "$CACHE_FILE" ]; then
    NEEDS_CONFIGURE=true
fi

# Validate build type match in existing cache
if [ "$NEEDS_CONFIGURE" = false ] && [ -f "$CACHE_FILE" ]; then
    cached_type=$(grep -E "^CMAKE_BUILD_TYPE:STRING=" "$CACHE_FILE" 2>/dev/null | cut -d'=' -f2 || true)
    if [ -n "$cached_type" ] && [ "$(echo "$cached_type" | tr '[:upper:]' '[:lower:]')" != "$(echo "$CONFIG" | tr '[:upper:]' '[:lower:]')" ]; then
        log_warn "[INFO] Cached CMAKE_BUILD_TYPE ($cached_type) differs from requested ($CONFIG). Forcing reconfigure..."
        NEEDS_CONFIGURE=true
    fi
fi

# Validate Visual Studio instance health in existing cache (if on Windows)
if [ "$NEEDS_CONFIGURE" = false ] && [ -f "$CACHE_FILE" ]; then
    if grep -qE "CMAKE_GENERATOR_INSTANCE:[^=]*=[^,\r\n]+,version=" "$CACHE_FILE" 2>/dev/null; then
        log_warn "[INFO] Detected pinned Visual Studio version in CMakeCache.txt. Normalizing..."
        sed -i -E 's/^(CMAKE_GENERATOR_INSTANCE:[^=]*=[^,\r\n]+),version=[^\r\n]+/\1/' "$CACHE_FILE"
    fi
fi

if [ "$NEEDS_CONFIGURE" = true ]; then
    log_warn "\n[2/4] Configuring project with CMake..."
    mkdir -p "$BUILD_FULL_PATH"

    CMAKE_ARGS=(
        "-B" "$BUILD_FULL_PATH"
        "-S" "$WORKSPACE_ROOT"
        "-DCMAKE_BUILD_TYPE=$CONFIG"
    )

    if [ -n "$VCPKG_TOOLCHAIN" ]; then
        CMAKE_ARGS+=("-DCMAKE_TOOLCHAIN_FILE=$VCPKG_TOOLCHAIN")
    fi

    if [ -n "$QT_DIR" ]; then
        CMAKE_ARGS+=("-DCMAKE_PREFIX_PATH=$QT_DIR")
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
    log_ok "[OK] CMake configuration succeeded."
else
    log_info "\n[2/4] CMake build tree is already configured (skipping configure step)."
fi

# ------------------------------------------------------------------------------
# 6. CMake Build
# ------------------------------------------------------------------------------
log_warn "\n[3/4] Compiling targets ($CONFIG)..."

# Clean up orphaned Ninja recompaction temporary files if left behind by interrupted runs
RESTAT_FILE="$BUILD_FULL_PATH/.ninja_log.restat"
LOG_FILE="$BUILD_FULL_PATH/.ninja_log"
if [ -f "$RESTAT_FILE" ]; then
    if [ ! -f "$LOG_FILE" ]; then
        mv -f "$RESTAT_FILE" "$LOG_FILE" 2>/dev/null || true
    else
        rm -f "$RESTAT_FILE" 2>/dev/null || true
    fi
fi
RECOMPACT_FILE="$BUILD_FULL_PATH/.ninja_deps.recompact"
if [ -f "$RECOMPACT_FILE" ]; then
    rm -f "$RECOMPACT_FILE" 2>/dev/null || true
fi

BUILD_ARGS=(
    "--build" "$BUILD_FULL_PATH"
    "--config" "$CONFIG"
    "--parallel" "$PARALLEL_JOBS"
)

if [ -n "$TARGET" ]; then
    BUILD_ARGS+=("--target" "$TARGET")
fi

log_info "Running: cmake ${BUILD_ARGS[*]}"
set +e
cmake "${BUILD_ARGS[@]}"
EXIT_CODE=$?
set -e
if [ $EXIT_CODE -ne 0 ]; then
    log_error "Compilation failed with exit code $EXIT_CODE"
    exit $EXIT_CODE
fi
log_ok "[OK] Compilation completed successfully."

# ------------------------------------------------------------------------------
# 7. Deploy Runtime Dependencies (vcpkg DLLs / shared libs)
# ------------------------------------------------------------------------------
log_warn "\n[4/4] Verifying and deploying runtime dependencies..."

VCPKG_BIN_DIR=""
if [ "$CONFIG" = "Debug" ]; then
    if [ -d "$WORKSPACE_ROOT/vcpkg_installed/x64-windows/debug/bin" ]; then
        VCPKG_BIN_DIR="$WORKSPACE_ROOT/vcpkg_installed/x64-windows/debug/bin"
    fi
else
    if [ -d "$WORKSPACE_ROOT/vcpkg_installed/x64-windows/bin" ]; then
        VCPKG_BIN_DIR="$WORKSPACE_ROOT/vcpkg_installed/x64-windows/bin"
    fi
fi

APP_OUTPUT_DIRS=(
    "$BUILD_FULL_PATH/app-sightline-qt/$CONFIG"
    "$BUILD_FULL_PATH/app-video-qt/$CONFIG"
    "$BUILD_FULL_PATH/app-pelcod-qt/$CONFIG"
    "$BUILD_FULL_PATH/app-fujinon-qt/$CONFIG"
    "$BUILD_FULL_PATH/app-visca-qt/$CONFIG"
    "$BUILD_FULL_PATH/app-onvif-qt/$CONFIG"
    "$BUILD_FULL_PATH/app-sightline-qt"
    "$BUILD_FULL_PATH/app-video-qt"
    "$BUILD_FULL_PATH/app-pelcod-qt"
    "$BUILD_FULL_PATH/app-fujinon-qt"
    "$BUILD_FULL_PATH/app-visca-qt"
    "$BUILD_FULL_PATH/app-onvif-qt"
)

if [ -n "$VCPKG_BIN_DIR" ] && [ -d "$VCPKG_BIN_DIR" ]; then
    DLL_FILTERS=("avformat*" "avcodec*" "avutil*" "swscale*" "swresample*" "glog*" "gflags*")
    for out_dir in "${APP_OUTPUT_DIRS[@]}"; do
        if [ -d "$out_dir" ]; then
            has_exe=false
            for f in "$out_dir"/*.exe "$out_dir"/*App* "$out_dir"/*Controller*; do
                if [ -f "$f" ]; then
                    has_exe=true
                    break
                fi
            done
            if [ "$has_exe" = true ]; then
                for filter in "${DLL_FILTERS[@]}"; do
                    for dll in "$VCPKG_BIN_DIR"/$filter.dll; do
                        if [ -f "$dll" ]; then
                            dest="$out_dir/$(basename "$dll")"
                            if [ ! -f "$dest" ]; then
                                cp -f "$dll" "$dest"
                            fi
                        fi
                    done
                done
            fi
        fi
    done
fi
log_ok "[OK] Runtime dependency deployment verified."

# ------------------------------------------------------------------------------
# 8. Run Automated Test Suite (if requested)
# ------------------------------------------------------------------------------
if [ "$RUN_TESTS" = true ]; then
    log_header "\n[TEST] Executing automated test suite..."

    if [ -n "$QT_DIR" ] && [ -d "$QT_DIR/bin" ]; then
        export PATH="$QT_DIR/bin:$PATH"
    fi
    if [ -n "$QT_DIR" ] && [ -d "$QT_DIR/lib" ]; then
        export LD_LIBRARY_PATH="$QT_DIR/lib:${LD_LIBRARY_PATH:-}"
    fi
    if [ -n "$VCPKG_BIN_DIR" ] && [ -d "$VCPKG_BIN_DIR" ]; then
        export PATH="$VCPKG_BIN_DIR:$PATH"
        export LD_LIBRARY_PATH="$VCPKG_BIN_DIR:${LD_LIBRARY_PATH:-}"
    fi
    export QT_QPA_PLATFORM="offscreen"

    CTEST_ARGS=(
        "--test-dir" "$BUILD_FULL_PATH"
        "-C" "$CONFIG"
        "--output-on-failure"
        "--parallel" "$PARALLEL_JOBS"
    )

    if [ -n "$TEST_FILTER" ]; then
        CTEST_ARGS+=("-R" "$TEST_FILTER")
    fi

    log_info "Running: ctest ${CTEST_ARGS[*]}"
    set +e
    ctest "${CTEST_ARGS[@]}"
    EXIT_CODE=$?
    set -e
    if [ $EXIT_CODE -ne 0 ]; then
        log_error "One or more tests failed! Exit code: $EXIT_CODE"
        exit $EXIT_CODE
    fi
    log_ok "[OK] All test suites passed successfully!"
fi

# ------------------------------------------------------------------------------
# Elapsed Time & Success
# ------------------------------------------------------------------------------
END_TIME=$(date +%s)
if [ -n "$START_TIME_MS" ]; then
    END_TIME_MS=$(python3 -c "import time; print(int(time.time() * 1000))" 2>/dev/null || true)
    if [ -n "$END_TIME_MS" ]; then
        ELAPSED_SEC=$(python3 -c "print(round(($END_TIME_MS - $START_TIME_MS) / 1000.0, 1))" 2>/dev/null || echo "$((END_TIME - START_TIME))")
    else
        ELAPSED_SEC=$((END_TIME - START_TIME))
    fi
else
    ELAPSED_SEC=$((END_TIME - START_TIME))
fi

log_header "\n================================================================="
log_ok " BUILD SUCCEEDED in ${ELAPSED_SEC}s"
log_header "================================================================="
exit 0
